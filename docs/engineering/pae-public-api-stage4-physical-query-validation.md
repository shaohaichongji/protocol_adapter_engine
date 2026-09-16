# PAE 公开 API 阶段 4 P：Binary 物理布局查询 Windows 验证

日期：2026-09-15。状态：已完成派发范围，待总控复核；不表示 Lab H1/H2、正式 SDK 发布、
稳定 ABI、Linux、真实协议、Golden、硬件、现场或性能验证通过。

## 1. 实施结论

P 首片已按 [查询契约](pae-public-api-stage4-physical-query-contract.md) 实施：

- 公开 `RecordRepresentation`、显式 `PhysicalQueryStatus`、byte range/length bounds、最多 8 项的
  physical bit masks 及 Message/Field 静态与按长度解析纯值；
- `CompiledProtocol` 新增 `MessageRepresentation`、`MessagePhysical`、
  `ResolveMessagePhysical`、`FieldPhysical`、`ResolveFieldPhysical`；
- Binary fixed/bounded 记录、BYTES 长度、integrity/computed storage 和实际 payload 范围直接映射
  冻结 Plan；ASCII 只可识别表示类别，物理查询明确返回不支持；
- 查询不读取 Frame、不执行 Matcher/Gate/Integrity/Codec，不证明 Decode 成功，不增加堆分配、
  identity token、Plan/Workspace/metadata 存储或 `CompileMemoryReport` 计费；
- Core、Plan、Loader、Schema、Codec、Host、Lab、Qt 均未修改；Encode raw E 保持延期。

实施中发现并修正一个 facade 映射细节：单字节位容器省略 `byte_order` 后，冻结 Plan 合法保留
`NOT_APPLICABLE`；多字节仍只接受 `BIG/LITTLE`。这没有改变配置规则或 Plan。

## 2. 测试先行证据

先加入 `pae.public_api.physical_query`，再构建目标。预实现 Configure 成功，Build 因公开
`ByteRange`、`PhysicalQueryStatus` 和五个查询尚不存在而失败，退出码 1：

- `out/stage4-p-validation/preimplementation/configure.log`
- `out/stage4-p-validation/preimplementation/build-debug.log`

首次 facade 实现后专项为 28 PASS / 2 FAIL，失败恰为单字节 `lsb0/msb0` mask；大小端跨字节、
有界布局、错误状态、Host 同调用关联与零分配均已通过。修正 `NOT_APPLICABLE` 内部表示后，最终
Debug/Release 直跑各 31/31：

- `out/stage4-p-validation/focused-debug.log`（首次失败）
- `out/stage4-p-validation/focused-final-direct.log`（最终 D/R 31/31）
- `out/stage4-p-validation/focused-final-debug-release.log`（最终 CTest D/R 1/1）

最终断言覆盖：

| 范围 | 精确断言 |
| --- | --- |
| 表示与固定布局 | Binary/ASCII 表示、18/18 与 25/25 固定记录、普通字段和固定 BYTES range |
| bit masks | 单字节 `lsb0/msb0`、大端 `lsb0` 跨字节、小端 `msb0` 跨字节、大端中间 16 位、little-endian 64 位满宽；逐 byte index/mask 独立预期 |
| bounded | 3～6 byte 记录，payload 零/中/最大分别 `{2,0}`/`{2,2}`/`{2,3}`，动态 integrity 分别 `{2,1}`/`{4,1}`/`{5,1}` |
| storage | 固定 SUM8 `{24,1}`、动态 SUM8 最大 `{5,1}`、computed length `{1,1}`、缺席为 `nullopt` |
| 失败关闭 | 空/moved-from owner、Message/Field 越界、固定/有界错误长度、ASCII physical unsupported；非 OK 无 value |
| 执行关联 | 成功 Host callback 内用同一 owner/message/frame 查询；Codec entered hook、Host decode attempts/successes、callback 均精确为 1，查询未触发第二次 Decode |
| 资源 | 连续 128 轮五类查询前后全局 allocation count 相等 |

## 3. Windows 定向回归与隔离

工具链：Visual Studio 2026 18.10.0 Developer PowerShell，MSVC 19.29.30159，v142
14.29.30133，Windows SDK 10.0.22621.0，CMake 4.3.1-msvc1，x64。

测试目录：`out/build/windows-msvc-stage4-p-tests/`。Debug/Release 串行执行；最终 public 标签各
8/8：Compiler metadata、header self-contained/boundary、Codec、consumer metadata、P 查询、
StreamFramer、Host。

- `out/stage4-p-validation/public-regression-final.log`

Testing-off 目录：`out/build/windows-msvc-stage4-p-testing-off/`。Debug/Release `pae_public_api`
均构建成功，`ctest -N` 为 `Total Tests: 0`：

- `out/stage4-p-validation/testing-off-configure.log`
- `out/stage4-p-validation/testing-off-debug-build.log`
- `out/stage4-p-validation/testing-off-release-build.log`
- `out/stage4-p-validation/testing-off-test-list.log`

未重跑 Lab/UI/网络矩阵：本片没有修改 Lab/Qt/Host/Codec 或网络代码；后续 H1/H2 应在各自改动后
执行 Binary 定向回归和用户授权的简短 UI 烟测。

## 4. SDK 候选与包外消费

最终新候选为 `out/sdk-stage4-p/candidate2-20260915/`；早期
`candidate-20260915/` 是 final facade 防御性复核前的隔离产物，已保留但不是交付候选。Stage 3
final6 未覆盖。复用既有 `scripts/package_sdk_stage3.ps1` 生成 source、static D/R、shared D/R，脚本
保留既有 `PAE_SDK_STAGE3_*` 成功标识；候选路径和本报告明确区分 Stage 4 P。

包外综合 consumer 已增加 `physical_query=1`，验证 Binary representation/fixed range、ASCII
representation/unsupported physical，并保留 Compiler/Codec/Framer/Host/multi-message Encode。结果：

- static Debug、static Release、shared Debug、shared Release：各 Configure/Build/Run 退出 0；
- source package：从包内源码而非开发仓库源码完成 Release Configure/Build/Run，退出 0；
- 五次均输出 `PAE_SDK_STAGE3_CONSUMER_PASS ... physical_query=1 ...`。

日志：`out/stage4-p-validation/final-consumer-*.log`。五包 `SHA256SUMS.txt` 与 `MANIFEST.txt`
逐项复算均通过：source 80/79、static 各 28/27、shared 各 24/23（Hash/Manifest 条目）；见
`final-package-integrity.log`。

关键二进制 SHA-256：

| 产物 | SHA-256 |
| --- | --- |
| static Debug `pae.lib` | `79ca1befe645f705e0cbd63923a0ebb4254956df7973ec828e271d540b547c11` |
| static Release `pae.lib` | `1959d105fcd5d1a010bdf04a58ac11a09e239e500be653b531b203ac80b63e2d` |
| shared Debug `pae.dll` | `d9c9f16b10e3df85b74bf2fd02d3d29214880e1b9a280def227c66736ee8968f` |
| shared Release `pae.dll` | `74eb8fe90a1c4aed7f10d1c26d67ef285138f4031b5091ef3b7944776e3871f6` |

`dumpbin /exports` 在 D/R DLL 各找到五个新查询导出；`/dependents` 只显示对应 MSVC/UCRT 与
Kernel32 运行库。见 `out/stage4-p-validation/final-dll-exports-dependents.log`。共享构建仍有 stage 3
已知 C4251 实验性 C++ DLL 警告，本片没有新增该类成员或稳定 ABI 声明。

## 5. 文件范围与未验证边界

本片增量：

- `include/pae/protocol_description.h`
- `include/pae/compiler.h`
- `src/public_api/compiler.cpp`
- `tests/public_api/CMakeLists.txt`
- `tests/public_api/public_physical_query_tests.cpp`
- `examples/public_api_sdk_consumer/main.cpp`
- `docs/engineering/pae-public-api-stage4-physical-query-contract.md`
- `docs/engineering/pae-public-api-stage4-physical-query-validation.md`

未修改根 CMake、Preset、安装规则或打包脚本；现有安装目录规则已自动包含更新公开头和 consumer，
因此无需扩大构建接线。未验证 H1/H2、ASCII 物理布局、Encode raw、Linux、真实协议、人工 UI、
Golden、硬件、现场、并发 move/destroy（仍属调用方禁止行为）或性能。P 查询没有身份 token；后续
Lab 必须遵守同 owner + 同 callback message/frame 关联契约。

当前限定结论：P 首片实现、Windows 定向回归及新 SDK 候选消费均通过，未发现本派发范围内新的
提交阻断项；是否进入 H1 由总控独立复核决定。
