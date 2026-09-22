# Lab 本机 deliverables 归集验证（2026-09-19）

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

## 1. 结论与边界

已将同批 `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` clean-checkpoint SDK 的 static/shared Release Lab 产品目录原样归集到仓库内被 Git 忽略的本机统一路径：

- `deliverables/lab/98df5e0/static-release`
- `deliverables/lab/98df5e0/shared-release`

两份目标与原验证部署的相对文件集及逐文件 SHA-256 完全一致。从新位置短暂隐藏启动后，static 的 Qt Core/Gui/Widgets/qwindows 和 shared 的上述 Qt 模块 + `pae.dll` 均实际从各自新目录加载，输入/目标/加载哈希一致。

本轮仅做本机内部归集、文件一致性和运行时来源核对；未交互操作 UI，未复做 Compile/Decode/Encode，不新增人工验收、正式发布或对外分发结论。

## 2. 基线与前置门禁

- 仓库：`<REPO_ROOT>`
- 归集时 HEAD：`main@0b89beded03b39d8ffe019bfec659b5a5885a8f1`
- static 来源：`<LOCAL_WORK_ROOT>/pae-lab-clean-sdk-static-20260919/deploy/release-testing-off/Release`
- shared 来源：`<LOCAL_WORK_ROOT>/pae-lab-clean-sdk-shared-20260919/deploy/release-testing-off/Release`

复制前确认：

- 两个目标目录均不存在；
- `git check-ignore -v` 均命中 `.gitignore` 的 `/deliverables/lab/`；
- 两个来源根及全部后代的 reparse 项均为 `0`；
- 两份 `INPUT_PROVENANCE.json` 的 repository/Debug SDK/Release SDK HEAD 均为 `98df5e0...`，package kind 分别为 static/shared，Release SDK `source_worktree_dirty=false`；
- 原 `release-testing-off-module-origin.json` 均为 Release、`all_loaded_from_deploy=true`、`all_hashes_match=true`，且当前来源文件哈希仍与记录一致。

## 3. 原样复制结果

未裁剪 Qt DLL、platform plugin 或 configs，未复制 build tree 或整套 Qt SDK，未修改来源、目标包内文件或本机环境。

| 形态 | 来源文件数 | 目标文件数 | 来源独有 | 目标独有 | SHA-256 不一致 | 目标 reparse |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| static Release | 17 | 17 | 0 | 0 | 0 | 0 |
| shared Release | 18 | 18 | 0 | 0 | 0 | 0 |

static 目录保留 EXE、3 个 Qt DLL、`platforms/qwindows.dll` 及 12 份 `configs/*.pae.json`；shared 目录在此基础上额外保留同批 `pae.dll`。

## 4. 新位置模块来源

使用原验证根中已有 `CapturePaeLabModules.ps1`，对两个新位置分别做一次 Release 隐藏启动。脚本在启动前核对输入/目标哈希，捕获模块后只终止它自己启动的进程。

| 形态 | 捕获 PID | 模块数 | 全部从新目录加载 | 输入/目标/加载哈希 | 捕获后 PID |
| --- | ---: | ---: | --- | --- | --- |
| static Release | 8292 | 4 | PASS | PASS | 已退出 |
| shared Release | 13528 | 5 | PASS | PASS | 已退出 |

证据位于 `deliverables/lab/98df5e0/资料/`：

- `static-release-module-origin.json`
- `shared-release-module-origin.json`

两份 JSON 均记录 `all_loaded_from_deploy=true`、`all_hashes_match=true`。本轮未发现现成、无交互、可直接复用的 UI Decode 验证入口，因此没有临时改代码或把“能启动”扩大为 Decode 成功。

## 5. 资料与许可边界

`资料/` 还保留了现有真实材料的原样副本：

- `static-INPUT_PROVENANCE.json`
- `shared-INPUT_PROVENANCE.json`
- `yyjson-LICENSE.txt`（来自 `third_party/yyjson/LICENSE`，SHA-256 一致）
- `yyjson-dependency.lock.json`（来自 `third_party/yyjson/dependency.lock.json`，SHA-256 一致）

没有编造或补入 Qt 许可文件。当前仓库没有可随包归集的 Qt 许可材料，PAE 也尚无项目级对外分发许可。本报告仅记录材料现状，不作许可法律结论；本机内部归集不是正式对外分发。

## 6. 未验证与 Git 状态

- 未构建、未运行 CTest/全量测试、未执行新人工烟测；
- 未复做 Lab Compile/Decode/Encode，未验证真实协议、Linux、稳定 ABI、硬件、现场或生产；
- 未删除原验证根，未覆盖旧部署，未修改全局 `PATH`；
- 未 Stage、Commit、Push 或发布。`deliverables/lab/` 为忽略的本机产物，仓库可追踪新增仅本报告及已授权的指南入口更新。
