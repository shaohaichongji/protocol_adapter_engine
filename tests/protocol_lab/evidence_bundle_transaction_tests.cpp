#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "evidence_bundle.h"
#include "protocol_lab.h"

namespace {

enum class FailurePoint {
  RUN_DIRECTORY_CREATE,
  NTH_FILE_WRITE,
  FILE_CLOSE,
  FILE_REREAD,
  VERIFICATION_MISMATCH,
  COMPLETE_WRITE,
  MANIFEST_WRITE,
  FINAL_RENAME
};

class InjectingFileSystem final : public pae::protocol_lab::RecordFileSystem {
 public:
  explicit InjectingFileSystem(FailurePoint failure_point) : failure_point_(failure_point) {}

  bool EnsureDirectory(const std::filesystem::path& path, std::string& error) override {
    return standard_.EnsureDirectory(path, error);
  }

  bool Exists(const std::filesystem::path& path, bool& exists, std::string& error) override {
    return standard_.Exists(path, exists, error);
  }

  bool CreateDirectory(const std::filesystem::path& path, std::string& error) override {
    const bool created = standard_.CreateDirectory(path, error);
    if (created && failure_point_ == FailurePoint::RUN_DIRECTORY_CREATE) {
      error = "injected Run directory creation acknowledgement failure";
      return false;
    }
    return created;
  }

  bool CreateDirectories(const std::filesystem::path& path, std::string& error) override {
    return standard_.CreateDirectories(path, error);
  }

  bool WriteClosedFile(const std::filesystem::path& path, const std::uint8_t* data,
                       std::size_t size, std::string& error) override {
    ++write_count_;
    if (failure_point_ == FailurePoint::NTH_FILE_WRITE && write_count_ == 3U) {
      error = "injected Nth record file write failure";
      return false;
    }
    const std::string filename = path.filename().generic_string();
    if (failure_point_ == FailurePoint::COMPLETE_WRITE && filename == "COMPLETE.tmp") {
      error = "injected COMPLETE write failure";
      return false;
    }
    if (failure_point_ == FailurePoint::MANIFEST_WRITE && filename == "SHA256SUMS.tmp") {
      error = "injected SHA256SUMS write failure";
      return false;
    }
    const bool written = standard_.WriteClosedFile(path, data, size, error);
    if (written && failure_point_ == FailurePoint::FILE_CLOSE && write_count_ == 1U) {
      error = "injected record file close acknowledgement failure";
      return false;
    }
    return written;
  }

  bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                std::string& error) override {
    ++read_count_;
    if (failure_point_ == FailurePoint::FILE_REREAD && read_count_ == 1U) {
      error = "injected record file reread failure";
      return false;
    }
    if (!standard_.ReadFile(path, output, error)) {
      return false;
    }
    if (failure_point_ == FailurePoint::VERIFICATION_MISMATCH && read_count_ == 1U) {
      output.push_back(0xA5U);
    }
    return true;
  }

  bool Rename(const std::filesystem::path& from, const std::filesystem::path& to,
              std::string& error) override {
    if (failure_point_ == FailurePoint::FINAL_RENAME && from.extension() == ".inprogress") {
      error = "injected final Run rename failure";
      return false;
    }
    return standard_.Rename(from, to, error);
  }

 private:
  FailurePoint failure_point_;
  std::size_t write_count_ = 0U;
  std::size_t read_count_ = 0U;
  pae::protocol_lab::StandardRecordFileSystem standard_;
};

struct DirectoryCounts {
  std::size_t in_progress = 0U;
  std::size_t completed = 0U;
};

DirectoryCounts CountRunDirectories(const std::filesystem::path& root) {
  DirectoryCounts counts;
  std::error_code error;
  for (std::filesystem::directory_iterator iterator{root, error}, end; iterator != end && !error;
       iterator.increment(error)) {
    if (!iterator->is_directory(error) || error) {
      continue;
    }
    const std::string name = iterator->path().filename().generic_string();
    if (name.size() >= 11U && name.substr(name.size() - 11U) == ".inprogress") {
      ++counts.in_progress;
    } else if (name.rfind("run_", 0U) == 0U) {
      ++counts.completed;
    }
  }
  return counts;
}

int RunCaptured(std::vector<std::string>& arguments,
                pae::protocol_lab::RecordFileSystem& file_system, std::string& output) {
  std::vector<char*> argv;
  argv.reserve(arguments.size());
  for (std::string& argument : arguments) {
    argv.push_back(argument.data());
  }

  std::ostringstream captured;
  std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
  const int exit_code = pae::protocol_lab::RunApplicationWithFileSystem(
      static_cast<int>(argv.size()), argv.data(), file_system);
  std::cout.rdbuf(previous);
  output = captured.str();
  return exit_code;
}

std::string ExtractFingerprint(std::string_view output) {
  constexpr std::string_view kPrefix = "\"deterministic_fingerprint\":\"";
  const std::size_t start = output.find(kPrefix);
  if (start == std::string_view::npos) {
    return {};
  }
  const std::size_t value_start = start + kPrefix.size();
  const std::size_t end = output.find('"', value_start);
  if (end == std::string_view::npos) {
    return {};
  }
  return std::string{output.substr(value_start, end - value_start)};
}

std::string GetSuccessfulFingerprint() {
  std::vector<std::string> arguments{
      "pae_protocol_lab", "inspect",
      "--config",         "data/synthetic_lab_exchange_slice.pae.json",
      "--frame-hex",      "data/lab_command_001.frame.hex",
      "--output",         "json"};
  pae::protocol_lab::StandardRecordFileSystem file_system;
  std::string output;
  if (RunCaptured(arguments, file_system, output) != 0) {
    return {};
  }
  return ExtractFingerprint(output);
}

bool RunFailureCase(FailurePoint failure_point, std::string_view case_name,
                    std::string_view successful_fingerprint) {
  const std::filesystem::path root = std::filesystem::path{"transaction-runs"} / case_name;
  std::error_code cleanup_error;
  std::filesystem::remove_all(root, cleanup_error);

  std::vector<std::string> arguments{
      "pae_protocol_lab", "inspect",
      "--config",         "data/synthetic_lab_exchange_slice.pae.json",
      "--frame-hex",      "data/lab_command_001.frame.hex",
      "--record-root",    root.generic_string(),
      "--output",         "json"};

  InjectingFileSystem file_system{failure_point};
  std::string output;
  const int exit_code = RunCaptured(arguments, file_system, output);
  const std::string failure_fingerprint = ExtractFingerprint(output);
  const DirectoryCounts counts = CountRunDirectories(root);
  if (exit_code != pae::protocol_lab::kRecordFailedExitCode || counts.completed != 0U ||
      counts.in_progress != 1U ||
      output.find("\"operation_status\":\"RECORD_FAILED\"") == std::string::npos ||
      output.find("\"id\":\"PAE_LAB_RECORD_FAILED\"") == std::string::npos ||
      failure_fingerprint.empty() || failure_fingerprint == successful_fingerprint) {
    std::cerr << "failure case=" << case_name << " exit=" << exit_code
              << " inprogress=" << counts.in_progress << " completed=" << counts.completed << '\n';
    return false;
  }
  return true;
}

}  // namespace

int main() {
  const std::string successful_fingerprint = GetSuccessfulFingerprint();
  if (successful_fingerprint.empty()) {
    std::cerr << "could not obtain successful-operation fingerprint\n";
    return 1;
  }
  bool passed = true;
  passed = RunFailureCase(FailurePoint::RUN_DIRECTORY_CREATE, "run-directory-create",
                          successful_fingerprint) &&
           passed;
  passed = RunFailureCase(FailurePoint::NTH_FILE_WRITE, "nth-file-write", successful_fingerprint) &&
           passed;
  passed = RunFailureCase(FailurePoint::FILE_CLOSE, "file-close", successful_fingerprint) && passed;
  passed =
      RunFailureCase(FailurePoint::FILE_REREAD, "file-reread", successful_fingerprint) && passed;
  passed = RunFailureCase(FailurePoint::VERIFICATION_MISMATCH, "verification-mismatch",
                          successful_fingerprint) &&
           passed;
  passed = RunFailureCase(FailurePoint::COMPLETE_WRITE, "complete-write", successful_fingerprint) &&
           passed;
  passed = RunFailureCase(FailurePoint::MANIFEST_WRITE, "manifest-write", successful_fingerprint) &&
           passed;
  passed =
      RunFailureCase(FailurePoint::FINAL_RENAME, "final-rename", successful_fingerprint) && passed;
  return passed ? 0 : 1;
}
