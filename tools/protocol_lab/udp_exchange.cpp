#include "udp_exchange.h"

#include <charconv>
#include <cstddef>
#include <limits>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace pae::protocol_lab {
namespace {

bool ParseUnsigned(std::string_view text, unsigned int maximum, unsigned int& output) {
  if (text.empty()) {
    return false;
  }
  unsigned int parsed = 0U;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed > maximum) {
    return false;
  }
  output = parsed;
  return true;
}

#if defined(_WIN32)

class WinsockSession {
 public:
  WinsockSession() {
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    error_ = result;
    ready_ = result == 0 && LOBYTE(data.wVersion) == 2 && HIBYTE(data.wVersion) == 2;
    if (result == 0 && !ready_) {
      error_ = WSAVERNOTSUPPORTED;
      WSACleanup();
    }
  }

  ~WinsockSession() {
    if (ready_) {
      WSACleanup();
    }
  }

  WinsockSession(const WinsockSession&) = delete;
  WinsockSession& operator=(const WinsockSession&) = delete;

  [[nodiscard]] bool Ready() const noexcept { return ready_; }
  [[nodiscard]] int Error() const noexcept { return error_; }

 private:
  bool ready_ = false;
  int error_ = 0;
};

class SocketHandle {
 public:
  explicit SocketHandle(SOCKET value) : value_(value) {}
  ~SocketHandle() {
    if (value_ != INVALID_SOCKET) {
      closesocket(value_);
    }
  }

  SocketHandle(const SocketHandle&) = delete;
  SocketHandle& operator=(const SocketHandle&) = delete;

  [[nodiscard]] SOCKET Get() const noexcept { return value_; }

 private:
  SOCKET value_ = INVALID_SOCKET;
};

sockaddr_in ToSockaddr(const UdpEndpoint& endpoint) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(endpoint.port);
  const std::uint32_t host_address = (static_cast<std::uint32_t>(endpoint.address[0]) << 24U) |
                                     (static_cast<std::uint32_t>(endpoint.address[1]) << 16U) |
                                     (static_cast<std::uint32_t>(endpoint.address[2]) << 8U) |
                                     static_cast<std::uint32_t>(endpoint.address[3]);
  address.sin_addr.s_addr = htonl(host_address);
  return address;
}

UdpEndpoint FromSockaddr(const sockaddr_in& address) {
  const std::uint32_t host_address = ntohl(address.sin_addr.s_addr);
  UdpEndpoint endpoint;
  endpoint.address = {
      static_cast<std::uint8_t>((host_address >> 24U) & 0xFFU),
      static_cast<std::uint8_t>((host_address >> 16U) & 0xFFU),
      static_cast<std::uint8_t>((host_address >> 8U) & 0xFFU),
      static_cast<std::uint8_t>(host_address & 0xFFU),
  };
  endpoint.port = ntohs(address.sin_port);
  return endpoint;
}

std::string WinsockDetail(std::string_view operation, int code) {
  return std::string{operation} + " failed with Winsock error " + std::to_string(code);
}

class WindowsUdpExchangeAdapter final : public IUdpExchangeAdapter {
 public:
  UdpExchangeResponse Exchange(const UdpExchangeRequest& request) override {
    UdpExchangeResponse response;
    if (request.payload.size() > kMaximumIpv4UdpPayloadBytes) {
      response.status = UdpExchangeStatus::DATAGRAM_TOO_LARGE;
      response.detail = "request exceeds the maximum IPv4 UDP payload";
      return response;
    }
    WinsockSession winsock;
    if (!winsock.Ready()) {
      response.status = UdpExchangeStatus::STARTUP_FAILED;
      response.detail = WinsockDetail("WSAStartup", winsock.Error());
      return response;
    }

    SocketHandle socket_handle{socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
    if (socket_handle.Get() == INVALID_SOCKET) {
      response.status = UdpExchangeStatus::SOCKET_CREATE_FAILED;
      response.detail = WinsockDetail("socket", WSAGetLastError());
      return response;
    }

    const sockaddr_in local = ToSockaddr(request.local);
    if (bind(socket_handle.Get(), reinterpret_cast<const sockaddr*>(&local), sizeof(local)) ==
        SOCKET_ERROR) {
      response.status = UdpExchangeStatus::BIND_FAILED;
      response.detail = WinsockDetail("bind", WSAGetLastError());
      return response;
    }

    sockaddr_in bound_local{};
    int bound_local_size = sizeof(bound_local);
    if (getsockname(socket_handle.Get(), reinterpret_cast<sockaddr*>(&bound_local),
                    &bound_local_size) == SOCKET_ERROR) {
      response.status = UdpExchangeStatus::BIND_FAILED;
      response.detail = WinsockDetail("getsockname", WSAGetLastError());
      return response;
    }
    response.bound_local = FromSockaddr(bound_local);
    response.local_bound = true;

    const sockaddr_in remote = ToSockaddr(request.remote);
    response.send_attempted = true;
    if (request.event_observer != nullptr) {
      request.event_observer->OnSendIntent();
    }
    const int sent =
        sendto(socket_handle.Get(), reinterpret_cast<const char*>(request.payload.data()),
               static_cast<int>(request.payload.size()), 0,
               reinterpret_cast<const sockaddr*>(&remote), sizeof(remote));
    if (sent == SOCKET_ERROR || static_cast<std::size_t>(sent) != request.payload.size()) {
      response.status = UdpExchangeStatus::SEND_FAILED;
      response.detail = sent == SOCKET_ERROR ? WinsockDetail("sendto", WSAGetLastError())
                                             : "sendto did not send one complete datagram";
      if (request.event_observer != nullptr) {
        request.event_observer->OnSendResult(false, "UDP_SEND_FAILED");
      }
      return response;
    }
    response.send_succeeded = true;
    if (request.event_observer != nullptr) {
      request.event_observer->OnSendResult(true, {});
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(socket_handle.Get(), &read_set);
    timeval timeout{};
    timeout.tv_sec = static_cast<long>(request.timeout_ms / 1000U);
    timeout.tv_usec = static_cast<long>((request.timeout_ms % 1000U) * 1000U);
    const int selected = select(0, &read_set, nullptr, nullptr, &timeout);
    if (selected == 0) {
      response.status = UdpExchangeStatus::TIMEOUT;
      response.detail = "timed out waiting for one UDP response";
      return response;
    }
    if (selected == SOCKET_ERROR) {
      response.status = UdpExchangeStatus::RECEIVE_FAILED;
      response.detail = WinsockDetail("select", WSAGetLastError());
      return response;
    }

    response.payload.resize(kMaximumIpv4UdpPayloadBytes);
    sockaddr_in peer{};
    int peer_size = sizeof(peer);
    const int received =
        recvfrom(socket_handle.Get(), reinterpret_cast<char*>(response.payload.data()),
                 static_cast<int>(response.payload.size()), 0, reinterpret_cast<sockaddr*>(&peer),
                 &peer_size);
    if (received == SOCKET_ERROR) {
      const int code = WSAGetLastError();
      response.payload.clear();
      response.status = code == WSAEMSGSIZE ? UdpExchangeStatus::DATAGRAM_TRUNCATED
                                            : UdpExchangeStatus::RECEIVE_FAILED;
      response.detail = WinsockDetail("recvfrom", code);
      return response;
    }
    response.payload.resize(static_cast<std::size_t>(received));
    response.peer = FromSockaddr(peer);
    response.response_received = true;
    response.status = UdpExchangeStatus::OK;
    return response;
  }
};

#else

class UnsupportedUdpExchangeAdapter final : public IUdpExchangeAdapter {
 public:
  UdpExchangeResponse Exchange(const UdpExchangeRequest&) override {
    UdpExchangeResponse response;
    response.status = UdpExchangeStatus::PLATFORM_UNSUPPORTED;
    response.detail = "UDP Exchange is not implemented on this platform";
    return response;
  }
};

#endif

}  // namespace

bool ParseUdpEndpoint(std::string_view text, bool allow_zero_port, UdpEndpoint& output,
                      std::string& error) {
  const std::size_t separator = text.rfind(':');
  if (separator == std::string_view::npos || separator == 0U || separator + 1U >= text.size() ||
      text.find(':') != separator) {
    error = "endpoint must use numeric IPv4:port";
    return false;
  }

  UdpEndpoint parsed;
  std::string_view address = text.substr(0U, separator);
  for (std::size_t index = 0U; index < parsed.address.size(); ++index) {
    const std::size_t dot = address.find('.');
    const std::string_view octet =
        dot == std::string_view::npos ? address : address.substr(0U, dot);
    unsigned int value = 0U;
    if (!ParseUnsigned(octet, 255U, value)) {
      error = "endpoint contains an invalid IPv4 octet";
      return false;
    }
    parsed.address[index] = static_cast<std::uint8_t>(value);
    if (index + 1U == parsed.address.size()) {
      if (dot != std::string_view::npos) {
        error = "endpoint contains too many IPv4 octets";
        return false;
      }
    } else {
      if (dot == std::string_view::npos) {
        error = "endpoint contains too few IPv4 octets";
        return false;
      }
      address.remove_prefix(dot + 1U);
    }
  }

  unsigned int port = 0U;
  if (!ParseUnsigned(text.substr(separator + 1U), std::numeric_limits<std::uint16_t>::max(),
                     port) ||
      (!allow_zero_port && port == 0U)) {
    error = allow_zero_port ? "endpoint port must be in range 0..65535"
                            : "remote endpoint port must be in range 1..65535";
    return false;
  }
  parsed.port = static_cast<std::uint16_t>(port);
  output = parsed;
  return true;
}

std::string FormatUdpEndpoint(const UdpEndpoint& endpoint) {
  std::ostringstream output;
  output << static_cast<unsigned int>(endpoint.address[0]) << '.'
         << static_cast<unsigned int>(endpoint.address[1]) << '.'
         << static_cast<unsigned int>(endpoint.address[2]) << '.'
         << static_cast<unsigned int>(endpoint.address[3]) << ':' << endpoint.port;
  return output.str();
}

bool SameUdpEndpoint(const UdpEndpoint& left, const UdpEndpoint& right) noexcept {
  return left.address == right.address && left.port == right.port;
}

std::unique_ptr<IUdpExchangeAdapter> CreatePlatformUdpExchangeAdapter() {
#if defined(_WIN32)
  return std::make_unique<WindowsUdpExchangeAdapter>();
#else
  return std::make_unique<UnsupportedUdpExchangeAdapter>();
#endif
}

std::string UdpDiagnosticId(UdpExchangeStatus status) {
  switch (status) {
    case UdpExchangeStatus::OK:
      return {};
    case UdpExchangeStatus::STARTUP_FAILED:
      return "UDP_STARTUP_FAILED";
    case UdpExchangeStatus::SOCKET_CREATE_FAILED:
      return "UDP_SOCKET_CREATE_FAILED";
    case UdpExchangeStatus::BIND_FAILED:
      return "UDP_BIND_FAILED";
    case UdpExchangeStatus::SEND_FAILED:
      return "UDP_SEND_FAILED";
    case UdpExchangeStatus::RECEIVE_FAILED:
      return "UDP_RECEIVE_FAILED";
    case UdpExchangeStatus::TIMEOUT:
      return "UDP_RESPONSE_TIMEOUT";
    case UdpExchangeStatus::DATAGRAM_TOO_LARGE:
      return "UDP_DATAGRAM_TOO_LARGE";
    case UdpExchangeStatus::DATAGRAM_TRUNCATED:
      return "UDP_DATAGRAM_TRUNCATED";
    case UdpExchangeStatus::PLATFORM_UNSUPPORTED:
      return "UDP_PLATFORM_UNSUPPORTED";
  }
  return "UDP_RECEIVE_FAILED";
}

}  // namespace pae::protocol_lab
