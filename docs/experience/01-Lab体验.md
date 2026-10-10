# 01 启动 Lab：先看两条正常解析

回到[总入口](README.md)。以下命令和路径均从体验包根执行；保留 `lab/` 下 EXE、Qt DLL、`platforms/` 和 `configs/` 的完整相对布局。

```powershell
$BundleRoot = (Resolve-Path .).Path
& (Join-Path $BundleRoot 'lab/pae_protocol_lab_ui.exe')
```

启动失败先检查目录完整性、Windows x64 及已安装的对应 MSVC Runtime；不要改全局 `PATH` 或混入其他 Qt DLL。GUI 的自动检查与人工体验分别记录，启动成功本身不是解析成功。

## 最短操作

在 Lab 中浏览 `lab/configs/` 下的配置并“加载 / 重新加载”，在“绑定设置”应用表中的 `endpoint / action / pipeline`，再到“操作”输入 Hex 并执行完整记录解析。每项单独加载、单独观察，不把上一次结果当作本次结果。

| 作者源 | 配置与绑定 | Hex 输入 | 应观察到 |
| --- | --- | --- | --- |
| JSON | `synthetic_binary_ui_stage1.pae.json`；`device / Decode / ui_pipeline` | `80 0D 03 00 01 00 CA FE 05 5A` | 命中 `typed_record`，9 字段；`count=1`、`payload=CAFE`、`marker=90` |
| YAML | `synthetic_ascii_literal_only.pae.yaml`；`device / Decode / ascii_pipeline` | `50 49 4E 47 0D 0A` | 命中 `ping`、解析成功、0 字段 |

YAML 的 Hex 是 `PING\r\n`。两项均是合成完整记录的 Decode，不验证 Encode、分块流、错误配置诊断或真实设备。若配置不能加载，先看转换/编译诊断；若加载成功但解析失败，检查所选绑定、输入字节和执行状态，见[配置与边界](03-配置与边界.md)。

隐藏 `--ui-smoke` 使用 `synthetic_ascii_literal_only.pae.json` 与同名 YAML 验证启动和配置入口；这里的 Binary 界面步骤另行核验。界面中的绑定应用、Hex 输入和字段展示仍须由使用者按本页步骤观察，不能用自动 smoke 代替人工体验。
