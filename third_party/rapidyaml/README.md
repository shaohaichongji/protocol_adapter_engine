# rapidyaml 0.16.0 单头依赖

本目录仅为可选的内部 YAML 前端保存固定上游原件，不表示 Lab、SDK 或公开 YAML API
已经接入。`PAE_BUILD_YAML_FRONTEND` 默认关闭；仅显式开启时根 CMake 才读取此头文件、
校验 SHA-256 并构建独立 `pae_yaml_frontend`。正常 Configure/Build 不联网、不读取旧 `out`。

## 来源与许可

- 上游：<https://github.com/biojppm/rapidyaml>
- 版本：`v0.16.0`；单头发布资产：
  <https://github.com/biojppm/rapidyaml/releases/download/v0.16.0/rapidyaml.v0.16.0.singlehdr.hpp>
- `rapidyaml.hpp`：1,751,073 字节，SHA-256
  `0D0B8076174CF62F034406B03529FDA542EBC9A17506D3BD6D949AEDC4BFA6AB`。
  从先前已验证的隔离 `out` 原件逐字节复制，本地修改：无。
- `LICENSE.txt`：官方 `v0.16.0` 的 MIT 原文，来源
  <https://github.com/biojppm/rapidyaml/blob/v0.16.0/LICENSE.txt>；SHA-256
  `D21ACDDE3276A3706F41C0A7E911A3953AC1CA6B33C5AE05F92D6651F77CE627`。
  单头含随版本内嵌的 c4core 代码；其上游 MIT 文本与该文件相同。
- 单头还含 fast_float（MIT）和 debugbreak（BSD 2-clause）的完整版权/许可文字，
  见原件内部声明及本目录 `THIRD_PARTY_NOTICES.txt`。这些声明随源码保留，不能因
  仅分发二进制而忽略相应通知义务。

`.gitattributes` 对上游头及官方 License 设置 `-text`，避免 Git 换行转换改变锁定字节。
`README.md`、通知整理文本及 `.gitattributes` 为本地归集材料，不冒充上游原件。

升级或替换必须另行复核上游版本、实际嵌入依赖与许可、文件长度和 Hash；按字节替换原件
后重新执行可选前端、JSON-only 隔离及搬迁/错误哈希门禁。不得仅改预期 Hash 接纳
未经审查的文件。
