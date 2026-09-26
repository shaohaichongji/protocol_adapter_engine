# YAML 可选 SDK 静态组件首片验证（2026-09-26）

## 结论与范围

本片按 `yaml-sdk-component-contract-20260926.md` 落实默认关闭的独立
`PAE::yaml_frontend` 静态附加组件。公开头仅提供转换状态、自有 JSON/来源映射的
move-only owner、值类型来源位置和试用资源约束 V0.1 查询；严格 JSON 仍交给原
`CompileProtocolJson`。Windows x64 的内部/公开专项、static/shared PAE 的
Debug/Release 安装树包外 consumer、JSON-only 及条件发现、配置错配、Release
搬迁均有下述实测。shared PAE 是 `pae.dll` 加静态 YAML 库，没有 YAML DLL。

本轮基线 `main@4da54b9bdc6fa3fed0f81ab346c2fe97a9a6329e`；接管时 Lab/总控
文档已有未提交变化，全部保留。本片不是正式 source SDK 包、发布物、人工体验或
稳定跨工具链 ABI；未 Stage、Commit、Push、删除或替换现用部署。输出只在忽略的
`out/build/yaml-sdk-component-20260926`、`out/evidence/yaml-sdk-component-20260926`。

## 实现与所有权

- `src/config_frontend_yaml/public/pae/yaml_frontend.h` 与 `public_facade.cpp`：
  `pae::yaml::ConvertToStrictJson` 同步借用输入，成功后结果自持 JSON、来源身份和
  JSON Pointer 映射；失败 `Json()`/来源查询为空。`ConversionResult` 的构造、
  移动、赋值和析构实现在组件内。移动后原对象可安全查询/销毁；已有 view 在
  owner 移动、赋值或销毁后不可继续使用。
- `FindSource` 仅查精确 Pointer；`FindNearestSource` 在祖先回退时设置
  `ancestor_fallback=true` 且对应 key/value 的近似标志也为 true。两者返回复制的
  行列与近似标志，不返回内部池指针。
  语法转换失败仍仅提供状态和原因，不臆造原 YAML 行列或修改编译器诊断。
- 内部 `Limits`、故障注入、Parser 分配计数及 `Result` 存储布局没有进入公开头。
  `TrialResourceLimitsV01()` 只读反映现值：输入 16 KiB、Parser/辅助各 128 KiB、
  JSON 32 KiB、512 节点、16 层、4 KiB 标量。新公开 owner 成功时额外进行
  **一次 `Impl` 分配**，不在内部 Parser/辅助预算中；分配失败安全返回
  `ALLOCATION_FAILED` 且无部分结果。该增量及分配器元数据、CRT/栈、进程 RSS
  未被宣称为全内存精确上界。内部转换算法与参数默认值未改。
- `cmake/PaeSdkInstall.cmake` 开关 ON 才安装静态附加库、专属公开头、Profile、
  rapidyaml 官方 License/内嵌通知及自包含合成 consumer；OFF 不安装这些文件。
  `cmake/PAEConfig.cmake.in` 为有组件包建立独立 imported target，缺组件请求
  明确拒绝；重复 `find_package` 不被既有 `PAE::pae` target 早退吞掉。
- `examples/yaml_sdk_consumer` 从包内 YAML 生成严格 JSON，真实调用公开 Compiler
  和 Codec，对独立已知帧 `AA 00 07` 验证 Decode/Encode。它只 include 安装头，
  链接安装目标；没有仓库 `src` 或旧 `out` 头路径。

## 实际命令与结果

环境为 Visual Studio 18 2026、MSVC v142 `14.29.30133`、x64；同一专项构建根的
Debug 与 Release 串行。表中日志均相对于上述 `out/evidence` 根。配置和消费命令
使用本地包根作为 `CMAKE_PREFIX_PATH`，不把固定机器绝对路径写进源码候选。

| 检查 | 命令要点 | 实际结果与日志 |
| --- | --- | --- |
| 非 Qt 专项 Debug/Release | `cmake -S tests/config_frontend_yaml -B out/build/yaml-sdk-component-20260926/specialized -G "Visual Studio 18 2026" -A x64 -DPAE_SOURCE_DIR=<仓库根>`；两配置依次构建 `pae_yaml_frontend_tests pae_yaml_frontend_public_tests` 并运行 `ctest -C Debug/Release -V` | 最终各 **2/2 CTest**；既有前端程序各 95 条 `PASS`，公开 owner 程序各 10 条 `PASS`。最终源码对应 `specialized-{debug,release}-{build,ctest}-source-final.log`；较早逐项输出和失败日志保留。 |
| static PAE + YAML 安装树 | 根配置 `BUILD_SHARED_LIBS=OFF`、公开 API/YAML ON、Testing/Lab OFF；D/R 分别构建 `pae_public_api pae_yaml_frontend`、`cmake --install --config Debug/Release --prefix <独立包根>` | 配置/构建/安装退出 0；`static-configure.log`，最终源码对应 `static-{debug,release}-build-source-final.log`、`static-{debug,release}-install-source-final.log`。 |
| static 包外 consumer | 从各自**安装包**的 `examples/yaml_sdk_consumer` 配置，`find_package(PAE COMPONENTS yaml_frontend)`，同配置构建并运行包内 YAML | Debug/Release 各退出 0，输出 `PAE_YAML_SDK_CONSUMER_PASS`；最终见 `static-{debug,release}-consumer-{build,run}-source-final.log`，首次配置见同名 `*-configure.log`。 |
| shared PAE + 静态 YAML | `BUILD_SHARED_LIBS=ON`，其余开关同上；D/R 分别安装并作包外消费 | 两配置均配置/构建/安装/运行退出 0；最终 `shared-{debug,release}-build-source-final.log`、`shared-{debug,release}-install-source-final.log` 和 `shared-{debug,release}-consumer-{build,run}-source-final.log`。三组 shared consumer（含搬迁）旁 `pae.dll` 与各自包内 SHA-256 相等，无 `pae_yaml_frontend.dll`；身份复核见末次检查日志。 |
| JSON-only | YAML OFF、公开 API ON、Testing/Lab OFF，安装独立 static Release 包；从该包构建/运行原 `getting_started` | 构建/安装/包外运行退出 0，输出 `GETTING_STARTED_BINARY_PASS`；无 YAML 头、附加库、示例或 rapidyaml 许可目录；`json-only-*`、`json-only-closure.log`。 |
| 缺组件与重复发现 | 隔离最小 CMake 先 `find_package(PAE CONFIG REQUIRED)`，再 `find_package(PAE CONFIG REQUIRED COMPONENTS yaml_frontend)` | 有组件包 Configure 退出 0（`find-repeat-positive-final.log`）；JSON-only 包退出非零，精确理由 `PAE package does not contain requested yaml_frontend component`（`find-repeat-missing-explicit.log`）。 |
| Debug/Release 错配 | Debug consumer 指向 static Release 包 | Configure 成功，Build 非零并命中 `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_RELEASE_PACKAGE.lib`；`mismatch-{configure,debug-build}.log`。 |
| 包搬迁 | 把最终 static/shared Release 安装树分别复制到含空格的新根，再仅从新包根 Configure/Build/Run consumer | 两组均退出 0并输出 `PAE_YAML_SDK_CONSUMER_PASS`；最终源码对应 `relocated-{static,shared}-checked-{configure,build,run}.log`。不是正式 source 包搬迁。 |

专项首次 Debug 的旧前端测试因 Windows `PAE_SOURCE_DIR` 反斜杠进入 C++ 宏字符串，
夹具路径错误而失败（`specialized-debug-ctest.log`）；公开 facade 测试同时通过。
在仅属本片的 `tests/config_frontend_yaml/CMakeLists.txt` 把测试路径转为正斜杠后，
`specialized-debug-ctest-final.log` 为 2/2；Release 为
`specialized-release-ctest.log` 2/2。缺组件负例最初只报告 `PAE_FOUND=FALSE`
（`find-repeat-missing.log`）；配置模板补精确理由后保留旧日志并用
`find-repeat-missing-explicit.log` 证实命中目标语义。历史失败没有回写为通过。

## 交付闭包与尚未验证

本片仅通过 `cmake --install` 形成**本地带 dirty 来源的安装树候选**；没有修改
`scripts/package_sdk_stage3.ps1` 的 source 白名单/正式包元数据，也没有对现用
`deliverables/` 做复制或替换。源码包归集、Lab 从内部头迁移到公开组件、standalone、
用户统一人工体验、Linux、真实私有协议、硬件/现场均待后续授权。包外消费证明所列
MSVC x64 配置可运行，不证明跨编译器 ABI、生产容量、进程 RSS 或许可已准许发布。
第三方许可文件随 ON 安装树附带，不能替代项目级 PAE 许可证决定。

末次闭包检查 `closure-source-final.log`：JSON-only 生成目标中 rapidyaml/前端引用为 0；
六个包外 consumer 工程中内部前端源码及旧探针路径引用为 0；四个启用组件安装树均含
官方 rapidyaml License（Hash 与随仓原件相同）和内嵌通知。包本身尚无正式
`MANIFEST.txt`/`SHA256SUMS.txt`/`PROVENANCE.json`，不能以本次局部检查冒充
Stage 3 包真实性/可发布性验证。

shared DLL 的最终同包 SHA-256 核对见 `shared-dll-identity-source-final.log`。

格式检查与最终 Git 状态以本轮交接时现场核对为准；本片停止在**待总控复核**。
