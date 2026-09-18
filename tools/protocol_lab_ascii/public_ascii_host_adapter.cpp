#include "public_ascii_host_adapter.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace pae::protocol_lab_ascii::public_offline {
namespace {

constexpr std::size_t kMax = (std::numeric_limits<std::size_t>::max)();

std::size_t Add(std::size_t left, std::size_t right) noexcept {
  return left > kMax - right ? kMax : left + right;
}

std::size_t Multiply(std::size_t left, std::size_t right) noexcept {
  return left != 0U && right > kMax / left ? kMax : left * right;
}

std::size_t StringBytes(const std::string& value) noexcept { return Add(value.size(), 1U); }

const OwnedMessageDescription* Message(const OwnedDescription& description,
                                       std::size_t index) noexcept {
  return index < description.messages.size() ? &description.messages[index] : nullptr;
}

const OwnedFieldDescription* Field(const OwnedMessageDescription& message,
                                   std::size_t index) noexcept {
  return index < message.fields.size() ? &message.fields[index] : nullptr;
}

const InputField* Input(const std::vector<InputField>& inputs, std::size_t index) noexcept {
  const auto found = std::find_if(inputs.begin(), inputs.end(), [index](const auto& value) {
    return value.field_index == index;
  });
  return found == inputs.end() ? nullptr : &*found;
}

bool Range(ByteView frame, ByteView field, ByteRange& output) noexcept {
  if (frame.data == nullptr || field.data == nullptr) return false;
  const auto begin = reinterpret_cast<std::uintptr_t>(frame.data);
  const auto pointer = reinterpret_cast<std::uintptr_t>(field.data);
  if (frame.size > (std::numeric_limits<std::uintptr_t>::max)() - begin) return false;
  const auto end = begin + frame.size;
  if (pointer < begin || pointer > end || field.size > end - pointer) return false;
  output = {static_cast<std::size_t>(pointer - begin), field.size};
  return true;
}

OperationResult CopyDecode(const Adapter& owner, const HostCandidateView& view) {
  OperationResult result;
  result.codec_called = true;
  result.codec_status = view.decode_status;
  result.matched_message_index = view.matched_message_index;
  result.failed_field_flat_index = view.failed_field_flat_index;
  result.conversion_error = view.conversion_error;
  result.output_tainted = view.output_tainted;
  if ((view.frame.data == nullptr && view.frame.size != 0U) ||
      view.frame.size > owner.LimitsForHost().max_frame_bytes) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
    return result;
  }
  if (view.decode_status != CodecStatus::OK || !view.matched_message_index ||
      !view.record.HasValue() || view.record.MessageIndex() != *view.matched_message_index) {
    result.local_status = LocalStatus::CODEC_FAILED;
    const auto needed = Add(sizeof(OperationResult), view.frame.size);
    if (needed > owner.LimitsForHost().max_result_bytes) {
      result.local_status = LocalStatus::MATERIALIZATION_FAILED;
      return result;
    }
    if (view.frame.size != 0U) {
      result.diagnostic_input_frame.assign(view.frame.data, view.frame.data + view.frame.size);
    }
    result.accounted_bytes = needed;
    return result;
  }
  const auto* message = Message(owner.Description(), view.record.MessageIndex());
  if (!message || view.record.FieldCount() > owner.LimitsForHost().max_fields) return result;
  result.local_status = LocalStatus::MATERIALIZATION_FAILED;
  std::size_t needed = Add(sizeof(OperationResult), view.frame.size);
  needed = Add(needed, StringBytes(message->id));
  needed = Add(needed, Multiply(view.record.FieldCount(), sizeof(OwnedFieldValue)));
  for (std::size_t ordinal = 0U; ordinal < view.record.FieldCount(); ++ordinal) {
    const auto queried = view.record.Field(ordinal);
    if (!queried || !queried->HasValue() || queried->Kind() != ValueKind::BYTES) return result;
    const auto bytes = queried->Bytes();
    const auto* field = Field(*message, queried->Field().field_index);
    ByteRange range;
    if (!bytes || !field || field->flat_index != queried->FlatFieldIndex() ||
        bytes->size > owner.LimitsForHost().max_field_bytes || !Range(view.frame, *bytes, range))
      return result;
    needed = Add(needed, StringBytes(field->id));
    needed = Add(needed, bytes->size);
  }
  if (needed > owner.LimitsForHost().max_result_bytes) return result;
  result.message_index = message->index;
  result.message_id = message->id;
  if (view.frame.size != 0U)
    result.frame.assign(view.frame.data, view.frame.data + view.frame.size);
  result.fields.reserve(view.record.FieldCount());
  for (std::size_t ordinal = 0U; ordinal < view.record.FieldCount(); ++ordinal) {
    const auto queried = view.record.Field(ordinal);
    if (!queried || !queried->HasValue() || queried->Kind() != ValueKind::BYTES) return {};
    const auto bytes = queried->Bytes();
    const auto* field = Field(*message, queried->Field().field_index);
    if (!bytes || !field || field->flat_index != queried->FlatFieldIndex() ||
        bytes->size > owner.LimitsForHost().max_field_bytes)
      return {};
    OwnedFieldValue copied;
    copied.flat_index = field->flat_index;
    copied.field_index = field->field_index;
    copied.id = field->id;
    if (bytes->size != 0U) copied.bytes.assign(bytes->data, bytes->data + bytes->size);
    if (!Range(view.frame, *bytes, copied.range)) return {};
    result.fields.push_back(std::move(copied));
  }
  result.local_status = LocalStatus::OK;
  result.accounted_bytes = needed;
  return result;
}

OperationResult CopyEncode(const Adapter& owner, std::size_t message_index,
                           const std::vector<InputField>& inputs, ByteView bytes) {
  OperationResult result;
  result.codec_called = true;
  result.codec_status = CodecStatus::OK;
  const auto* message = Message(owner.Description(), message_index);
  if (!message || !message->encode || bytes.size > owner.LimitsForHost().max_frame_bytes)
    return result;
  result.local_status = LocalStatus::MATERIALIZATION_FAILED;
  std::size_t needed = Add(sizeof(OperationResult), bytes.size);
  needed = Add(needed, StringBytes(message->id));
  std::size_t field_count = 0U;
  std::size_t cursor = 0U;
  for (const auto& segment : message->encode->segments) {
    if (segment.kind == AsciiSegmentKind::LITERAL) {
      if (cursor > bytes.size || segment.literal.size() > bytes.size - cursor ||
          (segment.literal.size() != 0U &&
           !std::equal(segment.literal.begin(), segment.literal.end(), bytes.data + cursor)))
        return result;
      cursor += segment.literal.size();
      continue;
    }
    if (!segment.field_index) return result;
    const auto* input = Input(inputs, *segment.field_index);
    const auto* field = Field(*message, *segment.field_index);
    if (!input || !field || input->bytes.size() > owner.LimitsForHost().max_field_bytes ||
        cursor > bytes.size || input->bytes.size() > bytes.size - cursor ||
        (input->bytes.size() != 0U &&
         !std::equal(input->bytes.begin(), input->bytes.end(), bytes.data + cursor)))
      return result;
    needed = Add(needed, StringBytes(field->id));
    needed = Add(needed, input->bytes.size());
    cursor += input->bytes.size();
    ++field_count;
  }
  needed = Add(needed, Multiply(field_count, sizeof(OwnedFieldValue)));
  if (cursor != bytes.size || needed > owner.LimitsForHost().max_result_bytes) return result;
  result.message_index = message_index;
  result.message_id = message->id;
  if (bytes.size != 0U) result.frame.assign(bytes.data, bytes.data + bytes.size);
  result.fields.reserve(field_count);
  cursor = 0U;
  for (const auto& segment : message->encode->segments) {
    if (segment.kind == AsciiSegmentKind::LITERAL) {
      if (cursor > bytes.size || segment.literal.size() > bytes.size - cursor ||
          (segment.literal.size() != 0U &&
           !std::equal(segment.literal.begin(), segment.literal.end(), bytes.data + cursor)))
        return {};
      cursor += segment.literal.size();
      continue;
    }
    if (!segment.field_index) return {};
    const auto* input = Input(inputs, *segment.field_index);
    const auto* field = Field(*message, *segment.field_index);
    if (!input || !field || cursor > bytes.size || input->bytes.size() > bytes.size - cursor ||
        (input->bytes.size() != 0U &&
         !std::equal(input->bytes.begin(), input->bytes.end(), bytes.data + cursor)))
      return {};
    OwnedFieldValue copied;
    copied.flat_index = field->flat_index;
    copied.field_index = field->field_index;
    copied.id = field->id;
    copied.bytes = input->bytes;
    copied.range = {cursor, input->bytes.size()};
    cursor += input->bytes.size();
    result.fields.push_back(std::move(copied));
  }
  if (cursor != bytes.size) return {};
  result.local_status = LocalStatus::OK;
  result.accounted_bytes = needed;
  return result;
}

struct DecodeContext {
  const Adapter* owner = nullptr;
  OperationResult copied;
  bool called = false;
  bool* fail_next_allocation = nullptr;
};

void AllocationCheckpoint(bool* fail_next_allocation) {
  if (fail_next_allocation != nullptr && *fail_next_allocation) {
    *fail_next_allocation = false;
    throw std::bad_alloc{};
  }
}

void PreserveDecodeFacts(OperationResult& result, const HostCandidateView& view,
                         LocalStatus local_status) noexcept {
  result = {};
  result.local_status = local_status;
  result.codec_called = true;
  result.codec_status = view.decode_status;
  result.matched_message_index = view.matched_message_index;
  result.failed_field_flat_index = view.failed_field_flat_index;
  result.conversion_error = view.conversion_error;
  result.output_tainted = view.output_tainted;
}

HostCallbackAction ObserveCandidate(const HostCandidateView& view, void* opaque) {
  auto& context = *static_cast<DecodeContext*>(opaque);
  if (view.decode_status == CodecStatus::OK) return HostCallbackAction::CONTINUE;
  try {
    AllocationCheckpoint(context.fail_next_allocation);
    context.copied = CopyDecode(*context.owner, view);
  } catch (const std::bad_alloc&) {
    PreserveDecodeFacts(context.copied, view, LocalStatus::ALLOCATION_FAILED);
  } catch (...) {
    PreserveDecodeFacts(context.copied, view, LocalStatus::MATERIALIZATION_FAILED);
  }
  context.called = true;
  return HostCallbackAction::CONTINUE;
}

HostCallbackAction CopyDecodedOutput(const HostOutputView& view, void* opaque) {
  auto& context = *static_cast<DecodeContext*>(opaque);
  HostCandidateView candidate;
  candidate.generation = view.generation;
  candidate.frame = view.frame;
  candidate.decode_status = CodecStatus::OK;
  candidate.record = view.record;
  candidate.matched_message_index = view.message_index;
  try {
    AllocationCheckpoint(context.fail_next_allocation);
    context.copied = CopyDecode(*context.owner, candidate);
  } catch (const std::bad_alloc&) {
    PreserveDecodeFacts(context.copied, candidate, LocalStatus::ALLOCATION_FAILED);
  } catch (...) {
    PreserveDecodeFacts(context.copied, candidate, LocalStatus::MATERIALIZATION_FAILED);
  }
  context.called = true;
  return HostCallbackAction::CONTINUE;
}

struct EncodeContext {
  const Adapter* owner = nullptr;
  const std::vector<InputField>* inputs = nullptr;
  OperationResult copied;
  bool called = false;
  bool* fail_next_allocation = nullptr;
};

void PreserveEncodeFacts(OperationResult& result, std::size_t message_index,
                         LocalStatus local_status) noexcept {
  result = {};
  result.local_status = local_status;
  result.codec_called = true;
  result.codec_status = CodecStatus::OK;
  result.message_index = message_index;
}

HostCallbackAction CopyEncodedOutput(const HostOutputView& view, void* opaque) {
  auto& context = *static_cast<EncodeContext*>(opaque);
  try {
    AllocationCheckpoint(context.fail_next_allocation);
    context.copied = CopyEncode(*context.owner, view.message_index, *context.inputs, view.bytes);
  } catch (const std::bad_alloc&) {
    PreserveEncodeFacts(context.copied, view.message_index, LocalStatus::ALLOCATION_FAILED);
  } catch (...) {
    PreserveEncodeFacts(context.copied, view.message_index, LocalStatus::MATERIALIZATION_FAILED);
  }
  context.called = true;
  return HostCallbackAction::CONTINUE;
}

}  // namespace

HostAdapter::HostAdapter(std::unique_ptr<Adapter> owner, std::unique_ptr<HostEndpoint> host,
                         std::vector<HostBinding> bindings,
                         std::vector<std::vector<Channel>> channels,
                         std::size_t accounted_bytes) noexcept
    : owner_(std::move(owner)),
      host_(std::move(host)),
      bindings_(std::move(bindings)),
      channels_(std::move(channels)),
      accounted_bytes_(accounted_bytes) {}

HostAdapter::~HostAdapter() = default;

const OwnedDescription& HostAdapter::Description() const noexcept { return owner_->Description(); }

HostPrepareResult HostAdapter::Create(CompiledProtocol compiled, std::vector<HostBinding> bindings,
                                      const Limits& limits,
                                      std::size_t previous_instance_bytes) noexcept {
  HostPrepareResult result;
  try {
    if (!compiled.HasValue() || bindings.empty()) return result;
    constexpr std::size_t kMaximumBindings = 64U;
    constexpr std::size_t kMaximumChannels = 64U;
    constexpr std::size_t kMaximumIdentityBytes = 256U;
    if (bindings.size() > kMaximumBindings) {
      result.status = LocalStatus::RESOURCE_LIMIT;
      result.host_status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
      return result;
    }
    std::size_t channel_count = 0U;
    auto local_requested = Add(sizeof(HostAdapter), Multiply(bindings.size(), sizeof(HostBinding)));
    local_requested = Add(local_requested, Multiply(bindings.size(), sizeof(std::vector<Channel>)));
    for (const auto& binding : bindings) {
      if (binding.endpoint.empty() || binding.pipeline_index >= compiled.PipelineCount() ||
          (binding.action != HostAction::DECODE && binding.action != HostAction::ENCODE)) {
        return result;
      }
      if (binding.endpoint.size() > kMaximumIdentityBytes) {
        result.status = LocalStatus::RESOURCE_LIMIT;
        result.host_status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
        return result;
      }
      local_requested = Add(local_requested, StringBytes(binding.endpoint));
      const auto flows = binding.action == HostAction::DECODE ? 2U : 1U;
      if (channel_count > kMaximumChannels - flows) {
        result.status = LocalStatus::RESOURCE_LIMIT;
        result.host_status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
        return result;
      }
      channel_count += flows;
    }
    local_requested = Add(local_requested, Multiply(channel_count, sizeof(Channel)));
    if (local_requested > limits.instance_bytes ||
        Add(local_requested, previous_instance_bytes) > limits.replacement_bytes) {
      result.status = LocalStatus::RESOURCE_LIMIT;
      result.host_status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
      return result;
    }
    auto prepared = Adapter::AdoptCompiled(std::move(compiled), limits, previous_instance_bytes);
    result.status = prepared.status;
    if (!prepared.adapter) return result;
    const auto owner_and_local = Add(prepared.adapter->InstanceAdmissionBytes(), local_requested);
    if (owner_and_local > limits.instance_bytes ||
        Add(owner_and_local, previous_instance_bytes) > limits.replacement_bytes) {
      result.status = LocalStatus::RESOURCE_LIMIT;
      result.host_status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
      return result;
    }
    std::vector<HostBindingSpec> specs;
    specs.reserve(bindings.size());
    for (const auto& binding : bindings) {
      const auto flows = binding.action == HostAction::DECODE ? 2U : 1U;
      specs.push_back({binding.endpoint, binding.action, binding.pipeline_index, flows, {}});
    }
    HostLimits host_limits;
    host_limits.max_bindings = bindings.size();
    host_limits.max_channels = channel_count;
    host_limits.max_identity_bytes = kMaximumIdentityBytes;
    auto created = CreateHostEndpoint(prepared.adapter->CompiledForHost(), specs.data(),
                                      specs.size(), host_limits);
    result.host_status = created.status;
    if (created.status != HostStatus::OK || !created.host) return result;
    std::vector<std::vector<Channel>> channels(bindings.size());
    for (std::size_t binding = 0U; binding < bindings.size(); ++binding) {
      const auto count = bindings[binding].action == HostAction::DECODE ? 2U : 1U;
      channels[binding].reserve(count);
      for (std::size_t flow = 0U; flow < count; ++flow) {
        auto found = created.host->Find(bindings[binding].endpoint, bindings[binding].action, flow);
        if (found.status != HostStatus::OK) {
          result.host_status = found.status;
          return result;
        }
        channels[binding].push_back({std::move(found.handle)});
      }
    }
    auto local_accounted =
        Add(sizeof(HostAdapter), Multiply(bindings.capacity(), sizeof(HostBinding)));
    for (const auto& binding : bindings) {
      local_accounted = Add(local_accounted, StringBytes(binding.endpoint));
    }
    local_accounted =
        Add(local_accounted, Multiply(channels.capacity(), sizeof(std::vector<Channel>)));
    for (const auto& binding_channels : channels) {
      local_accounted =
          Add(local_accounted, Multiply(binding_channels.capacity(), sizeof(Channel)));
    }
    auto accounted =
        Add(prepared.adapter->InstanceAdmissionBytes(), created.memory.host_accounted_total_bytes);
    accounted = Add(accounted, local_accounted);
    if (accounted > limits.instance_bytes ||
        Add(accounted, previous_instance_bytes) > limits.replacement_bytes) {
      result.status = LocalStatus::RESOURCE_LIMIT;
      result.host_status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
      return result;
    }
    result.adapter.reset(new HostAdapter(std::move(prepared.adapter), std::move(created.host),
                                         std::move(bindings), std::move(channels), accounted));
    result.status = LocalStatus::OK;
  } catch (const std::bad_alloc&) {
    result.status = LocalStatus::ALLOCATION_FAILED;
  } catch (...) {
    result.status = LocalStatus::PREPARATION_FAILED;
  }
  return result;
}

std::size_t HostAdapter::FlowCount(std::size_t binding) const noexcept {
  return binding < channels_.size() ? channels_[binding].size() : 0U;
}

OperationResult HostAdapter::Decode(std::size_t binding, std::size_t flow,
                                    ByteView frame) noexcept {
  OperationResult result;
  if (binding >= bindings_.size() || flow >= FlowCount(binding) ||
      bindings_[binding].action != HostAction::DECODE)
    return result;
  if ((frame.data == nullptr && frame.size != 0U) ||
      frame.size > owner_->LimitsForHost().max_frame_bytes) {
    result.local_status = frame.size > owner_->LimitsForHost().max_frame_bytes
                              ? LocalStatus::RESOURCE_LIMIT
                              : LocalStatus::INVALID_INPUT;
    return result;
  }
  DecodeContext context{owner_.get()};
#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  context.fail_next_allocation = &fail_next_callback_allocation_;
#endif
  const auto host = host_->Decode(channels_[binding][flow].handle, frame,
                                  {CopyDecodedOutput, &context}, {ObserveCandidate, &context});
  if (context.called) return context.copied;
  result.codec_called = host.codec_attempted;
  result.codec_status = host.codec_status;
  result.local_status = host.status == HostStatus::CODEC_FAILED ? LocalStatus::CODEC_FAILED
                                                                : LocalStatus::PREPARATION_FAILED;
  try {
    const auto needed = Add(sizeof(OperationResult), frame.size);
    if (needed > owner_->LimitsForHost().max_result_bytes) {
      result.local_status = LocalStatus::MATERIALIZATION_FAILED;
      return result;
    }
    if (frame.size != 0U) {
      result.diagnostic_input_frame.assign(frame.data, frame.data + frame.size);
    }
    result.accounted_bytes = needed;
  } catch (const std::bad_alloc&) {
    result.local_status = LocalStatus::ALLOCATION_FAILED;
    result.diagnostic_input_frame.clear();
    result.accounted_bytes = 0U;
  } catch (...) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
    result.diagnostic_input_frame.clear();
    result.accounted_bytes = 0U;
  }
  return result;
}

OperationResult HostAdapter::Encode(std::size_t binding, std::size_t message_index,
                                    const std::vector<InputField>& inputs) noexcept {
  OperationResult result;
  if (binding >= bindings_.size() || channels_[binding].empty() ||
      bindings_[binding].action != HostAction::ENCODE)
    return result;
  if (inputs.size() > owner_->LimitsForHost().max_fields) {
    result.local_status = LocalStatus::RESOURCE_LIMIT;
    return result;
  }
  try {
    for (const auto& input : inputs) {
      if (input.bytes.size() > owner_->LimitsForHost().max_field_bytes) {
        result.local_status = LocalStatus::RESOURCE_LIMIT;
        return result;
      }
    }
    std::vector<EncodeValue> values;
    values.reserve(inputs.size());
    for (const auto& input : inputs)
      values.push_back(EncodeValue::Bytes({message_index, input.field_index},
                                          {input.bytes.data(), input.bytes.size()}));
    EncodeContext context{owner_.get(), &inputs};
#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
    context.fail_next_allocation = &fail_next_callback_allocation_;
#endif
    const auto host = host_->Encode(channels_[binding][0U].handle, message_index, values.data(),
                                    values.size(), {CopyEncodedOutput, &context});
    if (context.called) return context.copied;
    result.codec_called = host.codec_attempted;
    result.codec_status = host.codec_status;
    result.local_status = host.status == HostStatus::CODEC_FAILED ? LocalStatus::CODEC_FAILED
                                                                  : LocalStatus::PREPARATION_FAILED;
  } catch (const std::bad_alloc&) {
    result.local_status = LocalStatus::ALLOCATION_FAILED;
  } catch (...) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
  }
  return result;
}

bool HostAdapter::Reset(std::size_t binding, std::size_t flow) noexcept {
  if (binding >= channels_.size() || flow >= channels_[binding].size()) return false;
  const auto reset = host_->Reset(channels_[binding][flow].handle);
  if (reset != HostStatus::OK) return false;
  auto found = host_->Find(bindings_[binding].endpoint, bindings_[binding].action, flow);
  if (found.status != HostStatus::OK) return false;
  channels_[binding][flow].handle = std::move(found.handle);
  return true;
}

}  // namespace pae::protocol_lab_ascii::public_offline
