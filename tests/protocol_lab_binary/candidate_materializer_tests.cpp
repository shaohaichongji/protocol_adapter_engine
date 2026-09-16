#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <string>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#include "../../src/config_compiler/config_compiler.h"
#include "candidate_materializer.h"

namespace {
namespace binary = pae::protocol_lab_binary;
namespace host = pae::host_endpoint;
namespace core = pae::protocol_core;
void Check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
std::string Read(const char* path) {
  std::ifstream file(path, std::ios::binary);
  Check(static_cast<bool>(file), "fixture open");
  return {std::istreambuf_iterator<char>(file), {}};
}
void Replace(std::string& text, const std::string& from, const std::string& to) {
  const auto pos = text.find(from);
  Check(pos != std::string::npos, "fixture replacement");
  text.replace(pos, from.size(), to);
}
std::unique_ptr<host::Session> Make(const std::string& json, const char* pipeline) {
  auto compiled = pae::config_compiler::CompileJsonToPlan(json);
  if (!compiled.Succeeded()) {
    if (compiled.Diagnostic()) std::cerr << compiled.Diagnostic()->detail << '\n';
    throw std::runtime_error("fixture compile");
  }
  const host::BindingSpec binding{"device", host::Action::DECODE, pipeline};
  auto created = host::Session::Create(std::move(compiled).TakePlan(), &binding, 1U);
  Check(created.status == host::Status::OK && created.session, "host creation");
  return std::move(created.session);
}
void Sum(std::vector<std::uint8_t>& bytes) {
  std::uint8_t sum = 0;
  for (std::size_t i = 0U; i + 1U < bytes.size(); ++i) sum += bytes[i];
  bytes.back() = sum;
}
struct Capture {
  binary::CandidateResult result;
  binary::Limits limits;
  std::size_t business = 0U;
  std::size_t inject_allocation = (std::numeric_limits<std::size_t>::max)();
  int corruption = 0;
  static host::SinkAction Observe(const host::Candidate& original, void* data) {
    auto& c = *static_cast<Capture*>(data);
    auto candidate = original;
    std::vector<core::DecodedFieldSlot> slots;
    if (c.corruption) {
      slots.assign(original.fields, original.fields + original.field_count);
      candidate.fields = slots.data();
      if (c.corruption == 1) slots[0].field.plan_scope = nullptr;
      if (c.corruption == 2) slots[1].field = slots[0].field;
      if (c.corruption == 3) slots[0].value_kind = core::LogicalValueKind::BYTES;
      if (c.corruption == 4) {
        host::Candidate missing_raw;
        missing_raw.plan = candidate.plan;
        missing_raw.frame = candidate.frame;
        missing_raw.decoded = candidate.decoded;
        missing_raw.fields = candidate.fields;
        missing_raw.field_count = candidate.field_count;
        candidate = missing_raw;
      }
      if (c.corruption == 5) slots.back().enum_value.reference.plan_scope = nullptr;
      if (c.corruption == 6) {
        slots[6].bytes_value = {original.frame.data + original.frame.size, 2U};
      }
      if (c.corruption == 7) --candidate.field_count;
      if (c.corruption == 8) slots[6].bytes_value = {original.frame.data, 2U};
    }
    auto remaining = c.inject_allocation;
    const binary::CopyTestHooks hooks{[](void* data) {
                                        auto& left = *static_cast<std::size_t*>(data);
                                        if (left == (std::numeric_limits<std::size_t>::max)())
                                          return;
                                        if (left == 0U) throw std::bad_alloc();
                                        --left;
                                      },
                                      &remaining};
    try {
      c.result = binary::MaterializeCandidate(candidate, c.limits, &hooks);
    } catch (const binary::MaterializationError& error) {
      std::cerr << "materialization rejection: " << error.what() << '\n';
      throw;
    }
    return host::SinkAction::STOP;
  }
  host::Result Run(host::Session& session, const std::vector<std::uint8_t>& bytes) {
    const auto outcome =
        session.Decode(session.Find("device", host::Action::DECODE), {bytes.data(), bytes.size()},
                       {[](const host::Output&, void* p) {
                          ++static_cast<Capture*>(p)->business;
                          return host::SinkAction::CONTINUE;
                        },
                        this},
                       {Observe, this});
    return outcome;
  }
};
void Decimal(std::string json) {
  Replace(json, "\"0.5\"", "\"0.9\"");
  auto session = Make(json, "sample_pipeline");
  std::vector<std::uint8_t> bytes(34U, 0U);
  bytes[6] = 2U;
  bytes[7] = 11U;
  std::fill(bytes.begin() + 8, bytes.begin() + 16, std::uint8_t{0xFF});
  bytes[24] = 0x80U;
  bytes[32] = 0xA5U;
  Sum(bytes);
  Capture capture;
  Check(capture.Run(*session, bytes).successful_outputs == 1U, "decimal materializes");
  Check(capture.result.fields[0].decimal64_value.coefficient == 123 &&
            capture.result.fields[0].decimal64_value.scale == 1 &&
            capture.result.fields[0].raw_integer->int64_value == 523 &&
            capture.result.fields[1].raw_integer->uint64_value ==
                (std::numeric_limits<std::uint64_t>::max)() &&
            capture.result.fields[3].raw_integer->int64_value ==
                (std::numeric_limits<std::int64_t>::min)() &&
            !capture.result.fields[4].raw_integer,
        "decimal precision and raw association");
  const auto saved = capture.result;
  for (const auto raw : {520U, 400U}) {
    bytes[6] = static_cast<std::uint8_t>(raw >> 8U);
    bytes[7] = static_cast<std::uint8_t>(raw);
    Sum(bytes);
    Check(capture.Run(*session, bytes).successful_outputs == 1U &&
              capture.result.fields[0].decimal64_value.scale == 0 &&
              capture.result.fields[0].decimal64_value.coefficient == (raw == 520U ? 12 : 0),
          "normalized decimal trailing zeros and zero remain valid");
  }
  bytes = saved.frame;
  capture.result = saved;
  for (int corruption : {1, 2, 3, 4, 7}) {
    capture.corruption = corruption;
    const auto result = capture.Run(*session, bytes);
    Check(result.status == host::Status::CALLBACK_FAILED && result.successful_outputs == 0U &&
              capture.result.frame == saved.frame,
          "bad identity/raw never partially publishes");
    session->Reset(session->Find("device", host::Action::DECODE));
  }
  capture.corruption = 0;
  capture.limits.max_total_bytes = 1U;
  Check(capture.Run(*session, bytes).status == host::Status::CALLBACK_FAILED,
        "copy budget failure suppresses business");
  session->Reset(session->Find("device", host::Action::DECODE));
  capture.limits = {};
  capture.limits.max_frame_bytes = (std::numeric_limits<std::size_t>::max)();
  Check(capture.Run(*session, bytes).status == host::Status::CALLBACK_FAILED,
        "caller cannot enlarge hard limit");
  session->Reset(session->Find("device", host::Action::DECODE));
  capture.limits = {};
  bool allocation_success = false;
  for (std::size_t i = 0U; i < 64U; ++i) {
    capture.inject_allocation = i;
    const auto result = capture.Run(*session, bytes);
    if (result.status == host::Status::OK) {
      allocation_success = true;
      break;
    }
    Check(result.status == host::Status::CALLBACK_FAILED && result.successful_outputs == 0U &&
              session->Observe(session->Find("device", host::Action::DECODE)).reset_required &&
              capture.result.frame == saved.frame,
          "controlled copy exception is atomic");
    session->Reset(session->Find("device", host::Action::DECODE));
  }
  Check(allocation_success, "all controlled copy checkpoints traversed");
  capture.inject_allocation = (std::numeric_limits<std::size_t>::max)();
  bytes.back() ^= 1U;
  Check(capture.Run(*session, bytes).status == host::Status::CODEC_FAILED &&
            capture.result.fields.empty() && capture.result.frame.empty() &&
            capture.result.diagnostic_frame == bytes,
        "failure DTO has no old success");
  session.reset();
  Check(saved.fields[0].id == "temperature" && saved.fields[0].raw_integer->int64_value == 523 &&
            saved.frame.size() == 34U,
        "DTO outlives Session and Plan");
  Check(saved.budget.accounted_total_bytes > 0U &&
            saved.budget.materialization_peak_bytes >= saved.budget.accounted_total_bytes,
        "local budget reports capacities");
}
void Types(std::string json) {
  Replace(json, "\"0.4\"", "\"0.9\"");
  Replace(json, "\"reject\"", "\"preserve\"");
  auto session = Make(json, "sample_pipeline");
  std::vector<std::uint8_t> bytes(25U, 0U);
  bytes[0] = 0xFFU;
  bytes[1] = 0xFFU;
  bytes[2] = 0xFEU;
  bytes[3] = 0xFEU;
  bytes[4] = 0xFFU;
  bytes[5] = 0xFFU;
  std::fill(bytes.begin() + 6, bytes.begin() + 10, std::uint8_t{0xFF});
  bytes[10] = 0xFEU;
  bytes[11] = 0x80U;
  bytes[19] = 254U;
  bytes[20] = 0xA1U;
  bytes[21] = 0xCAU;
  bytes[22] = 0xFEU;
  bytes[23] = 2U;
  Sum(bytes);
  Capture capture;
  Check(capture.Run(*session, bytes).successful_outputs == 1U, "all base types materialize");
  const auto& fields = capture.result.fields;
  Check(fields.size() == 8U && fields[0].int64_value == -2 && fields[1].int64_value == -2 &&
            fields[3].int64_value == (std::numeric_limits<std::int64_t>::min)() &&
            fields[4].uint64_value == 254U && fields[5].bool_value &&
            fields[6].bytes_value == std::vector<std::uint8_t>({0xCAU, 0xFEU}) &&
            fields[7].enum_value.known && fields[7].enum_value.item_id == "active" &&
            fields[7].enum_value.display_text.empty(),
        "typed owning values without fabricated display");
  for (int corruption : {5, 6, 8}) {
    capture.corruption = corruption;
    Check(capture.Run(*session, bytes).status == host::Status::CALLBACK_FAILED,
          "bad enum or byte view rejected");
    session->Reset(session->Find("device", host::Action::DECODE));
  }
  capture.corruption = 0;
  for (int limited : {0, 1, 2, 3}) {
    capture.limits = {};
    if (limited == 0) capture.limits.max_field_bytes = 1U;
    if (limited == 1) capture.limits.max_string_bytes = 1U;
    if (limited == 2) capture.limits.max_fields = 7U;
    if (limited == 3) capture.limits.max_frame_bytes = 24U;
    Check(capture.Run(*session, bytes).status == host::Status::CALLBACK_FAILED,
          "per component copy ceilings reject before publication");
    session->Reset(session->Find("device", host::Action::DECODE));
  }
  capture.limits = {};
  bytes[23] = 3U;
  Sum(bytes);
  const auto unknown = capture.Run(*session, bytes);
  Check(unknown.successful_outputs == 1U && capture.result.decoded.tainted &&
            !capture.result.fields[7].enum_value.known &&
            capture.result.fields[7].enum_value.raw_value == 3U &&
            capture.result.fields[7].enum_value.item_id.empty(),
        "unknown enum preserved");
  session.reset();
  Check(capture.result.fields[6].bytes_value[0] == 0xCAU, "owned BYTES outlive Plan");
}
void Bounded(std::string json) {
  Replace(json, "\"0.8\"", "\"0.9\"");
  auto session = Make(json, "synthetic_rx");
  Capture capture;
  for (const std::size_t length : {0U, 3U}) {
    std::vector<std::uint8_t> bytes(length + 3U, 0x12U);
    bytes[0] = 0xA5U;
    bytes[1] = static_cast<std::uint8_t>(bytes.size());
    Sum(bytes);
    Check(capture.Run(*session, bytes).successful_outputs == 1U &&
              capture.result.fields[1].bytes_value.size() == length &&
              capture.result.frame == bytes,
          "zero/max bounded payload actual length");
  }
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
    Decimal(Read(argv[1]));
    Types(Read(argv[2]));
    Bounded(Read(argv[3]));
    std::cout << "BINARY_CANDIDATE_MATERIALIZER=PASS\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
