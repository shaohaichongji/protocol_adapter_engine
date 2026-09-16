#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

#include "../../tools/protocol_lab_ui/binary_host_adapter.h"
#include "../../tools/protocol_lab_ui/compile_worker.h"
#include "../../tools/protocol_lab_ui/document_session.h"

namespace ui = pae::protocol_lab_ui;

std::string Read(const wchar_t* path) {
  std::ifstream input(std::filesystem::path(path), std::ios::binary);
  assert(input.good());
  return {std::istreambuf_iterator<char>{input}, {}};
}

void FailBinaryFlowCopy() { throw std::runtime_error("injected Binary Flow copy failure"); }

std::unique_ptr<ui::CompileCompletion> Compile(ui::CompileWorker& worker,
                                                ui::DocumentId document,
                                                ui::Revision revision,
                                                std::string_view text) {
  assert(worker.Submit(document, revision, text) == ui::SubmitStatus::ACCEPTED);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    for (auto ticket : worker.DrainReadyTickets()) {
      auto completion = worker.TakeResult(ticket);
      if (completion && completion->document_id == document &&
          completion->load_revision == revision) return completion;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  std::cerr << "H2_WORKER_TIMEOUT document=" << document << std::endl;
  std::exit(2);
}

int main() {
  const auto binary_json = Read(PAE_BINARY_UI_CONFIG);
  const auto legacy_json = Read(PAE_LEGACY_UI_CONFIG);
  const auto ascii_json = Read(PAE_ASCII_TEXT_CONFIG);
  ui::CompileWorker worker;
  auto bad_dispatch = Compile(worker, 101U, 1U,
      R"({"schema_version":"0.9","schema_\u0076ersion":"0.5"})");
  assert(bad_dispatch->route == ui::SchemaDispatchStatus::CLASSIFICATION_FAILED);
  assert(!bad_dispatch->classification_error.empty() &&
         bad_dispatch->compiler_attempt_count == 0U && !bad_dispatch->artifacts &&
         !bad_dispatch->public_compiled);
  auto bad_binary = Compile(worker, 102U, 1U, R"({"schema_version":"0.9"})");
  assert(bad_binary->route == ui::SchemaDispatchStatus::BINARY_PUBLIC &&
         bad_binary->compiler_attempt_count == 1U && bad_binary->public_diagnostic &&
         !bad_binary->artifacts && !bad_binary->diagnostic);
  auto bad_legacy = Compile(worker, 103U, 1U, R"({"schema_version":"0.5"})");
  assert(bad_legacy->route == ui::SchemaDispatchStatus::PRIVATE_LEGACY &&
         bad_legacy->compiler_attempt_count == 1U && bad_legacy->diagnostic &&
         !bad_legacy->public_compiled && !bad_legacy->public_diagnostic);
  auto legacy = Compile(worker, 104U, 1U, legacy_json);
  assert(legacy->route == ui::SchemaDispatchStatus::PRIVATE_LEGACY &&
         legacy->compiler_attempt_count == 1U && legacy->artifacts &&
         !legacy->public_compiled);
  auto ascii = Compile(worker, 105U, 1U, ascii_json);
  assert(ascii->route == ui::SchemaDispatchStatus::PRIVATE_ASCII &&
         ascii->compiler_attempt_count == 1U && ascii->artifacts &&
         !ascii->public_compiled);
  ui::DocumentSession session{106U};
  const auto load = session.BeginLoad();
  auto opening = Compile(worker, 106U, load, binary_json);
  const auto config_hash = opening->config_sha256;
  assert(opening->route == ui::SchemaDispatchStatus::BINARY_PUBLIC &&
         opening->compiler_attempt_count == 1U && opening->public_compiled &&
         !opening->artifacts && !opening->diagnostic);
  assert(session.ApplyCompileCompletion(std::move(opening)) &&
         session.IsBinaryHostDocument() && !session.BinaryHostActive() &&
         session.state() == ui::DocumentState::READY &&
         session.description()->messages[0].fields[7].decode_decimal64);

  auto compiled = pae::CompileProtocolJson(binary_json);
  assert(compiled.Succeeded());
  std::string error;
  auto adapter = ui::BinaryHostAdapter::CreatePublic(
      std::move(compiled).TakeCompiled(),
      {{"device", pae::HostAction::DECODE, "ui_pipeline", 2U},
       {"other", pae::HostAction::DECODE, "alternate_pipeline", 2U}},
      {106U, load, 1U, 9U, config_hash}, nullptr, 0U, 0U, error);
  assert(adapter && error.empty() && adapter->FlowCount(0U) == 2U &&
         adapter->FlowCount(1U) == 2U);
  assert(adapter->Description().messages[0].fields[0].physical_bits.size() > 0U);
  assert(adapter->AccountedInstanceBytes() <= 128U * 1024U * 1024U);
  const auto description_upper = adapter->DescriptionCopyUpperBoundBytes();
  assert(description_upper > 0U);
  int description_copy_calls = 0;
  ui::BinaryUiCopyControls copy_reject;
  copy_reject.description_copy_limit = description_upper - 1U;
  copy_reject.before_description_copy =
      +[](void* context) { ++*static_cast<int*>(context); };
  copy_reject.context = &description_copy_calls;
  auto description_reject_compiled = pae::CompileProtocolJson(binary_json);
  assert(description_reject_compiled.Succeeded());
  std::string description_reject_error;
  auto description_reject = ui::BinaryHostAdapter::CreatePublic(
      std::move(description_reject_compiled).TakeCompiled(),
      {{"device", pae::HostAction::DECODE, "ui_pipeline", 2U}},
      {107U, 1U, 1U, 1U, "description-reject"}, nullptr, 0U, 0U,
      description_reject_error, {}, &copy_reject);
  assert(!description_reject && description_copy_calls == 0 &&
         description_reject_error == "public Binary description copy preflight exceeded");
  const std::vector<std::uint8_t> frame{
      0x80U, 0x0DU, 0x03U, 0x00U, 0x01U, 0x00U, 0xCAU, 0xFEU, 0x05U, 0x5AU};
  auto first = adapter->DecodeComplete(0U, 0U, frame);
  assert(first.ok && first.public_host.codec_attempted && first.result &&
         first.result->fields.size() == 9U &&
         first.result->fields[0].raw_value == "未单独提供" &&
         first.result->fields[1].logical_value == "Active [active]" &&
         first.result->fields[6].raw_value == "CAFE" &&
         first.result->fields[7].raw_value == "5" &&
         first.result->fields[7].logical_value == "5@0");
  assert(adapter->Current(0U, 0U) && !adapter->Current(0U, 1U));
  assert(adapter->MapCurrent(0U, 0U).result && !adapter->MapCurrent(0U, 1U).result);
  adapter->SaveAndSelect(u"80 0D", 0U, 1U);
  assert(adapter->Draft(0U, 0U) == u"80 0D" && adapter->Draft(0U, 1U).empty());
  const std::vector<std::uint8_t> alternate{0xC3U, 0x2AU};
  auto second_binding = adapter->DecodeComplete(1U, 0U, alternate);
  assert(second_binding.ok && second_binding.result &&
         second_binding.result->message_id == "alternate_record" &&
         second_binding.result->fields[0].logical_value == "42");
  assert(adapter->MapCurrent(0U, 0U).result && adapter->MapCurrent(1U, 0U).result);
  const auto active_upper = adapter->CurrentResultCopyUpperBoundBytes(0U, 0U);
  const auto mapped_budget = adapter->UiViewReserveBytes() / 2U;
  assert(active_upper > 0U && active_upper <= mapped_budget);
  assert(adapter->SetPresentationRetainedBytes(mapped_budget - active_upper + 1U));
  bool rejected_mapping = false;
  try { (void)adapter->MapCurrent(0U, 0U); }
  catch (const std::exception&) { rejected_mapping = true; }
  assert(rejected_mapping && adapter->Current(0U, 0U));
  assert(adapter->SetPresentationRetainedBytes(0U));
  assert(adapter->MapCurrent(0U, 0U).result);
  auto invalid_frame = frame;
  invalid_frame.pop_back();
  auto failed = adapter->DecodeComplete(0U, 0U, invalid_frame);
  assert(!failed.ok && failed.failure && !adapter->MapCurrent(0U, 0U).result &&
         adapter->MapCurrent(1U, 0U).result);
  const auto old_instance = adapter->Instance();
  auto replacement_compiled = pae::CompileProtocolJson(binary_json);
  assert(replacement_compiled.Succeeded());
  std::string replacement_error;
  auto replacement = ui::BinaryHostAdapter::CreatePublic(
      std::move(replacement_compiled).TakeCompiled(),
      {{"device", pae::HostAction::DECODE, "ui_pipeline", 2U},
       {"other", pae::HostAction::DECODE, "alternate_pipeline", 2U}},
      {106U, load, 2U, 10U, config_hash}, adapter.get(), 0U, 0U,
      replacement_error);
  assert(replacement && replacement_error.empty() &&
         replacement->Instance() != old_instance && adapter->MapCurrent(1U, 0U).result &&
         !replacement->MapCurrent(1U, 0U).result);
  auto session_compiled = pae::CompileProtocolJson(binary_json);
  assert(session_compiled.Succeeded());
  std::string session_error;
  auto session_adapter = ui::BinaryHostAdapter::CreatePublic(
      std::move(session_compiled).TakeCompiled(),
      {{"device", pae::HostAction::DECODE, "ui_pipeline", 2U},
       {"other", pae::HostAction::DECODE, "alternate_pipeline", 2U}},
      {106U, load, 1U, 9U, config_hash}, nullptr, 0U, 0U, session_error);
  assert(session_adapter && session_error.empty());
  auto publication = session.PrepareBinaryHostPublication(std::move(session_adapter), 9U);
  assert(publication && publication->description.messages[0].fields[7].decode_decimal64);
  session.PublishBinaryHostPublication(std::move(*publication));
  assert(session.BinaryHostActive() && session.BinarySessionRevision() == 1U);
  assert(session.SetInspectDraftUtf16(u"80 0D 03 00 01 00 CA FE 05 5A"));
  auto fresh_flow = session.PrepareBinaryHostFlow(0U, 1U);
  if (!fresh_flow || !fresh_flow->inspect_draft_utf16.empty()) {
    std::cerr << "H2_FLOW_PRECONDITION_FAIL new Flow is not empty\n";
    return 2;
  }
  assert(session.PublishBinaryHostFlow(std::move(*fresh_flow)));
  if (session.BinaryHostFlowIndex() != 1U || !session.inspect_draft_utf16().empty() ||
      session.prepared()->binary_host_adapter->Draft(0U, 0U) !=
          u"80 0D 03 00 01 00 CA FE 05 5A" ||
      !session.prepared()->binary_host_adapter->Draft(0U, 1U).empty()) {
    std::cerr << "H2_FLOW_DRAFT_ISOLATION_FAIL first empty Flow inherited source draft\n";
    return 1;
  }
  const auto move_to = [&](std::size_t binding, std::size_t flow) {
    auto prepared = session.PrepareBinaryHostFlow(binding, flow);
    return prepared && session.PublishBinaryHostFlow(std::move(*prepared));
  };
  const auto matches = [&](std::size_t binding, std::size_t flow,
                           std::u16string_view draft, int count, std::uint8_t frame_byte) {
    if (session.BinaryHostBindingIndex() != binding || session.BinaryHostFlowIndex() != flow ||
        session.inspect_draft_utf16() != draft) return false;
    if (count < 0) return !session.inspect_result();
    return session.inspect_result() && session.inspect_result()->fields.size() == 9U &&
           session.inspect_result()->fields[4].logical_value == std::to_string(count) &&
           session.inspect_result()->input_frame.size() == 10U &&
           session.inspect_result()->input_frame[4] == frame_byte;
  };
  constexpr auto flow0_frame = u"80 0D 03 00 01 00 CA FE 05 5A";
  constexpr auto flow1_frame = u"80 0D 03 00 02 00 CA FE 05 5A";
  if (!session.SetInspectDraftUtf16(flow1_frame) || !session.Inspect() ||
      !matches(0U, 1U, flow1_frame, 2, 2U) || !move_to(0U, 0U) ||
      !matches(0U, 0U, flow0_frame, -1, 0U) || !session.Inspect() ||
      !matches(0U, 0U, flow0_frame, 1, 1U) || !move_to(0U, 1U) ||
      !matches(0U, 1U, flow1_frame, 2, 2U) || !move_to(0U, 0U) ||
      !matches(0U, 0U, flow0_frame, 1, 1U)) {
    std::cerr << "H2_FLOW_ROUNDTRIP_FAIL draft, count, or decoded frame mismatched\n";
    return 3;
  }
  constexpr auto uninspected = u"80 0D 03 00 03 00 CA FE 05 5A";
  if (!session.SetInspectDraftUtf16(uninspected) || session.inspect_result() ||
      !move_to(0U, 1U) || !matches(0U, 1U, flow1_frame, 2, 2U) ||
      !move_to(0U, 0U) || !matches(0U, 0U, uninspected, 1, 1U) ||
      !move_to(1U, 0U) || !matches(1U, 0U, u"", -1, 0U) ||
      session.prepared()->binary_host_adapter->Draft(0U, 0U) != uninspected) {
    std::cerr << "H2_FLOW_UNINSPECTED_OR_BINDING_FAIL binding="
              << session.BinaryHostBindingIndex() << " flow=" << session.BinaryHostFlowIndex()
              << " draft=" << session.inspect_draft() << " result="
              << session.inspect_result().has_value() << " saved0="
              << (session.prepared()->binary_host_adapter->Draft(0U, 0U) == uninspected)
              << "\n";
    return 4;
  }
  if (!session.SetInspectDraftUtf16(u"C3 2A") || !session.Inspect() ||
      !session.inspect_result() || session.inspect_result()->message_id != "alternate_record" ||
      session.inspect_result()->input_frame != alternate || !move_to(0U, 1U) ||
      !matches(0U, 1U, flow1_frame, 2, 2U) || !move_to(1U, 0U) ||
      session.inspect_draft_utf16() != u"C3 2A" || !session.inspect_result() ||
      session.inspect_result()->input_frame != alternate) {
    std::cerr << "H2_FLOW_CROSS_BINDING_FAIL alternate draft or result mismatched\n";
    return 5;
  }
  const auto* active = session.prepared()->binary_host_adapter.get();
  const std::u16string saved_other(active->Draft(1U, 0U));
  const std::u16string saved_flow1(active->Draft(0U, 1U));
  if (!session.SetInspectDraftUtf16(std::u16string(active->DraftLimit(1U, 0U) + 1U, u'A'))) {
    std::cerr << "H2_FLOW_FAILURE_PRECONDITION_FAIL oversize editor draft rejected\n";
    return 6;
  }
  auto rejected = session.PrepareBinaryHostFlow(0U, 1U);
  if (!rejected || session.PublishBinaryHostFlow(std::move(*rejected)) ||
      session.BinaryHostBindingIndex() != 1U || session.BinaryHostFlowIndex() != 0U ||
      active->Draft(1U, 0U) != saved_other || active->Draft(0U, 1U) != saved_flow1) {
    std::cerr << "H2_FLOW_SAVE_REJECTION_FAIL flow state partially published\n";
    return 7;
  }
  if (!session.SetInspectDraftUtf16(u"C3 2A")) return 8;
  const auto failed_copy = session.PrepareBinaryHostFlow(0U, 1U, &FailBinaryFlowCopy);
  if (failed_copy || session.BinaryHostBindingIndex() != 1U ||
      session.BinaryHostFlowIndex() != 0U || session.inspect_draft_utf16() != u"C3 2A" ||
      active->Draft(1U, 0U) != saved_other || active->Draft(0U, 1U) != saved_flow1) {
    std::cerr << "H2_FLOW_COPY_REJECTION_FAIL flow state partially published\n";
    return 9;
  }
  session.Close();
  assert(session.state() == ui::DocumentState::CLOSED);
  return 0;
}
