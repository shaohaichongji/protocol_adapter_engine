# adeae30 Windows x64 首次体验包归集与限定验证

## 范围与身份

- 仓库 `main@adeae30d942ba42cc518c244c220730b7e462d2c`；本轮未 Stage、Commit、Push、替换旧包或正式发布。
- 本地候选：`deliverables/sdk/adeae30-experience/PAE-Lab-Windows-x64-adeae30/`，同级 `PAE-Lab-Windows-x64-adeae30.zip` 与 `ZIP.sha256`。用户入口是 ZIP 解压后的 `README.md`，默认程序是 `lab/pae_protocol_lab_ui.exe`，不需要启动器。
- 五个 SDK 来自 `out/build/experience-sdk-docs-adeae30-20260927/` 的 Source、Static Debug/Release、Shared Debug/Release 交付版投影包。包内 `PROVENANCE.json` 同时保留产品 `source_head=adeae30...` 与文档派生 `experience-sdk-docs/1`；五包内部清单/哈希未修改。Lab 来自 `out/build/experience-lab-adeae30-20260927/standalone-static-Release/deploy/Release/` 的完整 19 文件纯运行目录。顶层 Schema、Profile、诊断契约来自 `docs/experience/sdk-docs/` 投影，不是原仓库同名历史文本。
- `notices/qt-package.md` 原字节复制仓库固定 Qt 说明。其内容表明 Qt 快照标为 5.13.0，但许可附件、原始构建记录及再分发审查未闭合；实际 Qt 许可文本没有在固定副本中找到。本包仅供本地体验，**不能据此认定可对外分发**。

## 归集与完整性

使用 `scripts/package_experience_bundle.ps1` 的固定白名单归集。顶层包共 302 文件、64,126,126 bytes，其中 300 个 payload 文件纳入根 `MANIFEST.txt` 与 `SHA256SUMS.txt`；两份清单因自引用问题均明确排除自己及对方。ZIP 长 17,124,994 bytes，SHA-256：

```text
7d396cce55feccd647ce83fff1a081f83a78d51b75fbf2235d6d7c4973ff37a1  PAE-Lab-Windows-x64-adeae30.zip
```

逐文件比较归集前后 SDK：Source 103、Static Debug/Release 各 44、Shared Debug/Release 各 40，长度与 SHA-256 全部相同；Lab 19 文件全部相同。五个归集后的 SDK 均由 `scripts/verify_yaml_sdk_package.ps1` 验证通过，Source 用 `-HasYaml $false`（可选源码而非已安装组件），四个二进制包用 `$true`。ZIP 解压到新目录 `out/build/experience-bundle-check-20260927/unpacked with spaces/` 后，302 文件逐项长度与 SHA-256 相同，根清单 300 条全部通过，解压出的五 SDK 再次通过同一 verifier。消费运行后 ZIP SHA-256 与 `ZIP.sha256` 仍一致。

## 解压后实际运行

下列操作均使用上述含空格解压目录，并分别使用新的包外 BuildRoot `out/build/experience-bundle-check-20260927/json-consumer` 与 `yaml-consumer`：

- Static Release JSON `examples/getting_started`：`cmake -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133'` 配置、构建 `pae_getting_started`、运行，退出 0，输出 `GETTING_STARTED_BINARY_PASS`，`AA 00 07 -> 7 -> AA 00 07`。
- 同包 YAML `examples/yaml_sdk_consumer`：独立配置、构建 `pae_yaml_sdk_consumer`、运行，退出 0，输出 `PAE_YAML_SDK_CONSUMER_PASS`。
- 解压后 `lab/pae_protocol_lab_ui.exe --ui-smoke` 分别传入邻接 `configs/synthetic_ascii_literal_only.pae.json` 与 `.pae.yaml`，隐藏运行，退出 0，输出 `UI_SMOKE_PASS detail=2 document(s)`；两条均为 6 字节、0 字段的合成 ASCII 配置。

这些是限定的包外示例与隐藏 UI smoke，不是用户可见界面验收、所有 SDK 组合测试、真实协议/设备、稳定 ABI、Linux 或现场长期运行证明。此前 SDK/Lab 的更广专项证据分别见 `experience-sdk-docs-validation-20260927.md` 与 `experience-lab-validation-20260927.md`，本轮没有重跑它们的全部测试。

## 内容审计和保留例外

- 全包 55 份 Markdown 中扫描到 49 个本地链接，解析后缺失 0；顶层与 SDK 投影文档均纳入扫描。300 个 payload 文件名无机器绝对路径；274 个文本候选文件未发现大写 Windows 盘符绝对路径。检查总计覆盖 302 个文件的字节内容；纯二进制通用正则会把 URL、指令字节误判成路径，因此另以构建机与 Qt 路径前缀复核。
- 22 个未改原件二进制（Lab EXE/Qt DLL/plugin，以及 SDK 的 EXE 以外的 DLL/LIB）确有构建工作区、开发环境和 Qt 构建机的内嵌绝对构建/调试路径。它们不构成当前指南的命令依赖；依照不改包内字节要求未剥除，也不能表述为全包无机器路径。Source SDK 的上游 rapidyaml 原件还含单个 `.gitattributes`；没有 `.git/` 仓库目录、日志、缓存、测试 EXE 或 PDB。
- Qt 许可/通知及再分发审查是实际未闭合项；本地 smoke 和哈希一致不解决该缺口。历史构建机路径可能暴露内部目录名称，对外发布前需另行评估并重新制包，不在本轮改写固定二进制。

## Git 与交接边界

本轮仅新增本报告与 `scripts/package_experience_bundle.ps1`，更新 `docs/experience/` 根部三份指南和 `deliverables/README.md`；`docs/experience/sdk-docs/` 投影未修改。包和解压验证根位于 Git 忽略区域，保留原有未跟踪文档与脚本。未 Stage、Commit、Push、部署替换或删除，已停止写入，待总控复核。

## 总控复核后的原始证据补录（2026-09-27）

总控通知其独立复算最终 ZIP SHA-256 与本报告一致，根 300 个 payload 哈希全部通过。原先 `out/evidence/experience-bundle-check-20260927/` 尚不存在：上轮 JSON/YAML 的 Configure、Build 及首次 Run 和 Lab smoke 输出仅在会话终端中，**没有原始落盘日志**。本轮没有重跑 Configure/Build，也没有把既有 EXE 或 CMake cache 伪造为原始构建日志；上节构建结论保留为会话记录，其日志落盘缺口仍在。

现已只在新 `out/evidence/experience-bundle-check-20260927/` 补齐以下可复核文件，输入仍为同一含空格解包根，未重建或修改交付包：

- `json-run.stdout.log`、`json-run.stderr.log`、`json-run.exit.txt`：运行现存 `json-consumer/Release/pae_getting_started.exe`，退出 0、`GETTING_STARTED_BINARY_PASS`；`yaml-run.*` 对应现存 YAML consumer，退出 0、`PAE_YAML_SDK_CONSUMER_PASS`。
- `lab-ui-smoke.stdout.log`、`lab-ui-smoke.stderr.log`、`lab-ui-smoke.exit.txt`：直接运行解包 Lab 与邻接 JSON/YAML 合成配置，退出 0、`UI_SMOKE_PASS detail=2 document(s)`。stderr 保留诊断输出，不能因它非空判为失败。
- `source-extracted-302-files.csv`：每个相对路径的源/解包长度和 SHA-256；302/302 匹配。`hash-summary.txt`：根 300 条 manifest/sha 核验及 ZIP 与 `ZIP.sha256` 复算，仍为 `7d396cce55feccd647ce83fff1a081f83a78d51b75fbf2235d6d7c4973ff37a1`。
- `markdown-local-links.csv` 与 `markdown-links-summary.txt`：55 份 Markdown、49 个本地链接、缺失 0；五组 `sdk-*-verifier.stdout.log`、`.stderr.log`、`.exit.txt`：解包后的 Source/Static D-R/Shared D-R verifier 输出与成功状态。

补证只新增上述 Git 忽略的 evidence 文件并更新本报告；没有修改已交付目录、ZIP、清单、SDK 或其他跟踪/未跟踪文件。未 Stage、Commit、Push，补证完成后停止写入，待总控复核。
