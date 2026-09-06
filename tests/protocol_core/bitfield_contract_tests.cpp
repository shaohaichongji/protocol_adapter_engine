#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "complete_record_codec.h"
#include "config_compiler.h"

namespace {

using pae::protocol_core::ByteView;
using pae::protocol_core::CodecStatus;
using pae::protocol_core::DecodedFieldSlot;
using pae::protocol_core::EncodeFieldValue;
using pae::protocol_core::ExecutionWorkspace;
using pae::protocol_core::FieldRef;
using pae::protocol_core::LogicalValueKind;
using pae::protocol_core::MutableByteBuffer;

std::string ReadText(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::vector<std::uint8_t> ParseHex(std::string text) {
  std::vector<std::uint8_t> bytes;
  for (std::size_t index = 0U; index < text.size();) {
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) {
      ++index;
    }
    if (index + 2U > text.size()) break;
    bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(index, 2U), nullptr, 16)));
    index += 2U;
  }
  return bytes;
}

bool Expect(bool condition, const char* message) {
  if (!condition) std::cerr << "FAILED: " << message << '\n';
  return condition;
}

bool CompileFails(std::string text, std::string_view from, std::string_view to) {
  const std::size_t position = text.find(from);
  if (position == std::string::npos) return false;
  text.replace(position, from.size(), to);
  return !pae::config_compiler::CompileJsonToPlan(text).Succeeded();
}

bool CompileFailsAt(std::string text, std::string_view from, std::string_view to,
                    pae::config_compiler::CompileStage stage,
                    pae::config_compiler::CompileError code, std::string_view pointer) {
  const std::size_t position = text.find(from);
  if (position == std::string::npos) return false;
  text.replace(position, from.size(), to);
  const auto compiled = pae::config_compiler::CompileJsonToPlan(text);
  const auto* diagnostic = compiled.Diagnostic();
  return !compiled.Succeeded() && diagnostic != nullptr && diagnostic->stage == stage &&
         diagnostic->code == code && diagnostic->json_pointer == pointer;
}

bool CompileWithFixedMatcher(std::string text, std::uint64_t byte_offset,
                             std::string_view bytes_hex, bool should_succeed,
                             std::string_view expected_pointer = {}) {
  const std::string_view original =
      "\"matcher\": {\"all\": [{\"kind\": \"frame_length_equals\", \"length_bytes\": 18}]}";
  const std::string replacement =
      "\"matcher\": {\"all\": [{\"kind\": \"frame_length_equals\", \"length_bytes\": "
      "18}, {\"kind\": \"fixed_bytes\", \"byte_offset\": " +
      std::to_string(byte_offset) + ", \"bytes\": \"" + std::string{bytes_hex} + "\"}]}";
  const std::size_t position = text.find(original);
  if (position == std::string::npos) return false;
  text.replace(position, original.size(), replacement);
  const auto compiled = pae::config_compiler::CompileJsonToPlan(text);
  if (should_succeed) return compiled.Succeeded();
  const auto* diagnostic = compiled.Diagnostic();
  return !compiled.Succeeded() && diagnostic != nullptr &&
         diagnostic->stage == pae::config_compiler::CompileStage::DOMAIN_VALIDATION &&
         diagnostic->code == pae::config_compiler::CompileError::MATCHER_CONFLICT &&
         diagnostic->json_pointer == expected_pointer;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string root = argv[1];
  const std::string config = ReadText(root + "/synthetic_bitfield_slice.pae.json");
  auto compiled = pae::config_compiler::CompileJsonToPlan(config);
  if (!Expect(compiled.Succeeded(), "Schema 0.2 public vector must compile")) return 1;
  auto plan = std::move(compiled).TakePlan();
  if (!Expect(plan->SchemaVersion() == "0.2", "frozen plan keeps Schema 0.2") ||
      !Expect(plan->Messages()[0].bit_containers.size() == 6U, "containers are frozen") ||
      !Expect(plan->GetResourceRequirements().total_bit_container_count == 6U,
              "container resource count is exact") ||
      !Expect(plan->GetExecutionResourceLayout().bit_container_value_count == 6U,
              "workspace container budget is exact"))
    return 1;

  ExecutionWorkspace workspace(*plan);
  std::vector<EncodeFieldValue> values(7U);
  const std::size_t indices[] = {0U, 1U, 3U, 4U, 5U, 6U, 7U};
  for (std::size_t i = 0U; i < values.size(); ++i) {
    values[i].field = FieldRef{plan.get(), 0U, indices[i]};
  }
  values[0].value_kind = LogicalValueKind::BOOL;
  values[0].bool_value = true;
  values[1].value_kind = LogicalValueKind::ENUM;
  values[1].enum_value = {plan.get(), 0U, 1U, 1U};
  values[2].uint64_value = 0x5AU;
  values[3].uint64_value = 0x101U;
  values[4].uint64_value = 0xBEEFU;
  values[5].uint64_value = UINT64_C(0x0123456789ABCDEF);
  values[6].value_kind = LogicalValueKind::BOOL;
  values[6].bool_value = false;

  std::vector<std::uint8_t> output(18U, 0xCCU);
  auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{output.data(), output.size()});
  const auto expected = ParseHex(ReadText(root + "/bitfield_record_001.frame.hex"));
  if (!Expect(encoded.status == CodecStatus::OK, "bitfield encode succeeds") ||
      !Expect(output == expected, "encode matches independently derived bytes"))
    return 1;
  std::reverse(values.begin(), values.end());
  ExecutionWorkspace second_workspace(*plan);
  std::vector<std::uint8_t> reordered(18U, 0x7EU);
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, second_workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{reordered.data(), reordered.size()});
  if (!Expect(encoded.status == CodecStatus::OK && reordered == expected,
              "input order is irrelevant and separate workspaces share one Plan"))
    return 1;
  std::reverse(values.begin(), values.end());

  std::vector<DecodedFieldSlot> slots(8U);
  auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{expected.data(), expected.size()}, slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::OK, "independent frame decodes") ||
      !Expect(slots[0].value_kind == LogicalValueKind::BOOL && slots[0].bool_value,
              "BOOL decodes natively") ||
      !Expect(slots[3].uint64_value == 0x5AU && slots[4].uint64_value == 0x101U,
              "cross-byte values decode") ||
      !Expect(slots[6].uint64_value == UINT64_C(0x0123456789ABCDEF), "full-width value decodes") ||
      !Expect(slots[7].value_kind == LogicalValueKind::BOOL && !slots[7].bool_value,
              "last bit decodes"))
    return 1;

  auto reserved_changed = expected;
  reserved_changed[0] ^= 0x10U;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{reserved_changed.data(), reserved_changed.size()},
      slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::OK,
              "Decode does not enforce base_value on uncovered bits"))
    return 1;

  values[2].uint64_value = 0x100U;
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{output.data(), output.size()});
  if (!Expect(encoded.status == CodecStatus::VALUE_NOT_REPRESENTABLE,
              "out-of-range dynamic member fails without truncation"))
    return 1;
  values[2].uint64_value = 0x5AU;
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size() - 1U,
      MutableByteBuffer{output.data(), output.size()});
  if (!Expect(encoded.status == CodecStatus::MISSING_FIELD, "missing member fails closed"))
    return 1;

  if (!Expect(CompileFails(config, "\"schema_version\": \"0.2\"", "\"schema_version\": \"0.1\""),
              "Schema 0.1 rejects bitfield and BOOL") ||
      !Expect(CompileFails(config, "\"bit_offset\": 3, \"bit_width\": 1",
                           "\"bit_offset\": 2, \"bit_width\": 1"),
              "overlapping members fail") ||
      !Expect(CompileFails(config, "\"container_id\": \"be_lsb16\", \"bit_offset\": 4",
                           "\"container_id\": \"be_lsb16\", \"bit_offset\": 12"),
              "member crossing container boundary fails") ||
      !Expect(CompileFails(config, "\"container_id\": \"tail8\"", "\"container_id\": \"flags8\""),
              "empty container fails") ||
      !Expect(CompileFails(config, "\"container_offset\": 3, \"container_width\": 2",
                           "\"container_offset\": 2, \"container_width\": 2"),
              "overlapping containers fail") ||
      !Expect(
          CompileFails(config, "\"fields\": [",
                       "\"fields\": [{\"id\":\"ordinary_overlap\",\"display_name\":\"ordinary\","
                       "\"description\":\"\",\"source_ref\":\"SYNTHETIC_FROM_SCRATCH:overlap\","
                       "\"value_type\":\"UINT64\",\"wire\":{\"codec\":\"unsigned_integer\","
                       "\"byte_offset\":0,\"byte_width\":1},\"encode\":{\"source\":\"input\"}},"),
          "ordinary field and container overlap fails") ||
      !Expect(CompileFails(config, "\"container_width\": 2, \"byte_order\": \"big_endian\"",
                           "\"container_width\": 2"),
              "multi-byte container requires byte order") ||
      !Expect(CompileFailsAt(config, "\"container_width\": 1, \"bit_numbering\": \"lsb0\"",
                             "\"container_width\": 1, \"byte_order\": \"big_endian\", "
                             "\"bit_numbering\": \"lsb0\"",
                             pae::config_compiler::CompileStage::STRUCTURAL,
                             pae::config_compiler::CompileError::UNKNOWN_PROPERTY,
                             "/messages/0/bit_containers/0/byte_order"),
              "single-byte container rejects explicit big-endian order at the property") ||
      !Expect(CompileFailsAt(config, "\"container_width\": 1, \"bit_numbering\": \"lsb0\"",
                             "\"container_width\": 1, \"byte_order\": \"little_endian\", "
                             "\"bit_numbering\": \"lsb0\"",
                             pae::config_compiler::CompileStage::STRUCTURAL,
                             pae::config_compiler::CompileError::UNKNOWN_PROPERTY,
                             "/messages/0/bit_containers/0/byte_order"),
              "single-byte container rejects explicit little-endian order at the property") ||
      !Expect(CompileFails(config,
                           "\"id\": \"flags8\", \"container_offset\": 0, \"container_width\": 1",
                           "\"id\": \"flags8\", \"container_offset\": 0, \"container_width\": 3"),
              "unsupported container width fails") ||
      !Expect(CompileFails(config, "\"base_value\": 160", "\"base_value\": 256"),
              "base value outside container width fails") ||
      !Expect(CompileFails(config, "\"container_id\": \"flags8\"",
                           "\"container_id\": \"missing_container\""),
              "unknown container reference fails") ||
      !Expect(CompileFails(config, "\"id\": \"tail8\"", "\"id\": \"flags8\""),
              "duplicate container id fails") ||
      !Expect(CompileFails(config, "\"id\": \"last_bit\"", "\"id\": \"enabled\""),
              "bit members share the ordinary field id namespace") ||
      !Expect(CompileFails(config,
                           "\"value_type\": \"BOOL\", \"wire\": {\"codec\": \"bitfield\", "
                           "\"container_id\": \"flags8\", \"bit_offset\": 0, \"bit_width\": 1",
                           "\"value_type\": \"BOOL\", \"wire\": {\"codec\": \"bitfield\", "
                           "\"container_id\": \"flags8\", \"bit_offset\": 0, \"bit_width\": 2"),
              "BOOL width other than one bit fails") ||
      !Expect(CompileFails(config, "\"id\": \"enabled\"", "\"id\": \"enabled\", \"extra\": 1"),
              "unknown member property fails closed"))
    return 1;

  if (!Expect(CompileWithFixedMatcher(config, 0U, "A5", false, "/messages/0/fields/2/encode/value"),
              "matcher conflict on a constant bit is rejected at DomainValidator") ||
      !Expect(CompileWithFixedMatcher(config, 0U, "AD", true),
              "matcher compatible with constant and reserved bits is accepted") ||
      !Expect(CompileWithFixedMatcher(config, 0U, "AE", true),
              "matcher is not rejected solely because dynamic member bits differ from base") ||
      !Expect(CompileWithFixedMatcher(config, 1U, "85 A0", true),
              "big-endian lsb0 matcher maps to encoded container bytes") ||
      !Expect(CompileWithFixedMatcher(config, 1U, "75 A0", false,
                                      "/messages/0/bit_containers/1/base_value"),
              "big-endian lsb0 reserved-bit conflict is rejected") ||
      !Expect(CompileWithFixedMatcher(config, 3U, "43 40", true),
              "little-endian msb0 matcher maps to encoded container bytes") ||
      !Expect(CompileWithFixedMatcher(config, 3U, "42 40", false,
                                      "/messages/0/bit_containers/2/base_value"),
              "little-endian msb0 reserved-bit conflict is rejected"))
    return 1;

  std::cout << "PAE_BITFIELD_CONTRACT_PASS\n";
  return 0;
}
