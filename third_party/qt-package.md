# Lab Qt 5.13.0 固定副本

2026-09-13 用户决定完整复制 `F:/PersonalWorkspace/DEI/third_party/windows/qt`，长期供 Lab 使用。
副本位置为本目录下 `qt/`，2440 个文件、296552738 字节；复制后逐文件 SHA-256 与原目录一致。
全部原始模块、工具、头文件、库、运行文件和调试资产原样保留，没有裁剪或修改包内字节。
`qt-files.json` 记录复制时的完整相对路径、大小和 SHA-256，不包含本文或清单自身。

CMake 的 `PAE_QT_ROOT` 新配置默认指向该副本；已有缓存中的显式路径不会被自动覆盖。
外部路径覆盖仍受支持。Qt 仅用于可选 Lab UI，当前直接链接 Core/Gui/Widgets，保留全包不等于新增模块使用。
消费工具链仍为 Windows x64 / MSVC v142 14.29.30133；Debug CRT 等系统依赖不由这个副本新增安装。
不修改 DEI 原包、现有 Qt5.11、系统 PATH、注册表或 Qt Creator。

来源证据采用本地既有包快照，不声称整个包是未经修改的官方归档。
此前核验所见 Core DLL 安装前缀及部分头文件文本格式差异保留原状。
许可附件、原始构建记录和最终分发审查尚未闭合；用户决定复用现有包不被表述为法律合规证明。
官方重新组包路线停止，相关历史检查记录保留于 docs/engineering/lab-qt-dependency-audit-plan.md。

更新时须单独明确版本与范围，重新生成清单、比较原包和副本，再做 Windows Debug/Release 定向验证。
Git 对 qt/** 禁用文本换行转换，避免入库改变原始头文件字节。当前未 Stage、Commit 或 Push。

## 本次验证

新目录 `out/build/windows-msvc-lab-vendored-qt` 沿用现有Host Observer配置功能开关，未传PAE_QT_ROOT，
配置输出确认采用仓库副本。Debug/Release的pae_protocol_lab_ui目标均构建成功。
清除测试子进程Qt/QML环境变量、PATH仅保留Windows目录后，两配置Qt窗口烟测各7/7通过。
日志位于该构建目录build-debug.log、build-release.log及Testing中。
这不是全部模块、纯净目标机部署或完整人工验收的验证结论。

## 收尾检查（2026-09-13）

- 清单与磁盘均为2440文件，逐项SHA-256复核无差异；2440项Git text属性均为unset，
  没有文件被忽略。最大文件Qt5Guid.pdb为41578496字节，无单文件达到100MiB。
- UI OFF隔离：新目录out/build/windows-msvc-qt-ui-off，关闭UI、启用COMPLETE_RECORD切片，
  显式指定不存在的Z:/intentionally-missing-qt；配置成功，生成vcxproj/props未检出Qt库、
  仓库Qt路径或moc/uic引用。本项为配置及生成文件检查，不是该配置的新构建/运行验证。
- 独立工具：清除子进程Qt/QML变量，PATH仅为仓库qt/bin/release及Windows目录；
  moc/rcc/uic的-v全部报告5.13.0并退出0。uic另成功把最小QWidget .ui生成Ui_CheckpointWidget头文件。
  该结果说明此固定副本的工具可按显式依赖环境运行，不代表裸PATH启动uic或所有工具功能已验证。
- 本轮日志、合成.ui及生成头文件在忽略目录out/qt-checkpoint-closeout，不进入提交。
- 用户已删除官方审计目录，再次检查确认不存在；此前官方候选包uic停滞未查明原因，
  不将其原因归于当前副本，也不继续该已停止的重组路线。

工程收尾完成，等待独立Git交付授权。许可及原始来源材料边界不因上述测试通过而消除。
