#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

#include "../../tools/protocol_lab_ui/document_session.h"

namespace ui = pae::protocol_lab_ui;
namespace ascii = pae::protocol_lab::ascii;
namespace host = pae::host_endpoint;
namespace public_ascii = pae::protocol_lab_ascii::public_offline;

namespace {

void Check(bool passed, int line, const char* expression) {
  if (passed) return;
  std::cerr << "ASCII_PUBLIC_A2_CHECK_FAILED line=" << line << " expression=" << expression << '\n';
  std::exit(1);
}

}  // namespace

#undef assert
#define assert(expression) Check(static_cast<bool>(expression), __LINE__, #expression)

namespace {

std::string Read() {
  std::ifstream input(std::filesystem::path{PAE_ASCII_TEXT_CONFIG}, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::unique_ptr<ui::CompileCompletion> CompileOnce(ui::CompileWorker& worker, ui::DocumentId id,
                                                   ui::Revision revision, const std::string& text) {
  assert(worker.Submit(id, revision, text) == ui::SubmitStatus::ACCEPTED);
  for (unsigned attempt = 0U; attempt < 5000U; ++attempt) {
    const auto tickets = worker.DrainReadyTickets();
    if (!tickets.empty()) return worker.TakeResult(tickets.front());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return nullptr;
}

std::unique_ptr<ui::AsciiHostAdapter> PublicHost(const std::string& text,
                                                 std::size_t previous_bytes) {
  auto compiled = pae::CompileProtocolJson(text);
  assert(compiled.Succeeded());
  std::string error;
  auto adapter = ui::AsciiHostAdapter::CreatePublic(
      std::move(compiled).TakeCompiled(),
      {{"device", host::Action::DECODE, 0U}, {"device", host::Action::ENCODE, 0U}}, previous_bytes,
      error);
  if (!adapter) std::cerr << error << '\n';
  assert(adapter && error.empty() && adapter->IsPublicCompleteRecord());
  return adapter;
}

std::vector<public_ascii::HostBinding> DefaultPublicBindings() {
  return {{"device", pae::HostAction::DECODE, 0U}, {"device", pae::HostAction::ENCODE, 0U}};
}

public_ascii::HostPrepareResult PreparePublicHost(
    const std::string& text, const public_ascii::Limits& limits = {},
    std::size_t previous_bytes = 0U,
    std::vector<public_ascii::HostBinding> bindings = DefaultPublicBindings()) {
  auto compiled = pae::CompileProtocolJson(text);
  assert(compiled.Succeeded());
  return public_ascii::HostAdapter::Create(std::move(compiled).TakeCompiled(), std::move(bindings),
                                           limits, previous_bytes);
}

void VerifyPublicHostResourceBoundaries(const std::string& text) {
  auto baseline = PreparePublicHost(text);
  assert(baseline.status == public_ascii::LocalStatus::OK && baseline.adapter);
  const auto accounted = baseline.adapter->AccountedBytes();
  assert(accounted > 1U);

  public_ascii::Limits exact_instance;
  exact_instance.instance_bytes = accounted;
  exact_instance.replacement_bytes = accounted;
  auto exact = PreparePublicHost(text, exact_instance);
  assert(exact.status == public_ascii::LocalStatus::OK && exact.adapter &&
         exact.adapter->AccountedBytes() == accounted);
  exact_instance.instance_bytes = accounted - 1U;
  auto short_instance = PreparePublicHost(text, exact_instance);
  assert(short_instance.status == public_ascii::LocalStatus::RESOURCE_LIMIT &&
         !short_instance.adapter);

  constexpr std::size_t previous = 17U;
  public_ascii::Limits exact_replacement;
  exact_replacement.instance_bytes = accounted;
  exact_replacement.replacement_bytes = accounted + previous;
  assert(PreparePublicHost(text, exact_replacement, previous).status ==
         public_ascii::LocalStatus::OK);
  --exact_replacement.replacement_bytes;
  auto short_replacement = PreparePublicHost(text, exact_replacement, previous);
  assert(short_replacement.status == public_ascii::LocalStatus::RESOURCE_LIMIT &&
         !short_replacement.adapter);

  auto short_endpoint = PreparePublicHost(text, {}, 0U, {{"a", pae::HostAction::DECODE, 0U}});
  auto long_endpoint = PreparePublicHost(text, {}, 0U, {{"abcdefgh", pae::HostAction::DECODE, 0U}});
  assert(short_endpoint.adapter && long_endpoint.adapter &&
         long_endpoint.adapter->AccountedBytes() - short_endpoint.adapter->AccountedBytes() ==
             2U * (std::string{"abcdefgh"}.size() - std::string{"a"}.size()));

  const std::vector<std::uint8_t> valid_frame{'R', 'X', ' ', 'A', '!', 'O', 'K', '\r', '\n'};
  public_ascii::Limits frame_limit;
  frame_limit.max_frame_bytes = valid_frame.size() - 1U;
  auto frame_limited = PreparePublicHost(text, frame_limit);
  assert(frame_limited.adapter);
  const auto oversized =
      frame_limited.adapter->Decode(0U, 0U, {valid_frame.data(), valid_frame.size()});
  assert(oversized.local_status == public_ascii::LocalStatus::RESOURCE_LIMIT &&
         !oversized.codec_called && oversized.diagnostic_input_frame.empty());
  const auto invalid_view = frame_limited.adapter->Decode(0U, 0U, {nullptr, 1U});
  assert(invalid_view.local_status == public_ascii::LocalStatus::INVALID_INPUT &&
         !invalid_view.codec_called && invalid_view.diagnostic_input_frame.empty());

  const std::vector<std::uint8_t> broken{'B', 'R', 'O', 'K', 'E', 'N'};
  public_ascii::Limits short_diagnostic;
  short_diagnostic.max_result_bytes = sizeof(public_ascii::OperationResult) + broken.size() - 1U;
  auto diagnostic_limited = PreparePublicHost(text, short_diagnostic);
  assert(diagnostic_limited.adapter);
  const auto rejected_diagnostic =
      diagnostic_limited.adapter->Decode(0U, 0U, {broken.data(), broken.size()});
  assert(rejected_diagnostic.codec_called &&
         rejected_diagnostic.local_status == public_ascii::LocalStatus::MATERIALIZATION_FAILED &&
         rejected_diagnostic.diagnostic_input_frame.empty() &&
         rejected_diagnostic.accounted_bytes == 0U);

  short_diagnostic.max_result_bytes = sizeof(public_ascii::OperationResult) + broken.size();
  auto diagnostic_exact = PreparePublicHost(text, short_diagnostic);
  assert(diagnostic_exact.adapter);
  const auto copied_diagnostic =
      diagnostic_exact.adapter->Decode(0U, 0U, {broken.data(), broken.size()});
  assert(copied_diagnostic.codec_called &&
         copied_diagnostic.local_status == public_ascii::LocalStatus::CODEC_FAILED &&
         copied_diagnostic.diagnostic_input_frame == broken &&
         copied_diagnostic.accounted_bytes ==
             sizeof(public_ascii::OperationResult) + broken.size());

  public_ascii::Limits field_limits;
  field_limits.max_fields = 3U;
  field_limits.max_field_bytes = 8U;
  auto encode_limited = PreparePublicHost(text, field_limits);
  assert(encode_limited.adapter);
  const auto too_many =
      encode_limited.adapter->Encode(1U, 0U, {{0U, {'A'}}, {1U, {'B'}}, {2U, {'C'}}, {3U, {'D'}}});
  assert(too_many.local_status == public_ascii::LocalStatus::RESOURCE_LIMIT &&
         !too_many.codec_called);
  const auto too_large = encode_limited.adapter->Encode(
      1U, 0U, {{0U, std::vector<std::uint8_t>(9U, 'A')}, {2U, {'Z'}}});
  assert(too_large.local_status == public_ascii::LocalStatus::RESOURCE_LIMIT &&
         !too_large.codec_called);

#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  auto callback_failure = PreparePublicHost(text);
  assert(callback_failure.adapter);
  callback_failure.adapter->FailNextCallbackAllocationForTesting();
  const auto decode_allocation =
      callback_failure.adapter->Decode(0U, 0U, {valid_frame.data(), valid_frame.size()});
  assert(decode_allocation.local_status == public_ascii::LocalStatus::ALLOCATION_FAILED &&
         decode_allocation.codec_called && decode_allocation.codec_status == pae::CodecStatus::OK &&
         decode_allocation.matched_message_index == 0U && decode_allocation.frame.empty() &&
         decode_allocation.fields.empty());
  callback_failure.adapter->FailNextCallbackAllocationForTesting();
  const auto encode_allocation =
      callback_failure.adapter->Encode(1U, 0U, {{0U, {'A'}}, {2U, {'Z'}}});
  assert(encode_allocation.local_status == public_ascii::LocalStatus::ALLOCATION_FAILED &&
         encode_allocation.codec_called && encode_allocation.codec_status == pae::CodecStatus::OK &&
         encode_allocation.message_index == 0U && encode_allocation.frame.empty() &&
         encode_allocation.fields.empty());
#endif
}

void VerifyPublicHostDecodeLayers(const std::string& text) {
  auto raw_compiled = pae::CompileProtocolJson(text);
  assert(raw_compiled.Succeeded());
  auto raw = public_ascii::HostAdapter::Create(
      std::move(raw_compiled).TakeCompiled(),
      {{"device", pae::HostAction::DECODE, 0U}, {"device", pae::HostAction::ENCODE, 0U}});
  assert(raw.status == public_ascii::LocalStatus::OK && raw.host_status == pae::HostStatus::OK &&
         raw.adapter);
  const std::vector<std::uint8_t> frame{'R', 'X', ' ', 'A', '!', 'O', 'K', '\r', '\n'};
  const auto copied = raw.adapter->Decode(0U, 0U, {frame.data(), frame.size()});
  assert(copied.local_status == public_ascii::LocalStatus::OK && copied.codec_called &&
         copied.codec_status == pae::CodecStatus::OK && copied.message_id == "greeting" &&
         copied.frame == frame && copied.fields.size() == 2U);

  auto facade = PublicHost(text, 0U);
  ascii::ExecutionIdentity identity;
  identity.pipeline_index = 0U;
  identity.pipeline_id = "ascii_pipeline";
  const auto converted = facade->Inspect(0U, 0U, identity, frame);
  assert(converted.status == ascii::AdapterStatus::OK && converted.core_called &&
         converted.message_id == "greeting" && converted.frame == frame &&
         converted.fields.size() == 2U);
}

void Load(ui::DocumentSession& session, ui::CompileWorker& worker, const std::string& text) {
  const auto revision = session.BeginLoad();
  auto completion = CompileOnce(worker, session.id(), revision, text);
  assert(completion && completion->compiler_attempt_count == 1U);
  assert(completion->route == ui::SchemaDispatchStatus::ASCII_PUBLIC);
  assert(completion->public_compiled && !completion->artifacts);
  assert(session.ApplyCompileCompletion(std::move(completion)));
  assert(session.prepared()->public_ascii_adapter && !session.prepared()->ascii_adapter);
}

}  // namespace

int main() {
  const auto text = Read();
  VerifyPublicHostResourceBoundaries(text);
  VerifyPublicHostDecodeLayers(text);
  ui::CompileWorker worker;
  ui::DocumentSession first{9101U};
  ui::DocumentSession second{9102U};
  Load(first, worker, text);
  Load(second, worker, text);

  assert(first.SetMode(ui::OperationMode::INSPECT));
  assert(first.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  assert(first.SetInspectDraftUtf16(u"RX ALICE!OK\\r\\n"));
  assert(first.Inspect() && first.inspect_result()->message_id == "greeting");
  assert(first.SetMode(ui::OperationMode::ENCODE));
  assert(first.SetDraft(0U, std::vector<std::uint8_t>{'A', 'L', 'I', 'C', 'E'}));
  assert(first.SetDraft(2U, std::vector<std::uint8_t>{'Z'}));
  assert(first.Encode());

  const auto first_previous = first.prepared()->public_ascii_adapter->InstanceAdmissionBytes();
  assert(first.ApplyHostAdapter(PublicHost(text, first_previous)) && first.HostActive());
  assert(first.prepared()->public_ascii_adapter == nullptr);
  assert(first.SelectHostFlow(0U, 0U));
  assert(first.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  assert(first.SetInspectDraftUtf16(u"RX A!OK\\r\\n"));
  assert(first.Inspect());
  assert(first.SetInspectDraftUtf16(u"BROKEN"));
  assert(!first.Inspect() && !first.inspect_result() && first.inspect_failure());
  assert(first.SetInspectDraftUtf16(u"RX A!OK\\r\\n"));
  assert(first.Inspect() && first.inspect_result() && !first.inspect_failure());
  assert(first.SelectHostFlow(0U, 1U));
  assert(first.SetInspectDraftUtf16(u"RX B!OK\\r\\n"));
  assert(first.SelectHostFlow(0U, 0U));
  assert(first.inspect_draft_utf16() == u"RX A!OK\\r\\n" && first.inspect_result());
  assert(first.SelectHostFlow(1U, 0U));
  assert(first.SetDraft(0U, std::vector<std::uint8_t>{'B'}));
  assert(first.SetDraft(2U, std::vector<std::uint8_t>{'Q'}));
  assert(first.Encode() && first.preview());
  const auto retained = first.preview()->encoded_frame;
  assert(!first.ApplyHostAdapter(nullptr));
  assert(first.HostActive() && first.preview()->encoded_frame == retained);

  auto invalid_compiled = pae::CompileProtocolJson(text);
  assert(invalid_compiled.Succeeded());
  std::string error;
  auto invalid = ui::AsciiHostAdapter::CreatePublic(
      std::move(invalid_compiled).TakeCompiled(), {{"bad", host::Action::DECODE, 99U}},
      first.prepared()->host_adapter->AccountedBytes(), error);
  assert(!invalid && first.HostActive() && first.preview()->encoded_frame == retained);

  assert(second.SetMode(ui::OperationMode::INSPECT));
  assert(second.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  assert(second.SetInspectDraftUtf16(u"RX SECOND!OK\\r\\n"));
  assert(second.Inspect());
  assert(first.SelectHostFlow(0U, 0U));
  assert(first.inspect_draft_utf16() == u"RX A!OK\\r\\n");
  const auto generation = first.plan_generation();
  assert(
      first.ApplyHostAdapter(PublicHost(text, first.prepared()->host_adapter->AccountedBytes())));
  assert(first.plan_generation() == generation + 1U && first.HostActive() &&
         first.inspect_draft_utf16().empty() && !first.inspect_result() && !first.preview());
  return 0;
}
