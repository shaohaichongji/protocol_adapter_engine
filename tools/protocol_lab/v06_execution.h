#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../src/protocol_plan/plan_memory.h"
#include "v06_format.h"

namespace pae::protocol_lab::v06 {

enum class ExecutionStage {
  PREPARATION,
  STRUCTURAL_QUERY,
  CODEC,
  MATERIALIZATION,
};

enum class MaterializationFailure {
  NONE,
  REVIEW_DECODE_FAILED,
  REVIEW_MESSAGE_MISMATCH,
  RAW_ASSOCIATION_FAILED,
  INTERNAL_ERROR,
};

enum class ExecutionPhase {
  PREPARATION,
  STRUCTURAL_QUERY,
  MAIN_CODEC,
  REVIEW_DECODE,
  RESULT_MAPPING,
};

class ExecutionObserver {
 public:
  virtual ~ExecutionObserver() = default;
  virtual void PhaseStarted(ExecutionPhase phase) = 0;
  virtual void PhaseFinished(ExecutionPhase phase, std::string_view status) = 0;
};

struct PreparationFailure {
  std::string diagnostic_id;
  std::string detail;
  std::optional<std::size_t> value_index;
};

struct ExecutionCounts {
  std::size_t structural_query_calls = 0U;
  std::size_t encode_calls = 0U;
  std::size_t decode_calls = 0U;
  std::size_t review_decode_calls = 0U;
};

struct ExecutionOutcome {
  std::string schema_version;
  ExecutionStage stage = ExecutionStage::PREPARATION;
  PreparationFailure preparation_failure;
  std::string structural_status;
  bool main_codec_called = false;
  std::string main_codec_status;
  bool review_decode_called = false;
  std::string review_decode_status;
  MaterializationFailure materialization_failure = MaterializationFailure::NONE;
  std::optional<Result> result;
  std::vector<std::uint8_t> encoded_frame;
  ExecutionCounts counts;
};

#if defined(PAE_ENABLE_OPERATION_COUNTERS)
struct ExecutionTestHooks {
  bool fail_review_decode = false;
  bool fail_review_decimal_conversion_with_internal_error = false;
  bool fail_structural_query = false;
  bool force_base_result_mapping_failure = false;
  bool force_review_message_mismatch = false;
  bool force_raw_association_failure = false;
  bool force_materialization_internal_error = false;
  bool force_pipeline_structural_unknown = false;
  bool force_pipeline_structural_ambiguous = false;
};
#endif

// Internal C1 bridge. It owns the frozen Plan and reusable workspaces, but all returned Result and
// Frame data is self-owned and remains valid after reuse or destruction of this object.
class ExecutionBridge final {
 public:
  static std::unique_ptr<ExecutionBridge> Prepare(std::string_view config_text,
                                                  PreparationFailure& failure);
  static std::unique_ptr<ExecutionBridge> AdoptCompiledPlan(protocol_plan::PlanOwner plan,
                                                            std::string config_sha256,
                                                            PreparationFailure& failure);

  ExecutionBridge(const ExecutionBridge&) = delete;
  ExecutionBridge& operator=(const ExecutionBridge&) = delete;
  ExecutionBridge(ExecutionBridge&&) = delete;
  ExecutionBridge& operator=(ExecutionBridge&&) = delete;
  ~ExecutionBridge();

  ExecutionOutcome Inspect(const std::vector<std::uint8_t>& frame
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                           ,
                           const ExecutionTestHooks* hooks = nullptr
#endif
                           ,
                           ExecutionObserver* observer = nullptr);
  ExecutionOutcome InspectPipeline(const std::vector<std::uint8_t>& frame,
                                   std::string_view pipeline_id
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                   ,
                                   const ExecutionTestHooks* hooks = nullptr
#endif
                                   ,
                                   ExecutionObserver* observer = nullptr);
  ExecutionOutcome EncodeValuesText(std::string values_text
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                    ,
                                    const ExecutionTestHooks* hooks = nullptr
#endif
                                    ,
                                    ExecutionObserver* observer = nullptr);
  ExecutionOutcome EncodeParsed(const ParsedValues& values
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                ,
                                const ExecutionTestHooks* hooks = nullptr
#endif
                                ,
                                ExecutionObserver* observer = nullptr);

  // Borrowed only for immediate descriptor/model construction. The pointer remains valid only
  // while this bridge is alive and must not be retained by UI models.
  const protocol_plan::PlanBundle* Plan() const noexcept;

 private:
  struct Impl;
  explicit ExecutionBridge(std::unique_ptr<Impl> implementation) noexcept;

  std::unique_ptr<Impl> implementation_;
};

}  // namespace pae::protocol_lab::v06
