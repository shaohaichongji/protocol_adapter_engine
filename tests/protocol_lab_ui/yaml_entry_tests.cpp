#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "../../tools/protocol_lab_ui/compile_worker.h"
#include "../../tools/protocol_lab_ui/document_session.h"
#include "test_support.h"

namespace {

using namespace pae::protocol_lab_ui;

std::string Read(const char* relative) {
  std::ifstream input(std::filesystem::u8path(PAE_TEST_SOURCE) / relative, std::ios::binary);
  assert(input);
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::unique_ptr<CompileCompletion> Take(CompileWorker& worker) {
  assert(test::WaitUntil([&] { return worker.StoredResultCountForTesting() != 0U; }));
  const auto tickets = worker.DrainReadyTickets();
  assert(tickets.size() == 1U);
  auto result = worker.TakeResult(tickets.front());
  assert(result);
  return result;
}

void CheckCodec(const pae::CompiledProtocol& compiled, bool ascii) {
  auto created = pae::CreateCompleteRecordCodec(compiled);
  assert(created.status == pae::CodecStatus::OK && created.codec);
  if (ascii) {
    const std::string input = "PING\r\n";
    const auto decoded = created.codec->Decode(
        0, {reinterpret_cast<const std::uint8_t*>(input.data()), input.size()});
    assert(decoded.status == pae::CodecStatus::OK);
    std::array<std::uint8_t, 6> output{};
    const auto encoded = created.codec->Encode(0, 0, nullptr, 0, {output.data(), output.size()});
    assert(encoded.status == pae::CodecStatus::OK);
    assert(std::string_view(reinterpret_cast<const char*>(output.data()), output.size()) ==
           "PONG\r\n");
  } else {
    const std::array<std::uint8_t, 12> frame = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
                                                 0x37, 0x38, 0x39, 0x29, 0xB1, 0xAA};
    const auto decoded = created.codec->Decode(0, {frame.data(), frame.size()});
    assert(decoded.status == pae::CodecStatus::OK);
    const std::array<std::uint8_t, 9> payload = {0x31, 0x32, 0x33, 0x34, 0x35,
                                                  0x36, 0x37, 0x38, 0x39};
    auto value = pae::EncodeValue::Bytes({0, 0}, {payload.data(), payload.size()});
    std::array<std::uint8_t, 12> output{};
    const auto encoded = created.codec->Encode(0, 0, &value, 1,
                                                {output.data(), output.size()});
    assert(encoded.status == pae::CodecStatus::OK && output == frame);
  }
}

void Positive(const char* yaml_path, const char* json_path, bool ascii, DocumentId document) {
  const auto source = Read(yaml_path);
  const auto original = Read(json_path);
  CompileWorker worker;
  assert(worker.Submit(document, 1U, source, ConfigSourceFormat::YAML, yaml_path) ==
         SubmitStatus::ACCEPTED);
  auto completion = Take(worker);
  assert(completion->route == (ascii ? SchemaDispatchStatus::ASCII_PUBLIC
                                     : SchemaDispatchStatus::LEGACY_PUBLIC));
  assert(completion->public_compiled && completion->yaml_conversion);
  assert(completion->yaml_conversion->SourceIdentity() == yaml_path);
  assert(completion->compiler_attempt_count == 1U);
  auto direct = pae::CompileProtocolJson(original);
  assert(direct.Succeeded());
  assert(completion->public_compiled->PipelineCount() == direct.Compiled()->PipelineCount());
  assert(completion->public_compiled->MessageCount() == direct.Compiled()->MessageCount());
  CheckCodec(*completion->public_compiled, ascii);
  CheckCodec(*direct.Compiled(), ascii);

  // Host reprepare must submit the generated JSON, preserving compiled-config hash.
  const std::string generated{completion->yaml_conversion->Json()};
  assert(worker.Submit(document, 2U, generated) == SubmitStatus::ACCEPTED);
  auto host = Take(worker);
  assert(host->public_compiled && !host->yaml_conversion);
  assert(host->config_sha256 == completion->config_sha256);
  worker.CloseDocument(document);
}

void FailureAndIsolation() {
  CompileWorker worker;
  const auto ascii = Read("tests/protocol_lab_ui/fixtures/synthetic_ascii_literal_only.pae.yaml");
  const auto json = Read("examples/config/synthetic_ascii_literal_only.pae.json");
  assert(worker.Submit(89U, 1U, json) == SubmitStatus::ACCEPTED);
  auto json_completion = Take(worker);
  assert(json_completion->route == SchemaDispatchStatus::ASCII_PUBLIC);
  assert(json_completion->public_compiled && !json_completion->yaml_conversion);
  worker.CloseDocument(89U);
  assert(worker.Submit(90U, 1U, std::string(16 * 1024 + 1, 'x'),
                       ConfigSourceFormat::YAML) == SubmitStatus::CONFIG_TOO_LARGE);
  assert(worker.Submit(90U, 2U, "a: [", ConfigSourceFormat::YAML) == SubmitStatus::ACCEPTED);
  auto invalid = Take(worker);
  assert(invalid->route == SchemaDispatchStatus::CLASSIFICATION_FAILED);
  assert(invalid->classification_error.find("YAML 转换失败") != std::string::npos);
  assert(invalid->classification_error.find("未提供") != std::string::npos);
  assert(invalid->compiler_attempt_count == 0U);
  worker.CloseDocument(90U);

  auto private_route = ascii;
  const auto version = private_route.find("schema_version: \"0.10\"");
  assert(version != std::string::npos);
  private_route.replace(version, std::string("schema_version: \"0.10\"").size(),
                        "schema_version: \"0.11\"");
  assert(worker.Submit(95U, 1U, private_route, ConfigSourceFormat::YAML) ==
         SubmitStatus::ACCEPTED);
  auto rejected = Take(worker);
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  assert(rejected->route == SchemaDispatchStatus::ASCII_PUBLIC);
  assert(rejected->compiler_attempt_count == 1U);
  assert(rejected->public_compiled);
#else
  assert(rejected->route == SchemaDispatchStatus::CLASSIFICATION_FAILED);
  assert(rejected->classification_error.find("public Lab compiler route") != std::string::npos);
  assert(rejected->compiler_attempt_count == 0U);
#endif
  worker.CloseDocument(95U);

  auto exact_source = ascii + "unknown_property: 1\n";
  assert(worker.Submit(91U, 1U, exact_source, ConfigSourceFormat::YAML) ==
         SubmitStatus::ACCEPTED);
  auto exact = Take(worker);
  assert(exact->structured_compile_diagnostic);
  assert(exact->structured_compile_diagnostic->json_pointer == "/unknown_property");
  assert(exact->structured_compile_diagnostic->yaml_line == 41U);
  assert(!exact->structured_compile_diagnostic->yaml_approximate);
  const auto exact_text = FormatCompileDiagnostic(*exact->structured_compile_diagnostic);
  assert(exact_text.find("生成 JSON 字节偏移") != std::string::npos);
  assert(exact_text.find("YAML 原文位置：41:1") != std::string::npos);
  worker.CloseDocument(91U);

  auto missing_source = ascii;
  const auto anchor = missing_source.find("resource_profile: desktop\n");
  assert(anchor != std::string::npos);
  missing_source.erase(anchor, std::string("resource_profile: desktop\n").size());
  assert(worker.Submit(92U, 1U, missing_source, ConfigSourceFormat::YAML) ==
         SubmitStatus::ACCEPTED);
  auto approximate = Take(worker);
  assert(approximate->structured_compile_diagnostic);
  assert(approximate->structured_compile_diagnostic->yaml_line);
  assert(approximate->structured_compile_diagnostic->yaml_approximate);
  assert(FormatCompileDiagnostic(*approximate->structured_compile_diagnostic)
             .find("近似/容器回退") != std::string::npos);
  CompileDiagnosticView unmapped;
  unmapped.yaml_source = true;
  assert(FormatCompileDiagnostic(unmapped).find("YAML 原文位置：未映射") != std::string::npos);
  worker.CloseDocument(92U);

  assert(worker.Submit(93U, 1U, ascii, ConfigSourceFormat::YAML) == SubmitStatus::ACCEPTED);
  worker.CloseDocument(93U);
  assert(test::WaitUntil([&] { return !worker.HasActiveRequestForTesting(); }));
  assert(worker.DrainReadyTickets().empty());

  // A previous load revision is rejected even when its conversion and compiled owner are valid.
  DocumentSession session{94U};
  const auto old_revision = session.BeginLoad();
  assert(worker.Submit(session.id(), old_revision, ascii, ConfigSourceFormat::YAML) ==
         SubmitStatus::ACCEPTED);
  auto stale = Take(worker);
  assert(stale->yaml_conversion && stale->public_compiled);
  session.BeginLoad();
  assert(!session.ApplyCompileCompletion(std::move(stale)));
  worker.CloseDocument(session.id());
  session.Close();
}

}  // namespace

int main() {
  Positive("tests/protocol_lab_ui/fixtures/synthetic_crc_slice.pae.yaml",
           "examples/config/synthetic_crc_slice.pae.json", false, 11U);
  Positive("tests/protocol_lab_ui/fixtures/synthetic_ascii_literal_only.pae.yaml",
           "examples/config/synthetic_ascii_literal_only.pae.json", true, 12U);
  FailureAndIsolation();
  std::puts("LAB_YAML_ENTRY_TEST_PASS");
}
