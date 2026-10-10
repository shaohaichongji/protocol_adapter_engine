# 9680cf9 static Release Lab 构建与验证（2026-10-10）

## 1. 阶段 B 结论与范围

按总控最新精简授权，本轮**仅新构建并验证 static Release Lab**，Configure、Build/Deploy、
现有六项 CTest 和最终部署 JSON/YAML 双文档 Qt smoke 均 exit 0。已完成派发范围，待总控复核。
没有构建 static Debug 或 shared Debug/Release；此前 513f6cc 加补丁四组结果仍属历史 patched
回归，不是 9680cf9 四组实跑。未重编 PAE Core、未改产品/制包脚本、未新增验证机制或打最终 ZIP。

固定产品 SHA：`9680cf90512f053962949cb55467d6af35959088`。开始核对主仓库 HEAD 一致，
主树已有 17 项文档变更与两份报告，属于其他任务正常并行工作，未消费、覆盖或还原。
实际产品来源是下列已有独立干净快照，起止 HEAD 一致且 status 为空，**零提交外补丁**。
已提交的显式 source/execution charset 与 YAML UI 独立数值码元断言直接由该快照进入本次构建。

- 干净来源：`out/build/aligned-sdk-9680cf9-20261010/clean-source/`。
- 原 SDK：`deliverables/sdk/9680cf9/static-Release/`；仅为准备脚本配对要求复制同批 static-Debug，
  没有 Debug 构建。两副本 provenance、全部数量/长度/hash 与原 SDK 相等，不通过改名冒充新来源。
- 新输入/构建/证据根：`out/build/aligned-lab-9680cf9-20261010/`。
- 最终实际部署：`deliverables/lab/9680cf9/static-Release/`。
- 新根和 deliverables/lab/9680cf9 在开始时均不存在；没有覆盖旧产物。

## 2. 实际流程与命令

执行忽略目录内的本轮命令记录辅助脚本：

```powershell
& ./out/build/aligned-lab-9680cf9-20261010/run.ps1
```

脚本使用固定快照原 `PrepareStandaloneInputs.ps1`，显式 RepositoryRoot 为固定快照，
DestinationRoot 为新 static-input，SdkCandidateRoot 为本轮逐字节 sdk-aliases，PackageKind static。
实际准备清单为原脚本产生的 `static-input/INPUT_SHA256.json`，共 2573 个完整输入；
Lab/Qt/yyjson 与固定源码逐项 hash 对照，普通 flat config 与原配置绑定，max fixture 由固定生成器
生成，两 SDK 输入与原目录全部 bytes/hash/数量一致。原准备 provenance 保留，不拿说明 note
当作实际成功/干净产品证明；实际来源、输入和执行证据另以 LabBuildIdentity 关联。

独立缓存为 `standalone-static-Release`，Configure 使用：

- VS `Visual Studio 18 2026`、x64、`v142,version=14.29.30133`。
- PAE_LAB_SOURCE_ROOT 指向隔离 `inputs/lab`；PAE_SDK_ROOT、PAE_DIR、CMAKE_PREFIX_PATH
  显式指向隔离 `inputs/sdk/pae-sdk-static-release` 及其 lib/cmake/PAE。
- Qt/yyjson/config 路径均指向本轮 inputs；EXPECTED_LIBRARY_KIND=STATIC、YAML_ENTRY=ON、
  BUILD_TESTING=ON、CRT `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`（本次 Release 为 /MD）。
- fresh vcxproj 构建前审核 OutDir/IntDir/ProgramDataBaseFile/ProgramDataBaseFileName/
  ObjectFileName/ImportLibrary/OutputFile，输出限定新根；未复用旧派生工程或其绝对 PDB 输出。

Build 命令为 `cmake --build <新build> --config Release --target` 六个
`pae_protocol_lab_ui_<name>_tests` 加 `pae_protocol_lab_ui --parallel 4`。
六个 name 为 yaml_entry、yaml_ui_smoke、compile_queue、document_state、schema_dispatch、unicode_literal。
Release 测试明确取消 NDEBUG；原 D9025 覆盖警告保留于 build-deploy.log，不是关闭断言。

先用精确正则 inventory 检查六项，再直接执行一次：

```powershell
ctest --test-dir <新build> -C Release -T Test --no-compress-output `
  -R '^pae\.tools\.protocol_lab_ui\.(yaml_entry|yaml_ui_smoke|compile_queue|document_state|schema_dispatch|unicode_literal)$' `
  --output-on-failure --verbose --timeout 60
```

真实 `standalone-static-Release/Testing/20261010-0102/Test.xml` 恰六个唯一所需名称，均 passed。
Unicode 实际结果 `PRODUCT_CHECKED=44 ANCHORS=3 FAILURES=0`，总体 6/6、exit 0。

原 POST_BUILD 部署与本次 build EXE hash 相等，复制至最终新 LabRoot 后再次逐项核对：19 文件，
包括 EXE、Qt5Core/Gui/Widgets.dll、platforms/qwindows.dll、13 JSON 与 1 YAML，
Qt/config 与实际固定输入一致；static 没有部署 pae.dll。完整部署清单留在 out 证据，不放运行 payload。
在**最终部署 EXE**运行 `--ui-smoke` 和 configs/synthetic_ascii_literal_only.pae.json/.pae.yaml，
仅进程调用期间设置 QT_QPA_PLATFORM=windows 后恢复；现有程序 compile timeout/CRT 诊断机制保留。
结果 `UI_SMOKE_PASS detail=2 document(s)`、exit 0；没有残留 cmake/ctest/cl/Lab 进程。

全部实际命令/参数、原 stdout/stderr 合并日志和退出码在 evidence/commands.jsonl、exits.log，
分阶段 prepare/configure/build-deploy/test-inventory/ctest/ui-smoke.log；未覆盖首次结果。

## 3. 阶段 C 精确交接路径与身份

下列路径均相对仓库根 `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`：

| 交接参数 | 路径 |
| --- | --- |
| cleanSource | `out/build/aligned-sdk-9680cf9-20261010/clean-source` |
| LabRoot | `deliverables/lab/9680cf9/static-Release` |
| PreparationProvenance | `out/build/aligned-lab-9680cf9-20261010/static-input/INPUT_PROVENANCE.json` |
| DeploymentHashes | `out/build/aligned-lab-9680cf9-20261010/evidence/DeploymentHashes.txt` |
| ActualBuildIdentity | `out/build/aligned-lab-9680cf9-20261010/evidence/LabBuildIdentity.json` |

已按提交中的 bundle Assert-LabBuildIdentity 字段生成 schema_version=1、static/Release、完整 SHA、
实际干净来源 dirty=false、patches=[]；并引用原准备文件、实际输入根/清单、实际 build 原日志/exit、
build EXE、真实 CTest XML/exit、最终部署 smoke 原日志/exit、完整部署清单，各原文件真实 hash。
身份文件不是代替阶段 C 执行 bundle 门禁的通过声明；本轮未调用完整 bundle 或生成 ZIP。
后续 SDK 文档派生包需把实际两个原 SDK 的 provenance/manifest hash 保留在 base 字段，供已有门禁核对。

| 证据 | SHA256 |
| --- | --- |
| LabBuildIdentity.json | `66305284610818C7D2EBFC604BF30A926628B823063AD314DFB0C36C966F5763` |
| 准备 INPUT_PROVENANCE.json | `7AD10CCE48661F0CDFE8DFAFC0A1D361E3C92AD5AEE8C9BB33402353B4CC00B0` |
| 构建前 INPUT_SHA256.json / 构建后 INPUT_AFTER_SHA256.json | `A907FB62470CA2AEDD3A158E61F8708BAE856262EDBAC56A036B0EFF7EF0A664` |
| build EXE / 最终部署 EXE | `08B03B68411E8CCACDE507503B5ACBC68628CA7389E0E4B8CEEF2B56FA067627` |
| Test.xml | `CC2844ECE55684D756983E7C62D14434945675310246AB42833C4FE860225C93` |
| DeploymentHashes.txt | `FADE8938710DD8DC89E55884DB720CC05AA690DC67DFF34853467D47D34B3D3D` |

构建前清单原文件不覆盖；构建结束重新枚举全部实际 inputs，独立保存 INPUT_AFTER_SHA256.json，
其序列化 hash 与前清单相同；input count/bytes/hash 一致。实际 build/deploy EXE 和最终运行部署
也以同一身份绑定。Hash 是一致性证据，不是签名、来源认证或替代总控复核。

## 4. 保护与未验证边界

23274 个固定源/SDK/历史根保护条目起止 bytes/hash 不变（条目可有重复路径），见
evidence/protected-before.json、protected-after.log；固定 source-final-status.log 为空。
旧诊断 PDB 保持事故后 hash `8AAC5112F333E5F8ADDCFCD4AC7D6B209458D08B50B4450D11262C14F792A5C3`，
不删除、不还原、不重建；其与旧 EXE 不匹配的历史结论不变。

唯一新增非忽略仓库文件是本报告。没有 Stage/Commit/Push、产品/脚本修改、删除/旧产物覆盖、
下载、本机 Qt 或全局环境改动；主树其他并行文档修改未消费。完成后向总控主动一次交接并停止写入。
未验证新基线 Debug/shared Lab、人工 UI、整体体验 ZIP/解包、Qt 分发许可、Linux、稳定 ABI、
真实协议、硬件/现场或全仓。阶段 C 归集与门禁执行由总控后续接续，本任务不擅自扩大范围。
