#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

#include "owned_description.h"

namespace {
namespace binary = pae::protocol_lab_binary;
void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
std::string Load(const std::string& directory, unsigned version) {
  std::ifstream input(directory + "/synthetic_ui_v0" + std::to_string(version) + ".pae.json");
  Check(input.good(), "fixture unavailable");
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string Promote(std::string json, unsigned version) {
  const auto position = json.find("\"0." + std::to_string(version) + "\"");
  Check(position != std::string::npos, "schema version absent");
  json.replace(position, 5U, "\"0.9\"");
  return json;
}
auto Compile(const std::string& json) {
  auto result = pae::config_compiler::CompileJsonToPlanWithMetadata(json, 4U * 1024U * 1024U);
  Check(result.Succeeded(), "compile failed");
  return std::move(result).TakeArtifacts();
}
binary::OwnedDescription Describe(const std::string& directory, unsigned version) {
  auto artifacts = Compile(Promote(Load(directory, version), version));
  return binary::BuildOwnedDescription(artifacts);
}
template <typename Function>
void Reject(Function function) {
  bool rejected = false;
  try {
    function();
  } catch (const binary::MaterializationError&) {
    rejected = true;
  }
  Check(rejected, "invalid description was accepted");
}
void Range(const std::optional<binary::ByteRange>& range, std::size_t offset, std::size_t length) {
  Check(range && range->offset == offset && range->length == length, "physical range mismatch");
}
void Bits(const binary::FieldDescriptor& field,
          std::initializer_list<binary::PhysicalBitMask> expected) {
  Check(field.physical_bits.size() == expected.size(), "bit range count mismatch");
  std::size_t index = 0U;
  for (auto bit : expected) {
    Check(field.physical_bits[index].frame_byte_index == bit.frame_byte_index &&
              field.physical_bits[index].uint8_mask == bit.uint8_mask,
          "physical mask mismatch");
    ++index;
  }
}
void Fixed(const std::string& directory) {
  // Compiler artifacts are already destroyed: all strings and containers must be owned.
  const auto description = Describe(directory, 5U);
  Check(description.display_name == "Public UI typed fields", "owned protocol text");
  Check(description.schema_version == "0.9" && description.pipelines.size() == 2U,
        "owned protocol shape");
  const auto& message = description.messages.at(0);
  Check(message.id == "typed_record" && message.fields.size() == 9U, "owned message");
  Check(description.pipelines[0].message_indices.at(0) == 0U, "pipeline index");
  Check(message.fields[0].source_ref == "SYNTHETIC:v05#enabled", "owned source text");
  const auto& enumeration = message.fields[1].enum_entries.at(1);
  Check(enumeration.id == "active" && enumeration.display_name == "Active" &&
            enumeration.raw_value == 2U,
        "owned enum display");
  Bits(message.fields[0], {{1U, 0x01U}});
  Bits(message.fields[1], {{1U, 0x06U}});
  Bits(message.fields[2], {{0U, 0x0FU}, {1U, 0xF0U}});
  Bits(message.fields[3], {{2U, 0xC0U}, {3U, 0x7FU}});
  Range(binary::ResolveActualFieldRange(message, message.fields[4], 10U), 4U, 1U);
  Check(!binary::ResolveActualFieldRange(message, message.fields[0], 10U),
        "bit field not full byte");
  Check(binary::IsActualFrameValid(message, 10U), "bit mapping frame precondition");
  const auto foreign_field = message.fields[4];
  Check(!binary::ResolveActualFieldRange(message, foreign_field, 10U), "foreign field rejected");
  for (auto size : {0U, 9U, 11U}) {
    Check(!binary::IsActualFrameValid(message, size), "bit mapping invalid frame rejected");
    Check(!binary::ResolveActualFieldRange(message, message.fields[4], size),
          "fixed frame size validation");
  }
  Check(description.accounted_total_bytes >= sizeof(binary::OwnedDescription) &&
            description.accounted_total_bytes <= 4U * 1024U * 1024U,
        "description budget");
  const auto crc = Describe(directory, 6U);
  Range(binary::ResolveActualIntegrityStorage(crc.messages[0], 12U), 9U, 2U);
  Check(!binary::ResolveActualIntegrityStorage(crc.messages[0], 11U), "CRC invalid frame");
  const auto length = Describe(directory, 7U);
  Range(length.messages[0].computed_length_storage, 1U, 2U);
}
void Bounded(const std::string& directory) {
  const auto description = Describe(directory, 8U);
  const auto& message = description.messages[0];
  Check(message.bounded_payload && message.integrity_storage_at_payload_end, "bounded metadata");
  for (std::size_t payload = 0U; payload <= 3U; ++payload) {
    Check(binary::IsActualFrameValid(message, 3U + payload), "bounded frame precondition");
    Range(binary::ResolveActualFieldRange(message, message.fields[1], 3U + payload), 2U, payload);
    Range(binary::ResolveActualFieldRange(message, message.fields[0], 3U + payload), 1U, 1U);
    Range(binary::ResolveActualIntegrityStorage(message, 3U + payload), 2U + payload, 1U);
  }
  for (auto size : {0U, 2U, 7U}) {
    Check(!binary::IsActualFrameValid(message, size), "bounded invalid frame rejected");
    Check(!binary::ResolveActualFieldRange(message, message.fields[0], size),
          "bounded fixed field validation");
    Check(!binary::ResolveActualFieldRange(message, message.fields[1], size),
          "bounded payload validation");
    Check(!binary::ResolveActualIntegrityStorage(message, size), "bounded integrity validation");
  }
}
void Limits(const std::string& directory) {
  auto artifacts = Compile(Promote(Load(directory, 5U), 5U));
  binary::DescriptionLimits limits;
  limits.max_description_bytes = 1U;
  Reject([&] { (void)binary::BuildOwnedDescription(artifacts, limits); });
  limits = {};
  limits.max_fields = 1U;
  Reject([&] { (void)binary::BuildOwnedDescription(artifacts, limits); });
  limits = {};
  limits.max_frame_bytes = 1U;
  Reject([&] { (void)binary::BuildOwnedDescription(artifacts, limits); });
  limits = {};
  limits.max_identity_bytes = 1U;
  Reject([&] { (void)binary::BuildOwnedDescription(artifacts, limits); });
  limits = {};
  limits.max_description_bytes = std::numeric_limits<std::size_t>::max();
  Reject([&] { (void)binary::BuildOwnedDescription(artifacts, limits); });
  auto wrong_schema = Compile(Load(directory, 5U));
  Reject([&] { (void)binary::BuildOwnedDescription(wrong_schema); });
  auto plan = artifacts.TakePlan();
  Reject([&] { (void)binary::BuildOwnedDescription(artifacts); });
  auto missing_description = Compile(Promote(Load(directory, 5U), 5U));
  auto sidecar = missing_description.TakeDescription();
  Reject([&] { (void)binary::BuildOwnedDescription(missing_description); });
}
}  // namespace
int main(int argc, char** argv) {
  try {
    Check(argc == 2, "expected fixture directory");
    Fixed(argv[1]);
    Bounded(argv[1]);
    Limits(argv[1]);
    std::cout << "owned description contract passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
