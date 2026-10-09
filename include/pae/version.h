#pragma once

#include <string_view>

namespace pae {

// 实验性公开 API 的契约标识，不是 Schema、业务协议或产品发布版本。
inline constexpr std::string_view kPublicApiVersion = "0.experimental.1";

// The Stage 1 compiler is built with the repository's complete Schema 0.1-0.11 feature set.
// This query describes that build contract; it does not promise future source or binary ABI
// compatibility for the experimental API.
// 精确匹配支持的版本字符串，不进行数值比较；支持版本不代表任意配置都能通过编译。
// 该查询不承诺未来源码/二进制 ABI 兼容性，也不证明具体 Frame 执行成功。
constexpr bool IsSchemaVersionSupported(std::string_view version) noexcept {
  return version == "0.1" || version == "0.2" || version == "0.3" || version == "0.4" ||
         version == "0.5" || version == "0.6" || version == "0.7" || version == "0.8" ||
         version == "0.9" || version == "0.10" || version == "0.11";
}

}  // namespace pae
