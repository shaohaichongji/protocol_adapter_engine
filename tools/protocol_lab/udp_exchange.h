#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "lab_types.h"

namespace pae::protocol_lab {

inline constexpr std::size_t kMaximumIpv4UdpPayloadBytes = 65507U;

struct UdpEndpoint {
  std::array<std::uint8_t, 4U> address{};
  std::uint16_t port = 0U;

  [[nodiscard]] bool IsLoopback() const noexcept { return address[0] == 127U; }
  [[nodiscard]] bool IsUnspecified() const noexcept {
    return address[0] == 0U && address[1] == 0U && address[2] == 0U && address[3] == 0U;
  }
  [[nodiscard]] bool IsMulticast() const noexcept {
    return address[0] >= 224U && address[0] <= 239U;
  }
  [[nodiscard]] bool IsBroadcast() const noexcept { return address[3] == 255U; }
};

bool ParseUdpEndpoint(std::string_view text, bool allow_zero_port, UdpEndpoint& output,
                      std::string& error);
std::string FormatUdpEndpoint(const UdpEndpoint& endpoint);
bool SameUdpEndpoint(const UdpEndpoint& left, const UdpEndpoint& right) noexcept;

enum class UdpExchangeStatus {
  OK,
  STARTUP_FAILED,
  SOCKET_CREATE_FAILED,
  BIND_FAILED,
  SEND_FAILED,
  RECEIVE_FAILED,
  TIMEOUT,
  DATAGRAM_TOO_LARGE,
  DATAGRAM_TRUNCATED,
  PLATFORM_UNSUPPORTED
};

struct UdpExchangeRequest {
  UdpEndpoint local;
  UdpEndpoint remote;
  std::uint32_t timeout_ms = kDefaultUdpTimeoutMs;
  std::vector<std::uint8_t> payload;
  class IUdpExchangeEventObserver* event_observer = nullptr;
};

class IUdpExchangeEventObserver {
 public:
  virtual ~IUdpExchangeEventObserver() = default;
  virtual void OnSendIntent() = 0;
  virtual void OnSendResult(bool succeeded, std::string_view diagnostic_id) = 0;
};

struct UdpExchangeResponse {
  UdpExchangeStatus status = UdpExchangeStatus::PLATFORM_UNSUPPORTED;
  bool send_attempted = false;
  bool send_succeeded = false;
  bool response_received = false;
  bool local_bound = false;
  std::vector<std::uint8_t> payload;
  UdpEndpoint bound_local;
  UdpEndpoint peer;
  std::string detail;
};

class IUdpExchangeAdapter {
 public:
  virtual ~IUdpExchangeAdapter() = default;
  virtual UdpExchangeResponse Exchange(const UdpExchangeRequest& request) = 0;
};

std::unique_ptr<IUdpExchangeAdapter> CreatePlatformUdpExchangeAdapter();
std::string UdpDiagnosticId(UdpExchangeStatus status);

}  // namespace pae::protocol_lab
