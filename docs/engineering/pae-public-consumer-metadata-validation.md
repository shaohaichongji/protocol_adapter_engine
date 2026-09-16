# PAE 公开消费准备 metadata Windows 验证

状态：2026-09-14 已完成阶段 1B 派发范围，待总控复核。本文记录本次实际实现和验证，
不表示 Lab 已迁移、独立 SDK 已形成或生产环境已验证。

## 1. 实施结果

- `ValueKind` 移至 metadata 与 Codec 可共同包含的公开描述头，保留 `pae::ValueKind` 名称；
  conversion 字段按 logical `DECIMAL64` 暴露，raw integer 仍由既有 Codec 结果独立表达。
- `FieldDescription` 新增 `encode_value_source`，严格区分 caller input、constant、computed 与
  `NOT_REFERENCED`。ASCII Decode-only 字段不会因内部默认 `encode_source` 被误报为输入。
- `PipelineMessageExecution()` 仅对实际 Pipeline/Message 关联返回结果，动作可用性来自冻结候选集合、
  allowed-message 位图和 ASCII action；不解析 direction 文本。
- Encode 输出公开 `EXACT`、`UPPER_BOUND`、`NOT_AVAILABLE`。固定 Binary 使用 `frame_size`，
  bounded 使用 `max_frame_length`，ASCII 使用 Encode action 的 `max_record_length`；本次实际长度仍由
  `EncodeResult` 决定。
- 查询只遍历冻结有界数组，无动态分配；未新增 Plan/Core 字段、Arena、Workspace 或资源计费项。

## 2. 自动化覆盖

新增 `pae.public_api.consumer_metadata`，Debug/Release 直接断言均为 21/21：

- Binary 的 logical type、caller input、constant、固定输出长度及跨 Pipeline 非成员拒绝；
- Decimal logical 类型与 raw 类型不混用，computed 字段来源及 bounded 输出上界；
- ASCII caller input、Decode-only `NOT_REFERENCED`、双向/单向动作和固定/变长输出；
- 空 owner、moved-from owner、越界查询和查询期零新增分配。

既有 Codec facade 专项增至 Debug/Release 各 57/57，并新增：

- preserved unknown ENUM 的 raw=3、known 为空、`output_tainted=true`；
- ASCII literal-only 零输入 Encode、零字段 Decode，以及缺失动作的
  `OPERATION_NOT_SUPPORTED`；
- stream-framed Pipeline 的完整记录 Decode/Encode 动作与 metadata 查询一致；
- Decimal Encode 失败的 status、conversion error、input ordinal、flat Field 位置和零交付。

公开示例改为先查 Pipeline/Message 动作、字段 logical type、Encode 来源和输出上界，再构造调用；
只包含 `pae` 公开头。

## 3. Windows 实际验证

工具链：Visual Studio 18 2026 generator、MSVC v142 `14.29.30133`、x64。Debug 与 Release 串行。

主要命令：

```powershell
cmake --build out/build/windows-msvc-public-codec-stage1 --config Debug `
  --target pae_public_consumer_metadata_tests pae_public_codec_tests `
           pae_public_header_compile_tests pae_public_codec_example -- /m:1
ctest --test-dir out/build/windows-msvc-public-codec-stage1 -C Debug `
  -R "pae\.public_api\." --output-on-failure

cmake --build out/build/windows-msvc-public-codec-stage1 --config Release `
  --target pae_public_consumer_metadata_tests pae_public_codec_tests `
           pae_public_header_compile_tests pae_public_codec_example -- /m:1
ctest --test-dir out/build/windows-msvc-public-codec-stage1 -C Release `
  -R "pae\.public_api\." --output-on-failure
```

结果：

- public_api CTest：Debug 5/5，Release 5/5；
- metadata 直测：Debug 21/21，Release 21/21；
- Codec facade 直测：Debug 57/57，Release 57/57；
- metadata 驱动公开示例：Debug/Release 均 `gate=PASS`；
- 独立 consumer 以 `add_subdirectory`、`PAE_BUILD_TESTING=OFF` 从公开头重建并运行：
  Debug/Release 退出码均为 0；
- product-only 既有隔离目录重建公开库与示例：Debug/Release 均成功，示例均通过，CTest 均注册 0 项。

日志均位于 `out/public-consumer-metadata-stage1/`。该目录及独立 build 目录由 `.gitignore`
明确忽略，没有覆盖 `out/public-codec-stage1/` 的上一片证据。

## 4. 格式与边界检查

- 对本次 C++/header 文件执行 `clang-format 22.1.3`；
- `git diff --check` 通过；因公开 API 目录当前整体尚未跟踪，另以空文件对本片候选逐个执行
  `git diff --no-index --check`，结果通过；
- 对本片候选扫描本机用户路径、IP、外部项目名和私有资料标识，无命中；out 产物保持 ignored；
- 未执行网络、Lab/UI、人工验收、Linux、硬件或现场验证；没有扩大此前人工 Lab 证据。

## 5. 剩余边界

本片仍不提供物理 byte/bit range、完整字段约束模型、Encode 同调用生成字段观察、Framer/Host 或
稳定 ABI。`encode_output_size` 是固定值或安全上界，不是所有调用都会成功的承诺，也不是性能结论。
当前未发现本派发范围内的确定提交阻断项；是否启动 Lab 消费侧复核与后续提交由总控决定。
