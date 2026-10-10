# SDK getting_started Windows 路径修复（2026-10-10）

## 独立定位

接管 `main@9680cf90512f053962949cb55467d6af35959088`，暂存空；已有体验文档投影和
三份交付报告保留。只修改 `examples/getting_started/main.cpp`，新增本报告。
没有修改 Core、公开 API、其他示例、CMake、打包脚本或任何候选包。

隔离根：`out/build/sdk-getting-started-path-fix-20261010`，下文日志均位于其 `evidence/`。
原始失败来自 `out/build/aligned-experience-9680cf9-stageC-20261010/final-validation/`；
相关原命令、退出和 stderr 已复制保留，未覆盖原文件。

- 原配置绝对路径为 265 个 UTF-16 代码单元，文件存在。旧 EXE 的绝对路径调用、
  显式包根 WorkingDirectory 的 ASCII 相对调用均退出 1 / `cannot read config`。
- 将完全相同配置字节复制到短路径，旧 EXE 的绝对调用和 ASCII 相对调用均退出 0。
  长短配置 SHA256 均为 `3F3F7FF2717F3D4CEE50231D0723077052FE668B697D2502DA872698C1C88A7E`。
- 原生 `CreateFileW` 直接打开实际长路径返回 Win32 3；加 `\\?\` 前缀后打开成功。
  因而本次实际失败由普通长路径打开限制解释，不能仅凭 stderr 乱码归因于编码。
- 当前 ACP 为 936；相同字节的短文件名 `配置😀.json` 经旧 `main(char**)` 调用失败，
  另行证明窄 argv 还存在不能表达完整 Unicode 文件名的限制。

上述证据见 `native-path-probe.json`、五份 `before-*.json` 和 `original-*`。
定位脚本 `probe.ps1` 退出 0；路径对照不是网络实验。

## 最小修复与身份

Windows 使用 `wmain(wchar_t**)`，路径以 `std::filesystem::path` 保留原生宽字符。
读取前解析相对路径为绝对路径、规范化分隔符和点段，并添加 Windows 扩展路径前缀；
UNC 分支使用 `\\?\UNC\`，已经扩展的路径不重复加前缀。不依赖修改系统长路径策略。
随后使用 C++17 `ifstream(path)` 读取原始字节。非 Windows 保留 `main(char**)`。
参数、配置编译、metadata、Decode 和 Encode 检查不变；失败不输出成功标记。

本轮链接的产品库仍来自已验证 `deliverables/sdk/9680cf9/static-Release`。
示例是未提交补丁，不是 9680cf9 原始源码；未原地替换原包，派生包身份由总控决定。

| 文件 | SHA256 |
| --- | --- |
| 修改后 `examples/getting_started/main.cpp` | `608C0A526470152565445BE95FBDCF2A87BB7B807ADFA7CE24F1EE43272E65F7` |
| `evidence/getting-started-path.patch` | `987B356AF80B1088A07B1C09C036ADBFF263801F3D0450618CBA153BAFC73DBA` |
| `consumer/Release/pae_getting_started.exe` | `4A087A97B338B697DA3926A20CEC4B1216F7789BF2676C8AB07D64E7EC6F883D` |

## 本次实际验证

仅新建一个 Static Release consumer 根；无 SDK/Lab 重建或 18 次消费矩阵重跑。
VS18 2026、x64、v142 14.29.30133、MSVC 19.29.30159.0、Windows SDK 10.0.22621.0。

```powershell
cmake -S examples/getting_started -B <隔离根>/consumer `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DPAE_DIR=<现有Static Release包>/lib/cmake/PAE" `
  "-DCMAKE_PREFIX_PATH=<现有Static Release包>"
cmake --build <隔离根>/consumer --config Release --target pae_getting_started --parallel 4
```

配置/构建均退出 0，实际 Cache 的两条选包路径规范化后与指定包一致。
初次格式检查发现一处缩进，clang-format 后再次构建这一目标并复核相同用例，
没有重配或修改依赖。`validate.ps1` 及最终 `validate.ps1 -Final` 均退出 0。

| 最终独立用例 | 退出与断言 |
| --- | --- |
| 实际 265 字符绝对路径 | 0 |
| 实际解压包根 WorkingDirectory + 原 ASCII 相对路径 | 0 |
| 短路径绝对调用 | 0 |
| 短 ASCII 相对调用 | 0 |
| 短 Unicode/emoji 相对调用 | 0 |
| 短路径缺失文件 / 深包根缺失文件 | 各 1，`cannot read config`，无成功标记 |
| 无参数 / 多参数 | 各 2，usage，无成功标记 |

五个成功用例均精确检查五行输出、Decode `AA 00 07` 得到 7、Encode 得到 `AA 00 07`、
末行 `GETTING_STARTED_BINARY_PASS` 和空 stderr。每个进程显式设置 WorkingDirectory，
不用 ShellExecute。最终九份 `final-after-*.json` 保存实际参数和结果；不是 CTest 数量。
构建日志为 `configure.log`、`build.log`、`final-build.log`，选包见 `cache-binding.log`。

264 个受保护文件（原五包全部文件、原失败配置及 EXE）最终长度/Hash 未变。
最终 clang-format dry-run/Werror 与 `git diff --check` 均退出 0。
本任务新增候选报告为人工合成证据说明，无私有报文、生产端点或绝对机器路径。

## 停点与限制

HEAD/分支不变、暂存空；未 Stage/Commit/Push、清理、改环境或注册表、改 Qt、引入依赖。
未执行 Debug、其他 SDK 配置、Lab、Linux、UNC/网络、极端路径/权限或整体最终包验收；
保留非 Windows 编译分支不等于本轮 Linux 已验证。这里只修复最小示例，不新增通用
文件框架、生产文件大小门禁或异常恢复。

本次复现问题的修复和限定验证已完成；没有发现该实际配置调用的剩余阻断。
最终体验包仍需总控处理派生示例身份、归集与复核。完成一次交接后停止写入，待总控复核。
