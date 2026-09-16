#include <pae/compiler.h>
#include <pae/version.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

static_assert(!std::is_copy_constructible_v<pae::CompiledProtocol>);
static_assert(std::is_nothrow_move_constructible_v<pae::CompiledProtocol>);
static_assert(!std::is_copy_constructible_v<pae::CompileResult>);
static_assert(std::is_nothrow_move_constructible_v<pae::CompileResult>);

class Runner final {
 public:
  void Check(bool condition, std::string_view name, std::string_view detail) {
    if (condition) {
      ++passed_;
      std::cout << "PASS case=" << name << '\n';
    } else {
      ++failed_;
      std::cerr << "FAIL case=" << name << " detail=" << detail << '\n';
    }
  }

  int Finish() const {
    std::cout << "PUBLIC_API_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
              << " gate=" << (failed_ == 0U ? "PASS" : "FAIL") << '\n';
    return failed_ == 0U ? 0 : 1;
  }

 private:
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

bool ReadFile(const std::filesystem::path& path, std::string& output) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  output = buffer.str();
  return stream.good() || stream.eof();
}

void CheckArtifact(Runner& runner, std::string_view case_name, const std::string& json,
                   std::string_view expected_schema, bool require_enum) {
  auto result = pae::CompileProtocolJson(json);
  const auto* compiled = result.Compiled();
  runner.Check(
      result.Succeeded() && compiled != nullptr, std::string(case_name) + "_compile",
      result.Diagnostic() == nullptr ? "valid configuration failed" : result.Diagnostic()->detail);
  if (compiled == nullptr) {
    return;
  }

  const auto protocol = compiled->Protocol();
  runner.Check(
      protocol.has_value() && protocol->schema_version == expected_schema && !protocol->id.empty(),
      std::string(case_name) + "_protocol", "protocol identity or schema differs");
  runner.Check(compiled->PipelineCount() > 0U && compiled->MessageCount() > 0U,
               std::string(case_name) + "_counts", "pipeline or message metadata is empty");

  bool associations_valid = true;
  for (std::size_t pipeline_index = 0U; pipeline_index < compiled->PipelineCount();
       ++pipeline_index) {
    const auto pipeline = compiled->Pipeline(pipeline_index);
    associations_valid = associations_valid && pipeline.has_value() && !pipeline->id.empty();
    if (!pipeline.has_value()) {
      continue;
    }
    for (std::size_t association = 0U; association < pipeline->message_count; ++association) {
      const auto message_index = compiled->PipelineMessageIndex(pipeline_index, association);
      associations_valid = associations_valid && message_index.has_value() &&
                           *message_index < compiled->MessageCount();
    }
  }
  runner.Check(associations_valid, std::string(case_name) + "_associations",
               "pipeline/message association is invalid");

  bool fields_valid = true;
  for (std::size_t field_index = 0U; field_index < compiled->FieldCount(); ++field_index) {
    const auto field = compiled->Field(field_index);
    fields_valid = fields_valid && field.has_value() && !field->id.empty() &&
                   field->message_index < compiled->MessageCount();
  }
  runner.Check(fields_valid, std::string(case_name) + "_fields",
               "field identity or association is invalid");

  bool enums_valid = !require_enum || compiled->EnumCount() > 0U;
  for (std::size_t enum_index = 0U; enum_index < compiled->EnumCount(); ++enum_index) {
    const auto item = compiled->Enum(enum_index);
    enums_valid = enums_valid && item.has_value() && !item->id.empty() &&
                  item->field_flat_index < compiled->FieldCount();
  }
  runner.Check(enums_valid, std::string(case_name) + "_enums",
               "enum identity, value, or association is invalid");
  if (require_enum) {
    const auto first_enum = compiled->Enum(0U);
    runner.Check(first_enum.has_value() && first_enum->id == "mode_idle" &&
                     first_enum->display_name == "空闲模式" && first_enum->raw_value == 1U,
                 std::string(case_name) + "_enum_semantics",
                 "enum id, display name, or raw value differs from the independent fixture");
  }

  const auto memory = compiled->MemoryReport();
  runner.Check(memory.plan_accounted_bytes > 0U && memory.metadata_accounted_bytes > 0U &&
                   memory.metadata_allocation_count == 1U && memory.facade_allocation_bytes > 0U,
               std::string(case_name) + "_memory", "memory categories are not distinguished");
}

}  // namespace

int main(int argc, char** argv) {
  Runner runner;
  if (argc != 5) {
    runner.Check(false, "arguments", "expected four configuration paths");
    return runner.Finish();
  }

  std::vector<std::string> inputs(4U);
  bool loaded = true;
  for (std::size_t index = 0U; index < inputs.size(); ++index) {
    loaded = ReadFile(argv[index + 1U], inputs[index]) && loaded;
  }
  runner.Check(loaded, "load_inputs", "could not read a configuration fixture");
  if (!loaded) {
    return runner.Finish();
  }

  runner.Check(pae::kPublicApiVersion == "0.experimental.1" &&
                   pae::IsSchemaVersionSupported("0.1") && pae::IsSchemaVersionSupported("0.11") &&
                   !pae::IsSchemaVersionSupported("0.12"),
               "version_contract", "public API or supported schema query differs");

  pae::CompiledProtocol empty;
  runner.Check(!empty.HasValue() && !empty.Protocol().has_value() && empty.PipelineCount() == 0U &&
                   !empty.Pipeline(0U).has_value() &&
                   !empty.PipelineMessageIndex(0U, 0U).has_value() && empty.MessageCount() == 0U &&
                   !empty.Message(0U).has_value() && empty.FieldCount() == 0U &&
                   !empty.Field(0U).has_value() && empty.EnumCount() == 0U &&
                   !empty.Enum(0U).has_value() && empty.MemoryReport().plan_accounted_bytes == 0U,
               "empty_owner_safe", "empty owner query did not fail closed");

  auto invalid = pae::CompileProtocolJson("{");
  const auto* invalid_diagnostic = invalid.Diagnostic();
  runner.Check(!invalid.Succeeded() && invalid.Compiled() == nullptr &&
                   invalid_diagnostic != nullptr &&
                   invalid_diagnostic->stage == pae::CompileStage::JSON_SYNTAX &&
                   invalid_diagnostic->code == pae::CompileError::JSON_SYNTAX_ERROR &&
                   invalid_diagnostic->byte_offset.has_value(),
               "structured_diagnostic", "invalid JSON did not preserve structured diagnostic");

  CheckArtifact(runner, "binary_0_1", inputs[0], "0.1", false);
  CheckArtifact(runner, "binary_enum_0_1", inputs[1], "0.1", true);
  CheckArtifact(runner, "ascii_0_10", inputs[2], "0.10", false);
  CheckArtifact(runner, "ascii_stream_0_11", inputs[3], "0.11", false);

  auto measured = pae::CompileProtocolJson(inputs[1]);
  runner.Check(measured.Succeeded(), "measure_metadata", "metadata measurement compile failed");
  if (measured.Compiled() != nullptr) {
    const std::size_t exact = measured.Compiled()->MemoryReport().metadata_accounted_bytes;
    auto exact_result = pae::CompileProtocolJson(inputs[1], pae::CompileOptions{exact});
    auto below_result = pae::CompileProtocolJson(inputs[1], pae::CompileOptions{exact - 1U});
    const auto* below = below_result.Diagnostic();
    runner.Check(exact_result.Succeeded(), "metadata_limit_exact", "exact metadata limit failed");
    runner.Check(!below_result.Succeeded() && below_result.Compiled() == nullptr &&
                     below != nullptr && below->stage == pae::CompileStage::RESOURCE_BUDGET &&
                     below->code == pae::CompileError::RESOURCE_LIMIT_EXCEEDED &&
                     below->resource_kind == pae::ResourceKind::METADATA_ACCOUNTED_MEMORY &&
                     below->required_bytes == exact && below->limit_bytes == exact - 1U,
                 "metadata_limit_minus_one", "minus-one limit did not retain resource diagnostic");
  }

  auto move_source = pae::CompileProtocolJson(inputs[0]);
  runner.Check(move_source.Succeeded(), "move_prepare", "move source compile failed");
  if (move_source.Succeeded()) {
    pae::CompiledProtocol first = std::move(move_source).TakeCompiled();
    runner.Check(!move_source.Succeeded() && move_source.Compiled() == nullptr && first.HasValue(),
                 "take_owner", "taking compiled owner did not empty result");
    const std::string protocol_id{first.Protocol()->id};
    pae::CompiledProtocol second = std::move(first);
    const auto reacquired = second.Protocol();
    runner.Check(!first.HasValue() && !first.Protocol().has_value() && reacquired.has_value() &&
                     reacquired->id == protocol_id,
                 "move_reacquire", "moved owner or reacquired view is invalid");
    pae::CompiledProtocol replacement;
    replacement = std::move(second);
    runner.Check(!second.HasValue() && replacement.HasValue(), "move_assign",
                 "move assignment did not transfer ownership");
    runner.Check(
        !replacement.Pipeline(replacement.PipelineCount()).has_value() &&
            !replacement.PipelineMessageIndex(replacement.PipelineCount(), 0U).has_value() &&
            !replacement.Message(replacement.MessageCount()).has_value() &&
            !replacement.Field(replacement.FieldCount()).has_value() &&
            !replacement.Enum(replacement.EnumCount()).has_value(),
        "out_of_range_safe", "out-of-range query did not return empty");
  }

  return runner.Finish();
}
