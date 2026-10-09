# 固定 a1c6010 standalone Lab 重建与白名单补丁验证（2026-10-09）

## 最新：最小白名单补丁验证

**已完成本次最小白名单修正与专项验证派发范围，待总控复核。**
用户经总控明确授权后，主仓库 `PrepareStandaloneInputs.ps1` 的 `labFiles` 仅增加一行：

```diff
     'tests\protocol_lab_ui\CMakeLists.txt', 'tests\protocol_lab_ui\test_support.h',
+    'tests\protocol_lab_ui\generate_unicode_literal_fixture.ps1',
     'tests\protocol_lab_ui\verify_crt_assert_probe.cmake',
```

没有改测试、CMake、生成器、产品实现、依赖或固定快照。
此次结果是“准备脚本补丁验证通过”，不是“新固定基线四组 Lab 重建完成”。
下方首次缺脚本失败记录及日志完整保留，作为 before。

### 输入与补丁身份

所有新证据均在 `out/build/aligned-lab-a1c6010-20261009/whitelist-fix/`，
辅助命令为该根 `run.ps1`；旧输入、构建和日志不复用、不覆盖。
实际执行脚本来自**主仓库未提交补丁**，但其 `-RepositoryRoot` 显式指向
`out/build/aligned-sdk-a1c6010-20261009/clean-source` 干净固定快照，
`-DestinationRoot` 为新子目录 `static-input`，`-SdkCandidateRoot` 为原已核验
`sdk-aliases`，`-PackageKind static`。本轮身份为 **a1c6010 + 未提交准备白名单补丁**。

脚本从 `RepositoryRoot` 自动复制 standalone 子树，因此**输入内的准备脚本仍是干净
a1c6010 原版，不是此次实际执行的补丁版**，不能通过输入内原版脚本独立重现新增白名单。
原 `INPUT_PROVENANCE.json` 记录的是产品源根的干净身份，并不完整表示执行脚本来源；
补充 `evidence/PATCH_EXECUTION_PROVENANCE.json` 明确记录两种脚本路径、Hash、
RepositoryRoot 参数、主仓库 dirty 状态和 SDK 根，另保存 `evidence/preparation.patch`。

| 身份 | SHA256 |
| --- | --- |
| 实际执行的主仓库补丁脚本 | `8728CD17DEF61BEA5C8F7F73975FE825A1852799EAB9A1D094CF23F0A3D3DEF6` |
| 固定快照及输入内原版脚本 | `C44EA1CC9008EAA09AEFD531725A6EFF3FF1131E6046FD46849AC5DC14D3DC9A` |
| 新增生成器，与固定快照一致 | `376F6650268657EFF34CFCA2C4126DB693599C85132C3CA1C3518CF77518EE95` |
| 新 `static-input/INPUT_SHA256.json` | `C1C7E3946EE2229405BFF6ED964A9CF384D4157AF6C34912D8D2FBC9D69DC7A2` |
| 新 `static-input/INPUT_PROVENANCE.json` | `2C9CDBB290D8E4FE46B2528DEBD8F0FD4A3CD60F408E3380190596A9D9528B21` |
| `PATCH_EXECUTION_PROVENANCE.json` | `A9060BE77F39AC4DB33AB6B58F2BF3B4E70A426C4C2D96C9F182926D777153C6` |

逐项复算新输入清单的长度/Hash，并与 before 清单按相对路径比较：原 2572 文件全部不变，
新清单为 2573 文件，唯一新增为生成器，且清单包含它。Lab 产品源、测试、CMake、
Qt、yyjson、SDK 等其他准备输入均相同，见 `evidence/input-identity.log`。
原 3517 受保护源/SDK/既有 SDK 报告文件及旧构建根 2901 文件（两组可能有路径重叠）
逐项复查不变，见 `evidence/protected-after.log`、`old-root-before.json`。

### 实际验证结果

Configure 参数完整记录于新 `evidence/configure-args.json`，沿用 before 的
VS 18 2026 x64、`-T v142,version=14.29.30133`、YAML/testing ON、STATIC，
但所有 Lab/Qt/yyjson/config/SDK 参数改指新隔离输入。
`PAE_SDK_ROOT=<新输入>/inputs/sdk/pae-sdk-static-debug`，
`PAE_DIR=<同包>/lib/cmake/PAE`；CRT 表达式仍为
`MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`，实际仅验证 Debug `/MDd`。

- Configure/Generate：退出 **0**，日志 `evidence/configure.log`。
- `cmake --build <新根>/standalone-static-Debug --config Debug --target
  pae_protocol_lab_ui_unicode_literal_tests --parallel 4`：退出 **0**，
  日志 `evidence/build.log`，生成器成功提取 **44** 处产品调用。
- 先用 `ctest --test-dir <build> -C Debug -R
  '^pae\.tools\.protocol_lab_ui\.unicode_literal$' --show-only=json-v1`
  核对唯一真实测试和执行文件，见 `evidence/test-inventory.json`。
- 再以相同精确过滤运行 `--output-on-failure --verbose --timeout 60`：退出 **0**，
  **1/1 PASS**，`PRODUCT_CHECKED=44 ANCHORS=3 FAILURES=0`，日志 `evidence/ctest.log`。
  所有 Configure/Build/Test 退出汇总于 `evidence/exits.log`。

`git diff --check` 通过，暂存区空；仅准备脚本有一行 tracked 差异，
两份 aligned 报告未跟踪，既有 SDK 报告未修改。
建议提交标题（仅建议，未授权执行）：`fix(lab): 补齐 standalone Unicode 测试生成器白名单`。

未验证：Release/shared、原五项最小专项、完整 Lab EXE、部署、Qt UI 烟测、四组重建、
归集与最终 ZIP；未做人工、Linux、设备或许可闭合验证。
本轮未 Stage/Commit/Push、清理、下载或发布。完成交接后停止写入，等待总控复核。

---

## 历史：首次阻塞记录（以下保留当时状态）

## 当前结论

**尚未完成派发范围；已停止写入，待总控决定最小修复授权。**
固定源码的 standalone 准备白名单漏掉现有 MSVC Unicode 测试依赖的
`tests/protocol_lab_ui/generate_unicode_literal_fixture.ps1`。
实际隔离 static Debug Configure/Generate 退出 0；现有
`pae_protocol_lab_ui_unicode_literal_tests` 构建退出 1，自定义生成命令退出 64，
日志明确指向缺失脚本。不是编译器、SDK 或 Qt 链接失败。

这证明该测试的隔离闭包不完整，不证明 Lab 产品或原五项最小专项均会构建失败。
本轮没有跳过新测试依赖来宣称同基线验证完成，也未补拷漏项、修改固定输入或继续四组构建。
尚无可运行 Lab 候选；`deliverables/lab/a1c6010` 未创建。

## 现场与保护

- 主仓库：`main@a1c6010a257b0790f09c639fd79556170887bbc1`，起点暂存为空，
  仅既有 `docs/engineering/aligned-sdk-validation-20261009.md` 未跟踪。
- 产品输入：`out/build/aligned-sdk-a1c6010-20261009/clean-source`，HEAD 同上，Git 状态干净。
- 新建根：`out/build/aligned-lab-a1c6010-20261009`，执行前不存在；未覆盖任何旧构建。
- 五个原 SDK 包、固定快照文件（不含 `.git`）和既有 SDK 报告共 3517 文件，
  保存长度/SHA256 后逐项复查未变。证据：`evidence/protected-before.json`、
  `evidence/protected-after.log`。Qt 随仓文件包含在固定快照保护中。
- 四个 binary SDK 在新根 `sdk-aliases/pae-sdk-{static,shared}-{debug,release}`
  中按原脚本要求复制布局，static 各 42 文件、shared 各 38 文件，长度/Hash 全部一致，
  见 `evidence/sdk-alias-identity.log`。未重打 SDK，也未借用旧库。

本轮手工新增文件仅本报告及新忽略根内 `preflight.ps1`；其余为复制输入、清单及构建生成文件。
未改产品源码、脚本、CMake、测试、SDK、入口或旧文档；未 Stage、Commit、Push、删除、下载、发布，
未改本机 Qt 或全局 PATH。

## 实际执行与证据

下列路径相对本轮新根，完整参数及调用顺序保存在 `preflight.ps1`。

1. 从固定快照调用原 `PrepareStandaloneInputs.ps1`，参数为
   `-RepositoryRoot <clean-source> -DestinationRoot <新根>/static-input`
   `-SdkCandidateRoot <新根>/sdk-aliases -PackageKind static`。
   准备成功，日志 `evidence/static-prepare.log`；未准备 shared 输入。
2. Configure：`cmake -S <static-input>/inputs/lab/tools/protocol_lab_ui/standalone`
   `-B <新根>/standalone-static-Debug -G "Visual Studio 18 2026" -A x64`
   `-T v142,version=14.29.30133`，YAML/testing ON，expected kind STATIC。
   显式 `PAE_SDK_ROOT=<static-input>/inputs/sdk/pae-sdk-static-debug`，
   `PAE_DIR=<同包>/lib/cmake/PAE`，Lab/Qt/yyjson/config 根均指向隔离复制树；
   CRT 为 `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`，即 Debug `/MDd`、Release `/MD`。
   参数原文：`evidence/static-Debug-configure-args.json`。
   原始日志：`evidence/static-Debug-configure.log`，退出 0。
   实际工具链 MSVC 19.29.30159.0、Windows SDK 10.0.22621.0。
3. `cmake --build <新根>/standalone-static-Debug --config Debug`
   `--target pae_protocol_lab_ui_unicode_literal_tests --parallel 4`。
   退出 1；原始 `evidence/static-Debug-unicode-build.log` 中缺失脚本报错和 MSB8066
   均保留，自定义命令退出 64。退出汇总为 `evidence/exits.log`。
4. `evidence/whitelist-gap.log` 记录固定快照有脚本、准备输入无脚本。
   固定 `tests/protocol_lab_ui/CMakeLists.txt:19,24` 明确使用并依赖该脚本；
   固定 `PrepareStandaloneInputs.ps1:66-67` 只列测试 CMake、support 与 CRT probe，
   后续自动收集只收 `.cpp` 和 standalone 子树，不覆盖该 `.ps1`。

原脚本生成的输入清单（非产品签名）：

| 文件 | SHA256 |
| --- | --- |
| `static-input/INPUT_SHA256.json` | `3D1B8A509BC6C6779915A7894869886B4E6DF22844EBE389AC7A70B06B185E58` |
| `static-input/INPUT_PROVENANCE.json` | `2C9CDBB290D8E4FE46B2528DEBD8F0FD4A3CD60F408E3380190596A9D9528B21` |

## 最小修复建议与停点

建议仅为 standalone 准备白名单补入
`tests/protocol_lab_ui/generate_unicode_literal_fixture.ps1`，不调整公共接口、PAE、Qt、SDK 或测试行为。
但这属于原派发禁止修改的产品脚本，且改变固定提交的输入闭包；需总控明确选择新的固定检查点，
或授权可追溯、独立标识的最小补丁输入。不得把补丁输入伪称为未经修改的 a1c6010。
本轮没有实施该建议。

依恢复派发的“白名单/脚本阻塞先报告最小方案停止”要求，四组 Lab 产品构建、
五项最小 CTest、Unicode 运行、自动 Qt 烟测、部署配置与 DLL 身份核查均未完成。
Release/shared 未 Configure；未生成候选、最终 ZIP 或教程/文档闭包。
不宣称完整体验交付、二进制可复现、许可闭合、稳定 ABI、Linux、真实设备或人工 UI 验收。

已向总控主动反馈本次阻塞，等待明确后续派发；不是完成通知或总控验收通过。
