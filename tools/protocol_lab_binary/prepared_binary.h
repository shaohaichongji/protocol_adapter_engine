#pragma once

#include <memory>
#include <string_view>

#include "owned_description.h"
#include "resource_budget.h"

namespace pae::protocol_lab_binary {

// UI revisions are supplied by the caller. instance/operation are generated here; neither
// a matching UI revision nor matching textual IDs can substitute for live Plan identity.
struct Revisions {
  std::uint64_t tab = 0U, load = 0U, session = 0U;
};
struct ObservationIdentity {
  Revisions revisions;
  std::uint64_t instance = 0U, operation = 0U, generation = 0U;
  std::size_t binding = 0U, flow = 0U, pipeline = 0U;
};
struct FieldPresentation {
  std::size_t field_index = 0U;
  std::optional<ByteRange> byte_range;
  std::vector<PhysicalBitMask> physical_bits;
  std::string enum_display_text;
};
struct ObservedCandidate {
  ObservationIdentity identity;
  CandidateResult value;
  std::vector<FieldPresentation> presentation;
  std::optional<ByteRange> integrity_storage;
  // Includes value, this wrapper and presentation capacity. Excludes prepared state/old results.
  std::size_t accounted_total_bytes = 0U;
  std::size_t copy_peak_bytes = 0U;
};
// Call-scoped typed input; no FieldRef/EnumValueRef or caller-supplied Plan identity.
struct EncodeInput {
  std::string_view field_id;
  protocol_core::LogicalValueKind kind = protocol_core::LogicalValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  protocol_core::ByteView bytes_value;
  std::string_view enum_item_id;
  protocol_core::Decimal64 decimal64_value;
};
enum class EncodeIssue {
  NONE,
  MESSAGE_NOT_BOUND,
  INVALID_FIELD,
  DUPLICATE_FIELD,
  MISSING_FIELD,
  TYPE_MISMATCH,
  UNKNOWN_ENUM,
  INVALID_BYTES,
  BUDGET_EXCEEDED,
  ALLOCATION_FAILED,
  OUTPUT_COPY_FAILED
};
struct ObservedEncode {
  ObservationIdentity identity;
  std::size_t message_index = protocol_core::kInvalidIndex;
  std::string message_id;
  // Logical INPUT values only, not decoded fields or independently observed raw integers.
  std::vector<FieldValue> inputs;
  std::vector<std::uint8_t> frame;
  // All fields, including constant/computed, mapped against this successful TX frame.
  std::vector<FieldPresentation> presentation;
  std::optional<ByteRange> integrity_storage;
  std::size_t accounted_total_bytes = 0U, work_peak_bytes = 0U;
};
struct ObservedOperation {
  host_endpoint::Result host;
  std::optional<ObservedCandidate> candidate;
  std::optional<ObservedEncode> encoded;
  EncodeIssue encode_issue = EncodeIssue::NONE;
  void Clear() noexcept {
    candidate.reset();
    encoded.reset();
    host = {};
    encode_issue = EncodeIssue::NONE;
  }
};
struct StreamState {
  host_endpoint::Observation host;
  std::size_t capacity = 0U, frozen_size = 0U, consumed = 0U;
  bool can_continue = false;
};
struct Selection {
  std::size_t binding = 0U, flow = 0U;
};

// Internal serialized owner. Admission and description copying finish before Session creation.
// No mutable Session/Plan escape. Execution bytes are already parsed. Drafts are opaque UTF16.
class PreparedBinary final {
 public:
  static std::unique_ptr<PreparedBinary> Create(
      config_compiler::CompiledProtocolArtifacts artifacts,
      const host_endpoint::BindingSpec* bindings, std::size_t count, Revisions revisions = {},
      const Limits& limits = {}, const DescriptionLimits& description_limits = {},
      const ResourceLimits& resource_limits = {}, const PreparedBinary* previous = nullptr);
  const OwnedDescription& Description() const noexcept { return description_; }
  std::uint64_t Instance() const noexcept { return instance_; }
  std::size_t BindingCount() const noexcept { return bindings_.size(); }
  const ResourceReport& Resources() const noexcept { return resources_; }
  // Borrowed result: invalidated by next operation/Reset on that flow or owner destruction.
  // Invalid-route result instead uses scratch and expires on the next execution call.
  // Explicit external copies are outside this owner's accounting. No result import API.
  const ObservedOperation& Decode(std::size_t binding, std::size_t flow,
                                  protocol_core::ByteView frame);
  const ObservedOperation& Submit(std::size_t binding, std::size_t flow,
                                  protocol_core::ByteView chunk);
  const ObservedOperation& Continue(std::size_t binding, std::size_t flow);
  const ObservedOperation& Encode(std::size_t binding, std::string_view message_id,
                                  const EncodeInput* inputs, std::size_t count);
  const ObservedOperation* Current(std::size_t binding, std::size_t flow) const noexcept;
  // Borrowed draft expires at its next save/Reset or owner destruction.
  std::u16string_view Draft(std::size_t binding, std::size_t flow) const noexcept;
  std::size_t DraftLimit(std::size_t binding, std::size_t flow) const noexcept;
  Selection Selected() const noexcept { return selected_; }
  // Validate destination and source draft before atomic save + selection publication.
  // Invalid Hex is intentionally preserved; parsing belongs to the submit boundary.
  void SaveAndSelect(std::u16string_view draft, std::size_t binding, std::size_t flow);
  StreamState Stream(std::size_t binding, std::size_t flow) const noexcept;
  host_endpoint::Observation Observe(std::size_t binding, std::size_t flow) const noexcept;
  // Host Reset is Decode-only. A faulted Encode channel requires a newly prepared Session.
  host_endpoint::Status Reset(std::size_t binding, std::size_t flow) noexcept;
  PreparedBinary(const PreparedBinary&) = delete;
  PreparedBinary& operator=(const PreparedBinary&) = delete;
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
  const protocol_plan::PlanBundle& TestPlan() const noexcept { return session_->Plan(); }
  void TestFailSave(bool fail) noexcept { fail_save_ = fail; }
  void TestFailEncodeCopy(bool fail) noexcept { fail_encode_copy_ = fail; }
  // Pure association rejection probe; not a Host execution or provenance proof.
  ObservedCandidate TestAssociate(const host_endpoint::Candidate&, std::size_t binding,
                                  std::size_t flow, std::uint64_t expected_generation) const;
#endif

 private:
  PreparedBinary() = default;
  struct Binding {
    std::string endpoint;
    host_endpoint::Action action = host_endpoint::Action::DECODE;
    std::size_t pipeline = 0U, streams = 0U, first_flow = 0U;
  };
  struct Flow {
    std::unique_ptr<std::uint8_t[]> frozen;
    std::size_t capacity = 0U, size = 0U, consumed = 0U;
    std::unique_ptr<char16_t[]> draft;
    std::size_t draft_limit = 0U, draft_size = 0U;
    std::unique_ptr<ObservedOperation> current;
    bool has_current = false;
  };
  void PushFrozen(std::size_t binding, std::size_t flow, ObservedOperation& out);
  void DecodeOnce(std::size_t binding, std::size_t flow, protocol_core::ByteView,
                  ObservedOperation&);
  void SubmitOnce(std::size_t binding, std::size_t flow, protocol_core::ByteView,
                  ObservedOperation&);
  void ContinueOnce(std::size_t binding, std::size_t flow, ObservedOperation&);
  void EncodeOnce(std::size_t binding, std::string_view message_id, const EncodeInput*,
                  std::size_t count, ObservedOperation&);
  const ObservedOperation& Publish(std::size_t binding, std::size_t flow) noexcept;
  bool Valid(std::size_t binding, std::size_t flow) const noexcept;
  ObservedCandidate Associate(const host_endpoint::Candidate&, const ObservationIdentity&) const;
  OwnedDescription description_;
  std::vector<Binding> bindings_;
  std::vector<Flow> flows_;
  std::unique_ptr<ObservedOperation> pending_;
  std::unique_ptr<char16_t[]> draft_scratch_;
  Selection selected_;
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
  bool fail_save_ = false;
  bool fail_encode_copy_ = false;
#endif
  std::unique_ptr<host_endpoint::Session> session_;
  Limits limits_;
  Revisions revisions_;
  ResourceReport resources_;
  std::uint64_t instance_ = 0U, operation_ = 0U;
};

}  // namespace pae::protocol_lab_binary
