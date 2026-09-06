#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef CreateDirectory
#undef CreateDirectory
#endif

#include "evidence_bundle.h"
#include "protocol_lab.h"
#include "sha256.h"
#include "udp_exchange.h"

namespace {

using pae::protocol_lab::IUdpExchangeAdapter;
using pae::protocol_lab::UdpEndpoint;
using pae::protocol_lab::UdpExchangeRequest;
using pae::protocol_lab::UdpExchangeResponse;
using pae::protocol_lab::UdpExchangeStatus;

class ExecutionObserver final : public pae::protocol_lab::IProtocolLabExecutionObserver {
 public:
  void OnProtocolOperation(const char* operation) override { operations.emplace_back(operation); }
  std::vector<std::string> operations;
};

class FakeUdpAdapter final : public IUdpExchangeAdapter {
 public:
  UdpExchangeResponse Exchange(const UdpExchangeRequest& request) override {
    ++call_count;
    last_request = request;
    if (!record_root.empty()) {
      std::error_code error;
      for (std::filesystem::recursive_directory_iterator iterator{record_root, error}, end;
           iterator != end && !error; iterator.increment(error)) {
        if (iterator->path().filename() == "000001_tx.bin") {
          tx_persisted_before_exchange = true;
        }
      }
    }
    if (response.send_attempted && request.event_observer != nullptr) {
      request.event_observer->OnSendIntent();
      request.event_observer->OnSendResult(
          response.send_succeeded,
          response.send_succeeded ? std::string_view{} : std::string_view{"UDP_SEND_FAILED"});
    }
    return response;
  }

  UdpExchangeResponse response;
  UdpExchangeRequest last_request;
  std::filesystem::path record_root;
  int call_count = 0;
  bool tx_persisted_before_exchange = false;
};

class FrameFailingFileSystem final : public pae::protocol_lab::RecordFileSystem {
 public:
  enum class Mode { WRITE, CLOSE, REREAD, MISMATCH };

  explicit FrameFailingFileSystem(std::string filename, Mode mode = Mode::WRITE)
      : filename_(std::move(filename)), mode_(mode) {}

  bool EnsureDirectory(const std::filesystem::path& path, std::string& error) override {
    return standard_.EnsureDirectory(path, error);
  }
  bool Exists(const std::filesystem::path& path, bool& exists, std::string& error) override {
    return standard_.Exists(path, exists, error);
  }
  bool CreateDirectory(const std::filesystem::path& path, std::string& error) override {
    return standard_.CreateDirectory(path, error);
  }
  bool CreateDirectories(const std::filesystem::path& path, std::string& error) override {
    return standard_.CreateDirectories(path, error);
  }
  bool WriteClosedFile(const std::filesystem::path& path, const std::uint8_t* data,
                       std::size_t size, std::string& error) override {
    if (path.filename() == filename_ && mode_ == Mode::WRITE) {
      error = "injected frame persistence failure";
      return false;
    }
    const bool written = standard_.WriteClosedFile(path, data, size, error);
    if (written && path.filename() == filename_ && mode_ == Mode::CLOSE) {
      error = "injected metadata close acknowledgement failure";
      return false;
    }
    return written;
  }
  bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                std::string& error) override {
    if (path.filename() == filename_ && mode_ == Mode::REREAD) {
      error = "injected metadata reread failure";
      return false;
    }
    if (!standard_.ReadFile(path, output, error)) {
      return false;
    }
    if (path.filename() == filename_ && mode_ == Mode::MISMATCH) {
      output.push_back(0xA5U);
    }
    return true;
  }
  bool Rename(const std::filesystem::path& from, const std::filesystem::path& to,
              std::string& error) override {
    return standard_.Rename(from, to, error);
  }

 private:
  std::string filename_;
  Mode mode_;
  pae::protocol_lab::StandardRecordFileSystem standard_;
};

class LoopbackPeer {
 public:
  explicit LoopbackPeer(std::vector<std::uint8_t> response) : response_(std::move(response)) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
      error_ = "test peer WSAStartup failed";
      return;
    }
    winsock_ready_ = true;
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
      error_ = "test peer socket failed";
      return;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = htonl(0x7F000001U);
    if (bind(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) ==
        SOCKET_ERROR) {
      error_ = "test peer bind failed";
      return;
    }
    int address_size = sizeof(address);
    if (getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &address_size) ==
        SOCKET_ERROR) {
      error_ = "test peer getsockname failed";
      return;
    }
    port_ = ntohs(address.sin_port);
    const DWORD receive_timeout_ms = 5000U;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&receive_timeout_ms),
                   sizeof(receive_timeout_ms)) == SOCKET_ERROR) {
      error_ = "test peer timeout setup failed";
    }
  }

  ~LoopbackPeer() {
    Join();
    if (socket_ != INVALID_SOCKET) {
      closesocket(socket_);
    }
    if (winsock_ready_) {
      WSACleanup();
    }
  }

  LoopbackPeer(const LoopbackPeer&) = delete;
  LoopbackPeer& operator=(const LoopbackPeer&) = delete;

  bool Start() {
    if (!error_.empty() || port_ == 0U) {
      return false;
    }
    thread_ = std::thread([this] {
      std::vector<std::uint8_t> buffer(pae::protocol_lab::kMaximumIpv4UdpPayloadBytes);
      sockaddr_in sender{};
      int sender_size = sizeof(sender);
      const int received =
          recvfrom(socket_, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()),
                   0, reinterpret_cast<sockaddr*>(&sender), &sender_size);
      if (received == SOCKET_ERROR) {
        error_ = "test peer recvfrom failed";
        return;
      }
      buffer.resize(static_cast<std::size_t>(received));
      captured_ = std::move(buffer);
      if (response_.empty()) {
        return;
      }
      const int sent = sendto(socket_, reinterpret_cast<const char*>(response_.data()),
                              static_cast<int>(response_.size()), 0,
                              reinterpret_cast<const sockaddr*>(&sender), sender_size);
      if (sent == SOCKET_ERROR || static_cast<std::size_t>(sent) != response_.size()) {
        error_ = "test peer sendto failed";
      }
    });
    return true;
  }

  void Join() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  [[nodiscard]] std::uint16_t Port() const noexcept { return port_; }
  [[nodiscard]] const std::vector<std::uint8_t>& Captured() const noexcept { return captured_; }
  [[nodiscard]] const std::string& Error() const noexcept { return error_; }

 private:
  SOCKET socket_ = INVALID_SOCKET;
  bool winsock_ready_ = false;
  std::uint16_t port_ = 0U;
  std::vector<std::uint8_t> response_;
  std::vector<std::uint8_t> captured_;
  std::string error_;
  std::thread thread_;
};

int RunCaptured(std::vector<std::string> arguments,
                pae::protocol_lab::RecordFileSystem& file_system, IUdpExchangeAdapter& udp_adapter,
                std::string& output, ExecutionObserver* observer = nullptr) {
  std::vector<char*> argv;
  argv.reserve(arguments.size());
  for (std::string& argument : arguments) {
    argv.push_back(argument.data());
  }
  std::ostringstream captured;
  std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
  const int exit_code = pae::protocol_lab::RunApplicationWithDependencies(
      static_cast<int>(argv.size()), argv.data(), file_system, udp_adapter, observer);
  std::cout.rdbuf(previous);
  output = captured.str();
  return exit_code;
}

std::vector<std::string> UdpArguments(const std::filesystem::path& record_root, std::string remote,
                                      bool send) {
  std::vector<std::string> arguments{
      "pae_protocol_lab",   "udp-exchange",
      "--config",           "data/synthetic_lab_exchange_slice.pae.json",
      "--values",           "data/valid_command.values.pae-lab.json",
      "--remote",           std::move(remote),
      "--receive-pipeline", "fixture_to_host",
      "--record-root",      record_root.generic_string(),
      "--output",           "json",
  };
  if (send) {
    arguments.emplace_back("--send");
  }
  return arguments;
}

bool LoadHexFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }
  std::string digits;
  char value = '\0';
  while (input.get(value)) {
    if (std::isspace(static_cast<unsigned char>(value)) != 0) {
      continue;
    }
    if (!std::isxdigit(static_cast<unsigned char>(value))) {
      return false;
    }
    digits.push_back(value);
  }
  if (digits.empty() || digits.size() % 2U != 0U) {
    return false;
  }
  const auto nibble = [](char digit) -> std::uint8_t {
    if (digit >= '0' && digit <= '9') {
      return static_cast<std::uint8_t>(digit - '0');
    }
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(digit)));
    return static_cast<std::uint8_t>(upper - 'A' + 10);
  };
  output.clear();
  output.reserve(digits.size() / 2U);
  for (std::size_t index = 0U; index < digits.size(); index += 2U) {
    output.push_back(
        static_cast<std::uint8_t>((nibble(digits[index]) << 4U) | nibble(digits[index + 1U])));
  }
  return true;
}

bool LoadTextFile(const std::filesystem::path& path, std::string& output) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }
  output.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  return input.good() || input.eof();
}

bool WriteTextFile(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  output.close();
  return static_cast<bool>(output);
}

bool WriteBinaryFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  output.close();
  return static_cast<bool>(output);
}

bool LoadBinaryFile(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }
  bytes.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  return input.good() || input.eof();
}

std::string HexUpper(const std::vector<std::uint8_t>& input) {
  static constexpr char kDigits[] = "0123456789ABCDEF";
  std::string output(input.size() * 2U, '0');
  for (std::size_t index = 0U; index < input.size(); ++index) {
    output[index * 2U] = kDigits[input[index] >> 4U];
    output[index * 2U + 1U] = kDigits[input[index] & 0x0FU];
  }
  return output;
}

std::filesystem::path OnlyCompletedRun(const std::filesystem::path& root) {
  std::filesystem::path result;
  std::error_code error;
  for (std::filesystem::directory_iterator iterator{root, error}, end; iterator != end && !error;
       iterator.increment(error)) {
    const std::string name = iterator->path().filename().generic_string();
    if (iterator->is_directory(error) && name.rfind("run_", 0U) == 0U &&
        iterator->path().extension() != ".inprogress") {
      if (!result.empty()) {
        return {};
      }
      result = iterator->path();
    }
  }
  return result;
}

bool HasInProgressRun(const std::filesystem::path& root) {
  std::error_code error;
  for (std::filesystem::directory_iterator iterator{root, error}, end; iterator != end && !error;
       iterator.increment(error)) {
    if (iterator->is_directory(error) && iterator->path().extension() == ".inprogress") {
      return true;
    }
  }
  return false;
}

bool Contains(std::string_view text, std::string_view expected) {
  return text.find(expected) != std::string_view::npos;
}

bool EventOffsetsAreNondecreasing(std::string_view events) {
  constexpr std::string_view marker = "\"monotonic_offset_ns\":";
  std::uint64_t previous = 0U;
  bool saw_event = false;
  std::size_t position = 0U;
  while ((position = events.find(marker, position)) != std::string_view::npos) {
    position += marker.size();
    std::uint64_t value = 0U;
    bool saw_digit = false;
    while (position < events.size() && events[position] >= '0' && events[position] <= '9') {
      saw_digit = true;
      value = value * 10U + static_cast<std::uint64_t>(events[position] - '0');
      ++position;
    }
    if (!saw_digit || (saw_event && value < previous)) {
      return false;
    }
    previous = value;
    saw_event = true;
  }
  return saw_event && Contains(events, "\"wall_clock_utc\":\"20");
}

bool SameHistoricalTransport(const pae::protocol_lab::StoredRun& source,
                             const pae::protocol_lab::StoredRun& replay) {
  const auto& actual = replay.historical_transport;
  if (!actual.present) {
    return false;
  }
  if (source.historical_transport.present) {
    const auto& expected = source.historical_transport;
    return actual.transport == expected.transport && actual.timeout_ms == expected.timeout_ms &&
           actual.status == expected.status && actual.diagnostic_id == expected.diagnostic_id &&
           actual.local_endpoint == expected.local_endpoint &&
           actual.remote_endpoint == expected.remote_endpoint &&
           actual.received_from == expected.received_from &&
           actual.send_attempted == expected.send_attempted &&
           actual.send_succeeded == expected.send_succeeded &&
           actual.response_received == expected.response_received &&
           actual.response_decoded == expected.response_decoded;
  }
  return actual.transport == source.transport && actual.timeout_ms == source.timeout_ms &&
         actual.status == source.operation_status && actual.diagnostic_id == source.diagnostic_id &&
         actual.local_endpoint == source.local_endpoint &&
         actual.remote_endpoint == source.remote_endpoint &&
         actual.received_from == source.received_from &&
         actual.send_attempted == source.send_attempted &&
         actual.send_succeeded == source.send_succeeded &&
         actual.response_received == source.response_received &&
         actual.response_decoded == source.response_decoded;
}

bool ReplayOnceAndCheck(const std::filesystem::path& source_run,
                        const std::filesystem::path& replay_root, std::string_view mode,
                        std::string_view subject, std::string_view protocol_operation,
                        int expected_exit, bool expected_response_decoded,
                        std::string_view comparison_status, std::filesystem::path& replay_run) {
  std::string source_manifest_before;
  if (!LoadTextFile(source_run / "SHA256SUMS", source_manifest_before)) {
    return false;
  }
  pae::protocol_lab::StandardRecordFileSystem file_system;
  FakeUdpAdapter adapter;
  ExecutionObserver observer;
  std::string output;
  std::vector<std::string> arguments{"pae_protocol_lab", "replay",
                                     "--bundle",         source_run.generic_string(),
                                     "--record-root",    replay_root.generic_string(),
                                     "--output",         "json"};
  const int exit_code = RunCaptured(std::move(arguments), file_system, adapter, output, &observer);
  std::string source_manifest_after;
  replay_run = OnlyCompletedRun(replay_root);
  const bool operation_matches =
      protocol_operation.empty()
          ? observer.operations.empty()
          : observer.operations == std::vector<std::string>{std::string{protocol_operation}};
  pae::protocol_lab::StoredRun source;
  pae::protocol_lab::StoredRun replay;
  std::string error;
  if (exit_code != expected_exit || adapter.call_count != 0 || !operation_matches ||
      replay_run.empty() || !LoadTextFile(source_run / "SHA256SUMS", source_manifest_after) ||
      source_manifest_before != source_manifest_after ||
      !pae::protocol_lab::LoadStoredRun(source_run, source, error) ||
      !pae::protocol_lab::LoadStoredRun(replay_run, replay, error)) {
    return false;
  }
  const std::string expected_current_status =
      mode == "NO_CODEC_REEXECUTION" ? "NOT_EVALUATED" : source.current_execution_status;
  const std::string expected_current_diagnostic =
      mode == "NO_CODEC_REEXECUTION" ? "" : source.current_execution_diagnostic_id;
  const std::string expected_operation_status =
      mode == "NO_CODEC_REEXECUTION" ? "EVIDENCE_VERIFIED" : expected_current_status;
  const bool comparison_equal = comparison_status == "EQUAL";
  const bool comparison_not_evaluated = comparison_status == "NOT_EVALUATED";
  return replay.command == "replay" && replay.operation_kind == "udp-exchange" &&
         replay.operation_status == expected_operation_status &&
         replay.exit_code == expected_exit && replay.replay_mode == mode &&
         replay.replay_subject == subject &&
         replay.current_execution_status == expected_current_status &&
         replay.current_execution_diagnostic_id == expected_current_diagnostic &&
         replay.diagnostic_id == expected_current_diagnostic && !replay.response_received &&
         replay.response_decoded == expected_response_decoded &&
         replay.comparison_status == comparison_status &&
         (comparison_not_evaluated ? !replay.comparison_equal.has_value()
                                   : replay.comparison_equal == comparison_equal) &&
         replay.tx_frame_hex == source.tx_frame_hex && replay.rx_frame_hex == source.rx_frame_hex &&
         SameHistoricalTransport(source, replay) &&
         std::filesystem::is_regular_file(replay_run / "result_summary_v0.2.json") &&
         std::filesystem::is_regular_file(replay_run / "run_record_v0.2.json") &&
         std::filesystem::is_regular_file(replay_run / "events_v0.2.jsonl") &&
         std::filesystem::is_regular_file(replay_run / "COMPLETE");
}

bool ReplayAndCheck(const std::filesystem::path& source_run,
                    const std::filesystem::path& replay_root, std::string_view mode,
                    std::string_view subject, std::string_view protocol_operation,
                    int expected_exit, bool expected_response_decoded,
                    std::string_view comparison_status) {
  std::filesystem::path replay_a;
  if (!ReplayOnceAndCheck(source_run, replay_root / "a", mode, subject, protocol_operation,
                          expected_exit, expected_response_decoded, comparison_status, replay_a)) {
    return false;
  }
  std::filesystem::path replay_b;
  return ReplayOnceAndCheck(replay_a, replay_root / "b", mode, subject, protocol_operation,
                            expected_exit, expected_response_decoded, comparison_status, replay_b);
}

bool RebuildManifest(const std::filesystem::path& run);

bool RefreshRunRecordPayload(const std::filesystem::path& run, std::string_view relative) {
  const std::filesystem::path v2 = run / "run_record_v0.2.json";
  const std::filesystem::path record_path =
      std::filesystem::is_regular_file(v2) ? v2 : run / "run_record_v0.1.json";
  std::string record;
  std::vector<std::uint8_t> bytes;
  std::ifstream input{run / std::filesystem::path{relative}, std::ios::binary};
  if (!LoadTextFile(record_path, record) || !input) {
    return false;
  }
  bytes.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  const std::string path_marker = "\"path\":\"" + std::string{relative} + "\"";
  const std::size_t path_position = record.find(path_marker);
  const std::size_t size_position = record.find("\"size\":", path_position);
  const std::size_t size_end = record.find(',', size_position);
  const std::size_t hash_position = record.find("\"sha256\":\"", size_end);
  if (path_position == std::string::npos || size_position == std::string::npos ||
      size_end == std::string::npos || hash_position == std::string::npos) {
    return false;
  }
  const std::size_t size_value = size_position + std::string{"\"size\":"}.size();
  record.replace(size_value, size_end - size_value, std::to_string(bytes.size()));
  const std::size_t adjusted_hash_position = record.find("\"sha256\":\"", path_position);
  const std::size_t hash_value = adjusted_hash_position + std::string{"\"sha256\":\""}.size();
  record.replace(hash_value, 64U, pae::protocol_lab::HashBytes(bytes));
  return WriteTextFile(record_path, record);
}

bool RejectsReplayInput(const std::filesystem::path& bundle,
                        const std::filesystem::path& record_root,
                        std::string_view expected_detail) {
  pae::protocol_lab::StandardRecordFileSystem file_system;
  FakeUdpAdapter adapter;
  std::string output;
  std::vector<std::string> arguments{"pae_protocol_lab", "replay",
                                     "--bundle",         bundle.generic_string(),
                                     "--record-root",    record_root.generic_string(),
                                     "--output",         "json"};
  return RunCaptured(std::move(arguments), file_system, adapter, output) == 3 &&
         adapter.call_count == 0 && Contains(output, "PAE_LAB_RUN_INPUT_ERROR") &&
         Contains(output, expected_detail) && OnlyCompletedRun(record_root).empty();
}

bool RejectsSelfConsistentButMismatchedRxMetadata(const std::filesystem::path& source_run,
                                                  const std::filesystem::path& tampered_run) {
  std::error_code file_error;
  std::filesystem::copy(source_run, tampered_run, std::filesystem::copy_options::recursive,
                        file_error);
  if (file_error) {
    return false;
  }
  const std::filesystem::path metadata_path = tampered_run / "frames/000002_rx.meta.json";
  std::string metadata;
  if (!LoadTextFile(metadata_path, metadata)) {
    return false;
  }
  constexpr std::string_view original = "127.0.0.1:23456";
  const std::size_t source = metadata.find(original);
  if (source == std::string::npos) {
    return false;
  }
  metadata.replace(source, original.size(), "127.0.0.1:23457");
  if (!WriteTextFile(metadata_path, metadata)) {
    return false;
  }
  if (!RefreshRunRecordPayload(tampered_run, "frames/000002_rx.meta.json") ||
      !RebuildManifest(tampered_run)) {
    return false;
  }
  pae::protocol_lab::StoredRun stored;
  std::string error;
  return !pae::protocol_lab::LoadStoredRun(tampered_run, stored, error) &&
         Contains(error, "RX metadata does not match");
}

bool RebuildManifest(const std::filesystem::path& run) {
  std::vector<std::string> paths;
  std::error_code error;
  for (std::filesystem::recursive_directory_iterator iterator{run, error}, end;
       iterator != end && !error; iterator.increment(error)) {
    if (iterator->is_regular_file(error) && iterator->path().filename() != "SHA256SUMS") {
      paths.push_back(std::filesystem::relative(iterator->path(), run, error).generic_string());
    }
  }
  if (error) {
    return false;
  }
  std::sort(paths.begin(), paths.end());
  std::ostringstream manifest;
  for (const std::string& relative : paths) {
    std::vector<std::uint8_t> bytes;
    std::ifstream input{run / relative, std::ios::binary};
    if (!input) {
      return false;
    }
    bytes.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    manifest << pae::protocol_lab::HashBytes(bytes) << "  " << relative << '\n';
  }
  return WriteTextFile(run / "SHA256SUMS", manifest.str());
}

bool CopyRun(const std::filesystem::path& source, const std::filesystem::path& destination) {
  std::error_code error;
  std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive, error);
  return !error;
}

bool ReplaceInFile(const std::filesystem::path& path, std::string_view from, std::string_view to) {
  std::string text;
  if (!LoadTextFile(path, text)) {
    return false;
  }
  const std::size_t position = text.find(from);
  if (position == std::string::npos) {
    return false;
  }
  text.replace(position, from.size(), to);
  return WriteTextFile(path, text);
}

bool MaterializeLegacyOfflineV1(const std::filesystem::path& bundle) {
  std::error_code error;
  std::filesystem::create_directories(bundle / "inputs", error);
  std::filesystem::create_directories(bundle / "frames", error);
  if (error) {
    return false;
  }
  const std::filesystem::path fixture{"data/legacy_offline_v0_1"};
  for (const std::string_view name :
       {"result_summary_v0.1.json", "run_record_v0.1.json", "events_v0.1.jsonl"}) {
    std::filesystem::copy_file(fixture / name, bundle / name,
                               std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
      return false;
    }
  }
  std::filesystem::copy_file("data/synthetic_lab_exchange_slice.pae.json",
                             bundle / "inputs/protocol.pae.json",
                             std::filesystem::copy_options::overwrite_existing, error);
  std::filesystem::copy_file(fixture / "000001_frame.hex", bundle / "frames/000001_frame.hex",
                             std::filesystem::copy_options::overwrite_existing, error);
  std::vector<std::uint8_t> frame;
  if (error || !LoadHexFile(fixture / "000001_frame.hex", frame) ||
      !WriteBinaryFile(bundle / "frames/000001_frame.bin", frame) ||
      !WriteTextFile(bundle / "COMPLETE", {})) {
    return false;
  }
  for (const std::string_view path :
       {"inputs/protocol.pae.json", "frames/000001_frame.bin", "frames/000001_frame.hex",
        "result_summary_v0.1.json", "events_v0.1.jsonl"}) {
    if (!RefreshRunRecordPayload(bundle, path)) {
      return false;
    }
  }
  return RebuildManifest(bundle);
}

bool LegacyV1LoadCompareReplay(const std::filesystem::path& root) {
  const std::filesystem::path bundle = root / "normalized-real-v0.1";
  if (!MaterializeLegacyOfflineV1(bundle)) {
    return false;
  }
  pae::protocol_lab::StoredRun stored;
  std::string error;
  if (!pae::protocol_lab::LoadStoredRun(bundle, stored, error) ||
      stored.format_version != "pae.lab.result/0.1" || stored.command != "inspect" ||
      stored.operation_kind != "inspect" || stored.operation_status != "OK" ||
      !stored.transport.empty() || stored.send_attempted || stored.send_succeeded ||
      stored.response_received || stored.response_decoded) {
    return false;
  }

  pae::protocol_lab::StandardRecordFileSystem file_system;
  FakeUdpAdapter adapter;
  std::string output;
  std::vector<std::string> compare{
      "pae_protocol_lab",      "compare",  "--left-run", bundle.generic_string(), "--right-run",
      bundle.generic_string(), "--output", "json"};
  if (RunCaptured(std::move(compare), file_system, adapter, output) != 0 ||
      adapter.call_count != 0 || !Contains(output, "\"operation_status\":\"EQUAL\"")) {
    return false;
  }

  ExecutionObserver observer;
  output.clear();
  std::vector<std::string> replay{"pae_protocol_lab", "replay",
                                  "--bundle",         bundle.generic_string(),
                                  "--record-root",    (root / "replay").generic_string(),
                                  "--output",         "json"};
  const int replay_exit = RunCaptured(std::move(replay), file_system, adapter, output, &observer);
  const std::filesystem::path replay_run = OnlyCompletedRun(root / "replay");
  pae::protocol_lab::StoredRun replayed;
  return replay_exit == 0 && adapter.call_count == 0 && observer.operations.empty() &&
         !replay_run.empty() && pae::protocol_lab::LoadStoredRun(replay_run, replayed, error) &&
         replayed.format_version == "pae.lab.result/0.1" && replayed.command == "replay" &&
         replayed.operation_kind == "inspect" && replayed.operation_status == "OK" &&
         replayed.frame_hex == stored.frame_hex && replayed.comparison_equal == true;
}

bool LegacyV1NegativeCases(const std::filesystem::path& source, const std::filesystem::path& root) {
  std::error_code directory_error;
  std::filesystem::create_directories(root, directory_error);
  if (directory_error) {
    return false;
  }
  pae::protocol_lab::StoredRun stored;
  std::string error;
  const std::filesystem::path unknown_field = root / "unknown-field";
  const std::filesystem::path wrong_type = root / "wrong-type";
  const std::filesystem::path unknown_version = root / "unknown-version";
  const std::filesystem::path inconsistent = root / "inconsistent-file";
  return CopyRun(source, unknown_field) &&
         ReplaceInFile(unknown_field / "run_record_v0.1.json",
                       "  \"limits\":", "  \"unknown_field\":1,\n  \"limits\":") &&
         RebuildManifest(unknown_field) &&
         !pae::protocol_lab::LoadStoredRun(unknown_field, stored, error) &&
         Contains(error, "unknown property") && CopyRun(source, wrong_type) &&
         ReplaceInFile(wrong_type / "run_record_v0.1.json", "\"send_attempted\":false",
                       "\"send_attempted\":\"false\"") &&
         RebuildManifest(wrong_type) && (error.clear(), true) &&
         !pae::protocol_lab::LoadStoredRun(wrong_type, stored, error) &&
         Contains(error, "send_attempted must be boolean") && CopyRun(source, unknown_version) &&
         ReplaceInFile(unknown_version / "run_record_v0.1.json", "pae.lab.record/0.1",
                       "pae.lab.record/9.9") &&
         RebuildManifest(unknown_version) && (error.clear(), true) &&
         !pae::protocol_lab::LoadStoredRun(unknown_version, stored, error) &&
         Contains(error, "version or manifest contract") && CopyRun(source, inconsistent) &&
         ReplaceInFile(inconsistent / "frames/000001_frame.hex", "0D00", "0E00") &&
         RebuildManifest(inconsistent) && (error.clear(), true) &&
         !pae::protocol_lab::LoadStoredRun(inconsistent, stored, error) &&
         Contains(error, "Run Record payload does not match");
}

bool TestLegacyOfflineV1Compatibility(const std::filesystem::path& root) {
  if (!LegacyV1LoadCompareReplay(root)) {
    return false;
  }
  return LegacyV1NegativeCases(root / "normalized-real-v0.1", root / "negative");
}

bool WriteDerivedConfig(const std::filesystem::path& path, std::string_view marker,
                        std::string_view replacement) {
  std::string config;
  return LoadTextFile("data/synthetic_lab_exchange_slice.pae.json", config) &&
         (config.find(marker) != std::string::npos) &&
         (config.replace(config.find(marker), marker.size(), replacement), true) &&
         WriteTextFile(path, config);
}

bool TestCrossConfigEncodeReplay(const std::filesystem::path& source_run,
                                 const std::filesystem::path& root) {
  std::error_code directory_error;
  std::filesystem::create_directories(root, directory_error);
  const std::filesystem::path different_config = root / "different-wire.pae.json";
  std::string config;
  if (directory_error || !LoadTextFile("data/synthetic_lab_exchange_slice.pae.json", config)) {
    return false;
  }
  const std::size_t matcher = config.find("\"bytes\": \"A7 31\"");
  const std::size_t constant = config.find("\"value\": 12711");
  if (matcher == std::string::npos || constant == std::string::npos) {
    return false;
  }
  config.replace(matcher, std::string{"\"bytes\": \"A7 31\""}.size(), "\"bytes\": \"A8 31\"");
  config.replace(config.find("\"value\": 12711"), std::string{"\"value\": 12711"}.size(),
                 "\"value\": 12712");
  if (!WriteTextFile(different_config, config)) {
    return false;
  }

  std::string source_manifest;
  std::string source_manifest_after;
  if (!LoadTextFile(source_run / "SHA256SUMS", source_manifest)) {
    return false;
  }
  pae::protocol_lab::StandardRecordFileSystem file_system;
  FakeUdpAdapter adapter;
  ExecutionObserver observer;
  std::string output;
  std::vector<std::string> arguments{"pae_protocol_lab", "replay",
                                     "--bundle",         source_run.generic_string(),
                                     "--config",         different_config.generic_string(),
                                     "--record-root",    (root / "different").generic_string(),
                                     "--output",         "json"};
  const int replay_exit =
      RunCaptured(std::move(arguments), file_system, adapter, output, &observer);
  const std::filesystem::path different_run = OnlyCompletedRun(root / "different");
  pae::protocol_lab::StoredRun source;
  pae::protocol_lab::StoredRun different;
  std::string load_error;
  if (replay_exit != 6 || adapter.call_count != 0 ||
      observer.operations != std::vector<std::string>{"ENCODE_TX"} || different_run.empty() ||
      !LoadTextFile(source_run / "SHA256SUMS", source_manifest_after) ||
      source_manifest != source_manifest_after ||
      !pae::protocol_lab::LoadStoredRun(source_run, source, load_error) ||
      !pae::protocol_lab::LoadStoredRun(different_run, different, load_error)) {
    return false;
  }
  std::vector<std::uint8_t> actual;
  if (!LoadBinaryFile(different_run / different.frame_file, actual)) {
    return false;
  }
  if (different.command != "replay" || different.replay_mode != "ENCODE_TX" ||
      different.replay_subject != "TX" || different.operation_status != "OK" ||
      different.current_execution_status != "OK" || !different.cross_config_replay ||
      different.comparison_status != "DIFFERENT" || different.comparison_equal != false ||
      different.frame_hex == source.tx_frame_hex || different.tx_frame_hex != source.tx_frame_hex ||
      HexUpper(actual) != different.frame_hex || different.tx_frame_file != "" ||
      !SameHistoricalTransport(source, different)) {
    return false;
  }

  std::filesystem::path replay_b;
  if (!ReplayOnceAndCheck(different_run, root / "different-replay", "ENCODE_TX", "TX", "ENCODE_TX",
                          0, false, "EQUAL", replay_b)) {
    return false;
  }
  pae::protocol_lab::StoredRun second;
  if (!pae::protocol_lab::LoadStoredRun(replay_b, second, load_error) ||
      second.frame_hex != different.frame_hex || second.tx_frame_hex != source.tx_frame_hex) {
    return false;
  }

  const std::filesystem::path failing_config = root / "encode-failure.pae.json";
  if (!WriteDerivedConfig(failing_config, "\"id\": \"mode_active\"",
                          "\"id\": \"mode_active_removed\"")) {
    return false;
  }
  observer.operations.clear();
  output.clear();
  arguments = {"pae_protocol_lab", "replay",
               "--bundle",         source_run.generic_string(),
               "--config",         failing_config.generic_string(),
               "--record-root",    (root / "failure").generic_string(),
               "--output",         "json"};
  const int failure_exit =
      RunCaptured(std::move(arguments), file_system, adapter, output, &observer);
  const std::filesystem::path failure_run = OnlyCompletedRun(root / "failure");
  pae::protocol_lab::StoredRun failure;
  return failure_exit == 5 && adapter.call_count == 0 &&
         observer.operations == std::vector<std::string>{"ENCODE_TX"} && !failure_run.empty() &&
         pae::protocol_lab::LoadStoredRun(failure_run, failure, load_error) &&
         failure.command == "replay" && failure.replay_mode == "ENCODE_TX" &&
         failure.replay_subject == "TX" && failure.operation_status != "OK" &&
         failure.current_execution_status == failure.operation_status &&
         failure.cross_config_replay && failure.frame_hex.empty() &&
         failure.tx_frame_hex == source.tx_frame_hex && failure.comparison_status == "DIFFERENT" &&
         failure.comparison_equal == false;
}

bool TestRunRecordValidation(const std::filesystem::path& source_run,
                             const std::filesystem::path& root) {
  const std::filesystem::path missing = root / "missing";
  const std::filesystem::path invalid = root / "invalid-json";
  const std::filesystem::path unknown = root / "unknown-version";
  const std::filesystem::path mismatch = root / "summary-mismatch";
  std::error_code error;
  std::filesystem::create_directories(root, error);
  if (error || !CopyRun(source_run, missing)) {
    return false;
  }
  std::filesystem::remove(missing / "run_record_v0.2.json", error);
  if (error || !RebuildManifest(missing) ||
      !RejectsReplayInput(missing, root / "missing-output", "required Evidence Bundle file")) {
    return false;
  }
  if (!CopyRun(source_run, invalid) ||
      !WriteTextFile(invalid / "run_record_v0.2.json", "{invalid") || !RebuildManifest(invalid) ||
      !RejectsReplayInput(invalid, root / "invalid-output", "Run Record is not valid JSON")) {
    return false;
  }
  if (!CopyRun(source_run, unknown) ||
      !ReplaceInFile(unknown / "run_record_v0.2.json", "pae.lab.record/0.2",
                     "pae.lab.record/9.9") ||
      !RebuildManifest(unknown) ||
      !RejectsReplayInput(unknown, root / "unknown-output", "version or manifest contract")) {
    return false;
  }
  if (!CopyRun(source_run, mismatch) ||
      !ReplaceInFile(mismatch / "run_record_v0.2.json", "\"replay_mode\":\"DECODE_RX\"",
                     "\"replay_mode\":\"ENCODE_TX\"") ||
      !RebuildManifest(mismatch) ||
      !RejectsReplayInput(mismatch, root / "mismatch-output",
                          "Run Record conflicts with the result summary")) {
    return false;
  }
  return true;
}

bool MutateEventAndReject(const std::filesystem::path& source_run,
                          const std::filesystem::path& tampered_run, std::string_view from,
                          std::string_view to, std::string_view expected_detail) {
  if (!CopyRun(source_run, tampered_run) ||
      !ReplaceInFile(tampered_run / "events_v0.2.jsonl", from, to) ||
      !RefreshRunRecordPayload(tampered_run, "events_v0.2.jsonl") ||
      !RebuildManifest(tampered_run)) {
    return false;
  }
  return RejectsReplayInput(
      tampered_run, tampered_run.parent_path() / (tampered_run.filename().string() + "-output"),
      expected_detail);
}

bool RejectsSendResultBeforeIntent(const std::filesystem::path& source_run,
                                   const std::filesystem::path& tampered_run) {
  if (!CopyRun(source_run, tampered_run)) {
    return false;
  }
  const std::filesystem::path events_path = tampered_run / "events_v0.2.jsonl";
  std::ifstream input{events_path};
  std::vector<std::string> lines{std::istream_iterator<std::string>{input},
                                 std::istream_iterator<std::string>{}};
  if (lines.size() < 3U || !Contains(lines[1], "SEND_INTENT") ||
      !Contains(lines[2], "SEND_RESULT")) {
    return false;
  }
  constexpr std::string_view offset_marker = "\"monotonic_offset_ns\":";
  auto offset = [&](const std::string& line) {
    const std::size_t begin = line.find(offset_marker) + offset_marker.size();
    const std::size_t end = line.find('}', begin);
    return line.substr(begin, end - begin);
  };
  const std::string intent_offset = offset(lines[1]);
  const std::string result_offset = offset(lines[2]);
  std::swap(lines[1], lines[2]);
  if (lines[1].find("\"event_id\":3") == std::string::npos ||
      lines[2].find("\"event_id\":2") == std::string::npos) {
    return false;
  }
  lines[1].replace(lines[1].find("\"event_id\":3"), std::string{"\"event_id\":3"}.size(),
                   "\"event_id\":2");
  lines[2].replace(lines[2].find("\"event_id\":2"), std::string{"\"event_id\":2"}.size(),
                   "\"event_id\":3");
  lines[1].replace(lines[1].find(result_offset, lines[1].find(offset_marker)), result_offset.size(),
                   intent_offset);
  lines[2].replace(lines[2].find(intent_offset, lines[2].find(offset_marker)), intent_offset.size(),
                   result_offset);
  std::ostringstream output;
  for (const std::string& line : lines) {
    output << line << '\n';
  }
  if (!WriteTextFile(events_path, output.str()) ||
      !RefreshRunRecordPayload(tampered_run, "events_v0.2.jsonl") ||
      !RebuildManifest(tampered_run)) {
    return false;
  }
  return RejectsReplayInput(tampered_run, tampered_run.parent_path() / "order-output",
                            "event sequence conflicts");
}

bool TestEventBindingValidation(const std::filesystem::path& source_run,
                                const std::filesystem::path& root) {
  std::error_code error;
  std::filesystem::create_directories(root, error);
  if (error) {
    return false;
  }
  return RejectsSendResultBeforeIntent(source_run, root / "send-result-before-intent") &&
         MutateEventAndReject(source_run, root / "tx-length", "\"frame_length\":13",
                              "\"frame_length\":12", "does not bind") &&
         MutateEventAndReject(source_run, root / "rx-hash", "\"frame_sha256\":\"4c71ca",
                              "\"frame_sha256\":\"5c71ca", "does not bind") &&
         MutateEventAndReject(source_run, root / "unknown-kind", "\"event_kind\":\"FRAME\"",
                              "\"event_kind\":\"OTHER\"", "unsupported event_kind") &&
         MutateEventAndReject(source_run, root / "wrong-type", "\"succeeded\":true",
                              "\"succeeded\":1", "succeeded must be boolean") &&
         MutateEventAndReject(source_run, root / "missing-field", "\"remote_endpoint\":\"\",", "",
                              "is missing property remote_endpoint") &&
         MutateEventAndReject(source_run, root / "unknown-field",
                              "\"peer_kind\":", "\"unknown_field\":", "unknown property");
}

bool RejectsMissingRxMetadata(const std::filesystem::path& source_run,
                              const std::filesystem::path& missing_run) {
  std::error_code error;
  std::filesystem::copy(source_run, missing_run, std::filesystem::copy_options::recursive, error);
  if (error) {
    return false;
  }
  std::filesystem::remove(missing_run / "frames/000002_rx.meta.json", error);
  if (error || !RebuildManifest(missing_run)) {
    return false;
  }
  pae::protocol_lab::StoredRun stored;
  std::string load_error;
  return !pae::protocol_lab::LoadStoredRun(missing_run, stored, load_error) &&
         Contains(load_error, "has no RX metadata");
}

bool TestVersionHandling(const std::filesystem::path& source_run,
                         const std::filesystem::path& root) {
  std::error_code error;
  const std::filesystem::path legacy = root / "legacy-udp-v0.1";
  const std::filesystem::path unknown = root / "unknown-version";
  std::filesystem::create_directories(root, error);
  std::filesystem::copy(source_run, legacy, std::filesystem::copy_options::recursive, error);
  if (error) {
    return false;
  }
  std::string result;
  const std::filesystem::path legacy_v2 = legacy / "result_summary_v0.2.json";
  if (!LoadTextFile(legacy_v2, result)) {
    return false;
  }
  result.replace(result.find("pae.lab.result/0.2"), std::string{"pae.lab.result/0.2"}.size(),
                 "pae.lab.result/0.1");
  const std::vector<std::string> v2_fields{"  \"receive_pipeline_id\":",
                                           "  \"replay_mode\":",
                                           "  \"replay_subject\":",
                                           "  \"current_execution_status\":",
                                           "  \"current_execution_diagnostic_id\":",
                                           "  \"comparison_status\":",
                                           "  \"comparison_reason\":",
                                           "  \"historical_transport\":"};
  std::istringstream lines{result};
  std::ostringstream legacy_result;
  std::string line;
  while (std::getline(lines, line)) {
    if (std::none_of(v2_fields.begin(), v2_fields.end(),
                     [&](const std::string& prefix) { return line.rfind(prefix, 0U) == 0U; })) {
      legacy_result << line << '\n';
    }
  }
  const std::filesystem::path legacy_v1 = legacy / "result_summary_v0.1.json";
  if (!WriteTextFile(legacy_v1, legacy_result.str())) {
    return false;
  }
  std::filesystem::remove(legacy_v2, error);
  std::string event_text;
  std::string record_text;
  const std::filesystem::path event_v2 = legacy / "events_v0.2.jsonl";
  const std::filesystem::path event_v1 = legacy / "events_v0.1.jsonl";
  const std::filesystem::path record_v2 = legacy / "run_record_v0.2.json";
  const std::filesystem::path record_v1 = legacy / "run_record_v0.1.json";
  if (error || !LoadTextFile(event_v2, event_text) || !LoadTextFile(record_v2, record_text)) {
    return false;
  }
  std::size_t position = 0U;
  while ((position = event_text.find("pae.lab.event/0.2")) != std::string::npos) {
    event_text.replace(position, std::string{"pae.lab.event/0.2"}.size(), "pae.lab.event/0.1");
  }
  record_text.replace(record_text.find("pae.lab.record/0.2"),
                      std::string{"pae.lab.record/0.2"}.size(), "pae.lab.record/0.1");
  record_text.replace(record_text.find("result_summary_v0.2.json"),
                      std::string{"result_summary_v0.2.json"}.size(), "result_summary_v0.1.json");
  record_text.replace(record_text.find("events_v0.2.jsonl"),
                      std::string{"events_v0.2.jsonl"}.size(), "events_v0.1.jsonl");
  std::istringstream record_lines{record_text};
  std::ostringstream legacy_record;
  while (std::getline(record_lines, line)) {
    if (std::none_of(v2_fields.begin(), v2_fields.end(),
                     [&](const std::string& prefix) { return line.rfind(prefix, 0U) == 0U; })) {
      legacy_record << line << '\n';
    }
  }
  if (!WriteTextFile(event_v1, event_text) || !WriteTextFile(record_v1, legacy_record.str())) {
    return false;
  }
  std::filesystem::remove(event_v2, error);
  if (error) {
    return false;
  }
  std::filesystem::remove(record_v2, error);
  if (error || !RefreshRunRecordPayload(legacy, "result_summary_v0.1.json") ||
      !RefreshRunRecordPayload(legacy, "events_v0.1.jsonl") || !RebuildManifest(legacy)) {
    return false;
  }

  pae::protocol_lab::StandardRecordFileSystem file_system;
  FakeUdpAdapter adapter;
  std::string output;
  std::vector<std::string> arguments{"pae_protocol_lab",      "replay",   "--bundle",
                                     legacy.generic_string(), "--output", "json"};
  if (RunCaptured(std::move(arguments), file_system, adapter, output) != 3 ||
      !Contains(output, "PAE_LAB_REPLAY_EVIDENCE_INSUFFICIENT") || adapter.call_count != 0) {
    return false;
  }

  error.clear();
  std::filesystem::copy(source_run, unknown, std::filesystem::copy_options::recursive, error);
  if (error || !LoadTextFile(unknown / "result_summary_v0.2.json", result)) {
    return false;
  }
  const std::size_t version = result.find("pae.lab.result/0.2");
  if (version == std::string::npos) {
    return false;
  }
  result.replace(version, std::string{"pae.lab.result/0.2"}.size(), "pae.lab.result/9.9");
  if (!WriteTextFile(unknown / "result_summary_v0.2.json", result) || !RebuildManifest(unknown)) {
    return false;
  }
  output.clear();
  arguments = {"pae_protocol_lab",       "replay",   "--bundle",
               unknown.generic_string(), "--output", "json"};
  return RunCaptured(std::move(arguments), file_system, adapter, output) == 3 &&
         Contains(output, "unsupported result format_version") && adapter.call_count == 0;
}

bool TestPreviewUsesNoSocket(const std::filesystem::path& root) {
  FakeUdpAdapter adapter;
  adapter.record_root = root;
  pae::protocol_lab::StandardRecordFileSystem file_system;
  std::string output;
  const int exit_code =
      RunCaptured(UdpArguments(root, "127.0.0.1:9", false), file_system, adapter, output);
  const std::filesystem::path run = OnlyCompletedRun(root);
  return exit_code == 0 && adapter.call_count == 0 && !run.empty() &&
         std::filesystem::is_regular_file(run / "frames/000001_tx.bin") &&
         !std::filesystem::exists(run / "frames/000002_rx.bin") &&
         Contains(output, "\"send_attempted\":false") &&
         Contains(output, "\"send_succeeded\":false") &&
         ReplayAndCheck(run, root / "replay", "ENCODE_TX", "TX", "ENCODE_TX", 0, false, "EQUAL") &&
         TestCrossConfigEncodeReplay(run, root / "cross-config");
}

bool TestRealLoopback(const std::filesystem::path& root,
                      const std::vector<std::uint8_t>& expected_request,
                      const std::vector<std::uint8_t>& response) {
  LoopbackPeer peer{response};
  if (!peer.Start()) {
    std::cerr << peer.Error() << '\n';
    return false;
  }
  pae::protocol_lab::StandardRecordFileSystem file_system;
  std::unique_ptr<IUdpExchangeAdapter> adapter =
      pae::protocol_lab::CreatePlatformUdpExchangeAdapter();
  std::string output;
  auto arguments = UdpArguments(root, "127.0.0.1:" + std::to_string(peer.Port()), true);
  arguments.insert(arguments.end(), {"--timeout-ms", "2000"});
  const int exit_code = RunCaptured(std::move(arguments), file_system, *adapter, output);
  peer.Join();
  const std::filesystem::path run = OnlyCompletedRun(root);
  if (exit_code != 0 || peer.Captured() != expected_request || !peer.Error().empty() ||
      run.empty() || !std::filesystem::is_regular_file(run / "frames/000001_tx.bin") ||
      !std::filesystem::is_regular_file(run / "frames/000002_rx.bin") ||
      !Contains(output, "\"send_attempted\":true") ||
      !Contains(output, "\"send_succeeded\":true") ||
      !Contains(output, "\"response_received\":true") ||
      Contains(output, "\"local_endpoint\":\"127.0.0.1:0\"") ||
      !Contains(output, "\"received_from\":\"127.0.0.1:" + std::to_string(peer.Port()) + "\"") ||
      !Contains(output, "\"message_id\":\"lab_report\"")) {
    return false;
  }

  pae::protocol_lab::StoredRun stored;
  std::string events;
  std::string run_record;
  std::string error;
  if (!pae::protocol_lab::LoadStoredRun(run, stored, error) ||
      stored.tx_frame_hex != HexUpper(expected_request) ||
      stored.rx_frame_hex != HexUpper(response) ||
      !LoadTextFile(run / "events_v0.2.jsonl", events) ||
      !LoadTextFile(run / "run_record_v0.2.json", run_record)) {
    std::cerr << "could not validate UDP Run: " << error << '\n';
    return false;
  }
  const std::size_t tx_event = events.find("\"event_kind\":\"FRAME\",\"direction\":\"TX\"");
  const std::size_t intent_event = events.find("\"event_kind\":\"SEND_INTENT\"");
  const std::size_t result_event = events.find("\"event_kind\":\"SEND_RESULT\"");
  const std::size_t rx_event = events.find("\"event_kind\":\"FRAME\",\"direction\":\"RX\"");
  if (tx_event == std::string::npos || intent_event <= tx_event || result_event <= intent_event ||
      rx_event <= result_event || Contains(run_record, "\"local_endpoint\":\"127.0.0.1:0\"") ||
      !Contains(run_record, "\"max_frame_bytes\":13") ||
      !std::filesystem::is_regular_file(run / "frames/000002_rx.meta.json") ||
      !Contains(events, "\"succeeded\":true,\"diagnostic_id\":\"\"") ||
      !EventOffsetsAreNondecreasing(events) ||
      !Contains(run_record,
                "\"received_from\":\"127.0.0.1:" + std::to_string(peer.Port()) + "\"")) {
    return false;
  }

  if (!ReplayAndCheck(run, root / "replay", "DECODE_RX", "RX", "DECODE_RX", 0, true, "EQUAL")) {
    std::cerr << "loopback replay chain validation failed\n";
    return false;
  }
  if (!TestVersionHandling(run, root / "version-handling")) {
    std::cerr << "loopback version validation failed\n";
    return false;
  }
  if (!TestRunRecordValidation(run, root / "record-validation")) {
    std::cerr << "loopback Run Record validation failed\n";
    return false;
  }
  if (!TestEventBindingValidation(run, root / "event-validation")) {
    std::cerr << "loopback Event binding validation failed\n";
    return false;
  }
  return true;
}

bool TestRealTimeout(const std::filesystem::path& root) {
  LoopbackPeer peer{{}};
  if (!peer.Start()) {
    return false;
  }
  pae::protocol_lab::StandardRecordFileSystem file_system;
  std::unique_ptr<IUdpExchangeAdapter> adapter =
      pae::protocol_lab::CreatePlatformUdpExchangeAdapter();
  std::string output;
  auto arguments = UdpArguments(root, "127.0.0.1:" + std::to_string(peer.Port()), true);
  arguments.insert(arguments.end(), {"--timeout-ms", "50"});
  const int exit_code = RunCaptured(std::move(arguments), file_system, *adapter, output);
  peer.Join();
  const std::filesystem::path run = OnlyCompletedRun(root);
  std::string events;
  return exit_code == 9 && peer.Error().empty() &&
         Contains(output, "\"id\":\"UDP_RESPONSE_TIMEOUT\"") && !run.empty() &&
         LoadTextFile(run / "events_v0.2.jsonl", events) &&
         !Contains(events, "UDP_RESPONSE_TIMEOUT") &&
         ReplayAndCheck(run, root / "replay", "ENCODE_TX", "TX", "ENCODE_TX", 0, false, "EQUAL");
}

bool TestTxAndRxRecordOrdering(const std::filesystem::path& root,
                               const std::vector<std::uint8_t>& response) {
  FakeUdpAdapter adapter;
  adapter.record_root = root / "success";
  adapter.response.status = UdpExchangeStatus::OK;
  adapter.response.send_attempted = true;
  adapter.response.send_succeeded = true;
  adapter.response.response_received = true;
  adapter.response.payload = response;
  adapter.response.peer = UdpEndpoint{{127U, 0U, 0U, 1U}, 23456U};
  pae::protocol_lab::StandardRecordFileSystem file_system;
  std::string output;
  const int exit_code = RunCaptured(UdpArguments(adapter.record_root, "127.0.0.1:23456", true),
                                    file_system, adapter, output);
  if (exit_code != 0 || !adapter.tx_persisted_before_exchange) {
    return false;
  }
  const std::filesystem::path successful_run = OnlyCompletedRun(adapter.record_root);
  if (successful_run.empty() ||
      !RejectsSelfConsistentButMismatchedRxMetadata(successful_run,
                                                    root / "rx-meta-binding-mismatch") ||
      !RejectsMissingRxMetadata(successful_run, root / "rx-meta-missing")) {
    return false;
  }

  FakeUdpAdapter tx_blocked_adapter;
  FrameFailingFileSystem tx_failure{"000001_tx.bin.tmp"};
  std::string tx_failure_output;
  if (RunCaptured(UdpArguments(root / "tx-failure", "127.0.0.1:23456", true), tx_failure,
                  tx_blocked_adapter, tx_failure_output) != 7 ||
      tx_blocked_adapter.call_count != 0) {
    return false;
  }

  FakeUdpAdapter rx_adapter;
  rx_adapter.response = adapter.response;
  FrameFailingFileSystem rx_failure{"000002_rx.bin.tmp"};
  std::string rx_failure_output;
  ExecutionObserver rx_observer;
  if (RunCaptured(UdpArguments(root / "rx-failure", "127.0.0.1:23456", true), rx_failure,
                  rx_adapter, rx_failure_output, &rx_observer) != 7 ||
      rx_adapter.call_count != 1 ||
      rx_observer.operations != std::vector<std::string>{"ENCODE_TX"} ||
      !OnlyCompletedRun(root / "rx-failure").empty() || !HasInProgressRun(root / "rx-failure")) {
    return false;
  }

  const std::vector<FrameFailingFileSystem::Mode> metadata_failures{
      FrameFailingFileSystem::Mode::WRITE, FrameFailingFileSystem::Mode::CLOSE,
      FrameFailingFileSystem::Mode::REREAD, FrameFailingFileSystem::Mode::MISMATCH};
  for (std::size_t index = 0U; index < metadata_failures.size(); ++index) {
    FakeUdpAdapter metadata_adapter;
    metadata_adapter.response = adapter.response;
    FrameFailingFileSystem metadata_failure{"000002_rx.meta.json.tmp", metadata_failures[index]};
    ExecutionObserver metadata_observer;
    std::string metadata_output;
    const std::filesystem::path case_root = root / ("rx-meta-failure-" + std::to_string(index));
    if (RunCaptured(UdpArguments(case_root, "127.0.0.1:23456", true), metadata_failure,
                    metadata_adapter, metadata_output, &metadata_observer) != 7 ||
        metadata_adapter.call_count != 1 ||
        metadata_observer.operations != std::vector<std::string>{"ENCODE_TX"} ||
        !OnlyCompletedRun(case_root).empty() || !HasInProgressRun(case_root)) {
      return false;
    }
  }
  return true;
}

bool TestFailuresAndSafety(const std::filesystem::path& root,
                           const std::vector<std::uint8_t>& response) {
  struct FailureCase {
    UdpExchangeStatus status;
    std::string diagnostic;
    bool send_attempted;
    bool send_succeeded;
  };
  const std::vector<FailureCase> failures{
      {UdpExchangeStatus::STARTUP_FAILED, "UDP_STARTUP_FAILED", false, false},
      {UdpExchangeStatus::SOCKET_CREATE_FAILED, "UDP_SOCKET_CREATE_FAILED", false, false},
      {UdpExchangeStatus::BIND_FAILED, "UDP_BIND_FAILED", false, false},
      {UdpExchangeStatus::SEND_FAILED, "UDP_SEND_FAILED", true, false},
      {UdpExchangeStatus::RECEIVE_FAILED, "UDP_RECEIVE_FAILED", true, true},
      {UdpExchangeStatus::DATAGRAM_TRUNCATED, "UDP_DATAGRAM_TRUNCATED", true, true},
      {UdpExchangeStatus::PLATFORM_UNSUPPORTED, "UDP_PLATFORM_UNSUPPORTED", false, false},
  };
  pae::protocol_lab::StandardRecordFileSystem file_system;
  for (std::size_t index = 0U; index < failures.size(); ++index) {
    FakeUdpAdapter adapter;
    adapter.response.status = failures[index].status;
    adapter.response.detail = "injected transport failure";
    adapter.response.send_attempted = failures[index].send_attempted;
    adapter.response.send_succeeded = failures[index].send_succeeded;
    std::string output;
    const std::filesystem::path case_root = root / ("transport-" + std::to_string(index));
    if (RunCaptured(UdpArguments(case_root, "127.0.0.1:23456", true), file_system, adapter,
                    output) != 8 ||
        !Contains(output, failures[index].diagnostic)) {
      return false;
    }
    const std::filesystem::path run = OnlyCompletedRun(case_root);
    std::string events;
    const bool should_have_send_events = failures[index].send_attempted;
    if (run.empty() || !LoadTextFile(run / "events_v0.2.jsonl", events) ||
        Contains(events, "\"event_kind\":\"SEND_INTENT\"") != should_have_send_events ||
        Contains(events, "\"event_kind\":\"SEND_RESULT\"") != should_have_send_events ||
        (failures[index].send_succeeded && Contains(events, failures[index].diagnostic)) ||
        (!failures[index].send_succeeded && failures[index].send_attempted &&
         !Contains(events, "UDP_SEND_FAILED"))) {
      return false;
    }
    const bool no_codec = failures[index].status == UdpExchangeStatus::DATAGRAM_TRUNCATED;
    if (!ReplayAndCheck(run, case_root / "replay", no_codec ? "NO_CODEC_REEXECUTION" : "ENCODE_TX",
                        no_codec ? "RX_INCOMPLETE" : "TX", no_codec ? "" : "ENCODE_TX", 0, false,
                        no_codec ? "NOT_EVALUATED" : "EQUAL")) {
      return false;
    }
  }

  FakeUdpAdapter mismatch;
  mismatch.response.status = UdpExchangeStatus::OK;
  mismatch.response.send_attempted = true;
  mismatch.response.send_succeeded = true;
  mismatch.response.response_received = true;
  mismatch.response.payload = response;
  mismatch.response.peer = UdpEndpoint{{127U, 0U, 0U, 1U}, 23457U};
  std::string output;
  const std::filesystem::path mismatch_root = root / "peer-mismatch";
  if (RunCaptured(UdpArguments(mismatch_root, "127.0.0.1:23456", true), file_system, mismatch,
                  output) != 8 ||
      !Contains(output, "UDP_PEER_MISMATCH") ||
      !std::filesystem::is_regular_file(OnlyCompletedRun(mismatch_root) / "frames/000002_rx.bin") ||
      !std::filesystem::is_regular_file(OnlyCompletedRun(mismatch_root) /
                                        "frames/000002_rx.meta.json")) {
    return false;
  }
  std::string mismatch_events;
  if (!LoadTextFile(OnlyCompletedRun(mismatch_root) / "events_v0.2.jsonl", mismatch_events) ||
      Contains(mismatch_events, "UDP_PEER_MISMATCH") ||
      !ReplayAndCheck(OnlyCompletedRun(mismatch_root), mismatch_root / "replay",
                      "NO_CODEC_REEXECUTION", "RX", "", 0, false, "NOT_EVALUATED")) {
    return false;
  }

  FakeUdpAdapter oversized;
  oversized.response = mismatch.response;
  oversized.response.peer.port = 23456U;
  oversized.response.payload.assign(pae::protocol_lab::kMaximumIpv4UdpPayloadBytes + 1U, 0xA5U);
  output.clear();
  if (RunCaptured(UdpArguments(root / "oversized", "127.0.0.1:23456", true), file_system, oversized,
                  output) != 8 ||
      !Contains(output, "UDP_DATAGRAM_TOO_LARGE") ||
      !ReplayAndCheck(OnlyCompletedRun(root / "oversized"), root / "oversized/replay",
                      "NO_CODEC_REEXECUTION", "RX", "", 0, false, "NOT_EVALUATED")) {
    return false;
  }

  FakeUdpAdapter malformed;
  malformed.response = mismatch.response;
  malformed.response.peer.port = 23456U;
  malformed.response.payload = response;
  malformed.response.payload[2] ^= 0x01U;
  output.clear();
  const std::filesystem::path malformed_root = root / "malformed-response";
  if (RunCaptured(UdpArguments(malformed_root, "127.0.0.1:23456", true), file_system, malformed,
                  output) != 5 ||
      !Contains(output, "PAE_LAB_CODEC_UNKNOWN_MESSAGE") ||
      !Contains(output, "\"response_received\":true") ||
      !Contains(output, "\"response_decoded\":false") ||
      !std::filesystem::is_regular_file(OnlyCompletedRun(malformed_root) /
                                        "frames/000002_rx.bin")) {
    return false;
  }
  std::string malformed_events;
  if (!LoadTextFile(OnlyCompletedRun(malformed_root) / "events_v0.2.jsonl", malformed_events) ||
      Contains(malformed_events, "PAE_LAB_CODEC_UNKNOWN_MESSAGE") ||
      !ReplayAndCheck(OnlyCompletedRun(malformed_root), malformed_root / "replay", "DECODE_RX",
                      "RX", "DECODE_RX", 5, false, "EQUAL")) {
    return false;
  }

  FakeUdpAdapter rejected;
  output.clear();
  if (RunCaptured(UdpArguments(root / "non-loopback", "192.0.2.1:12345", true), file_system,
                  rejected, output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_NON_LOOPBACK_CONFIRMATION_REQUIRED")) {
    return false;
  }
  output.clear();
  if (RunCaptured(UdpArguments(root / "non-loopback-preview", "192.0.2.1:12345", false),
                  file_system, rejected, output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_NON_LOOPBACK_CONFIRMATION_REQUIRED")) {
    return false;
  }
  FakeUdpAdapter confirmed_non_loopback;
  confirmed_non_loopback.response.status = UdpExchangeStatus::TIMEOUT;
  confirmed_non_loopback.response.send_attempted = true;
  confirmed_non_loopback.response.send_succeeded = true;
  confirmed_non_loopback.response.detail = "injected timeout after confirmed non-Loopback";
  output.clear();
  auto confirmed = UdpArguments(root / "confirmed-non-loopback", "192.0.2.1:12345", true);
  confirmed.emplace_back("--allow-non-loopback");
  if (RunCaptured(std::move(confirmed), file_system, confirmed_non_loopback, output) != 9 ||
      confirmed_non_loopback.call_count != 1 || !Contains(output, "UDP_RESPONSE_TIMEOUT")) {
    return false;
  }
  output.clear();
  auto non_loopback_local = UdpArguments(root / "non-loopback-local", "127.0.0.1:23456", true);
  non_loopback_local.insert(non_loopback_local.end(), {"--local", "192.0.2.2:0"});
  if (RunCaptured(std::move(non_loopback_local), file_system, rejected, output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_NON_LOOPBACK_CONFIRMATION_REQUIRED")) {
    return false;
  }
  output.clear();
  auto multicast = UdpArguments(root / "multicast", "239.1.2.3:12345", true);
  multicast.emplace_back("--allow-non-loopback");
  if (RunCaptured(std::move(multicast), file_system, rejected, output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_UDP_REMOTE_FORBIDDEN")) {
    return false;
  }
  output.clear();
  auto directed_broadcast = UdpArguments(root / "directed-broadcast", "192.0.2.255:12345", true);
  directed_broadcast.emplace_back("--allow-non-loopback");
  if (RunCaptured(std::move(directed_broadcast), file_system, rejected, output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_UDP_REMOTE_FORBIDDEN")) {
    return false;
  }
  output.clear();
  auto limited_broadcast = UdpArguments(root / "limited-broadcast", "255.255.255.255:12345", true);
  limited_broadcast.emplace_back("--allow-non-loopback");
  if (RunCaptured(std::move(limited_broadcast), file_system, rejected, output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_UDP_REMOTE_FORBIDDEN")) {
    return false;
  }
  output.clear();
  if (RunCaptured(UdpArguments(root / "unspecified", "0.0.0.0:12345", true), file_system, rejected,
                  output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_UDP_REMOTE_FORBIDDEN")) {
    return false;
  }
  output.clear();
  if (RunCaptured(UdpArguments(root / "dns-forbidden", "localhost:12345", true), file_system,
                  rejected, output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_UDP_ENDPOINT_INVALID")) {
    return false;
  }
  output.clear();
  if (RunCaptured(UdpArguments(root / "zero-remote-port", "127.0.0.1:0", true), file_system,
                  rejected, output) != 2 ||
      rejected.call_count != 0 || !Contains(output, "PAE_LAB_UDP_ENDPOINT_INVALID")) {
    return false;
  }
  output.clear();
  auto invalid_timeout = UdpArguments(root / "invalid-timeout", "127.0.0.1:23456", true);
  invalid_timeout.insert(invalid_timeout.end(), {"--timeout-ms", "0"});
  if (RunCaptured(std::move(invalid_timeout), file_system, rejected, output) != 2 ||
      rejected.call_count != 0) {
    return false;
  }
  output.clear();
  auto unknown_pipeline = UdpArguments(root / "unknown-pipeline", "127.0.0.1:23456", true);
  const auto pipeline =
      std::find(unknown_pipeline.begin(), unknown_pipeline.end(), "fixture_to_host");
  if (pipeline == unknown_pipeline.end()) {
    return false;
  }
  *pipeline = "missing_pipeline";
  return RunCaptured(std::move(unknown_pipeline), file_system, rejected, output) == 2 &&
         rejected.call_count == 0 && Contains(output, "PAE_LAB_UNKNOWN_RECEIVE_PIPELINE");
}

bool TestAdapterRejectsOversizedRequest() {
  std::unique_ptr<IUdpExchangeAdapter> adapter =
      pae::protocol_lab::CreatePlatformUdpExchangeAdapter();
  UdpExchangeRequest request;
  request.local = UdpEndpoint{{127U, 0U, 0U, 1U}, 0U};
  request.remote = UdpEndpoint{{192U, 0U, 2U, 1U}, 12345U};
  request.payload.assign(pae::protocol_lab::kMaximumIpv4UdpPayloadBytes + 1U, 0xA5U);
  const UdpExchangeResponse response = adapter->Exchange(request);
  return response.status == UdpExchangeStatus::DATAGRAM_TOO_LARGE && !response.send_attempted &&
         !response.send_succeeded && !response.response_received;
}

bool TestZeroLengthResponse(const std::filesystem::path& root) {
  FakeUdpAdapter adapter;
  adapter.response.status = UdpExchangeStatus::OK;
  adapter.response.send_attempted = true;
  adapter.response.send_succeeded = true;
  adapter.response.response_received = true;
  adapter.response.peer = UdpEndpoint{{127U, 0U, 0U, 1U}, 23456U};
  pae::protocol_lab::StandardRecordFileSystem file_system;
  ExecutionObserver observer;
  std::string output;
  const int exit_code = RunCaptured(UdpArguments(root, "127.0.0.1:23456", true), file_system,
                                    adapter, output, &observer);
  const std::filesystem::path run = OnlyCompletedRun(root);
  return exit_code == 5 &&
         observer.operations == std::vector<std::string>({"ENCODE_TX", "DECODE_RX"}) &&
         !run.empty() && Contains(output, "\"response_received\":true") &&
         Contains(output, "\"response_decoded\":false") &&
         std::filesystem::file_size(run / "frames/000002_rx.bin") == 0U &&
         std::filesystem::is_regular_file(run / "frames/000002_rx.meta.json") &&
         ReplayAndCheck(run, root / "replay", "DECODE_RX", "RX", "DECODE_RX", 5, false, "EQUAL");
}

}  // namespace

int main() {
  std::error_code error;
  const std::filesystem::path root{"udp-exchange-runs"};
  std::filesystem::remove_all(root, error);

  std::vector<std::uint8_t> request;
  std::vector<std::uint8_t> response;
  if (!LoadHexFile("data/lab_command_001.frame.hex", request) ||
      !LoadHexFile("data/lab_report_001.frame.hex", response)) {
    std::cerr << "could not load UDP test vectors\n";
    return 1;
  }

  const auto run_case = [](std::string_view name, bool result) {
    if (!result) {
      std::cerr << "failed case: " << name << '\n';
    }
    return result;
  };
  bool passed = true;
  passed =
      run_case("legacy-offline-v0.1", TestLegacyOfflineV1Compatibility(root / "legacy-v0.1")) &&
      passed;
  passed = run_case("preview-zero-socket", TestPreviewUsesNoSocket(root / "preview")) && passed;
  passed =
      run_case("real-loopback", TestRealLoopback(root / "loopback", request, response)) && passed;
  passed = run_case("real-timeout", TestRealTimeout(root / "timeout")) && passed;
  passed = run_case("record-order", TestTxAndRxRecordOrdering(root / "record-order", response)) &&
           passed;
  passed =
      run_case("failures-and-safety", TestFailuresAndSafety(root / "failures", response)) && passed;
  passed = run_case("adapter-oversized-request", TestAdapterRejectsOversizedRequest()) && passed;
  passed = run_case("zero-length-response", TestZeroLengthResponse(root / "zero-length")) && passed;
  if (!passed) {
    std::cerr << "Protocol Lab UDP Exchange tests failed\n";
  }
  return passed ? 0 : 1;
}
