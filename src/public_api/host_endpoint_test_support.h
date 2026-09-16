#pragma once

#include <cstdint>

#include "pae/host_endpoint.h"

namespace pae::detail {

class HostEndpointTestAccess final {
 public:
  static bool SetGeneration(HostEndpoint& host, const HostChannelHandle& handle,
                            std::uint64_t generation) noexcept;
};

}  // namespace pae::detail
