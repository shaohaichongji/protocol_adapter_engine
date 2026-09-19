# Lab 使用安装版 PAE SDK 的包外独立构建预检

日期：2026-09-19

状态：**已完成静态盘点，待总控合并功能审计后确定实施切片**

基线：`main@c5b369298c79f27cf71dd2f73be9fbb4a4203a31`，本地提交、未 Push；盘点时仅总控综合计划存在未提交修改

## 1. 结论

1. 当前非 Qt public adapters 已具备“复制 adapter 源码到仓库外、只通过安装包的 `PAE::pae`
   消费”的局部事实：Binary H1、ASCII A1/A2/stream 的公开 adapter target 均只 include `pae/**`
   公开头并链接 `PAE::pae`；既有 static Debug/Release 包外日志是局部证据。
2. 整个 Qt Lab **尚不能**仅凭“Lab 源码 + 安装版 PAE SDK + 随仓 Qt”独立构建。当前
   `pae_protocol_lab_ui_headless_internal` 无条件依赖旧 C1 execution、private Compiler/Core/Plan/yyjson；
   多个活动头直接 include `src/**`，`schema_dispatch.cpp` 还直接 include 未安装的 `yyjson.h`。
3. 现有推荐 preset `windows-msvc-pae-lab` 是开发仓库一体化入口：它从同一顶层 CMake 构建 PAE
   私有 target、legacy adapters 和 Qt Lab，不是安装 SDK 的包外入口。`package_sdk_stage3.ps1` 只打包
   PAE，不打包 Lab 或 Qt，也不能替代 Lab standalone CMake。
4. 下一片应先用安装版 **static Debug/Release** SDK 闭合整个 Qt Lab 的源码/链接边界，再单独考虑
   shared SDK 的 `pae.dll` 部署。这样能先隔离“private 源码依赖”与“DLL 运行闭包”两类风险；这不是
   建议删减功能，功能覆盖必须以并行的 Lab 运行路径报告和总控合并结论为准。
5. 需要总控/用户拍板的核心问题不是是否复制更多仓库目录，而是：legacy 0.5-0.8/C1 功能如何在
   public-only 构建中保留，以及顶层 Schema dispatch 的 yyjson 依赖归属。未拍板前不应新增 PAE
   公共 API、引入新的 Lab 第三方包或关闭旧功能来“刷过”独立构建。

## 2. 当前 target、源码与链接闭包

### 2.1 已有非 Qt 安装 SDK 消费边界

| Target | 当前源码 | 公开依赖 | 私有 PAE 依赖判断 |
| --- | --- | --- | --- |
| `pae_protocol_lab_binary_public_h1` | `tools/protocol_lab_binary/public_binary_decode.{h,cpp}` | `pae/compiler.h`、`pae/host_endpoint.h`、`PAE::pae` | target 本身未 include `src/**`，未链接 private PAE target |
| `pae_protocol_lab_ascii_public_a1` | `public_ascii_offline_adapter.{h,cpp}`；A2/stream 时再加 `public_ascii_host_adapter.{h,cpp}` | `pae/compiler.h`、`pae/codec.h`、`pae/host_endpoint.h`、`PAE::pae` | target 本身未 include `src/**`，未链接 private PAE target |
| `pae_protocol_lab_ui_binary_public_h2` | `binary_host_adapter_public.{h,cpp}`、`public_binary_description.{h,cpp}` | public H1 + public PAE types | 该 target 自身无 private PAE link；仍不是整个 UI 闭包 |

[`tools/protocol_lab_ascii/CMakeLists.txt`](../../tools/protocol_lab_ascii/CMakeLists.txt) 和
[`tools/protocol_lab_binary/CMakeLists.txt`](../../tools/protocol_lab_binary/CMakeLists.txt) 仍给这些 target
追加仓库内部 `pae::project_options`；仓库外入口需改为 Lab 自己的 warnings/options target，不能要求
安装 PAE SDK 导出该内部 target。

这些 target 的源码闭包与顶层 option 门禁也要分开判断：当前根 CMake 仍要求 public H1/A1 同时
开启仓库内 `PAE_BUILD_PUBLIC_API_STAGE1`，A2/stream 还要求 private `PAE_BUILD_HOST_ENDPOINT_SLICE`，
stream UI 继续要求 legacy ASCII stream observer 和 Host observer。standalone 入口应按实际 target
依赖重建 fail-closed 条件，不能照搬这些为一体化源码构建服务的 option 前置，也不能为满足门禁而
把 private PAE 源码带入外部根。

既有 [0.11 非 Qt 验证](lab-public-ascii-stream-validation.md) 记录 static D/R consumer 的
`CMAKE_PREFIX_PATH`、`PAE_DIR` 均指向
`out/sdk-public-stream-description/candidate1-20260918/pae-sdk-static-{debug,release}`，consumer CMake
只执行 `find_package(PAE CONFIG REQUIRED)` 并链接 `PAE::pae`。这是 adapter 局部闭包证据，不是 Qt
Lab 可独立构建证据。该候选的 [SDK 验证报告](pae-public-stream-sdk-validation.md) 仍记录生成时
dirty provenance；本轮不重算、重打或将它改称 `c5b3692` 的干净发布物。

### 2.2 整个 Qt Lab 的当前硬依赖

当前 [`tools/protocol_lab_ui/CMakeLists.txt`](../../tools/protocol_lab_ui/CMakeLists.txt) 形成如下闭包：

```text
pae_protocol_lab_ui.exe
├─ PAE::Qt5Widgets -> PAE::Qt5Gui -> PAE::Qt5Core
└─ pae_protocol_lab_ui_headless_internal
   ├─ pae_protocol_lab_c1_execution_internal
   ├─ pae_config_compiler
   ├─ pae_protocol_core_slice
   ├─ pae::protocol_plan
   ├─ pae::yyjson
   ├─ legacy ASCII adapter（Schema 0.10 开启时当前仍强制）
   ├─ public Binary H1/H2（相应开关开启时）
   └─ public ASCII A1/A2/stream（相应开关开启时）
```

直接 private/source-root 耦合证据：

| 文件 | 当前直接依赖 | 对包外构建的影响 |
| --- | --- | --- |
| `tools/protocol_lab_ui/compile_worker.h` | `src/config_compiler/config_compiler.h`，同时含 `pae/compiler.h` | public route 已存在，但 private completion/artifacts 仍进入公共 UI worker 类型 |
| `tools/protocol_lab_ui/description_mapping.h` | `src/config_compiler/ui_description.h`、`src/protocol_plan/plan_bundle.h`；并条件 include legacy ASCII adapter | UI DTO 仍同时承载 private Plan/sidecar 与 public DTO 映射 |
| `tools/protocol_lab_ui/document_session.h` | `tools/protocol_lab/v06_execution.h`、legacy ASCII/Host 类型 | Session/TypedDraft/PreparedDocument 仍持有 C1/private owner 和 legacy adapter |
| `tools/protocol_lab/v06_execution.h` | `src/protocol_plan/plan_memory.h` | `pae_protocol_lab_c1_execution_internal` 无法由二进制 SDK 提供 |
| `tools/protocol_lab_ascii/ascii_offline_adapter.h` | private Compiler/Core/Framing | 当前 UI CMake 在 Schema 0.10 开启时仍无条件要求 legacy ASCII target |
| `tools/protocol_lab_ascii/host_observer_adapter.h` | private `src/host_endpoint` | legacy Host observer 不是安装 SDK 消费者 |
| `tools/protocol_lab_ui/schema_dispatch.cpp` | `<yyjson.h>` | 安装 PAE SDK 不安装 yyjson 头，也不将 `pae::yyjson` 作为公共消费接口 |

另外，顶层 [`CMakeLists.txt`](../../CMakeLists.txt) 在 UI 开启时主动加入
`src/protocol_plan`、`src/config_compiler`、`src/protocol_core`，并为 public API 加入 `src/public_api`；
这使现有开发构建天然掩盖包外缺失。当前 `PAE_BUILD_PROTOCOL_LAB_UI` 还会强制旧 Loader/Core 与
Schema 0.5-0.7 开关，不能把开发仓库 Configure 成功当作 installed-SDK closure。

Binary legacy materializer 的 private includes 仍存在于 `candidate_materializer.h`、
`owned_description.h` 等文件，但 public H2 分支的 `binary_host_adapter.h` 会在宏成立时排除其旧实现，
且 `pae_protocol_lab_binary_materializer` 可关闭。因此它是否进入未来白名单应由功能审计确认，不按目录
整体复制，也不在本报告推定可删除。

## 3. Qt 构建与部署闭包

[`cmake/PaeQt513.cmake`](../../cmake/PaeQt513.cmake) 当前提供了有价值的 fail-closed 门禁：

- Windows + MSVC + Visual Studio generator + x64；
- 显式 `-T v142,version=14.29.30133`，`cl.exe` 路径与版本 `19.29.30159.0`；
- `PAE_QT_ROOT` 中 Qt 5.13.x `qconfig.h`、Core/Gui/Widgets 的 D/R import lib 与 DLL；
- `moc.exe`、`rcc.exe`、`uic.exe` 和 D/R `qwindows` platform plugin。

随仓 [`third_party/qt`](../../third_party/qt) 当前包含 build/runtime 所需的固定副本。整个副本的来源、
2440 文件清单与验证边界见 [`third_party/qt-package.md`](../../third_party/qt-package.md)；本轮不裁剪、
复制或重新校验。其许可附件/来源分发审查尚未闭合，不能因本地独立构建通过就升级为可正式分发。

[`cmake/DeployProtocolLabUi.cmake`](../../cmake/DeployProtocolLabUi.cmake) 当前运行白名单为：

- `pae_protocol_lab_ui.exe`；
- `Qt5Core[d].dll`、`Qt5Gui[d].dll`、`Qt5Widgets[d].dll`；
- `platforms/qwindows[d].dll`；
- 显式 `.pae.json` 合成配置。

它会限制部署目录必须位于给定 allowed root 下、只接受 Debug/Release，并在目标目录内重建部署。
这些安全门禁应保留，但当前 helper 的配置白名单和输出路径绑定 `PROJECT_SOURCE_DIR`/
`PROJECT_BINARY_DIR`，且只处理 Qt runtime：

- 若下一片先用 static PAE SDK，现有运行闭包不需 `pae.dll`；
- 若以后验证 shared PAE SDK，部署必须从 `PAE::pae` 的 imported location 复制匹配配置的
  `pae.dll`，并检查其与 SDK 包内 DLL 哈希一致；不得从开发仓库 build tree fallback；
- MSVC CRT 仍由匹配的已安装 runtime 提供，不从仓库复制系统 DLL。

当前部署配置来源分散在 `tests/protocol_lab_ui/fixtures`、`examples/config` 和生成目录。包外输入应把
总控确认仍需覆盖的合成配置复制到单一 Lab-owned `configs/` 白名单，并由 standalone CMake 显式列出；
不能让部署脚本回读开发仓库的 `tests/**` 或 `examples/**`。本报告不删减任何现有配置或功能。

## 4. 候选输入白名单

这里的白名单用于未来专用验证根，不是正式发布清单。

### 4.1 Lab 源码输入

1. 独立顶层 `CMakeLists.txt` 和 Lab-local warnings/options target；不得 include 仓库顶层 CMake。
2. `tools/protocol_lab_ui/` 中经功能审计确认的 headless、Qt widget、public H2/A2/stream 源码。
3. `tools/protocol_lab_binary/public_binary_decode.{h,cpp}`。
4. `tools/protocol_lab_ascii/public_ascii_offline_adapter.{h,cpp}` 与
   `public_ascii_host_adapter.{h,cpp}`。
5. `tools/protocol_lab/sha256.{h,cpp}` 等真正属于 Lab 的通用工具；其余 C1/legacy 源码只有在总控确认
   保留方式后才进入兼容层白名单，不能整体复制 `tools/protocol_lab/**`。
6. Lab-owned 合成配置、最大 fixture 生成脚本、Qt import/deploy helper 的 standalone 版本。
7. 验证专用测试源单列为 testing whitelist，不进入产品运行目录。

凡仍含 `../../src/**` 的文件不得进入“public-only 已闭合”集合；若为保留功能必须暂时存在，应明确
标记为 blocker/compatibility slice，而不是把对应 `src/**` 一并复制进 Lab 包。

### 4.2 安装版 PAE SDK 输入

- 只通过 `PAE_SDK_ROOT` 定位安装包根；CMake 使用
  `find_package(PAE CONFIG REQUIRED PATHS "${PAE_SDK_ROOT}" NO_DEFAULT_PATH)`；
- consumer 只 include 安装包 `include/pae/**`，只链接 `PAE::pae`；不点名 SDK 内部 `.lib`；
- Debug/Release 各用对应单配置包，保留既有
  `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_*_PACKAGE` 负向门禁；
- 下一片可把现有 `candidate1-20260918` static D/R 作为已知本地输入，但必须保留它原有 dirty
  provenance；若要求与 `c5b3692` 建立新发布身份，应另行授权生成/验证新 SDK，不能在本预检中推定。

### 4.3 随仓 Qt 输入

- 仅通过显式 `PAE_QT_ROOT` 指向复制到独立验证根的固定 Qt 目录，或在本地开发预检中指向当前
  `third_party/qt`；不得搜索系统 Qt、Qt 5.11、`PATH` 或 DEI 原目录作为 fallback；
- build inputs：QtCore/Gui/Widgets headers、D/R import libs、`moc/rcc/uic`；
- runtime inputs：匹配配置的 Core/Gui/Widgets DLL 与 `qwindows[d].dll`；
- 完整 Qt 副本是否作为未来 Lab 包输入、能否裁剪和如何满足许可证/来源要求，需单独拍板。本报告不
  根据当前链接模块推定可删除副本中其他文件。

## 5. 禁止开发仓库 fallback 的门禁建议

1. **物理隔离：**把白名单 Lab 源码、安装 PAE SDK 和 Qt 副本复制到仓库外新的只读输入根；Configure
   时不提供开发仓库路径。单纯在仓库内换 `CMAKE_PREFIX_PATH` 不足以证明无相对 include 回退。
2. **PAE 定位：**`NO_DEFAULT_PATH` 查找后读取 `PAE_DIR`，确认其位于 `PAE_SDK_ROOT`；缺包或错配置
   必须失败，不允许转为 `add_subdirectory(PAE_SOURCE_DIR)`。
3. **Qt 定位：**全部 imported target 的 include/import-lib/runtime/tool 路径必须位于规范化后的
   `PAE_QT_ROOT`；不调用通用 `find_package(Qt5)`，不读环境中的 Qt/CMake package registry。
4. **源码门禁：**静态扫描 Lab product sources 的 `../../src/`、private target 名、绝对开发路径和
   `<yyjson.h>`；允许项必须有总控确认的兼容层清单，零命中前不得宣称 public-only。
5. **生成物门禁：**检查 `CMakeCache.txt`、`.vcxproj`、`.props`、link command 和 compiler include
   trace；允许源路径只属于外部 Lab root，PAE include/lib 只属于 SDK root，Qt 只属于 Qt root。
6. **负向证明：**分别移除/改错 PAE SDK root、Qt root、Debug/Release 包配置，确认 Configure 或 Link
   在明确门禁处失败；不得因机器上恰有仓库 build tree、Qt 或 package registry 而成功。
7. **运行门禁：**用受控 PATH/清空 Qt 相关环境启动部署目录，检查 EXE 实际加载的 PAE（shared 时）、
   Qt DLL 和 platform plugin 均来自部署目录；不把一次启动成功扩大为历史访问违例已解决。

## 6. 最小独立构建入口改动清单

以下是建议实施顺序，不是本轮修改授权：

1. 在 Lab 范围新增 standalone 顶层入口，例如 `tools/protocol_lab_ui/standalone/CMakeLists.txt`；只接收
   `PAE_LAB_SOURCE_ROOT`、`PAE_SDK_ROOT`、`PAE_QT_ROOT`、配置根和部署根，创建 Lab-local options
   target，并用 `find_package(PAE ... NO_DEFAULT_PATH)`。
2. 将 Qt toolchain/import 逻辑提取为不依赖仓库顶层的 Lab module；保留当前严格 MSVC/x64/v142/Qt
   文件门禁。不要改全局 PATH 或本机 Qt。
3. 总控先合并并行功能报告，再把 `compile_worker`、`description_mapping`、`document_session` 中仍需的
   private/C1/legacy 类型按运行路径逐项迁移或隔离。不得通过关闭 Schema、移除按钮或不编译旧分支
   来伪造闭包；若公开能力不足，留证停报并单独决策 API。
4. 单独处理 Schema dispatch 的 yyjson 归属：可选方向是批准 Lab 自有、带许可证的最小 parser
   依赖，或由既有公开编译结果消除二次分类需求，或另行设计公开只读分类能力。三者外部依赖/API/
   重复编译语义不同，不能由构建整理任务代选。
5. 将部署 helper 参数化为 Lab/config/Qt/PAE runtime 明确输入，移除对开发仓库 `tests/examples` 路径
   形状的依赖；保留 bounded deployment root 和 D/R 门禁。static 首片不复制 PAE DLL；shared 后片
   从 imported target 复制并核对。
6. 为 standalone 入口增加 Testing-off product gate 和可选 Testing-on target；推荐仓库内 preset 保留
   为开发入口，新增外部预检入口不能静默回退到它。

## 7. 未来 Windows Debug/Release 验证方案

### 第 0 步：静态 closure gate

- 冻结总控确认的功能矩阵和源码白名单；检查 private include/target、yyjson、绝对路径和配置来源；
- 生成外部输入根后对三类输入分别记录文件清单、来源和 SHA-256；不复用开发 build directory。

### 第 1 步：非 Qt adapters 基线

- 使用安装版 static Debug/Release SDK，在仓库外重新编译 public Binary H1 与 ASCII A1/A2/stream
  adapters；只链接 `PAE::pae`，记录 `PAE_DIR`、include/lib origin、完整命令和退出码；
- 运行现有定向 consumer/断言以确认迁移前基线。既有日志可作历史对照，不能替代新 standalone
  输入白名单下的执行。

### 第 2 步：整个 Qt Lab static D/R

- 串行 Configure/Build，Testing-on 跑总控选定的 headless/public adapter/UI regression 与 Qt smoke；
- 另做 Testing-off product build，确认无 tests、private PAE target、开发仓库 include/lib；
- 部署 D/R EXE + 匹配 Qt DLL/plugin + 白名单 configs，在清理 Qt 环境和受控 PATH 下启动；记录 EXE、
  Qt DLL/plugin 与配置哈希；
- 对照并行功能审计覆盖全部保留入口，至少证明 load/dispatch、Binary、ASCII complete、ASCII stream、
  Host/Flow、Encode/Decode 和窗口关闭生命周期没有因构建拆分被静默禁用。具体用例由总控合并确定。

### 第 3 步：shared PAE 后置验证

- 分别使用 shared Debug/Release SDK 重做 Configure/Build/Run；部署匹配 `pae.dll`；
- 检查 `dumpbin /dependents`、实际 DLL 来源与包内哈希，验证 D/R 错配稳定拒绝；
- 只有 static 与 shared 均在仓库外通过，才能声明两种 installed SDK 形态支持整个 Qt Lab；任一局部
  adapter PASS 都不能替代此结论。

所有步骤仍只证明指定 Windows x64/MSVC/Qt 组合；不等于稳定 ABI、Linux、正式发布、许可证闭合、
历史 Qt 访问违例修复、真实协议、硬件、现场或生产验证。

## 8. 下一片、依赖先后与拍板项

建议最小下一片：**总控合并本报告与 `lab-public-consumption-residual-audit.md`，冻结“功能必须保留”
矩阵和 private 依赖处置表；随后只做 static D/R standalone CMake + private-source fail-closed 门禁，先
不做 shared 部署和正式打包。**

依赖顺序：

1. 功能/运行路径审计定稿；
2. C1/legacy DTO/owner/执行依赖迁移或明确兼容层；
3. yyjson/Schema dispatch 归属拍板；
4. standalone static D/R；
5. shared D/R 与 `pae.dll` 部署；
6. 许可证与正式分发另行处理。

必须拍板：

- legacy Schema 0.5-0.8/C1 功能是迁移到现有公开能力、保留 Lab 私有兼容组件，还是确有新的 PAE
  public API 缺口；本报告不选择，也不接受删功能规避；
- Schema dispatch 使用何种依赖边界；新增第三方依赖或公共 API 均需单独授权；
- 首片验收只要求 static D/R，还是同时要求 shared D/R；建议先 static、后 shared；
- Qt 固定副本在未来交付中是完整随包还是另行安装输入，以及正式外发前许可证/来源材料如何闭合。

## 9. 本轮验证边界

本轮只读取 AGENTS、实时 Git、综合计划、根/局部 CMake、Lab/Qt/SDK 源码与既有报告；只新增本报告。
未 Configure、Build、Test、运行 Lab、创建构建目录/包/脚本，未复制/下载/部署/删除文件，未重算全
SDK 或 Qt 清单，未 Stage/Commit/Push/发布。文中 future gate/command 均为建议，尚无执行证据。
