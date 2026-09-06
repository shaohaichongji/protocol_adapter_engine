#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lab_types.h"

namespace pae::protocol_lab {

class RecordFileSystem {
 public:
  virtual ~RecordFileSystem() = default;

  virtual bool EnsureDirectory(const std::filesystem::path& path, std::string& error) = 0;
  virtual bool Exists(const std::filesystem::path& path, bool& exists, std::string& error) = 0;
  virtual bool CreateDirectory(const std::filesystem::path& path, std::string& error) = 0;
  virtual bool CreateDirectories(const std::filesystem::path& path, std::string& error) = 0;
  virtual bool WriteClosedFile(const std::filesystem::path& path, const std::uint8_t* data,
                               std::size_t size, std::string& error) = 0;
  virtual bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                        std::string& error) = 0;
  virtual bool Rename(const std::filesystem::path& from, const std::filesystem::path& to,
                      std::string& error) = 0;
};

class StandardRecordFileSystem final : public RecordFileSystem {
 public:
  bool EnsureDirectory(const std::filesystem::path& path, std::string& error) override;
  bool Exists(const std::filesystem::path& path, bool& exists, std::string& error) override;
  bool CreateDirectory(const std::filesystem::path& path, std::string& error) override;
  bool CreateDirectories(const std::filesystem::path& path, std::string& error) override;
  bool WriteClosedFile(const std::filesystem::path& path, const std::uint8_t* data,
                       std::size_t size, std::string& error) override;
  bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                std::string& error) override;
  bool Rename(const std::filesystem::path& from, const std::filesystem::path& to,
              std::string& error) override;
};

class EvidenceBundleTransaction {
 public:
  explicit EvidenceBundleTransaction(RecordFileSystem& file_system);

  EvidenceBundleTransaction(const EvidenceBundleTransaction&) = delete;
  EvidenceBundleTransaction& operator=(const EvidenceBundleTransaction&) = delete;

  bool Begin(const std::filesystem::path& record_root, std::string_view config,
             const std::optional<std::string>& values, OperationResult& result, std::string& error);
  bool RecordFrame(const std::filesystem::path& relative_stem,
                   const std::vector<std::uint8_t>& frame, std::string& error);
  bool RecordReceivedFrame(const std::filesystem::path& relative_stem,
                           const std::vector<std::uint8_t>& frame, std::string_view received_from,
                           std::string& error);
  void CaptureTxFrame(const std::vector<std::uint8_t>& frame);
  void CaptureOfflineFrame(const OperationResult& result);
  void CaptureSendIntent(std::string_view remote_endpoint);
  void CaptureSendResult(bool succeeded, std::string_view diagnostic_id);
  bool Complete(OperationResult& result, std::string& error);
  [[nodiscard]] bool Begun() const noexcept { return begun_; }

 private:
  LabEvent NewEvent(std::string_view event_kind, std::string_view direction);
  RecordFileSystem& file_system_;
  std::filesystem::path in_progress_;
  std::filesystem::path final_;
  std::string run_id_;
  std::string timestamp_;
  std::chrono::steady_clock::time_point monotonic_start_;
  std::vector<RecordedFile> files_;
  std::vector<LabEvent> events_;
  bool begun_ = false;
  bool completed_ = false;
};

bool CreateEvidenceBundle(const std::filesystem::path& record_root, std::string_view config,
                          const std::optional<std::string>& values, OperationResult& result,
                          RecordFileSystem& file_system, std::string& error);
bool LoadStoredRun(const std::filesystem::path& bundle, StoredRun& output, std::string& error);

}  // namespace pae::protocol_lab
