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
  auto decoded = session->Push(rx, Bytes(std::string_view("\x34", 1U)), capture.Sink());
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
}  // namespace
int main(int argc, char** argv) {
#if defined(_MSC_VER)
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  if (argc != 4) return 1;
  try {
    const auto ascii = Read(argv[1]);
    Registration(ascii);
    Ascii(ascii, Read(argv[3]));
    Budgets(ascii);
    Binary(Read(argv[2]));
    std::cout << "HOST_ENDPOINT_CONTRACT=PASS\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
