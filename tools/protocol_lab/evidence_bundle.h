#pragma once

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

bool CreateEvidenceBundle(const std::filesystem::path& record_root, std::string_view config,
                          const std::optional<std::string>& values, OperationResult& result,
                          RecordFileSystem& file_system, std::string& error);
bool LoadStoredRun(const std::filesystem::path& bundle, StoredRun& output, std::string& error);

}  // namespace pae::protocol_lab
