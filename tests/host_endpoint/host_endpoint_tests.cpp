#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#include "../../src/config_compiler/config_compiler.h"
#include "host_endpoint.h"

namespace {
bool fail_allocation = false;
std::size_t fail_after = (std::numeric_limits<std::size_t>::max)();
bool count_allocations = false;
std::size_t allocations = 0U;
}  // namespace
void* operator new(std::size_t size) {
  if (fail_allocation) {
    fail_allocation = false;
    throw std::bad_alloc();
  }
  if (fail_after != (std::numeric_limits<std::size_t>::max)()) {
    if (fail_after == 0U) {
      fail_after = (std::numeric_limits<std::size_t>::max)();
      throw std::bad_alloc();
    }
    --fail_after;
  }
  if (count_allocations) ++allocations;
  if (void* p = std::malloc(size ? size : 1U)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

namespace {
namespace host = pae::host_endpoint;
namespace core = pae::protocol_core;
namespace framing = pae::protocol_framing;
void Check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
std::string Read(const char* path) {
  std::ifstream file(path, std::ios::binary);
  Check(static_cast<bool>(file), "fixture open");
  return {std::istreambuf_iterator<char>(file), {}};
}
pae::protocol_plan::PlanOwner Compile(const std::string& json) {
  auto compiled = pae::config_compiler::CompileJsonToPlan(json);
  if (!compiled.Succeeded()) {
    if (compiled.Diagnostic()) std::cerr << compiled.Diagnostic()->detail << '\n';
    throw std::runtime_error("fixture compile");
  }
  return std::move(compiled).TakePlan();
}
core::ByteView Bytes(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}
struct Capture {
  std::size_t calls = 0U;
  std::size_t fields = 0U;
  std::uint64_t number = 0U;
  std::uint64_t generation = 0U;
  core::LogicalValueKind kind = core::LogicalValueKind::UINT64;
  std::string message, field, text, encoded;
  bool stop = true;
  bool throws = false;
  host::Session* reenter = nullptr;
  host::Handle handle;
  host::Status reentrant_status = host::Status::OK;
  static host::SinkAction Call(const host::Output& output, void* opaque) {
    auto& c = *static_cast<Capture*>(opaque);
    if (c.throws) throw std::runtime_error("host callback fault");
    ++c.calls;
    c.generation = output.generation;
    c.message = output.plan->Messages()[output.message_index].id.View();
    c.fields = output.field_count;
    if (output.field_count) {
      const auto& first = output.fields[0];
      c.kind = first.value_kind;
      c.number = first.uint64_value;
      c.field = output.plan->Messages()[first.field.message_index]
                    .fields[first.field.field_index]
                    .id.View();
      if (first.value_kind == core::LogicalValueKind::BYTES)
        c.text.assign(reinterpret_cast<const char*>(first.bytes_value.data),
                      first.bytes_value.size);
    }
    if (output.action == host::Action::ENCODE)
      c.encoded.assign(reinterpret_cast<const char*>(output.bytes.data), output.bytes.size);
    if (c.reenter) {
      c.reentrant_status = c.reenter->Reset(c.handle);
      Check(c.reenter->Observe(c.handle).status == host::Status::BUSY, "reentrant observe");
      Check(c.reenter->Push(c.handle, Bytes("ONLY\r\n"), {Call, &c}).status == host::Status::BUSY,
            "reentrant push");
    }
    return c.stop ? host::SinkAction::STOP : host::SinkAction::CONTINUE;
  }
  host::Sink Sink() { return {Call, this}; }
};
std::unique_ptr<host::Session> Make(const std::string& json, const host::BindingSpec* specs,
                                    std::size_t count, const host::Limits& limits = {}) {
  auto created = host::Session::Create(Compile(json), specs, count, limits);
  Check(created.status == host::Status::OK && created.session, "session create");
  return std::move(created.session);
}
void Registration(const std::string& json) {
  host::BindingSpec specs[] = {{"device", host::Action::DECODE, "ascii_pipeline", 2U},
                               {"device", host::Action::ENCODE, "ascii_pipeline"}};
  auto session = Make(json, specs, 2U);
  Check(session->Observe(session->Find("device", host::Action::DECODE, 2U)).status ==
            host::Status::INVALID_BINDING,
        "missing stream");
  for (const auto& bad : {host::BindingSpec{"", host::Action::DECODE, "ascii_pipeline"},
                          host::BindingSpec{"other", host::Action::DECODE, "missing"},
                          host::BindingSpec{"device", host::Action::DECODE, "ascii_pipeline"}}) {
    specs[1] = bad;
    auto failed = host::Session::Create(Compile(json), specs, 2U);
    Check(!failed.session && failed.status == host::Status::INVALID_BINDING,
          "atomic registration failure");
  }
  specs[1] = {"other", host::Action::DECODE, "encode_only_pipeline"};
  Check(host::Session::Create(Compile(json), specs, 2U).status == host::Status::UNSUPPORTED,
        "no decode action");
  specs[1] = {"other", host::Action::ENCODE, "decode_only_pipeline"};
  Check(host::Session::Create(Compile(json), specs, 2U).status == host::Status::UNSUPPORTED,
        "no encode action");
  host::Limits small;
  small.max_accounted_bytes = 1U;
  Check(
      host::Session::Create(Compile(json), specs, 1U, small).status == host::Status::LIMIT_EXCEEDED,
      "memory budget reject");
  specs[0].streams = (std::numeric_limits<std::size_t>::max)();
  Check(host::Session::Create(Compile(json), specs, 1U).status == host::Status::LIMIT_EXCEEDED,
        "stream overflow reject");
  specs[0].streams = 1U;
  auto owner = Compile(json);
  fail_allocation = true;
  auto failed = host::Session::Create(std::move(owner), specs, 1U);
  Check(!failed.session && failed.status == host::Status::ALLOCATION_FAILED,
        "allocation failure catch");
  auto other = Make(json, specs, 1U);
  // Every creation allocation is a possible failure, including after prior channels exist.
  const host::BindingSpec fault_specs[] = {{"device", host::Action::DECODE, "ascii_pipeline", 2U},
                                           {"device", host::Action::ENCODE, "ascii_pipeline"}};
  auto measured_plan = Compile(json);
  count_allocations = true;
  allocations = 0U;
  auto measured = host::Session::Create(std::move(measured_plan), fault_specs, 2U);
  count_allocations = false;
  const auto creation_allocations = allocations;
  Check(measured.status == host::Status::OK, "measured creation");
  for (std::size_t n = 0U; n < creation_allocations; ++n) {
    auto failure_plan = Compile(json);
    std::cerr << "allocation failure point " << n << '/' << creation_allocations << '\n';
    fail_after = n;
    auto failure = host::Session::Create(std::move(failure_plan), fault_specs, 2U);
    fail_after = (std::numeric_limits<std::size_t>::max)();
    Check(failure.status != host::Status::OK && !failure.session,
          "every allocation fails atomically");
  }
  host::Limits exact;
  exact.max_accounted_bytes = other->AccountedBytes();
  auto exact_session = Make(json, specs, 1U, exact);
  --exact.max_accounted_bytes;
  Check(!host::Session::Create(Compile(json), specs, 1U, exact).session,
        "accounted exact boundary");
  const auto stale = session->Find("device", host::Action::DECODE);
  Check(other->Reset(stale) == host::Status::INVALID_BINDING, "foreign instance");
  session.reset();
  Check(other->Reset(stale) == host::Status::INVALID_BINDING, "expired identity");
}
void Ascii(const std::string& json, const std::string& complete) {
  const host::BindingSpec specs[] = {{"port", host::Action::DECODE, "ascii_pipeline", 2U},
                                     {"port", host::Action::ENCODE, "ascii_pipeline"},
                                     {"record", host::Action::DECODE, "decode_only_pipeline"},
                                     {"tx", host::Action::ENCODE, "encode_only_pipeline"}};
  auto session = Make(json, specs, 4U);
  const auto rx = session->Find("port", host::Action::DECODE);
  const auto rx2 = session->Find("port", host::Action::DECODE, 1U);
  const auto tx = session->Find("port", host::Action::ENCODE);
  Capture capture;
  Check(session->Decode(rx, Bytes("ONLY\r\n"), capture.Sink()).status ==
            host::Status::WRONG_INPUT_KIND,
        "record on stream reject");
  Check(
      session->Push(tx, Bytes("ONLY\r\n"), capture.Sink()).status == host::Status::INVALID_BINDING,
      "wrong action");
  session->Push(rx, Bytes("RX A!"), capture.Sink());
  session->Push(rx2, Bytes("ON"), capture.Sink());
  Check(session->Observe(rx).framing.buffered_bytes == 5U &&
            session->Observe(rx2).framing.buffered_bytes == 2U,
        "stream isolation");
  Check(session->Push(rx, {}, capture.Sink()).status == host::Status::INVALID_ARGUMENT,
        "no empty half flush");
  auto first = session->Push(rx, Bytes("OK\r\nONLY\r\n"), capture.Sink());
  Check(first.status == host::Status::OK && first.framing.bytes_consumed == 4U &&
            first.successful_outputs == 1U,
        "exact STOP prefix");
  Check(capture.text == "A" && capture.field == "name" && capture.fields == 2U &&
            capture.kind == core::LogicalValueKind::BYTES,
        "ASCII typed fields");
  const auto saved = capture.text;
  session->Push(rx, Bytes("ONLY\r\n"), capture.Sink());
  Check(capture.fields == 0U && saved == "A", "zero fields and copied bytes");
  Check(session->Reset(rx) == host::Status::OK && session->Observe(rx).generation == 1U &&
            session->Observe(rx2).framing.buffered_bytes == 2U,
        "target reset");
  session->Push(rx2, Bytes("LY\r\n"), capture.Sink());
  Check(capture.message == "decode_only" && capture.generation == 0U,
        "second stream survives reset");
  capture.stop = false;
  auto mixed = session->Push(rx, Bytes("BAD\r\nONLY\r\n"), capture.Sink());
  Check(mixed.status == host::Status::CODEC_FAILED && mixed.decode_failures == 1U &&
            mixed.codec_status == core::CodecStatus::OK && mixed.framing.frames_delivered == 2U &&
            mixed.successful_outputs == 1U,
        "failure zero output and recovery");
  auto too_long = session->Push(rx, Bytes("XXXXXXXXXXXXX"), capture.Sink());
  Check(
      too_long.framing.malformed_candidates == 1U &&
          session->Observe(rx).framing.phase == framing::StreamFramingPhase::DISCARDING_UNTIL_CRLF,
      "oversize discard");
  Check(session->Push(rx, Bytes("\r\nONLY\r\n"), capture.Sink()).successful_outputs == 1U,
        "discard recovery");
  session->Push(rx, Bytes("RX ABCD!OK\r"), capture.Sink());
  Check(session->Push(rx, Bytes("\n"), capture.Sink()).successful_outputs == 1U &&
            capture.text == "ABCD",
        "M and split CRLF");
  host::NamedValue values[2];
  values[0].field_id = "name";
  values[0].value.value_kind = core::LogicalValueKind::BYTES;
  values[0].value.bytes_value = Bytes("A");
  values[1].field_id = "tx_tag";
  values[1].value.value_kind = core::LogicalValueKind::BYTES;
  values[1].value.bytes_value = Bytes("Z");
  Check(session->Encode(tx, "greeting", values, 2U, capture.Sink()).status == host::Status::OK &&
            capture.encoded == "TX A!Z\r\n",
        "independent ASCII encode");
  const auto calls = capture.calls;
  Check(session->Encode(tx, "decode_only", nullptr, 0U, capture.Sink()).status ==
            host::Status::UNSUPPORTED,
        "decode only encode reject");
  Check(session->Encode(session->Find("tx", host::Action::ENCODE), "greeting", values, 2U,
                        capture.Sink())
                .status == host::Status::INVALID_BINDING,
        "foreign message");
  Check(session->Encode(tx, "greeting", nullptr, 0U, capture.Sink()).status ==
                host::Status::CODEC_FAILED &&
            capture.calls == calls,
        "missing fields zero output");
  values[0].value.field.plan_scope = &session->Plan();
  Check(session->Encode(tx, "greeting", values, 2U, capture.Sink()).status ==
            host::Status::INVALID_BINDING,
        "raw field reference reject");
  capture.reenter = session.get();
  capture.handle = rx;
  session->Push(rx, Bytes("ONLY\r\n"), capture.Sink());
  Check(capture.reentrant_status == host::Status::BUSY, "reentry refused");
  capture.reenter = nullptr;
  capture.throws = true;
  const auto fault = session->Push(rx, Bytes("ONLY\r\nONLY\r\n"), capture.Sink());
  Check(fault.status == host::Status::CALLBACK_FAILED && fault.framing.bytes_consumed == 6U &&
            fault.successful_outputs == 0U,
        "throw exact consumption");
  Check(session->Push(rx, Bytes("ONLY\r\n"), capture.Sink()).status == host::Status::RESET_REQUIRED,
        "fault requires reset");
  Check(session->Reset(rx) == host::Status::OK, "fault reset");
  capture.throws = false;
  const auto record = session->Find("record", host::Action::DECODE);
  Check(session->Decode(record, Bytes("ONLY\r\n"), capture.Sink()).successful_outputs == 1U,
        "V11 complete zero fields");
  Check(session->Push(record, Bytes("ONLY\r\n"), capture.Sink()).status ==
            host::Status::WRONG_INPUT_KIND,
        "chunk on record reject");
  const host::BindingSpec complete_specs[] = {{"c", host::Action::DECODE, "ascii_pipeline"},
                                              {"c", host::Action::ENCODE, "ascii_pipeline"}};
  auto old = Make(complete, complete_specs, 2U);
  Check(old->Decode(old->Find("c", host::Action::DECODE), Bytes("RX ALICE!OK\r\n"), capture.Sink())
                    .successful_outputs == 1U &&
            capture.text == "ALICE",
        "V10 complete");
  values[0].value.field = {};
  values[0].value.bytes_value = Bytes("ALICE");
  Check(old->Encode(old->Find("c", host::Action::ENCODE), "greeting", values, 2U, capture.Sink())
                    .status == host::Status::OK &&
            capture.encoded == "TX ALICE!Z\r\n",
        "V10 encode");
  count_allocations = true;
  allocations = 0U;
  auto no_alloc = session->Push(
      rx, Bytes("ONLY\r\n"),
      {[](const host::Output&, void*) { return host::SinkAction::CONTINUE; }, nullptr});
  count_allocations = false;
  Check(no_alloc.status == host::Status::OK && allocations == 0U,
        "host push has no allocation excluding callback");
}
void Budgets(const std::string& json) {
  host::BindingSpec spec{"p", host::Action::DECODE, "ascii_pipeline"};
  spec.framing_limits.max_frames_per_submit = 1U;
  auto session = Make(json, &spec, 1U);
  auto rx = session->Find("p", host::Action::DECODE);
  Capture capture;
  capture.stop = false;
  auto first = session->Push(rx, Bytes("ONLY\r\nONLY\r\n"), capture.Sink());
  Check(first.framing.bytes_consumed == 12U && first.successful_outputs == 1U &&
            session->Observe(rx).framing.has_internal_work,
        "pending retained");
  Check(session->Push(rx, {}, capture.Sink()).successful_outputs == 1U &&
            !session->Observe(rx).framing.has_internal_work,
        "empty pending once");
  session->Push(rx, Bytes("ONLY\r\nONLY\r\n"), capture.Sink());
  Check(session->Reset(rx) == host::Status::OK && !session->Observe(rx).framing.has_internal_work,
        "reset pending");
  session->Push(rx, Bytes("XXXXXXXXXXXXX"), capture.Sink());
  Check(session->Reset(rx) == host::Status::OK &&
            session->Observe(rx).framing.phase == framing::StreamFramingPhase::COLLECTING,
        "reset discard");
  spec.framing_limits.max_work_units = 5U;
  session = Make(json, &spec, 1U);
  rx = session->Find("p", host::Action::DECODE);
  const std::string input = "ONLY\r\n";
  std::size_t offset = 0U, delivered = 0U;
  bool saw_budget = false;
  for (std::size_t step = 0U; step < 32U; ++step) {
    if (offset == input.size() && !session->Observe(rx).framing.has_internal_work) break;
    const auto result =
        session->Push(rx, Bytes(std::string_view(input).substr(offset)), capture.Sink());
    Check(result.framing.bytes_consumed <= input.size() - offset &&
              result.framing.work_units_used <= 5U,
          "work budget exact");
    Check(result.framing.bytes_consumed || result.framing.frames_delivered ||
              result.framing.work_units_used,
          "bounded driver progress");
    offset += result.framing.bytes_consumed;
    delivered += result.successful_outputs;
    saw_budget |= result.framing.stop_reason == framing::SubmitStopReason::WORK_BUDGET_REACHED;
  }
  Check(saw_budget && offset == input.size() && delivered == 1U, "work resume no replay");
  spec.framing_limits.max_submit_bytes = 4U;
  session = Make(json, &spec, 1U);
  rx = session->Find("p", host::Action::DECODE);
  auto over = session->Push(rx, Bytes("ONLY\r\n"), capture.Sink());
  Check(over.status == host::Status::FRAMING_FAILED && over.framing.bytes_consumed == 0U &&
            session->Observe(rx).reset_required,
        "API reject facts");
}
void Binary(std::string json) {
  host::BindingSpec specs[] = {{"binary", host::Action::DECODE, "fixed_rx", 2U},
                               {"binary", host::Action::ENCODE, "fixed_rx"}};
  auto session = Make(json, specs, 2U);
  const auto rx = session->Find("binary", host::Action::DECODE);
  Capture capture;
  session->Push(rx, Bytes(std::string_view("\xAA\x12", 2U)), capture.Sink());
  const host::CandidateObserver no_conversion{[](const host::Candidate& candidate, void*) {
    core::RawIntegerValue raw;
    Check(candidate.RawIntegerCount() == 0U && !candidate.GetRawInteger(0U, raw),
          "unconverted Binary field has no conversion raw entry");
    return host::SinkAction::CONTINUE;
  }, nullptr};
  auto decoded = session->Push(rx, Bytes(std::string_view("\x34", 1U)), capture.Sink(),
                                no_conversion);
  Check(decoded.successful_outputs == 1U && capture.kind == core::LogicalValueKind::UINT64 &&
            capture.number == 0x1234U && capture.field == "fixed_payload",
        "binary stream uint64");
  host::NamedValue value;
  value.field_id = "fixed_payload";
  value.value.uint64_value = 0x1234U;
  Check(session->Encode(session->Find("binary", host::Action::ENCODE), "fixed_message", &value, 1U,
                        capture.Sink())
                    .status == host::Status::OK &&
            capture.encoded == std::string("\xAA\x12\x34", 3U),
        "binary exact encode");
  const auto start = json.find("\"input_kind\": \"stream_chunk\"");
  const auto end = json.find("\n    }", start);
  Check(start != std::string::npos && end != std::string::npos, "binary complete fixture rewrite");
  json.replace(start, end - start, "\"input_kind\": \"complete_record\"");
  auto complete = Make(json, specs, 2U);
  Check(complete->Decode(complete->Find("binary", host::Action::DECODE),
                         Bytes(std::string_view("\xAA\x12\x34", 3U)), capture.Sink())
                    .successful_outputs == 1U &&
            capture.number == 0x1234U,
        "binary complete decode");
  const auto calls = capture.calls;
  Check(
      complete->Decode(complete->Find("binary", host::Action::DECODE), Bytes("BAD"), capture.Sink())
                  .status == host::Status::CODEC_FAILED &&
          calls == capture.calls,
      "binary failure zero delivery");
  const host::BindingSpec sync_specs[] = {{"fixed", host::Action::DECODE, "sync_fixed_rx"},
                                          {"length", host::Action::DECODE, "sync_length_rx"}};
  auto sync = Make(json, sync_specs, 2U);
  const auto fixed = sync->Find("fixed", host::Action::DECODE);
  sync->Push(fixed, Bytes(std::string_view("\x00\xA5", 2U)), capture.Sink());
  Check(sync->Push(fixed, Bytes(std::string_view("\x5A\x12\x34", 3U)), capture.Sink())
                    .successful_outputs == 1U &&
            capture.number == 0x1234U,
        "sync fixed split");
  const auto length = sync->Find("length", host::Action::DECODE);
  sync->Push(length, Bytes(std::string_view("\xC3\x3C\x06", 3U)), capture.Sink());
  Check(sync->Push(length, Bytes(std::string_view("\x12\x34\x55", 3U)), capture.Sink())
                    .successful_outputs == 1U &&
            capture.number == 6U && capture.fields == 2U,
        "sync length split");
}
void CandidateObservation(const std::string& json) {
  const host::BindingSpec specs[] = {{"rx", host::Action::DECODE, "ascii_pipeline"},
                                     {"record", host::Action::DECODE, "decode_only_pipeline"}};
  auto session = Make(json, specs, 2U);
  const auto rx = session->Find("rx", host::Action::DECODE);
  Capture business;
  business.stop = false;
  struct Diagnostic {
    std::size_t calls = 0U;
    std::string frame, value;
    bool throws = false;
    bool stop = true;
    std::uint64_t generation = 0U;
    host::Session* session = nullptr;
    host::Handle handle;
    static host::SinkAction Call(const host::Candidate& candidate, void* data) {
      auto& d = *static_cast<Diagnostic*>(data);
      ++d.calls;
      Check(d.session->Reset(d.handle) == host::Status::BUSY, "observer reentry rejected");
      core::RawIntegerValue raw;
      Check(candidate.RawIntegerCount() == 0U && !candidate.GetRawInteger(0U, raw),
            "ASCII success, zero-field and failure candidates have no converted raw");
      if (d.throws) throw std::runtime_error("diagnostic copy fault");
      d.frame.assign(reinterpret_cast<const char*>(candidate.frame.data), candidate.frame.size);
      d.generation = candidate.generation;
      if (candidate.decoded.status != core::CodecStatus::OK)
        Check(!candidate.fields && candidate.field_count == 0U, "failure has no valid fields");
      if (candidate.field_count) {
        const auto bytes = candidate.fields[0].bytes_value;
        d.value.assign(reinterpret_cast<const char*>(bytes.data), bytes.size);
      }
      return d.stop ? host::SinkAction::STOP : host::SinkAction::CONTINUE;
    }
  } diagnostic;
  diagnostic.session = session.get();
  diagnostic.handle = rx;
  host::CandidateObserver observer{Diagnostic::Call, &diagnostic};
  auto bad = session->Push(rx, Bytes("BAD\r\nONLY\r\n"), business.Sink(), observer);
  Check(bad.framing.bytes_consumed == 5U && bad.decode_failures == 1U &&
            bad.successful_outputs == 0U && diagnostic.calls == 1U && diagnostic.frame == "BAD\r\n",
        "failed candidate STOP before next success");
  auto good = session->Push(rx, Bytes("ONLY\r\n"), business.Sink(), observer);
  Check(good.successful_outputs == 1U && good.decode_successes == 1U &&
            good.observed_candidates == 1U && diagnostic.calls == 2U,
        "observer STOP keeps current zero-field success");
  session->Push(rx, Bytes("RX A!OK\r\n"), business.Sink(), observer);
  Check(diagnostic.value == "A" && diagnostic.frame == "RX A!OK\r\n", "borrowed field copied");
  auto early = session->Push({}, Bytes("ONLY\r\n"), business.Sink(), observer);
  Check(!early.codec_attempted && diagnostic.calls == 3U, "early reject no candidate");
  diagnostic.throws = true;
  auto fault = session->Push(rx, Bytes("ONLY\r\nONLY\r\n"), business.Sink(), observer);
  Check(fault.status == host::Status::CALLBACK_FAILED && fault.framing.bytes_consumed == 6U &&
            fault.decode_successes == 1U && fault.observed_candidates == 0U &&
            fault.successful_outputs == 0U && session->Observe(rx).reset_required,
        "observation fault keeps consumption and suppresses business");
  session->Reset(rx);
  diagnostic.throws = false;
  session->Push(rx, Bytes("ONLY\r\n"), business.Sink(), observer);
  Check(diagnostic.generation == 1U, "observation generation");
  auto record = session->Decode(session->Find("record", host::Action::DECODE), Bytes("ONLY\r\n"),
                                business.Sink(), observer);
  Check(record.observed_candidates == 1U && record.successful_outputs == 1U,
        "complete record observation");
  diagnostic.stop = false;
  business.stop = true;
  auto stopped = session->Push(rx, Bytes("ONLY\r\nONLY\r\n"), business.Sink(), observer);
  Check(stopped.framing.bytes_consumed == 6U && stopped.observed_candidates == 1U,
        "business STOP still stops candidates");
}
void RawCandidateObservation(std::string json) {
  const auto version = json.find("\"0.5\"");
  Check(version != std::string::npos, "raw fixture schema");
  json.replace(version, 5U, "\"0.9\"");
  const host::BindingSpec spec{"raw", host::Action::DECODE, "sample_pipeline"};
  auto session = Make(json, &spec, 1U);
  const auto handle = session->Find("raw", host::Action::DECODE);
  std::array<std::uint8_t, 34> frame{};
  frame[6] = 2U;
  frame[7] = 11U;
  for (std::size_t i = 8U; i < 16U; ++i) frame[i] = 0xFFU;
  frame[24] = 0x80U;
  frame[32] = 0xA5U;
  frame[33] = 0x2AU;
  struct Diagnostic {
    std::size_t calls = 0U;
    bool valid = true;
    bool throws = false;
    static host::SinkAction Call(const host::Candidate& candidate, void* opaque) {
      auto& d = *static_cast<Diagnostic*>(opaque);
      ++d.calls;
      core::RawIntegerValue raw;
      raw.uint64_value = 77U;
      const auto count = candidate.RawIntegerCount();
      d.valid &= !candidate.GetRawInteger(count, raw) && raw.uint64_value == 77U;
      d.valid &= !candidate.GetRawInteger((std::numeric_limits<std::size_t>::max)(), raw);
      if (candidate.decoded.status != core::CodecStatus::OK) {
        d.valid &= count == 0U && candidate.field_count == 0U && !candidate.fields;
      } else {
        d.valid &= count == 4U && candidate.field_count == 5U;
        for (std::size_t i = 0U; i < 4U; ++i) {
          const bool read = candidate.GetRawInteger(i, raw);
          d.valid &= read;
          if (!read) continue;
          d.valid &= raw.field.plan_scope == candidate.plan &&
                     raw.field.message_index == candidate.decoded.message_index &&
                     raw.field.field_index == i &&
                     raw.field.field_index == candidate.fields[i].field.field_index;
          if (i == 0U) {
            d.valid &= raw.kind == core::RawIntegerKind::INT64 && raw.int64_value == 523 &&
                       candidate.fields[i].decimal64_value.coefficient == 123 &&
                       candidate.fields[i].decimal64_value.scale == 1;
          } else if (i == 1U) {
            d.valid &= raw.kind == core::RawIntegerKind::UINT64 &&
                       raw.uint64_value == (std::numeric_limits<std::uint64_t>::max)();
          } else if (i == 2U) {
            d.valid &= raw.kind == core::RawIntegerKind::UINT64 && raw.uint64_value == 0U;
          } else {
            d.valid &= raw.kind == core::RawIntegerKind::INT64 &&
                       raw.int64_value == (std::numeric_limits<std::int64_t>::min)() &&
                       candidate.fields[i].decimal64_value.coefficient ==
                           (std::numeric_limits<std::int64_t>::max)();
          }
        }
      }
      if (d.throws) throw std::runtime_error("raw observer fault");
      return host::SinkAction::STOP;
    }
  } diagnostic;
  std::size_t business_calls = 0U;
  const host::Sink sink{[](const host::Output&, void* opaque) {
    ++*static_cast<std::size_t*>(opaque);
    return host::SinkAction::CONTINUE;
  }, &business_calls};
  const host::CandidateObserver observer{Diagnostic::Call, &diagnostic};
  const core::ByteView bytes{frame.data(), frame.size()};
  allocations = 0U;
  count_allocations = true;
  const auto result = session->Decode(handle, bytes, sink, observer);
  count_allocations = false;
  Check(result.status == host::Status::OK && diagnostic.valid, "raw candidate values and bounds");
  Check(allocations == 0U && result.observed_candidates == 1U && business_calls == 1U,
        "raw observer STOP delivers once without allocation");
  allocations = 0U;
  count_allocations = true;
  const auto unobserved = session->Decode(handle, bytes, sink);
  count_allocations = false;
  Check(unobserved.status == host::Status::OK && allocations == 0U && diagnostic.calls == 1U,
        "no observer no allocation or callback");
  session->Decode({}, bytes, sink, observer);
  Check(diagnostic.calls == 1U, "early rejection does not observe stale raw");
  frame[33] ^= 1U;
  const auto failed = session->Decode(handle, bytes, sink, observer);
  Check(failed.status == host::Status::CODEC_FAILED && diagnostic.valid && business_calls == 2U,
        "failed candidate hides previous raw");
  frame[33] ^= 1U;
  frame[16] = 0x80U;
  frame[33] = 0xAAU;
  const auto overflow = session->Decode(handle, bytes, sink, observer);
  Check(overflow.status == host::Status::CODEC_FAILED && diagnostic.valid && business_calls == 2U,
        "conversion failure hides partially collected raw");
  frame[16] = 0U;
  frame[33] = 0x2AU;
  diagnostic.throws = true;
  const auto fault = session->Decode(handle, bytes, sink, observer);
  Check(fault.status == host::Status::CALLBACK_FAILED && diagnostic.valid && business_calls == 2U,
        "raw observer exception suppresses business");

  const auto framing_kind = json.find("\"input_kind\": \"complete_record\"");
  Check(framing_kind != std::string::npos, "raw fixture framing");
  json.replace(framing_kind, std::string("\"input_kind\": \"complete_record\"").size(),
               "\"input_kind\": \"stream_chunk\", \"strategy\": \"fixed_length\", "
               "\"frame_length_bytes\": 34");
  session = Make(json, &spec, 1U);
  const auto stream = session->Find("raw", host::Action::DECODE);
  diagnostic.throws = false;
  std::array<std::uint8_t, 68> pair{};
  std::copy(frame.begin(), frame.end(), pair.begin());
  std::copy(frame.begin(), frame.end(), pair.begin() + 34);
  allocations = 0U;
  count_allocations = true;
  const auto stopped = session->Push(stream, {pair.data(), pair.size()}, sink, observer);
  count_allocations = false;
  Check(stopped.framing.bytes_consumed == 34U && stopped.successful_outputs == 1U &&
            stopped.observed_candidates == 1U && diagnostic.valid && allocations == 0U,
        "stream raw STOP preserves suffix and allocates nothing");
  diagnostic.throws = true;
  const auto stream_fault = session->Push(stream, {pair.data() + 34, 34U}, sink, observer);
  Check(stream_fault.status == host::Status::CALLBACK_FAILED &&
            stream_fault.framing.bytes_consumed == 34U && stream_fault.successful_outputs == 0U &&
            session->Observe(stream).reset_required && diagnostic.valid,
        "stream raw exception preserves consumption and requires reset");
  Check(session->Reset(stream) == host::Status::OK, "raw stream reset");
  diagnostic.throws = false;
  Check(session->Push(stream, bytes, sink, observer).successful_outputs == 1U && diagnostic.valid,
        "raw observation recovers after reset");
}
}  // namespace
int main(int argc, char** argv) {
#if defined(_MSC_VER)
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  if (argc != 5) return 1;
  try {
    const auto ascii = Read(argv[1]);
    Registration(ascii);
    Ascii(ascii, Read(argv[3]));
    Budgets(ascii);
    Binary(Read(argv[2]));
    CandidateObservation(ascii);
    RawCandidateObservation(Read(argv[4]));
    std::cout << "HOST_ENDPOINT_CONTRACT=PASS\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
