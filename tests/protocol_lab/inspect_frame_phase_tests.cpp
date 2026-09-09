#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "config_compiler.h"
#include "protocol_operations.h"

namespace {

std::string ReadText(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool Expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

std::size_t ObjectEnd(std::string_view text, std::size_t begin) {
  std::size_t depth = 0U;
  bool in_string = false;
  bool escaped = false;
  for (std::size_t index = begin; index < text.size(); ++index) {
    const char value = text[index];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (value == '\\') {
        escaped = true;
      } else if (value == '"') {
        in_string = false;
      }
      continue;
    }
    if (value == '"') {
      in_string = true;
    } else if (value == '{') {
      ++depth;
    } else if (value == '}' && --depth == 0U) {
      return index + 1U;
    }
  }
  return std::string_view::npos;
}

bool ReverseTwoPipelines(std::string& config) {
  const std::size_t pipelines = config.find("\"pipelines\"");
  const std::size_t array = config.find('[', pipelines);
  const std::size_t first_begin = config.find('{', array);
  const std::size_t first_end = ObjectEnd(config, first_begin);
  const std::size_t second_begin = config.find('{', first_end);
  const std::size_t second_end = ObjectEnd(config, second_begin);
  if (pipelines == std::string::npos || array == std::string::npos ||
      first_begin == std::string::npos || first_end == std::string::npos ||
      second_begin == std::string::npos || second_end == std::string::npos) {
    return false;
  }
  const std::string first = config.substr(first_begin, first_end - first_begin);
  const std::string separator = config.substr(first_end, second_begin - first_end);
  const std::string second = config.substr(second_begin, second_end - second_begin);
  config.replace(first_begin, second_end - first_begin, second + separator + first);
  return true;
}

bool DuplicateFirstPipeline(std::string& config, std::string_view pipeline_id) {
  const std::size_t pipelines = config.find("\"pipelines\"");
  const std::size_t array = config.find('[', pipelines);
  const std::size_t first_begin = config.find('{', array);
  const std::size_t first_end = ObjectEnd(config, first_begin);
  if (pipelines == std::string::npos || array == std::string::npos ||
      first_begin == std::string::npos || first_end == std::string::npos) {
    return false;
  }
  std::string duplicate = config.substr(first_begin, first_end - first_begin);
  const std::string id = "\"id\": \"" + std::string(pipeline_id) + "\"";
  const std::size_t id_position = duplicate.find(id);
  if (id_position == std::string::npos) {
    return false;
  }
  duplicate.replace(id_position, id.size(), id.substr(0U, id.size() - 1U) + "_alias\"");
  config.insert(first_end, ",\n" + duplicate);
  return true;
}

pae::protocol_plan::PlanOwner Compile(std::string config) {
  auto compiled = pae::config_compiler::CompileJsonToPlan(config);
  if (!compiled.Succeeded()) {
    return {};
  }
  return std::move(compiled).TakePlan();
}

bool CheckAmbiguous(const pae::protocol_plan::PlanBundle& plan,
                    const std::vector<std::uint8_t>& frame) {
  pae::protocol_lab::InspectFrameExecutionCounts counts;
  const auto result = pae::protocol_lab::InspectFrame(plan, frame, &counts);
  return Expect(result.status == "AMBIGUOUS_MESSAGE", "cross-Pipeline match is ambiguous") &&
         Expect(result.pipeline_id.empty() && result.message_id.empty() && result.fields.empty(),
                "ambiguous result delivers no candidate identity or fields") &&
         Expect(counts.structural_query_calls == 2U, "both Pipelines are queried structurally") &&
         Expect(counts.decode_calls == 0U,
                "ambiguous first phase invokes no Decode, SUM8, or field delivery");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    return 2;
  }
  const std::string root = argv[1];
  const std::string public_config = ReadText(root + "/synthetic_sum8_slice.pae.json");
  auto public_plan = Compile(public_config);
  if (!Expect(static_cast<bool>(public_plan), "public Schema 0.3 SUM8 Plan compiles")) {
    return 1;
  }

  pae::protocol_lab::InspectFrameExecutionCounts counts;
  const std::vector<std::uint8_t> unknown{0x70U};
  auto result = pae::protocol_lab::InspectFrame(*public_plan, unknown, &counts);
  if (!Expect(result.status == "UNKNOWN_MESSAGE", "zero structural candidates stay unknown") ||
      !Expect(counts.structural_query_calls == 1U && counts.decode_calls == 0U,
              "zero candidates invoke no Decode, SUM8, or field delivery")) {
    return 1;
  }

  const std::vector<std::uint8_t> valid{0x70U, 0xF0U, 0x20U, 0x01U, 0x02U, 0x13U, 0x7EU};
  result = pae::protocol_lab::InspectFrame(*public_plan, valid, &counts);
  if (!Expect(result.status == "OK" && result.fields.size() == 3U,
              "unique candidate decodes and delivers fields") ||
      !Expect(counts.structural_query_calls == 1U && counts.decode_calls == 1U,
              "unique candidate executes Decode exactly once")) {
    return 1;
  }

  auto corrupt = valid;
  corrupt[1] ^= 0x01U;
  result = pae::protocol_lab::InspectFrame(*public_plan, corrupt, &counts);
  if (!Expect(result.status == "INTEGRITY_FAILED" && result.fields.empty(),
              "unique candidate preserves integrity-before-fields failure") ||
      !Expect(result.pipeline_id == "synthetic_direction" && result.message_id == "sum8_record",
              "unique failed candidate keeps its structural identity") ||
      !Expect(counts.decode_calls == 1U, "integrity failure is produced by one Decode")) {
    return 1;
  }

  std::string ambiguous_config = ReadText(root + "/sum8_cross_pipeline_ambiguous.pae.json");
  auto ambiguous_plan = Compile(ambiguous_config);
  const std::vector<std::uint8_t> ambiguous_frame{0x01U, 0x02U, 0x01U};
  if (!Expect(static_cast<bool>(ambiguous_plan), "cross-Pipeline ambiguity Plan compiles") ||
      !CheckAmbiguous(*ambiguous_plan, ambiguous_frame)) {
    return 1;
  }
  if (!Expect(ReverseTwoPipelines(ambiguous_config), "Pipeline order mutation succeeds")) {
    return 1;
  }
  auto reversed_plan = Compile(ambiguous_config);
  if (!Expect(static_cast<bool>(reversed_plan), "reordered Pipeline Plan compiles") ||
      !CheckAmbiguous(*reversed_plan, ambiguous_frame)) {
    return 1;
  }

  std::string repeated_message_config = public_config;
  if (!Expect(DuplicateFirstPipeline(repeated_message_config, "synthetic_direction"),
              "same Message cross-Pipeline mutation succeeds")) {
    return 1;
  }
  auto repeated_message_plan = Compile(repeated_message_config);
  if (!Expect(static_cast<bool>(repeated_message_plan),
              "same Message in two Pipelines remains a valid Plan") ||
      !CheckAmbiguous(*repeated_message_plan, valid)) {
    return 1;
  }

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::string repeated_length_config = ReadText(root + "/synthetic_length_slice.pae.json");
  if (!Expect(DuplicateFirstPipeline(repeated_length_config, "synthetic_rx"),
              "Schema 0.7 length Pipeline duplication succeeds")) {
    return 1;
  }
  auto repeated_length_plan = Compile(repeated_length_config);
  const std::vector<std::uint8_t> length_frame{0xAAU, 0x00U, 0x06U, 0x05U, 0x7EU, 0x55U};
  if (!Expect(static_cast<bool>(repeated_length_plan),
              "same length Message in two Pipelines remains a valid Plan") ||
      !CheckAmbiguous(*repeated_length_plan, length_frame)) {
    return 1;
  }
#endif

  std::string no_integrity = public_config;
  const std::string fixed_marker =
      "          {\"kind\": \"fixed_bytes\", \"byte_offset\": 6, \"bytes\": \"7E\"}";
  const std::string fixed_replacement =
      "          {\"kind\": \"fixed_bytes\", \"byte_offset\": 5, \"bytes\": \"13\"},\n"
      "          {\"kind\": \"fixed_bytes\", \"byte_offset\": 6, \"bytes\": \"7E\"}";
  const std::string integrity =
      "      \"integrity\": {\n"
      "        \"algorithm\": \"sum8\",\n"
      "        \"range\": {\"byte_offset\": 1, \"byte_length\": 4},\n"
      "        \"storage\": {\"byte_offset\": 5}\n"
      "      },\n";
  const std::size_t fixed_position = no_integrity.find(fixed_marker);
  const std::size_t integrity_position = no_integrity.find(integrity);
  if (!Expect(fixed_position != std::string::npos && integrity_position != std::string::npos,
              "no-integrity mutations are available")) {
    return 1;
  }
  no_integrity.replace(fixed_position, fixed_marker.size(), fixed_replacement);
  const std::size_t adjusted_integrity_position = no_integrity.find(integrity);
  no_integrity.erase(adjusted_integrity_position, integrity.size());
  auto no_integrity_plan = Compile(no_integrity);
  if (!Expect(static_cast<bool>(no_integrity_plan),
              "Schema 0.3 message without integrity compiles")) {
    return 1;
  }
  result = pae::protocol_lab::InspectFrame(*no_integrity_plan, valid, &counts);
  if (!Expect(result.status == "OK" && counts.decode_calls == 1U,
              "new-generation no-integrity message uses the same unique Decode phase")) {
    return 1;
  }

  std::cout << "PAE_LAB_INSPECT_PHASE_PASS\n";
  return 0;
}
