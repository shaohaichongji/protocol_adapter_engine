#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "v06_format.h"

namespace pae::protocol_lab::v06 {

inline constexpr std::string_view kRecordFormat = "pae.lab.record/0.6";
inline constexpr std::string_view kEventFormat = "pae.lab.event/0.6";

class EvidenceFileSystem {
 public:
  virtual ~EvidenceFileSystem() = default;
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

class StandardEvidenceFileSystem final : public EvidenceFileSystem {
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

struct BundleInput {
  std::string config_text;
  std::optional<std::string> values_text;
  std::optional<std::vector<std::uint8_t>> frame;
  std::optional<std::vector<std::uint8_t>> tx_frame;
  std::optional<std::vector<std::uint8_t>> rx_frame;
  std::optional<std::string> rx_received_from;
  Result result;
};

struct StoredBundle {
  std::string config_text;
  std::optional<std::string> values_text;
  std::optional<std::vector<std::uint8_t>> frame;
  std::optional<std::vector<std::uint8_t>> tx_frame;
  std::optional<std::vector<std::uint8_t>> rx_frame;
  std::optional<std::string> rx_received_from;
  Result result;
  std::string deterministic_fingerprint;
};

bool WriteEvidenceBundle(const std::filesystem::path& record_root, std::string_view run_name,
                         const BundleInput& input, EvidenceFileSystem& file_system,
                         std::filesystem::path& published_path, std::string& error);
bool LoadEvidenceBundle(const std::filesystem::path& bundle, StoredBundle& output,
                        std::string& error);
// Internal test seam for observing Reader I/O. Production callers use the overload above.
bool LoadEvidenceBundleForTest(const std::filesystem::path& bundle, StoredBundle& output,
                               EvidenceFileSystem& file_system, std::string& error);

}  // namespace pae::protocol_lab::v06
