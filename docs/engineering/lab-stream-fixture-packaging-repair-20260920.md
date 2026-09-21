# Lab Binary 流式配置打包修复记录（2026-09-20）

## 1. 结论

本片已在 `main@fe1683c9d540d7f5180cad1d0368cef4ccf1136d` 上完成限定修复：standalone Lab 的真实部署函数现在会把 `synthetic_stream_framing_slice.pae.json` 纳入 `PAE_CONFIG_FILES`，缺少该文件时配置阶段会明确失败，文件存在时可随部署复制且内容哈希与仓库 canonical 一致。

按授权，对统一试用交付的 static/shared、Debug/Release 四个 Testing-off 部署目录进行了 post-`fe1683c` 配置补充。补充前后清单证明每个目录都只新增该 JSON；既有 EXE、DLL、Qt 文件及其他配置的路径和 SHA-256 均未变化。没有改写旧 `INPUT_PROVENANCE.json`、`INPUT_SHA256.json` 或其他历史身份记录。

本片未重打 SDK 五包，未运行 UI、完整构建、完整测试或全仓验证，不把本次小型部署函数验证扩大为产品功能、人工体验或正式发布结论。

## 2. 基线与范围

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 分支与 HEAD：`main@fe1683c9d540d7f5180cad1d0368cef4ccf1136d`
- canonical 配置：`examples/config/synthetic_stream_framing_slice.pae.json`
- canonical 长度：5868 bytes
- canonical SHA-256：`3F3F7FF2717F3D4CEE50231D0723077052FE668B697D2502DA872698C1C88A7E`
- 本片仓库修改仅为 `tools/protocol_lab_ui/standalone/cmake/DeployPaeLab.cmake` 与本报告。
- 总控已有的 `deliverables/README.md`、`docs/guides/02-首次运行与Lab体验.md`、`docs/guides/03-Windows-SDK集成.md` 改动均保留且未由本片修改。

## 3. 根因与最小修复

`PrepareStandaloneInputs.ps1` 已把 Binary 流式配置复制到 standalone 输入闭包，但 `DeployPaeLab.cmake` 的 `config_names` 没有列出该文件。因此输入快照具备配置，真实部署函数却不会把它写入 `PAE_CONFIG_FILES`，最终 Testing-off 部署目录缺少该配置。

本次只在 `config_names` 中增加：

```cmake
synthetic_stream_framing_slice.pae.json
```

没有修改部署语义、公共契约、PAE/Core/Host/stream 实现或 UI 代码。

## 4. 真实部署函数验证

验证根：

`F:\PersonalWorkspace\pae-trial-7b4205e-20260920\lab\evidence\stream-fixture-packaging-repair-fe1683c`

小型外部 CMake harness 直接 `include` 仓库中修改后的 `DeployPaeLab.cmake` 并调用 `pae_lab_configure_deployment`，未复制或重写被测函数。

### 4.1 生成命令包含配置

正例配置成功；生成的 `build-positive/packaging_probe.vcxproj` 的部署命令中，`PAE_CONFIG_FILES` 明确包含：

`.../standalone-configs/synthetic_stream_framing_slice.pae.json`

日志：`positive-configure.log`。

### 4.2 缺失文件精确失败

从独立配置输入集合中移除该文件后，CMake 配置失败，并由真实部署函数在 `DeployPaeLab.cmake:17` 报告：

```text
Missing standalone Lab config:
.../synthetic_stream_framing_slice.pae.json
```

日志：`missing-config-configure.log`。日志同时记录 `Configuring incomplete, errors occurred!`，证明缺失项没有被静默忽略。

### 4.3 正例部署与哈希

正例 Release 构建/部署退出码为 0；生成 `deploy-positive/Release/configs/synthetic_stream_framing_slice.pae.json`。部署文件长度为 5868 bytes，SHA-256 与 canonical 相同：

`3F3F7FF2717F3D4CEE50231D0723077052FE668B697D2502DA872698C1C88A7E`

日志：`positive-build-deploy.log`。

## 5. 四个现有部署目录的限定补充

补充前四个目标均不存在该文件。仅在确认缺失后复制 canonical，结果如下：

| 部署 | 补充前文件数 | 补充后文件数 | 唯一新增 | 删除 | 既有文件哈希变化 |
| --- | ---: | ---: | --- | ---: | ---: |
| static Debug | 17 | 18 | `configs/synthetic_stream_framing_slice.pae.json` | 0 | 0 |
| static Release | 17 | 18 | 同上 | 0 | 0 |
| shared Debug | 18 | 19 | 同上 | 0 | 0 |
| shared Release | 18 | 19 | 同上 | 0 | 0 |

四个新增文件均为 5868 bytes，SHA-256 均为：

`3F3F7FF2717F3D4CEE50231D0723077052FE668B697D2502DA872698C1C88A7E`

目标目录：

- `lab/static/deploy/debug-testing-off/Debug`
- `lab/static/deploy/release-testing-off/Release`
- `lab/shared/deploy/debug-testing-off/Debug`
- `lab/shared/deploy/release-testing-off/Release`

上述相对路径均位于 `F:\PersonalWorkspace\pae-trial-7b4205e-20260920`。详细补充前/后清单和差异汇总：

- `actual-deploy-before/*.json`
- `actual-deploy-after/*.json`
- `actual-deploy-supplement-summary.json`

汇总中四项 `only_expected_addition` 均为 `true`，`removed` 与 `changed` 均为空。

## 6. 验证边界与 Git 状态

- 已验证：真实部署函数生成命令、缺失配置负例、正例复制和 canonical 哈希、四个现有部署目录的补充前后全量清单差异。
- 未验证：Lab 功能执行、可见 UI、人工体验、全量测试、全仓构建、SDK 五包重打、正式发布。
- 未修改：SDK、历史交付、Testing-on、系统 Qt、旧 provenance/manifest、总控三份入口文档。
- 无 Stage、Commit、Push、发布或删除操作。

本片已停止写入，状态为“已完成派发范围，待总控复核”。
