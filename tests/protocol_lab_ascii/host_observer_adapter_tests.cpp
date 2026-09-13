#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include "host_observer_adapter.h"

namespace ascii = pae::protocol_lab::ascii;
namespace host = pae::host_endpoint;
void Check(bool condition, const char* why) {
  if (!condition) throw std::runtime_error(why);
}
std::vector<std::uint8_t> Bytes(std::string_view value) { return {value.begin(), value.end()}; }
pae::config_compiler::CompiledUiArtifacts Compile(const std::string& json) {
  auto result = pae::config_compiler::CompileJsonToPlanWithUiDescription(
      json, pae::config_compiler::DerivedUiDescriptionMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  Check(result.Succeeded(), "compile");
  return std::move(result).TakeArtifacts();
}
int main(int argc, char** argv) {
  try {
    Check(argc == 3, "arguments");
    std::ifstream file(argv[1]);
    const std::string json{std::istreambuf_iterator<char>(file), {}};
    std::string error;
    auto adapter = ascii::HostObserverAdapter::Create(
        Compile(json), {{"device", host::Action::DECODE, 0U}, {"device", host::Action::ENCODE, 0U}},
        error);
    Check(adapter != nullptr, error.c_str());
    ascii::ExecutionIdentity identity;
    identity.pipeline_id = adapter->Description().pipelines[0].id;
    auto first = adapter->Submit(0U, 0U, identity, Bytes("RX A!"));
    Check(first.status == ascii::AdapterStatus::OK && first.after.buffered_bytes == 5U, "half");
    adapter->Submit(0U, 1U, identity, Bytes("ON"));
    auto second = adapter->Submit(0U, 0U, identity, Bytes("OK\r\nBAD\r\nONLY\r\n"));
    Check(second.candidate && second.candidate->status == ascii::AdapterStatus::OK &&
              second.candidate->fields[0].bytes == Bytes("A") &&
              second.framing.bytes_consumed == 4U,
          "decode");
    auto saved = *second.candidate;
    Check(second.candidate->identity.binding_index == 0U &&
              second.candidate->identity.stream_index == 0U,
          "candidate identity");
    auto bad = adapter->Continue(0U, 0U, identity);
    Check(bad.candidate && bad.candidate->status == ascii::AdapterStatus::CORE_FAILED &&
              bad.candidate->diagnostic_input_frame == Bytes("BAD\r\n") &&
              bad.framing.bytes_consumed == 5U && bad.framing.frames_delivered == 1U,
          "bad candidate STOP");
    auto good = adapter->Continue(0U, 0U, identity);
    Check(good.candidate && good.candidate->status == ascii::AdapterStatus::OK &&
              good.candidate->fields.empty() && !good.after.frozen_input_bytes,
          "zero fields");
    Check(good.after.total_candidates == 3U && good.after.total_decode_successes == 2U &&
              good.after.total_observed_candidates == 3U && good.after.total_business_outputs == 2U,
          "separate candidate/core/observer/business counters");
    Check(adapter->Reset(0U, 0U) && adapter->Observe(0U, 1U)->buffered_bytes == 2U &&
              adapter->Observe(0U, 0U)->generation == 1U,
          "reset isolation");
    Check(adapter->HasDiscardableState(), "nonselected state");
    auto other = adapter->Submit(0U, 1U, identity, Bytes("LY\r\n"));
    Check(other.candidate && other.candidate->status == ascii::AdapterStatus::OK, "other flow");
    auto wrong = identity;
    wrong.pipeline_id = "missing";
    Check(!adapter->Submit(0U, 0U, wrong, Bytes("ONLY\r\n")).push_called, "identity rejection");
    Check(!ascii::HostObserverAdapter::Create(
              Compile(json), {{"dup", host::Action::DECODE, 0U}, {"dup", host::Action::DECODE, 0U}},
              error),
          "duplicate reject");
    identity.message_index = 0U;
    identity.message_id = adapter->Description().messages[0].id;
    auto encoded =
        adapter->Encode(1U, identity, {{0U, "name", Bytes("A")}, {2U, "tx_tag", Bytes("Z")}});
    Check(encoded.status == ascii::AdapterStatus::OK && encoded.frame == Bytes("TX A!Z\r\n"),
          "encode");
    identity.message_index.reset();
    identity.message_id.reset();
    adapter->SetCopyLimitForTesting(0U);
    auto fault = adapter->Submit(0U, 0U, identity, Bytes("ONLY\r\nONLY\r\n"));
    Check(fault.status == ascii::AdapterStatus::MATERIALIZATION_FAILED &&
              fault.framing.bytes_consumed == 6U && fault.after.reset_required && !fault.candidate,
          "copy fault");
    Check(fault.after.total_decode_successes == 1U && fault.after.total_observed_candidates == 0U &&
              fault.after.total_business_outputs == 0U,
          "copy failure delivery counters");
    adapter.reset();
    Check(saved.frame == Bytes("RX A!OK\r\n") && saved.fields[0].bytes == Bytes("A"),
          "owned results");
    std::ifstream complete_file(argv[2]);
    const std::string complete_json{std::istreambuf_iterator<char>(complete_file), {}};
    auto complete = ascii::HostObserverAdapter::Create(
        Compile(complete_json),
        {{"device", host::Action::DECODE, 0U}, {"device", host::Action::ENCODE, 0U}}, error);
    Check(complete != nullptr, "complete registration");
    identity.pipeline_id = complete->Description().pipelines[0].id;
    auto decoded = complete->Inspect(0U, 1U, identity, Bytes("RX ALICE!OK\r\n"));
    Check(decoded.status == ascii::AdapterStatus::OK && decoded.fields.size() == 2U &&
              decoded.fields[0].bytes == Bytes("ALICE") &&
              decoded.frame == Bytes("RX ALICE!OK\r\n"),
          "complete borrowed result copied");
    auto rejected = complete->Inspect(0U, 0U, identity, Bytes("BAD\r\n"));
    Check(rejected.status == ascii::AdapterStatus::CORE_FAILED && rejected.fields.empty() &&
              rejected.diagnostic_input_frame == Bytes("BAD\r\n"),
          "complete failed candidate");
    Check(!complete->Submit(0U, 0U, identity, Bytes("ONLY\r\n")).push_called,
          "complete rejects stream");
    auto isolated = ascii::HostObserverAdapter::Create(Compile(json),
                                                       {{"rx", host::Action::DECODE, 0U},
                                                        {"tx", host::Action::ENCODE, 1U},
                                                        {"complete", host::Action::DECODE, 2U}},
                                                       error);
    Check(isolated != nullptr, "one-way supported bindings");
    Check(!ascii::HostObserverAdapter::Create(Compile(json), {{"x", host::Action::DECODE, 1U}},
                                              error),
          "encode-only rejects Decode registration");
    Check(!ascii::HostObserverAdapter::Create(Compile(json), {{"x", host::Action::ENCODE, 2U}},
                                              error),
          "decode-only rejects Encode registration");
    Check(!ascii::HostObserverAdapter::Create(Compile(json), {{"x", host::Action::DECODE, 99U}},
                                              error),
          "unknown Pipeline");
    identity.pipeline_index = 2U;
    identity.pipeline_id = "decode_only_pipeline";
    Check(isolated->Inspect(2U, 0U, identity, Bytes("ONLY\r\n")).status == ascii::AdapterStatus::OK,
          "V11 complete Decode");
    identity.pipeline_index = 1U;
    identity.pipeline_id = "encode_only_pipeline";
    identity.message_index = 2U;
    identity.message_id = "encode_only";
    Check(isolated->Encode(1U, identity, {}).frame == Bytes("SEND\r\n"), "V11 complete Encode");
    identity.message_index = 0U;
    identity.message_id = "greeting";
    Check(isolated->Encode(1U, identity, {}).status != ascii::AdapterStatus::OK,
          "cross Pipeline Message rejected");
    identity.message_index.reset();
    identity.message_id.reset();
    identity.pipeline_index = 0U;
    identity.pipeline_id = "ascii_pipeline";
    auto cr = isolated->Submit(0U, 0U, identity, Bytes("RX B!OK\r"));
    auto lf = isolated->Submit(0U, 0U, identity, Bytes("\n"));
    Check(!cr.candidate && lf.candidate && lf.candidate->frame == Bytes("RX B!OK\r\n"),
          "split CR LF");
    auto discarded = isolated->Submit(0U, 0U, identity, Bytes("1234567890123"));
    Check(!discarded.candidate && discarded.after.total_malformed_candidates == 1U &&
              isolated->HasDiscardableState(),
          "discard is not a Core candidate");
    auto recovered = isolated->Submit(0U, 0U, identity, Bytes("\r\nONLY\r\n"));
    Check(recovered.candidate && recovered.candidate->status == ascii::AdapterStatus::OK,
          "overlong recovery");
    pae::protocol_framing::FramingLimitOverrides limits;
    limits.max_submit_bytes = 8U;
    limits.max_work_units = 6U;
    auto budgeted = ascii::HostObserverAdapter::Create(
        Compile(json), {{"rx", host::Action::DECODE, 0U}}, error, limits);
    Check(budgeted != nullptr, "work budget registration");
    auto short_step = budgeted->Submit(0U, 0U, identity, Bytes("RX A!"));
    Check(short_step.framing.bytes_consumed == 3U && short_step.after.frozen_cursor == 3U,
          "strict suffix");
    auto tail = budgeted->Continue(0U, 0U, identity);
    Check(tail.framing.bytes_consumed == 2U && tail.after.buffered_bytes == 5U, "suffix no replay");
    Check(!budgeted->Submit(0U, 0U, identity, Bytes("123456789")).push_called,
          "whole chunk capacity rejection");
    std::cout << "LAB_HOST_ADAPTER=PASS\n";
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
