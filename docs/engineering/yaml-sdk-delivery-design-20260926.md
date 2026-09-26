# YAML SDK 可选组件交付边界预检（2026-09-26）

## 结论与推荐

本片只读预检后，推荐把 YAML→严格 JSON 转换做成**独立、默认不随 JSON-only 包启用的
静态附加组件**：源码包在显式开关下构建；static 与 shared PAE 二进制包均可另外携带
`PAE::yaml_frontend` 静态目标。后者是“shared PAE + 静态 YAML 前端”，**不是**动态 YAML
组件或稳定插件 ABI。YAML 组件仅转换作者源并给出来源映射，consumer 再显式调用既有
`PAE::pae` 的 `CompileProtocolJson`；不得把 YAML 解释逻辑移入 Core、改变 Schema/Plan/
Codec、扩大受限 Profile，或在安装包缺组件时静默回退下载。

这是后续实施建议，不是本轮已实现/验证的 SDK 能力。当前 `main@4da54b9bdc6fa3fed0f81ab346c2fe97a9a6329e`，
暂存区为空；Lab YAML 入口和总控计划/契约/索引已有未提交变更，本片不触碰。YAML 单独
人工体验按用户决定延期至成品统一体验，不因此获得人工验收结论。本报告是本轮唯一写入文件；
未构建、测试、打包、替换部署、Stage、Commit 或 Push。

## 现场依据与待固化边界

| 已确认的当前事实 | 依据及影响 |
| --- | --- |
| 根 `PAE_BUILD_YAML_FRONTEND` 默认 OFF；`pae_yaml_frontend` 当前为 `STATIC`，仅包含非 Qt `frontend.cpp`，固定校验随仓 rapidyaml 单头 Hash，不链接 Core | `CMakeLists.txt`、`src/config_frontend_yaml/CMakeLists.txt`；可保持 JSON-only 不引入 ryml，但当前目标及头仍是内部候选。 |
| 前端以借用 `string_view` 输入，同步转换，成功时自持 JSON、来源身份/映射；失败时清空 JSON 与映射 | `src/config_frontend_yaml/frontend.h/.cpp`；应保留成功/失败及 owner 语义，不能直接把内部存储布局写入公开头。 |
| 内部 `Limits` 包含实验上限与 `fail_parser_allocation`、`fail_frontend_allocation`；`Result` 暴露 `unique_ptr` 存储、分配计数和可返回内部 Entry 指针的查询 | 同上及 `tests/config_frontend_yaml/frontend_tests.cpp`；故障点和内部容量布局不能成为 SDK 承诺。 |
| Lab 当前直接 include `frontend.h`，保存内部 `Result`，用 `Find/FindNearest` 映射下游 JSON Pointer，再调用公开 `CompileProtocolJson` | `tools/protocol_lab_ui/compile_worker.h/.cpp`、`tools/protocol_lab_ui/CMakeLists.txt`；SDK 化后需迁移消费接口，不能让包外 consumer include 仓库 `src`。 |
| `PAE::pae` 是当前唯一安装消费目标；`PAEConfig.cmake.in` 按 static/shared、配置、x64 与 MSVC runtime 构造其 imported target | `src/public_api/CMakeLists.txt`、`cmake/PaeSdkInstall.cmake`、`cmake/PAEConfig.cmake.in`；YAML target 必须是独立可选导出，不能自动写入 `PAE::pae` 链接接口。 |
| source SDK 的显式白名单只含既有源码/yyjson，binary 完整性检查及许可只认识现有文件 | `scripts/package_sdk_stage3.ps1`；只在仓库内打开开关无法得到包外可用组件。 |

已有非 Qt 前端和依赖构建证据分别见
`docs/engineering/yaml-frontend-slice-validation-20260926.md`、
`docs/engineering/yaml-build-integration-validation-20260926.md`；仓库内 Lab 接入的定向证据见
`docs/engineering/lab-yaml-minimal-entry-validation-20260926.md`。这些既有日志不等于本轮新测，
也不证明安装包、跨包 ABI、source SDK 白名单或搬迁后的包外消费。

## 两种交付形态比较

| 方案 | static PAE 包 | shared PAE 包 | 主要代价与判断 |
| --- | --- | --- | --- |
| A：独立静态附加组件（推荐） | `PAE::pae` 与 `PAE::yaml_frontend` 各自静态链接；YAML 前端内部包含 rapidyaml | `PAE::pae` 仍为 DLL；YAML 静态库链接进 consumer，不新增 YAML DLL | 两类包共享一套 YAML 目标/所有权实现，需保证同包 Debug/Release、x64、MSVC/CRT 匹配；绝不宣称“动态 YAML 组件”。前端不调用 PAE，转换结果由 consumer 在自身模块持有，再传生成 JSON 给 `PAE::pae`。这是最小可验证落点。 |
| B：随 PAE 库类型生成 YAML 前端 | 静态 YAML 库 | 另造 YAML DLL/import lib，或把前端并入 `pae.dll` | shared 分支须设计导出宏/符号可见性、跨 DLL 的创建与释放、异常和 STL/CRT 边界、部署路径及版本匹配；若并入 `pae.dll` 又扩大核心目标的依赖和开关矩阵。首片收益不足以覆盖成本，暂不推荐。 |

推荐 A 不承诺跨编译器或跨运行库 ABI；它仍要求与当前 SDK 相同的受控 Windows MSVC
工具链/CRT 配对。`PAE::yaml_frontend` 作为 CMake target 名称与 `PAE::pae` 并列，
不自动链接后者，不改变现有 JSON-only `PAE::pae` 安装和运行行为。若未来确需真正
动态 YAML 边界，应另立契约与 ABI 验证，不能仅将现有 `add_library(... STATIC)` 改为
`BUILD_SHARED_LIBS`。

## 建议的最小公开接口与所有权

建议新增专属可选头（例如 `src/config_frontend_yaml/public/pae/yaml_frontend.h`，安装时
映射为 `include/pae/yaml_frontend.h`），在 `pae::yaml` 命名空间提供：

- `ConversionStatus`：明确成功、输入/解析/辅助/输出限额、分配失败、非法 YAML、Profile
  拒绝；失败不提供可用的部分 JSON 或来源映射。
- move-only `ConversionResult`：公开不透明实现或等价私有 owner；提供 `Succeeded()`、
  `Json()`、`SourceIdentity()`、`Status()`、受 owner 生命周期约束的失败原因，以及
  `FindSource(json_pointer)` / `FindNearestSource(json_pointer)` 返回**值类型**位置
  （有/无、key/value 行列、近似标志），不返回内部 `SourceEntry*` 或池内指针。
- `ConvertToStrictJson(yaml_bytes, source_identity)`：同步借用输入，返回自有转换结果；
  原 YAML 字节/来源身份只在调用期间借用，结果保存所需副本。生成 JSON 的 view 仅在
  owner 存活且未移动/销毁时有效；consumer 必须先持有结果，再调用
  `CompileProtocolJson(result.Json())`。生成 JSON offset 不等于原 YAML offset。

公开头不包含 rapidyaml 类型，不暴露 `json_storage`、`pointer_storage`、`entries`、
`SourceEntry` 物理布局、Parser 内部计数或故障注入开关。现有内部测试可继续从非公开
头访问注入参数；生产公开接口不为测试增加 fault 参数。当前实验默认值是 16 KiB
输入、Parser/辅助各 128 KiB、32 KiB JSON、512 节点、16 层、4 KiB 标量；
`schema/pae_yaml_profile_v0.1.md` 明确它们**不是获批生产资源档位**。
实现公开组件前须决定对外如何陈述并版本化这些实际限制；本报告建议首片保持现值、
不开放任意预算调参，并将限额拒绝状态明示，不能在本次预检中静默提升默认值。

即使采用推荐的静态附加组件，move/destroy 的实现及对象分配/释放仍应归属前端目标，
避免把内部 owner 的 `delete` 或数组释放泄漏给调用方；公开类型不要按值暴露 STL 容器
布局。若未来进入 DLL，需使用真正的模块内销毁函数或 out-of-line destructor、显式
导出宏及同工具链 CRT 约束，并检查所有返回 view/异常是否越过模块边界。现有
`PAE_API`/`CompiledProtocol` 是既有 PAE DLL 模型的参考，不自动等于 YAML 新接口
具备稳定 C ABI 或跨编译器 ABI。

## 三类包的交付闭包

1. **源码包**：`scripts/package_sdk_stage3.ps1` source 白名单须在后续实施中增加 YAML
   目标、实现、可选公开头、`third_party/rapidyaml` 的原件与许可通知、受限 Profile、
   独立合成 YAML 示例及可复核 consumer。包内 `PAE_BUILD_YAML_FRONTEND` 保持 OFF 默认；
   consumer 显式打开后从**解包目录**取依赖，不读取开发仓库或旧 `out`。生成的
   strict JSON 仍送现有公开编译器。
2. **静态二进制包**：开关 ON 时安装 `pae_yaml_frontend.lib` 和可选公开头，
   `PAEConfig.cmake` 仅在该包实有组件时提供 `PAE::yaml_frontend`；consumer 显式链接
   `PAE::yaml_frontend` 与 `PAE::pae`。开关 OFF 的包不得出现误导性的可用 target/
   Header 或在 `PAE::pae` 中增加 ryml 链接依赖。
3. **动态 PAE 包**：保持 `pae.dll`/`PAE::pae` 的既有导入与复制方式；另携同配置的
   `pae_yaml_frontend.lib` 静态附加组件，consumer 同时链接两目标。YAML 代码在
   consumer 模块内，不产生 `pae_yaml_frontend.dll`；`PAEConfig` 需让缺组件请求在
   Configure 时明确失败，且沿用现有 Debug/Release 错配门禁、x64 和 `/MDd`/`/MD`
   约束，不能只凭 import target 存在声称 ABI 已验证。

现有 `PaeSdkInstall.cmake` 对 `include/pae` 做整个目录安装。为了保持 JSON-only 安装
不出现不可用的 YAML 头，建议把可选公开头的**源文件**放在上面的独立 public 子目录，
仅开关 ON 时单独安装；若选择放在现有 `include/pae`，则必须改为条件排除/安装。
同理 `PAEConfig.cmake.in` 须有可选组件的包内相对路径和缺失门禁，不能把机器绝对路径
或旧构建树写进导出目标。`package_sdk_stage3.ps1` 的 source 白名单、binary 必需文件、
`MANIFEST.txt`/`SHA256SUMS.txt`/`PROVENANCE.json` 与包根说明应按开关分支维护，
避免 JSON-only 包被强制要求 YAML 文件。随包至少保留 rapidyaml 官方 MIT 原文及
单头内 fast_float MIT、debugbreak BSD-2 通知；此为第三方通知闭包，不替 PAE
选择项目级许可，也不自动取得对外再分发授权。

## Lab 迁移清单（仅计划）

当前仓库内 Lab 的 `compile_worker.h` 直接 include 内部 `frontend.h`，completion 保存
内部 `Result`，`compile_worker.cpp` 调用 `Convert` 并根据 `SourceEntry*` 选择 key/value
行列。后续迁移应改为 include 可选公开头、链接 `PAE::yaml_frontend`、保存公开
`ConversionResult` owner，并用值类型来源查询保留精确/容器近似/未映射与原有 JSON
Pointer、生成 JSON offset 的分离。当前 worker 的文档 ID/revision、原文路径身份、
生成 JSON 哈希与 Host 重准备、取消/重载/关闭时的 owner 清理均应保持；YAML 转换失败
仍不得调用编译器。更新 Lab 可选 CMake 与定向测试即可，不在本预检改 Lab 源码或
复做人工 YAML 体验。

## 后续最小验证矩阵与停点

下表是**未来实施门禁**，本轮全部未执行。公开合成 Binary CRC 与 ASCII YAML/JSON
对照可复用既有用例，但包外 consumer 必须真实运行，不得用 Configure 成功替代。

| 组合 | 最低可观察证据 |
| --- | --- |
| source 包，Windows x64 Debug/Release | 从新解包根启用 YAML/公开 PAE，包外 consumer 仅 include 包内公开头、链接两个目标；分别运行 YAML→JSON→`CompileProtocolJson`→已知 Codec Decode/Encode。 |
| static 包，Windows x64 Debug/Release | `find_package(PAE)` 导入两目标，同配置构建并运行上述 consumer；确认安装头、静态库、许可/示例完整及错配置失败。 |
| shared PAE 包，Windows x64 Debug/Release | 包外运行同一 consumer，确认 `pae.dll` 来自同包，YAML 来自静态附加库且无 YAML DLL；验证配置/CRT 边界与不从开发仓库加载。 |
| JSON-only：source、static、shared | 开关 OFF 或包不含 YAML 时，只用 `PAE::pae` 的原 consumer 仍能配置、构建、运行；`PAE::yaml_frontend` 不可误用，ryml 不成为 JSON 链接/安装依赖。 |
| 来源与失败 | 包外输入保留原 YAML 身份；下游错误 JSON Pointer/生成 offset 与精确或近似 YAML 行列分别断言；转换失败不调用公开编译器、不交付部分 JSON。 |
| 搬迁与完整性 | 三类包至少各有解包到含空格的新根的消费对照；检查目标路径/运行模块/Hash/许可，不允许引用原仓库、旧 `out` 或运行时下载。 |

建议将六个 D/R 正向组合和 JSON-only 门禁作为首批必要项；搬迁可优先用每种包各一组
Release，再以 Debug 原路径补齐配置隔离，不增加一轮冗余人工矩阵。正式性能、Linux、
真实私有协议、设备/现场、跨编译器 ABI、YAML 语法级错误精确行列及真正 YAML DLL
均后置，不由本次 SDK 自动证据代替。源码单头的构建哈希已见历史报告；包外应重新
核对**包内**文件与 manifest/Hash 的关联。

## 后续最小实施文件清单与待决策项

若总控批准推荐 A，预计最小改动涉及：

- `src/config_frontend_yaml/public/pae/yaml_frontend.h`（新）及同目录/现有实现文件的
  公开 facade；`src/config_frontend_yaml/CMakeLists.txt` 增加 build/install include 与
  `PAE::yaml_frontend` 本地 alias；根 `CMakeLists.txt` 保持默认 OFF，仅增加必要包门禁。
- `cmake/PaeSdkInstall.cmake`、`cmake/PAEConfig.cmake.in`、
  `scripts/package_sdk_stage3.ps1`：条件安装/导入、source/binary 文件清单和许可闭包。
- `examples` 中独立公开 YAML/JSON/Codec 合成 consumer 与样例、相应 CMake/脚本测试；
  `tools/protocol_lab_ui/compile_worker.h/.cpp` 及相关 CMake/定向测试迁移消费；
  `docs/sdk/README.md`、第三方说明和新的 SDK 验证报告同步边界。

须由总控/用户在实施前确认：① 是否采用“静态 YAML 附加组件同时随 static/shared PAE
包交付”，接受 shared PAE 并无 YAML DLL；② 对外公开当前实验资源默认值的约束口径
与后续版本策略；③ 若初版成品拟对外再分发，项目级 PAE 许可及第三方通知的发布审查
如何处理。若选择 B，需另拍动态模块 ABI/销毁与部署契约，不能沿本报告文件清单直接实施。

本片仅形成建议和证据边界，状态：**已完成派发范围，待总控复核**。
