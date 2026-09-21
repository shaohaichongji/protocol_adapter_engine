# 协议元数据内部命名整理验证（2026-09-20）

## 1. 范围与基线

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 分支与基线：`main@fa81329563dd3aea9bb167ef9bd34526a2606161`
- 授权依据：`docs/archive/engineering-20260921/protocol-metadata-naming-plan-20260920.md`
- 本次仅整理 config compiler 内部协议元数据命名及其直接引用；公开 API、Schema、布局、预算、所有权和执行顺序未改变。
- 接管时已有入口、清理和 G2 交付相关 Markdown 修改均保留。本报告是本任务唯一新增 Markdown。
- 未执行 Stage、Commit、Push、SDK 重打包、部署覆盖、网络收发或人工 UI 验收。

执行过程中主机曾关机。恢复检查确认无残留 `cmake`/`msbuild` 进程，Git 基线、空暂存区和工作区修改均保留；中断发生在新构建目录配置完成、尚未开始构建的位置，因此从构建阶段继续，没有清理或重建既有受保护目录。

## 2. 实际修改

### 2.1 文件和固定符号

按已确认映射完成以下内部文件重命名：

- `src/config_compiler/ui_description.h` → `protocol_metadata.h`
- `src/config_compiler/ui_description_internal.h` → `protocol_metadata_internal.h`
- `src/config_compiler/ui_description.cpp` → `protocol_metadata.cpp`
- `tests/config_compiler/ui_description_tests.cpp` → `protocol_metadata_tests.cpp`

固定类型、函数、测试探针及布局测试名称全部迁移到 `ProtocolMetadata*` / `Compile*WithMetadata` 命名。内部异常诊断中误导性的 `UI compiler` 表述改为 `protocol metadata compiler`；错误码、阶段和失败分支未变。

测试 target、CTest 名和标签 `pae_config_compiler_ui_description_tests`、`pae.config_compiler.ui_description`、`ui_description` 有意保留，避免破坏已有脚本和历史命令。Lab 自有 `UiDescriptionBytes`、`ui_description_bytes_` 及 UI 展示语义也按契约保留，不做全局 `Ui` 替换。

### 2.2 直接引用

同步更新：

- 编译器与冻结产物：`src/config_compiler/{CMakeLists.txt,config_compiler.cpp,config_compiler.h,schema_ir.h}`；
- public implementation（非公开头）：`src/public_api/{compiled_state_internal.h,compiler.cpp}`；
- SDK 源包白名单：`scripts/package_sdk_stage3.ps1`；
- 编译器与 public header 边界测试：`tests/config_compiler/CMakeLists.txt`、`tests/public_api/verify_public_headers.cmake`；
- Framer、ASCII、Binary 和 Lab UI 的直接消费者及对应测试。

受影响的消费者文件为：

- `tools/protocol_lab_ascii/{ascii_offline_adapter.cpp,ascii_offline_adapter.h,host_observer_adapter.cpp,host_observer_adapter.h}`；
- `tools/protocol_lab_binary/{owned_description.cpp,owned_description.h,prepared_binary.cpp,prepared_binary.h}`；
- `tools/protocol_lab_ui/{ascii_host_adapter_compat.cpp,ascii_host_adapter_compat.h,binary_host_adapter.cpp,binary_host_adapter.h,compile_worker.cpp,compile_worker.h,description_mapping.cpp,description_mapping.h,document_session.h}`；
- `tests/protocol_framing/stream_framer_tests.cpp`；
- `tests/protocol_lab_ascii/{ascii_offline_adapter_tests.cpp,host_observer_adapter_tests.cpp}`；
- `tests/protocol_lab_binary/{encode_tests.cpp,integration_audit_tests.cpp,owned_description_tests.cpp,prepared_binary_tests.cpp,saved_state_tests.cpp,stream_tests.cpp}`；
- `tests/protocol_lab_ui/{ascii_session_tests.cpp,ascii_stream_session_tests.cpp,binary_stage1_tests.cpp,host_session_tests.cpp,test_support.h}`。

根 `CMakeLists.txt` 和 `include/pae` 无差异。两个 CMake 修改仅替换源文件名及一个内部循环变量，没有新增 link 依赖；config compiler 的非 Qt 依赖方向不变。测试探针仍是 config compiler 内部的 thread-local scoped probe，未增加公开测试接口或改变注入时机。

## 3. 静态门禁

静态审计日志：`out/validation/protocol-metadata-20260920/static-audit.log`。

- 固定旧符号扫描：0 个命中；
- 旧源文件名扫描：0 个命中；
- 四个新文件均存在，四个旧路径均不存在；
- 剩余 `UiDescription` / `ui_description` 仅为明确保留的 Lab 自有计费名称，以及历史兼容的测试 target、CTest 名和标签；
- SDK 白名单只引用三个新 `protocol_metadata` 路径，路径均存在，无旧路径；
- `git diff -- include/pae`：无差异；
- `git diff -- CMakeLists.txt`：根 CMake 无差异；
- `git diff --check`：退出 0；仅提示 `scripts/package_sdk_stage3.ps1` 工作区 LF 将来可能由 Git 转为 CRLF，无空白错误。

### 3.1 整文件格式化噪声返修

此前曾对 40 个修改的 C++/头文件执行整文件 clang-format 22.1.3，导致基线中原本不属于命名整理的排版被重排。总控限定返修后，以 `git show HEAD:<path>` 为基准，仅施加固定符号映射、授权诊断文字和必要局部命名，再用 `apply_patch` 逐项恢复原排版；未使用 checkout/reset，也未再次运行整文件格式化。

返修后的 40 文件归一化比较结果：

- 36 个文件在施加固定映射和下述授权文字变换后与 HEAD 文本完全一致；
- `tests/protocol_lab_binary/stream_tests.cpp`、`tools/protocol_lab_binary/owned_description.h`、`tools/protocol_lab_binary/prepared_binary.h` 仅因新符号长度不同保留局部换行差异，token 序列一致；
- `src/config_compiler/config_compiler.cpp` 除授权变换外只调整 include 顺序，使 `protocol_metadata_internal.h` 位于 `schema_ir.h` 之前；其余 token 序列一致；
- `tools/protocol_lab_ui/binary_host_adapter.cpp` 已由返修前 85/60 行 diff 收缩为 1/1 行类型名替换；`description_mapping.cpp` 由 37/22 行收缩为 3/3 行（类型名和一条授权诊断）；未发现额外控制流、数据流、常量或调用变化。

归一化时允许且逐项解释的非固定映射只有：

- config compiler/metadata 实现中的局部变量 `sidecar` → `metadata`，测试局部变量 `invalid_ui` → `invalid_metadata`；
- “UI compiler/configuration compilation/UI description” 等内部诊断改为 “protocol metadata compiler/compilation/protocol metadata”，错误码和分支不变；
- `UI pipeline metadata span` 改为 `Pipeline metadata span`；
- test-only 注释中的 `UI compile call boundary` 改为 `protocol metadata compile boundary`；
- `tests/config_compiler/CMakeLists.txt` 的内部循环变量 `pae_ui_description_example` → `pae_protocol_metadata_example`。

返修后的工作树未再执行整文件 clang-format；此前“40/40 clang-format 通过”只适用于返修前的整文件格式化状态，不作为当前最小 diff 的验证结论。当前格式门禁仅记录 `git diff --check` 通过。

## 4. Windows 构建与测试

### 4.1 配置

```powershell
cmake --preset windows-msvc-stage4-h2 `
  -B out/build/windows-msvc-protocol-metadata-20260920 `
  -T v142,version=14.29.30133
```

结果：退出 0，MSVC 19.29.30159、v142 14.29.30133、仓库 Qt 5.13.0；日志：
`out/validation/protocol-metadata-20260920/configure.log`。

### 4.2 构建

```powershell
cmake --build out/build/windows-msvc-protocol-metadata-20260920 --config Debug -- /m:4
cmake --build out/build/windows-msvc-protocol-metadata-20260920 --config Release -- /m:4
```

结果：Debug、Release 全目标均退出 0。格式化后的 Debug 增量构建再次退出 0。Release 构建日志明确显示测试目标用 `/UNDEBUG` 覆盖 `/DNDEBUG`，断言保持启用。以上构建发生在排版噪声返修前；返修只恢复空白/换行，40 文件归一化比较未发现额外逻辑 token 变化，因此本次按限定要求未重跑构建。

日志：

- `out/validation/protocol-metadata-20260920/build-debug.log`
- `out/validation/protocol-metadata-20260920/build-debug-after-format.log`
- `out/validation/protocol-metadata-20260920/build-release.log`

### 4.3 定向 CTest

Debug 与 Release 串行执行同一组 31 项相关测试，覆盖：

- config compiler 协议元数据专项；
- public Compiler、Codec、consumer metadata、physical query、ASCII facts、StreamFramer、HostEndpoint 和公开头边界；
- 公开 Binary H1 / stream 与 Framer；
- ASCII adapter / host adapter；
- Lab UI owned presentation、Binary stream Qt offscreen、description mapping、compile queue、document/session/state、Binary H2/header 等 headless 消费者。

结果：

- Debug：31/31 通过，退出 0；
- Release：31/31 通过，退出 0。

以上 CTest 同样发生在排版噪声返修前。返修后的 token/文本比较边界见第 3.1 节；本次未把历史通过时间点改写为返修后重新执行。

日志：

- `out/validation/protocol-metadata-20260920/ctest-debug-final.log`
- `out/validation/protocol-metadata-20260920/ctest-release-targeted.log`

全部测试均为离线/隐藏执行；未进行 UDP 或其他网络收发，未启动人工可见窗口。

## 5. 证据缺口与未验证项

初次把完整应用测试 `pae.tools.protocol_lab_ui.qt_smoke_binary_stage1` 纳入 Debug 选择时，CTest 因 `bin/Debug` 缺 Qt DLL 以 `0xc0000135` 启动失败；该结果保存在 `out/validation/protocol-metadata-20260920/ctest-debug-targeted.log`。补充 Qt 运行路径并以 `offscreen` 重跑后，当前 Debug 测试以 SegFault 退出；该结果保存在 `out/validation/protocol-metadata-20260920/ctest-debug-qt-stage1-rerun.log`。这两次失败均未计入第 4.3 节的 31 项通过结果。

执行过程中还调用过当前部署产物及断电前 G2-C common 部署产物的 Debug/Release 对照命令，交互式工具输出中曾显示异常退出码；但原命令指定的以下四份文件实际均未落盘，在仓库 `out` 下也未找到同名文件：

- `debug-qt-stage1-deployed.log`、`baseline-debug-qt-stage1-deployed.log`
- `release-qt-stage1-deployed.log`、`baseline-release-qt-stage1-deployed.log`

因此，原报告中依据“本轮与基线退出结果相同”进一步断言“不是本次引入”的结论缺少可持久复核证据，现撤回该结论。本次按总控限定要求未重新运行崩溃程序，也未扩修 UI；完整应用 smoke 与本次改名之间的关系保持未证明。现有可追溯证据只能支持：第 4.3 节列出的 31 项定向测试在 Debug/Release 均通过，而完整应用 `qt_smoke_binary_stage1` 未通过且基线对照日志缺失。

本任务未重打 SDK、未做包外消费、未做人工 UI、Linux、真实设备、现场或性能验证；这些边界不因 Windows 定向测试通过而升级。

## 6. 结论

固定命名映射、直接消费者引用、CMake 源列表和 SDK 源白名单已同步；公开 API/Schema、非 Qt 依赖方向及执行语义保持不变。整文件格式化噪声已移除，40 文件归一化比较未发现额外逻辑修改；`git diff --check` 通过。Debug/Release 全目标构建与各 31 项定向测试是返修前证据，因返修仅恢复空白/换行而未重复执行。完整应用 smoke 的基线对照仍缺少落盘日志，不能据现有证据排除本次回归；是否可进入提交候选由总控结合实际 diff 和该证据缺口决定。

状态：**已完成派发范围，待总控复核**。

总控限定复核：已检查内部编译链、直接消费者与打包白名单差异；三份新元数据文件对照 HEAD 的固定映射/局部名/文字变更检查未见额外逻辑差异。确认公开头、Schema、根 CMake 无差异，读取现存 D/R 日志各31/31通过；无关整文件排版噪声已收窄，diff检查通过。命名整理可限定收口，但不表示完整应用 smoke 验证通过；其失败归因及缺失基线证据仍保留，未重跑、未提交推送。
