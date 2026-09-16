# PAE Stage 3 SDK 内容与许可证盘点

日期：2026-09-15

状态：已完成并行只读盘点，待总控结合阶段 3 实际包与验证证据复核

现场基线：`main`，base HEAD `dfb08f351cdb22a9b50c9e64e688e3b666e669bc`，工作树已有未提交共享变更

## 1. 可执行结论

1. 三类包应共用同一份公开头和 `PAE::pae` 消费入口，但不能机械复制开发仓库。源码包采用明确白名单，二进制包采用安装/导出清单；包内必须记录 base HEAD、dirty 状态、工具链、架构、配置、CRT、内容清单和 SHA-256。
2. 当前公开实现的源码闭包是 `include/pae`、`src/public_api`、`src/config_compiler`、`src/protocol_plan`、`src/protocol_core`、`src/protocol_framing` 和随仓 `yyjson`。其中 `ui_description.*` 实际承担 Compiler 公开 metadata 的构建与冻结，不依赖 Qt，属于必要闭包，不能按名称排除。
3. 静态包的最高风险是“单一 `.lib`”并不自动包含其所链接的其他静态 archive。现有 `pae_public_api` 私有链接 Compiler、Core、Framing、Plan，Compiler 再链接 `yyjson`；阶段 3 必须通过导出全部传递目标或聚合实现形成完整闭包，并保证用户仍只写 `target_link_libraries(... PRIVATE PAE::pae)`，不得要求用户认识内部 target。
4. 动态包已有 `PAE_API` 宏基础，但当前 target 固定为 `STATIC`、公开定义 `PAE_STATIC_DEFINE=1`，尚不能据此认定 DLL 导出闭合。应以 DLL + import lib + public headers + 可重定位 CMake config 的仓库外 Debug/Release consumer 和导出/运行依赖检查为准。
5. 已确认第三方依赖只有当前执行闭包内的 `yyjson 0.12.0`，许可证为 MIT，锁文件记录 Commit `8b4a38dc994a110abaec8a400615567bd996105f` 且本地修改为 none。仓库根未发现 PAE 自身 `LICENSE`/`COPYING`/`NOTICE`；这是对外发布前必须由用户决定的门槛，不阻断本轮本地打包验证，也不能用 yyjson 的 MIT License 代替 PAE 授权。

## 2. 当前确认的依赖闭包

当前构建关系来自 [顶层 CMake](../../CMakeLists.txt) 及各目标定义：

```text
PAE::pae (pae_public_api)
├─ pae_config_compiler
│  ├─ pae_protocol_plan
│  └─ pae_yyjson_vendor
├─ pae_protocol_core_slice
│  └─ pae_protocol_plan
├─ pae_protocol_framing
│  └─ pae_protocol_plan
└─ pae_project_options
```

- 公共 API：[`include/pae`](../../include/pae) 全目录。`export.h` 和 `version.h` 也必须随包提供，不能只复制被示例直接 include 的头。
- 公开 facade 实现：[`src/public_api`](../../src/public_api) 全目录。两个 `*_test_support.h` 目前被产品 `.cpp` 直接 include，即使 Testing 关闭也属于现有源码编译闭包；若实施侧消除直接依赖，才可据最终源码清单排除。
- Compiler/metadata：[`src/config_compiler`](../../src/config_compiler) 全目录。`compiler.cpp` 调用 `CompileJsonToPlanWithUiDescription()`，而该实现由 `ui_description.cpp/.h/_internal.h` 提供，因此这些文件是通用公开 metadata 依赖，不是 Qt/Lab UI 依赖。
- Plan、Codec、Framing：[`src/protocol_plan`](../../src/protocol_plan)、[`src/protocol_core`](../../src/protocol_core)、[`src/protocol_framing`](../../src/protocol_framing) 的当前 target 源文件。
- JSON 依赖：[`third_party/yyjson`](../../third_party/yyjson) 中锁文件、`src/yyjson.c`、`src/yyjson.h`、原始 `LICENSE`；源码包还应保留来源说明。当前正常 Configure 不联网下载。
- Schema 契约材料：[`schema/pae.schema.json`](../../schema/pae.schema.json)、[`schema/strict_json_profile_v0.1.md`](../../schema/strict_json_profile_v0.1.md)、[`schema/protocol_plan_execution_semantics_v0.1.md`](../../schema/protocol_plan_execution_semantics_v0.1.md) 及目录 README。源码检索未发现运行时读取这些文件；它们是配置作者/审查者所需的交付契约，不应误写成运行时依赖。

## 3. 三类包建议白名单

| 包形态 | 必须包含 | 需由阶段 3 实现/验证确认 |
| --- | --- | --- |
| 源码包 | 公共头；上述必要私有实现；独立、最小的顶层构建入口；`PaeYyjson.cmake`、`VerifyYyjsonVendor.cmake` 或等价离线校验逻辑；yyjson 锁、源码、License、来源说明；Schema 契约；公开 Compiler/Codec/Framer/Host 示例及其 4 份合成配置；使用说明、包清单、哈希、来源/dirty/toolchain 记录 | 从包外全新目录离线 Configure/Build；无开发仓库绝对路径或回读；Testing hooks 关闭；若沿用开发仓库顶层 CMake，必须连同其在公开 API 分支中无条件 `add_subdirectory` 的示例目录；更稳妥的是包内专用最小入口 |
| 静态 SDK | `include/pae/**`；Release/Debug 分离的静态库交付；可重定位 `PAEConfig.cmake`/targets；最小 find_package consumer；配置与 ABI/CRT 说明；yyjson MIT License/Notice；清单和哈希 | `PAE::pae` 是否真正封装 Compiler/Core/Framing/Plan/yyjson 的全部链接符号；导出的 CMake targets 是否不引用包外路径和未交付内部 target；Debug/Release 与 CRT 匹配策略 |
| 动态 SDK | `include/pae/**`；DLL、对应 import lib；可重定位 CMake config/targets；最小 find_package consumer；运行部署说明；yyjson MIT License/Notice；清单和哈希 | `PAE_BUILDING_LIBRARY`/`PAE_STATIC_DEFINE` 的配置是否正确；全部公开非内联函数、成员和析构是否导出；内部符号不泄漏为消费契约；DLL 运行依赖及 Debug/Release 分目录是否闭合 |

现有公开示例可作为内容来源：

- Compiler：[`examples/public_api_compile`](../../examples/public_api_compile)；
- Codec：[`examples/public_api_codec`](../../examples/public_api_codec)，配置为 `synthetic_lab_exchange_slice.pae.json`、`synthetic_ascii_text_slice.pae.json`；
- Framer：[`examples/public_api_framer`](../../examples/public_api_framer)，配置为 `synthetic_stream_framing_slice.pae.json`；
- Host：[`examples/public_api_host`](../../examples/public_api_host)，配置为 `synthetic_ascii_stream_slice.pae.json`。

这些示例当前主要通过源码树 `PAE_SOURCE_DIR` 或开发仓库 target 消费。静态/动态包必须另以 `find_package(PAE CONFIG REQUIRED)` + `PAE::pae` 的形式验证，不能把现有 standalone 用法直接当作二进制 SDK 消费证据。

## 4. 明确排除项

- `tools/**`（含 Protocol Lab CLI/UI/适配层）、`third_party/qt/**` 和任何 Qt CMake 模块、Qt DLL/插件/License；
- `tests/**`、测试框架、测试专用 fixture/generator、`cmake/VerifyYyjsonVendor*Fixture.cmake` 与测试 hooks；
- `spikes/**` 及 nlohmann/json、RapidJSON 历史比较材料；
- 内部旧 Host slice `src/host_endpoint/**`、旧 business embedding/内部示例及非公开 Lab 配置；当前公开 Host 实现在 `src/public_api/host_endpoint.cpp`，其现有 target 不链接内部 Host slice；
- `out/**`、构建缓存、PDB/日志/验证临时目录、`.git/**`、Git 元数据、编辑器配置及工程历史文档；PDB 若以后需要，应作为明确的可选符号交付，不混入运行时必需清单；
- 私有协议、客户数据、真实设备配置、Socket/串口/线程/重试/业务路由实现；
- 系统 DLL、VC Runtime 安装包、安装器、签名与上传产物；本轮不获这些授权。

注意：`ui_description.*` 不在排除项中。是否属于 Qt/UI 应按类型、调用链和链接依赖判断，而不是按文件名判断。

## 5. 许可证与来源记录

| 对象 | 当前证据 | 交付要求 |
| --- | --- | --- |
| PAE 自身源码/二进制 | 仓库根未发现 `LICENSE*`、`COPYING*`、`NOTICE*`；根 README 明确尚未选择或授予开源 License | 本轮清单标记“PAE license not granted / local validation only”；正式对外发布前由用户决定许可证和版权声明，不推断开源或商业授权 |
| yyjson 0.12.0 | [`dependency.lock.json`](../../third_party/yyjson/dependency.lock.json)、[`README.md`](../../third_party/yyjson/README.md)、[`LICENSE`](../../third_party/yyjson/LICENSE)；MIT、锁定 Commit、none 本地修改 | 三类包均保留原始 License；源码包同时保留来源/版本/锁和原始源码；二进制包至少保留第三方 Notices/License 与版本来源记录 |
| Qt | 不在 PAE 执行依赖闭包；只属于 Lab | 三类 PAE 包全部排除 Qt 及其许可证，避免错误暗示 PAE SDK 依赖 Qt |
| MSVC/C++ Runtime | 当前工程未显式固定 `CMAKE_MSVC_RUNTIME_LIBRARY` | 文档记录实际 toolset、`/MD[d]` 或 `/MT[d]`、Debug/Release 与再分发前提；本轮不复制系统 DLL或 VC Runtime |

## 6. 离线源码构建与二进制消费风险

1. **开发仓库顶层入口偏重。** 当前 `PAE_BUILD_PUBLIC_API_STAGE1=ON` 会联动全部 Schema 0.1-0.11 切片，并无条件加入三个公开示例。若源码包删去这些目录却原样复制顶层 CMake，Configure 会失败。应提供包专用最小入口，或让白名单与实际 `add_subdirectory` 完全一致。
2. **静态链接闭包不能凭 target 的 `PRIVATE` 关系推断完成。** MSVC 静态 archive 通常不合并被链接 archive；导出配置也可能出现未交付的 `$<LINK_ONLY:...>` 依赖。必须检查生成 targets 文件并在开发仓库外只链接 `PAE::pae`，覆盖 Compiler、Codec、Framer、Host 才能收口。
3. **DLL 导出尚待实证。** `export.h` 已定义 Windows import/export 宏，但当前 target 固定静态构建。切换为 DLL 后需检查公开类析构、非内联方法和自由函数，并用 consumer 链接、运行和导出表三方面验证；不能用“DLL 生成成功”代替 API 可消费。
4. **CRT/ABI 未冻结。** 当前仅确认 C++17，未看到显式 CRT 固定；公开 API 还标记 `0.experimental.1`，也不承诺稳定 C++ ABI。包说明必须限定 Windows x64、编译器/toolset、配置和 CRT，不宣称跨编译器通用。
5. **离线依赖完整性需保留。** `PaeYyjson.cmake` 依赖 `VerifyYyjsonVendor.cmake` 和 lock 内精确字节/哈希；遗漏任一锁定文件将按设计 Configure 失败。包外验证还应扫描绝对开发路径、网络下载逻辑和未交付目录引用。
6. **配置契约与运行时材料需分开描述。** Schema 文档随源码交付是为了配置编写和追溯；当前实现从传入的 JSON bytes 编译，并未在运行时加载 `schema/**`。不要要求二进制运行时部署 Schema 文件，也不要因此从源码包删掉契约材料。
7. **dirty 来源必须显式。** 本轮基线存在未提交共享变更。manifest 应同时记录 base HEAD 与 dirty/source snapshot 标识，并对实际包逐文件计算 SHA-256，不能将其标为 `dfb08f3` 的纯净构建。

## 7. 最小用户文档建议

每类包至少提供一份短 README，且内容以最终验证事实为准：

1. 支持边界：公开 Compiler/metadata、Codec、Framer、Host；不含 Transport、设备生命周期、Lab/Qt、业务路由；合成配置验证不等于真实协议或生产验收。
2. 兼容矩阵：PAE API 版本、Schema 0.1-0.11 能力说明、Windows x64、MSVC generator/toolset、C++17、Debug/Release、CRT、稳定 ABI 不承诺项。
3. 目录布局和快速开始：源码包用离线 `add_subdirectory` 或包定义的入口；二进制包统一 `find_package(PAE CONFIG REQUIRED)` 后链接 `PAE::pae`，不暴露内部库名。
4. 运行说明：动态包 DLL 搜索路径和 Debug/Release 匹配；静态包 CRT/配置匹配；不指导复制系统 DLL。
5. 四条最小调用链与合成配置参数：Compile/metadata、Codec、Framer 每候选只 Decode 一次、Host 同一 Encode Handle 的调用期 `message_index` 选择。
6. 生命周期与错误边界：借用 view/callback 有效期、Reset/stale handle、异常后 `RESET_REQUIRED`、候选形成不等于 Decode 或发送成功。
7. 来源与法律说明：base HEAD + dirty 状态、manifest/hash、yyjson 版本与 MIT License、PAE 自身尚无发布许可证及仅限本地验证的状态。

## 8. 建议总控验收清单

- 三类包各自 manifest 与实际文件逐项一致；禁入目录和绝对开发路径扫描为零，yyjson 锁定文件校验通过；
- 同一个仓库外 public-only consumer 分别消费源码、静态、动态包，Debug/Release 串行覆盖 Compiler/metadata、Codec、Framer、Host 和同 Handle 多消息 Encode；
- 静态 consumer 的链接命令不出现需要用户手写的内部 PAE 库；动态 consumer 的导入库、DLL 和运行依赖闭合；
- 二进制 CMake config 在包移动/解压到另一目录后仍可重定位，生成 targets 不引用开发仓库、构建树或私有头；
- 产品构建关闭 test hooks，不带 Qt/Lab/tests/spikes；公开头集合在三类包中一致；
- 验证报告分别写明实际 generator、toolset、架构、CRT、Debug/Release、命令、日志和未验证项；不把本地 SDK 验证扩展为正式发布、稳定 ABI、Linux、真实协议、硬件、现场或生产结论；
- 根 PAE License 决策保持为正式对外发布门槛；本轮仅验证包内容和可消费性，不擅自补 License。

## 9. 本轮证据边界

本轮只读核对当前源码、CMake、公开示例、Schema、yyjson 锁与许可证，并新增本报告；未构建、未测试、未生成或检查阶段 3 半成品包，未验证最终 archive/DLL/CMake config、运行依赖、重定位或仓库外 consumer。最终白名单必须由总控对照实施任务交付的实际 manifest 与验证日志复核。
