# 513f6cc Lab 编译阻塞与 charset 补丁四组验证（2026-10-09）

## 最新结论：数值校验的 YAML UI 测试修复及四组 patched 回归通过

本节是后续授权执行结果；下方 charset-only 的 5/6 及原 D8016 失败均为历史证据，保留不覆盖。
实际来源为 `513f6cc9bd46b64b3b6d1d4283f13c806039eebf` 固定快照、同批只读 installed SDK，
加明确未提交 standalone charset 和 YAML UI 测试两个补丁；**不是 clean 513f6cc 或正式交付身份**。

根因探针位于 `out/build/yaml-ui-smoke-diagnostic-513f6cc-20261009/`：实际诊断包含正确
UTF-16 `672A 63D0 4F9B`，原 `QStringLiteral("未提供")` 的期望却成为
`93C8 E045 5F41 6E1A 003F`。显式 u 字面量与独立数字参考相等；实际诊断包含数字参考，
不包含原错误期望，三次后续事件循环观察仍一致。保留原断言失败 exit=3，不把产品诊断判成错误。
最小修复仅在 `tests/protocol_lab_ui/yaml_ui_smoke_tests.cpp:91` 使用 `QStringLiteral(u"未提供")`，
断言长度 3、每个码元与 `{0x672AU,0x63D0U,0x4F9BU}` 相等、与 fromUtf16 数字参考相等，
并继续断言实际诊断包含正确文本。所有原功能断言保留；产品逻辑/API 未改。
测试文件 SHA256 `819718ECDACF6AA4CB36F0BCE1AEA8358EF5A715B79D4B95CCC4B944B51D9A5D`。
既有 CMake charset 补丁未扩大，hash 仍为下方 E53D18EC...。

### 新根、命令与完整验证结果

全部新输入/构建/候选位于 `out/build/yaml-ui-smoke-fix-513f6cc-20261009/`。
执行 `& ./out/build/yaml-ui-smoke-fix-513f6cc-20261009/run.ps1`；固定原准备脚本生成输入后
只叠加两个授权补丁，保存原准备 provenance/清单、两补丁、PATCH_PROVENANCE 和实际完整
PATCHED_INPUT_SHA256。Fresh configure 后、构建前逐 vcxproj 检查 EXE/OBJ/PDB/library 输出
均归新根；不复用诊断派生工程。VS18 x64 v142、MDd/MD、YAML ON，与历史组合相同。

| 组合 | Configure / Build-Deploy | 六项 CTest | JSON/YAML Qt smoke | 候选文件数 |
| --- | --- | --- | --- | --- |
| static Debug | 0 / 0 | 6/6，exit 0 | PASS，exit 0 | 19 |
| static Release | 0 / 0 | 6/6，exit 0 | PASS，exit 0 | 19 |
| shared Debug | 0 / 0 | 6/6，exit 0 | PASS，exit 0 | 20 |
| shared Release | 0 / 0 | 6/6，exit 0 | PASS，exit 0 | 20 |

先行 static Debug 精确 yaml_ui_smoke + unicode_literal 为 2/2，exit 0，然后才运行四组完整
yaml_entry/yaml_ui_smoke/compile_queue/document_state/schema_dispatch/unicode_literal。
Release 六测试显式取消 NDEBUG，断言未关闭。四组 Unicode 均 PRODUCT_CHECKED=44、ANCHORS=3、
FAILURES=0；Qt 实际候选均 `UI_SMOKE_PASS detail=2 document(s)`。13 JSON + 1 YAML、Qt DLL/plugin、
shared pae.dll 与原 SDK 完整部署 hash 白名单核对通过，EXE build/deploy 一致；dumpbin static
不导入 pae.dll、shared 导入；68 ClCompile 输入闭包均限新隔离 Lab/yyjson/build，不重编 Core。

按总控补充约定另执行 `retain-xml.ps1`，每组实际 CTest 增加 `-T Test --no-compress-output`，
精确六项、timeout 60，原首次日志不覆盖。四组真实 `Testing/20261009-1013/Test.xml` 均恰六个
唯一所需名称且 Status=passed，exit 0。原输出另存 `evidence/{kind}-{config}-ctest-xml.log`；
原始命令及退出追加 commands.jsonl/exits.log，XML 路径/hash/六项状态存 ctest-xml-evidence.json。

构建前实际清单 PATCHED_INPUT_SHA256 原文件保留；构建结束逐项复算、补充 XML 前又复算完整
数量/bytes/hash，生成独立 `evidence/{static,shared}-actual-input-after.json`，hash 与构建前相等：
static `0082E46510131DC972813E9F8377A83D8826D0ADAAF3C1A741C6A1F4318E1081`；
shared `CC70E6821647E62E28FD0F809208A7A70063795129D1255A43C5131BF39D2A8E`。
实际 EXE hash、部署完整清单及 smoke 原日志保留在 four-candidate-summary.json、各候选 manifest、
SHA256SUMS、candidate-json-yaml-smoke.log；构建原日志为各 configure/build-deploy.log。
与工程整理第6.1的真实证据格式兼容，但本轮两个未提交补丁必然不满足最终 clean 门禁，
不生成最终 LabBuildIdentity，不填写 dirty=false 或 patches=[]；后续需另行冻结新 SHA 并构建。

### 旧 PDB 事故及保护边界

此前诊断探针误保留旧绝对 linker PDB 输出，仅覆盖旧
`out/build/aligned-lab-513f6cc-20261009/charset-fix/standalone-static-Debug/tests-protocol-lab-ui/Debug/pae_protocol_lab_ui_yaml_ui_smoke_tests.pdb`。
之前 22679552 bytes，hash `7EF2B796882A5631587AB8CF8BB0C1873F7BAB164E35064D204CA8E480E5FD1E`；
之后 23588864 bytes，hash `8AAC5112F333E5F8ADDCFCD4AC7D6B209458D08B50B4450D11262C14F792A5C3`。
原字节未备份，未恢复；当前 PDB 不再与旧测试 EXE 匹配，不用于原符号真实性证明。
事故已向用户/总控报告，未隐去；本轮保护的是事故后的既有状态，不冒称还原。
新根构建前后 16418 个源/SDK/旧根/旧候选/旧日志/既有报告保护条目全部 hash 不变
（条目可有重复路径），protected-before.json/protected-after.log 可复核。

本轮仅新增测试补丁、更新本报告和忽略 out 证据/候选；无最终 deliverables/入口、Core/公共
契约/Host/stream/产品 UI 逻辑改动，无 Stage/Commit/Push/删除/发布/下载/全局环境修改。
未验证人工 UI、最终包/ZIP、Qt 许可、Linux、稳定 ABI、真实设备/现场或全仓。
状态：已完成派发范围，待总控复核；交接后停止写入。

## 历史结论：charset 补丁四组均已执行，当时仍有测试失败

按总控后续授权，仅修改主仓库 `tools/protocol_lab_ui/standalone/CMakeLists.txt:141`：
将 `/utf-8` 替换为 `/source-charset:utf-8 /execution-charset:utf-8`，其他选项不变。
**D8016 修复有效，四组 Configure、完整指定目标构建与部署成功，四组部署后 JSON/YAML
Qt 烟测通过；但四组最小六项 CTest 都为 5/6，不能宣称全项验证通过或正式交付完成。**
唯一失败均为 `yaml_ui_smoke_tests.cpp:91` 的中文诊断断言。
该新失败未擅自修复；本次限定 CMake 修复及四组验证执行已完成，剩余测试缺口待总控决定。

### 精确差异与补丁身份

```diff
-  target_compile_options(pae_lab_project_options INTERFACE /W4 /permissive- /Zc:__cplusplus /utf-8)
+  target_compile_options(pae_lab_project_options INTERFACE /W4 /permissive- /Zc:__cplusplus /source-charset:utf-8 /execution-charset:utf-8)
```

全部新输入、构建、测试部署及证据位于原新根下独立 `charset-fix/` 子目录，
原 D8016 失败根、clean-source、SDK 和旧包均未修改。
先用**固定快照原准备脚本**生成新 static/shared 输入，再将唯一授权的 CMake 文件补丁
复制到两类输入树。补丁语义检查为逆向替换后与固定原文件相同（只忽略 CRLF/LF差异）。
原生成的 `INPUT_SHA256.json` 与 `INPUT_PROVENANCE.json` 原样保留，属于叠加前的干净输入；
不能拿它们声明实际构建树完全干净。叠加后分别生成完整 `PATCHED_INPUT_SHA256.json`，
文件数不变、相对路径不变，只有 standalone CMake 的长度/Hash 改变。

实际来源为 **513f6cc 固定产品源 + 未提交 charset CMake 补丁 + 同批 513f6cc installed SDK**。
`charset-fix/evidence/PATCH_PROVENANCE.json`、charset.patch、input-identity.log 明确记录身份。
主仓库工程整理的 dirty 文档/脚本未消费、未覆盖；既有公开路由开关、Qt/yyjson、SDK 和
所有产品 C++/测试源仍与固定源一致。没有测试、API 或产品逻辑补丁。

| 身份文件 | SHA256 |
| --- | --- |
| 固定原 standalone CMake | `325257DE5F671ACAA376F8A4C6CDDE21E425F922944204BB2D7BDF5C5E84C1E6` |
| 主仓库与实际两输入树的补丁 CMake | `E53D18EC7E35BC2FAB9FE51A5CF0E39ADA2581248E0F7408980F9382B3B171B0` |
| `charset-fix/static-input/PATCHED_INPUT_SHA256.json` | `76C4CD41EBA56A18DD6D9E316B7A4E5559BFE3B4B18F9A2B05E754786A8E8A9A` |
| `charset-fix/shared-input/PATCHED_INPUT_SHA256.json` | `7F7D0AF21D67FF3A2E84FD3459135D98A0A35867BFF6E5C4CD36F57662D8DD05` |
| `charset-fix/evidence/PATCH_PROVENANCE.json` | `B87121F9C736FBE348F10CF7BEC7E681E7E66F62F8CCF3355590AD7465104A73` |

两类新输入叠加前、叠加后及验证结束都逐项复算：static 2573/shared 2565 文件，
除声明的 CMake 补丁无变化。源、SDK、旧包/报告和原失败根共 9285 个保护条目起止不变，
记录于新 evidence/protected-before.json、protected-after.log（条目可能有重复路径）。

### 四组实际命令与结果

构建根为 `charset-fix/standalone-{static,shared}-{Debug,Release}`，
严格按 static Debug → static Release → shared Debug → shared Release 串行。
VS 18 2026 x64 / v142 14.29.30133，MSVC 19.29.30159.0，Windows SDK 10.0.22621.0；
Debug `/MDd`、Release `/MD`，无 CL/_CL_ 覆写和全局环境更改。
每组 PAE_DIR、PAE_SDK_ROOT、CMAKE_PREFIX_PATH 精确指向其新隔离输入的匹配 kind/config SDK；
YAML/testing ON、既定 standalone public-only/ASCII/Binary/Host 路由由未改的 CMake 设置。

每组 Configure 使用与下方 before 同一生成器及参数语义，但全部根路径换成
charset-fix 新输入；构建六测试（yaml_entry、yaml_ui_smoke、compile_queue、document_state、
schema_dispatch、unicode_literal）及完整 `pae_protocol_lab_ui`，`--parallel 4`。
随后先核对精确六测试 inventory，再运行：

```text
ctest --test-dir <本组build> -C Debug|Release
      -R '^pae\.tools\.protocol_lab_ui\.(yaml_entry|yaml_ui_smoke|compile_queue|document_state|schema_dispatch|unicode_literal)$'
      --output-on-failure --verbose --timeout 60
```

复制本组实际部署到 charset-fix/test-candidates 后，在该目录执行新 EXE：
`--ui-smoke <candidate>/configs/synthetic_ascii_literal_only.pae.json`
`<candidate>/configs/synthetic_ascii_literal_only.pae.yaml`，进程内 `QT_QPA_PLATFORM=windows`，
结束还原此验证进程环境；四组均退出 0 且精确标记 `UI_SMOKE_PASS detail=2 document(s)`。

| 组合 | Configure / Build-Deploy | 六项 CTest | Unicode | 候选 JSON/YAML Qt smoke |
| --- | --- | --- | --- | --- |
| static Debug | 0 / 0 | 5/6，退出 8 | PASS | PASS，退出 0 |
| static Release | 0 / 0 | 5/6，退出 8 | PASS | PASS，退出 0 |
| shared Debug | 0 / 0 | 5/6，退出 8 | PASS | PASS，退出 0 |
| shared Release | 0 / 0 | 5/6，退出 8 | PASS | PASS，退出 0 |

四组 Unicode 均为 `PRODUCT_CHECKED=44 ANCHORS=3 FAILURES=0`。
yaml_entry、compile_queue、document_state、schema_dispatch 也均通过。
yaml_ui_smoke 四组都先打印 `contains(QStringLiteral("未提供"))` 断言失败：
Debug 随后 30 秒 Timeout，Release 随后以 `0xc0000409` Exception 结束。
保留首次失败及两配置差异，不将 timeout 简化成纯性能问题。
测试原有 30 秒属性不因命令行 timeout 60 改写。

生成 vcxproj 核对不再包含 `/utf-8`，同时包含两个显式 charset 选项，记录
generated-charset-options.log。Release 六目标保留 NDEBUG 取消定义，记录
release-test-options.log；编译 D9025 覆写警告和测试失败均证明断言没有被关闭。
shared 的既有 C4251、测试中文宏 C4819 等警告完整保留，不声明零警告。

原始命令/退出：charset-fix/evidence/commands.jsonl、exits.log；分组日志为
`{kind}-{config}-{configure,build-deploy,test-inventory,ctest,candidate-json-yaml-smoke}.log`。
完整辅助流程为 charset-fix/run.ps1。首次测试失败后仅调整辅助流程收集剩余组，
不重跑成功阶段或覆盖首次失败；progress.log 的 PRESERVE 标记说明复用的是本轮已执行日志。
两次辅助检查修正也单独记录：helper-audit-correction.log（MSBuild 将 `/UNDEBUG` 映射为
UndefinePreprocessorDefinitions，原 AdditionalOptions-only 检查误报）、
helper-variable-correction.log（PowerShell XML 类型约束变量复用）。
这些不是产品修复；最终执行器正常结束仅表示矩阵执行结束，四个 CTest 退出 8 仍是失败。

### 仅测试候选的部署与清单

不写 `deliverables/lab/513f6cc`，下列均是补丁测试候选，相对仓库：

- `out/build/aligned-lab-513f6cc-20261009/charset-fix/test-candidates/static-debug/`
- `out/build/aligned-lab-513f6cc-20261009/charset-fix/test-candidates/static-release/`
- `out/build/aligned-lab-513f6cc-20261009/charset-fix/test-candidates/shared-debug/`
- `out/build/aligned-lab-513f6cc-20261009/charset-fix/test-candidates/shared-release/`

static 各 19 文件、shared 各 20 文件，按固定部署脚本的精确白名单核对：
13 JSON + 1 YAML、EXE、Qt 三 DLL、qwindows[d] 插件，shared 另含 pae.dll。
所有 EXE 与各自构建输出一致，Qt 从固定源 → 隔离输入 → 部署 → 候选 Hash 一致；
shared DLL 与同配置原 SDK 及 test-bin 的副本一致，static 不含/不导入 pae.dll，
无 YAML DLL。每组 68 个 ClCompile 引用均只位于隔离 Lab/yyjson/生成树，
没有开发树 PAE 私有 src 输入，见 compile-closure.log。
dumpbin /DEPENDENTS 原始日志为 `{kind}-{config}-dependents.log`，
Debug/Release CRT 与 Qt suffix 不混用；宿主系统 CRT 来源/可分发许可不据此闭合。

候选完整长度/Hash 清单及 SHA256SUMS 放在 evidence，不往部署额外混入文件。
四组结束后独立复算全部候选文件及清单 Hash 通过，见 `{kind}-{config}-final-audit.log`。

| evidence 中候选清单 | SHA256 |
| --- | --- |
| `static-Debug-candidate-manifest.json` | `A091BD3D316698FE2D1745EEF50F116790A73BC2448BA4E26F81D4944222980A` |
| `static-Release-candidate-manifest.json` | `68E82271B5C0A9EF3A6FB75D7B32D9AEFAA26B1EE95BEF20F22C091FE733BC13` |
| `shared-Debug-candidate-manifest.json` | `F342A7DF2D0F7F855FF12FFAEEE600EE00ECE53D1CF9E59B44B19F9C2CD1E805` |
| `shared-Release-candidate-manifest.json` | `43A88C2D57E63CFE0E51D01C6C1E742D108C5E6BE91A5CA339A035C95032A15E` |

four-candidate-summary.json 汇总四 EXE Hash、manifest Hash、SHA256SUMS Hash、身份与
`ctest_passed=false`/`qt_smoke_passed=true`，不把可运行候选说成验收通过产品。

### 剩余问题、建议与停点

已确认：测试断言四组稳定失败，失败行使用未加 `u` 的中文 QStringLiteral，
编译存在 C4819；随仓 Qt 的宏为 `u"" str`，而产品字面量 Unicode 独立期望验证通过。
合理推测：测试自己的中文期望可能受 MSVC 混合前缀字面量影响，不能据此宣布产品诊断错误。
需下一片先增加独立数字 Unicode 期望或诊断实际 UTF-16/UTF-8 证据，确认再最小修正测试；
本片未改测试、产品逻辑、API，也未绕过该断言。

本次 Git 增量仅 standalone CMake 的一行替换和本报告更新，工程整理现有 dirty 均保留，
固定快照与 SDK/旧包不变。无 Stage/Commit/Push、清理、下载或发布。
建议提交标题仅供总控决定：`fix(lab): 对齐 standalone MSVC 字符集选项`。
最终固定同基线产品需待总控决定提交及重新核验/构建；补丁四组不能改称纯 513f6cc 正式产物。
未验证人工 UI、真实 Host/设备、全仓矩阵、最终 ZIP/教程文档闭包、许可、Linux、稳定 ABI。
完成本次完整交接后停止写入，等待总控复核与剩余测试缺口授权。

---

## 历史：纯 513f6cc 首次编译阻塞（以下保留当时状态）

## 结论与停点

**尚未完成四组 Lab 派发范围；发现新越界修复项，已停止写入待总控决定。**
原白名单缺生成器问题已由固定提交解决，static/shared 隔离准备均成功。
static Debug Configure/Generate 退出 0，但首组最小测试及 Lab 构建退出 1：
三个公共适配/展示依赖目标报 MSVC **D8016**，实际编译命令同时包含
`/source-charset:utf-8` 和 `/utf-8`。尚未构建成指定测试或 Lab EXE，未运行 CTest、
Unicode、Qt 烟测或部署，未创建 `deliverables/lab/513f6cc`。
其余三组未 Configure/Build；不把计划命令、上一轮专项或 SDK PASS 算作本轮 Lab PASS。

## 固定来源与保护

- 主仓库与产品快照 HEAD 均为 `513f6cc9bd46b64b3b6d1d4283f13c806039eebf`。
  产品源为 `out/build/aligned-sdk-513f6cc-20261009/clean-source`，起止 Git 状态干净。
- 主仓库起点有工程整理正在修改的 `docs/sdk`、`docs/experience`、experience 制包脚本
  等，另有未跟踪 SDK 报告。完整起止列表保存于新 evidence/main-before-status.log、
  main-after-status.log；这些文件不进入 Lab 输入，也未由本片修改或冻结并行任务。
- 新根 `out/build/aligned-lab-513f6cc-20261009` 与候选根执行前均不存在，旧 a1c6010
  构建、白名单修复输入、失败证据、SDK 与报告保留原身份。本轮未覆盖旧产物。
- 固定源、513f6cc 五 SDK、旧 a1c6010 五 SDK 与三份既有报告共 **3783** 文件保存
  长度/SHA256 并复查不变，见 `evidence/protected-before.json`、protected-after.log。
  随仓 Qt、yyjson 文件包括在固定源保护中。工程整理的并行工作树不作为产品来源。

## 准备输入与身份

本轮所有执行脚本和产品源均来自同一固定干净快照，不存在未提交准备补丁输入。
执行与复制入两个输入树的 `PrepareStandaloneInputs.ps1` Hash 一致：
`A74C3033A724E5119C492E2D5952A87C51B706129DC4122CE13F62374C1E775B`。
新增生成器已包含在固定白名单和本轮完整输入清单中。

1. 将 `deliverables/sdk/513f6cc/{static-Debug,static-Release,shared-Debug,shared-Release}`
   逐包复制到新根 `sdk-aliases/pae-sdk-{static,shared}-{debug,release}`。
   原包 provenance source_head/dirty 检查通过，static 各 42、shared 各 38 文件，
   两段 SDK 复制（原包到别名、原包到最终隔离输入）长度/Hash 均一致。
2. 执行固定快照的原 `PrepareStandaloneInputs.ps1`，`-RepositoryRoot` 为固定快照，
   `-DestinationRoot` 分别为新根 `static-input`、`shared-input`，
   `-SdkCandidateRoot` 为新根 `sdk-aliases`，`-PackageKind static|shared`。
   两次准备退出 0，保留 static-prepare.log、shared-prepare.log。
3. 每类 109 个同路径 Lab 文件与固定源逐项 Hash 一致；平铺配置与准备树原文件一致，
   max fixture 由固定生成脚本产生。Qt/yyjson 复制文件与固定快照逐项 Hash 一致。
   两份 INPUT_PROVENANCE.json 均记录固定 HEAD 和空产品源 Git 状态。
4. 对输入文件完整清单复算长度/Hash：static **2573**、shared **2565** 文件全部一致；
   构建失败后再逐项核验仍不变。证据 input-identity.log、input-manifest-verify.log、
   {static,shared}-input-after-failure.log。

输入清单 SHA256：

| 文件（相对新根） | SHA256 |
| --- | --- |
| `static-input/INPUT_SHA256.json` | `2142AB07803DE80B83758BD817E42F19B31D36DF6567ABDE4C13B7A6C22A9425` |
| `shared-input/INPUT_SHA256.json` | `9D84C4A1915BB7423FC738E6170785C4721BEFF26B0F81D824795D20A9A28ADC` |

## 实际 Configure/Build 与根因

新根 `run.ps1` 保存完整辅助流程，实际执行过的命令和原始参数见
`evidence/commands.jsonl`，退出见 exits.log，时序见 progress.log。
未执行到的后续分支只是计划，不能当验证证据。

Configure 使用：

```text
cmake -S <static-input>/inputs/lab/tools/protocol_lab_ui/standalone
      -B <新根>/standalone-static-Debug
      -G "Visual Studio 18 2026" -A x64 -T v142,version=14.29.30133
```

显式 `PAE_SDK_ROOT=<static-input>/inputs/sdk/pae-sdk-static-debug`、
`PAE_DIR=<同包>/lib/cmake/PAE`、`CMAKE_PREFIX_PATH=<同包>`；
Lab/Qt/yyjson/config 根均为新隔离输入，expected kind STATIC，YAML/testing ON。
CRT 表达式 `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`，Debug 工程为 `/MDd`。
生成器实际识别 MSVC 19.29.30159.0、Windows SDK 10.0.22621.0。
Cache 核对 PAE_DIR/PAE_SDK_ROOT 路由精确匹配，见 build-input-routing.log。

构建命令为 `cmake --build <build> --config Debug --parallel 4 --target` 加下列目标，
日志 `evidence/static-Debug-build-deploy.log`、退出 **1**：

- `pae_protocol_lab_ui_yaml_entry_tests`
- `pae_protocol_lab_ui_yaml_ui_smoke_tests`
- `pae_protocol_lab_ui_compile_queue_tests`
- `pae_protocol_lab_ui_document_state_tests`
- `pae_protocol_lab_ui_schema_dispatch_tests`
- `pae_protocol_lab_ui_unicode_literal_tests`
- `pae_protocol_lab_ui`

失败发生在首目标的依赖编译，yyjson C 库生成成功；三个 C++ 目标
`pae_protocol_lab_binary_public_h1`、`pae_protocol_lab_ui_owned_presentation`、
`pae_protocol_lab_ascii_public_a1` 报相同 D8016。

可直接复核的因果链：

- 固定 `tools/protocol_lab_ui/standalone/CMakeLists.txt:141` 给 Lab 工程选项加入 `/utf-8`。
- 所消费包 `lib/cmake/PAE/PAEConfig.cmake:121` 在消费目标未声明
  `PAE_MSVC_SOURCE_CHARSET_SUPPLIED` 时，自动提供 `/source-charset:utf-8`。
- 生成 `standalone-static-Debug/pae_protocol_lab_binary_public_h1.vcxproj:83`
  的 AdditionalOptions 同时出现两个选项，随后 cl 报 D8016。
- 固定仓库根 `CMakeLists.txt:558-559` 已使用分开的 `/source-charset:utf-8`
  和 `/execution-charset:utf-8`，而 standalone 仍是旧的 `/utf-8` 组合开关。

因此不是生成器漏项再次出现，不是 SDK Hash、路径或 CRT 混包。
最小建议：仅将 standalone 的 `/utf-8` 换为与仓库相同的两个显式 charset 选项，
维持 UTF-8 源/执行字符集语义并避免冲突；不要改 SDK、测试或公共接口。
需总控确认修复授权及后续固定基线安排，本轮**未实施**、未通过改缓存或目标属性绕过。

## 交付边界与 Git

本片只新增本报告及授权新忽略根的辅助流程、复制输入和生成日志/构建文件。
未改产品实现、准备脚本、CMake、测试、SDK、旧报告、旧包、入口、全局 Qt 或环境；
未 Stage/Commit/Push、下载、清理、删除或发布。

没有 Lab 候选或候选清单；四组专项、Qt 烟测、部署配置与 DLL 核查未完成。
未验证 Release/shared、完整 UI、人工、ZIP/教程闭包、许可、Linux、真实协议/设备或稳定 ABI。
向总控反馈本次准确阻塞后停止写入，等待明确修复派发；不能称“全部待办已解决”。
