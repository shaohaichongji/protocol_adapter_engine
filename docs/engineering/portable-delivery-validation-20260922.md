# 本地可移植交付归集验证（2026-09-22）

## 1. 结论

在 `main@657d085` 现场基线上，按《本地试用根归集与公共仓库路径治理预检》第 4 节的精确集合，将当前五个 SDK 包、Lab static Release 部署及必要证据归集到仓库内 Git 忽略目录。共复制并逐文件核验 287 个文件、52,033,006 bytes，源与目标的长度及 SHA-256 零差异。

当前推荐入口为：

- SDK：`deliverables/sdk/7b4205e/`
- Lab：`deliverables/lab/fe1683c-plus-patches/static-release/`
- 本地证据：`deliverables/evidence/portable-delivery/`

这些目录均被 Git 忽略，归集没有把二进制产物纳入版本控制；仅 clone 仓库不会获得它们。本报告只记录相对路径和摘要，带原始绝对路径的逐文件清单保留在本地忽略目录中。

## 2. 执行边界

- 未覆盖任何既有目标；执行前确认四个归集目标均不存在，目标父目录不是 reparse point。
- 精确选择集合没有重复源、重复目标或 reparse 文件；复制前目标命中数为 0。
- 未修改源试用根、包内 provenance、manifest 或 hash 文件。
- 未删除第三方试用根或其他生成目录，等待总控复核后另行决定。
- 未改源码、构建配置或第三方依赖，未重新构建 SDK 或 Lab。
- 共享工作树中的并行指南与脚本变化全部保留，本任务未编辑。
- 无 Stage、Commit、Push、发布或现用环境替换。

## 3. 归集清单与逐文件核验

| 集合 | 文件数 | 字节数 | 归集位置 |
| --- | ---: | ---: | --- |
| SDK 五包 | 189 | 29,955,541 | `deliverables/sdk/7b4205e/` |
| SDK 原始证据 | 49 | 79,862 | `deliverables/evidence/portable-delivery/sdk-7b4205e/` |
| Lab Release 部署 | 18 | 21,482,235 | `deliverables/lab/fe1683c-plus-patches/static-release/` |
| Lab static 身份证据 | 10 | 484,322 | `deliverables/evidence/portable-delivery/lab-fe1683c-plus-patches/` |
| Lab 编译诊断证据及手工负例 | 7 | 2,366 | 同上 |
| Lab 示例修复证据 | 14 | 28,680 | 同上 |
| **合计** | **287** | **52,033,006** |  |

复制后按每个源/目标对重新计算长度和 SHA-256：287/287 一致，失败 0。逐文件原始路径映射、长度和哈希保存在 Git 忽略的 `deliverables/evidence/portable-delivery/portable-delivery-manifest.tsv`，不进入公共文档。

## 4. SDK 自校验

归集后的五个 SDK 包分别验证包内 `SHA256SUMS` 和 `MANIFEST`：

- `SHA256SUMS` 共 184 条，失败 0；
- `MANIFEST` 共 179 条，失败 0；
- 五包 provenance 的 `source_head` 均为 `7b4205ea899cf16b9c73ba6ebc4c64382b71dc63`；
- 五包均记录 `source_worktree_dirty=false`。

这是内容与来源身份核验，不等于重新运行六组包外消费构建和测试。

## 5. Lab 部署与证据解释

归集后的 Lab static Release 有 18 个文件，其中 `configs/` 有 13 份配置；18 个部署文件均与所选源逐文件同长且同 SHA-256。

证据中两个流式配置哈希适用对象不同：

- `synthetic_ascii_stream_slice.pae.json` 为 `5F6B871DFBFC099070EE8FB9881F02C8671496C61BFC028CC24DB332E7E8BE4C`，与结构化编译诊断阶段的 `verified-release-product.json` 中通用字段 `fixture_sha256` 一致；
- 后续补入的 `synthetic_stream_framing_slice.pae.json` 为 `3F3F7FF2717F3D4CEE50231D0723077052FE668B697D2502DA872698C1C88A7E`，与示例打包修复阶段的部署后清单一致。

因此两个值不是同一文件的源/目标冲突。原始 `verified-release-product.json` 的字段没有携带配置名，存在脱离上下文时容易误读的歧义；本轮保留原始证据，不回写历史文件，并在此明确适用范围。

## 6. 归集位置隐藏启动检查

从新 Lab 目录执行一次且仅一次隐藏、离线启动检查：

```powershell
& '.\deliverables\lab\fe1683c-plus-patches\static-release\pae_protocol_lab_ui.exe' --ui-smoke '.\deliverables\lab\fe1683c-plus-patches\static-release\configs\synthetic_binary_ui_stage1.pae.json'
```

结果：进程正常退出，退出码 0；标准输出包含 `UI_BINARY_HOST_SMOKE_DOCUMENT index=1 fields=9` 和 `UI_SMOKE_PASS detail=1 document(s)`。执行前后均无同名残留进程，没有终止用户进程，也未修改系统 Qt 或通信配置。输出保存在 Git 忽略的 `deliverables/evidence/portable-delivery/lab-fe1683c-plus-patches/relocation-smoke/`。

该检查仅证明新位置的 EXE、邻接 Qt 运行库、平台插件和一份 synthetic Binary 配置可完成既有隐藏 smoke；不等于全量测试、真实设备/通信、人工 UI、shared 运行、模块来源矩阵、ABI、许可或生产验收。

## 7. 剩余事项

- 旧试用根清理已完成，删除后复核见第 8 节。
- 若要在其他机器使用，需要提供独立的产物传递或按既有流程重建；Git clone 本身不携带本地忽略目录。
- 对外分发的许可、通知材料与正式发布身份仍未闭合，本轮不升级成熟度声明。

## 8. 用户手动删除后的复核

2026-09-22，用户确认通过 PowerShell 手动删除旧试用根 `pae-trial-7b4205e-20260920`。
总控现场确认该根不存在，并依据 Git 忽略目录中的
`deliverables/evidence/portable-delivery/portable-delivery-manifest.tsv` 重新核对全部归集目标：
287 文件、52,033,006 bytes，文件长度与 SHA-256 全部一致。

本次仅验证删除状态及保留内容完整性，未重跑构建或 Lab，未测量物理磁盘释放空间，
未 Stage/Commit/Push。前文未删除的描述属于原归集阶段边界，以本节为最新清理状态。
