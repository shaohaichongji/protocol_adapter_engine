# Lab Qt 依赖核验与随仓方案

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

日期：2026-09-13。当前决定：用户改为完整复用 DEI Qt 包，已复制至 third_party/qt 并设置新配置默认路径。
第1至11节为历史核验过程，不再代表继续推进官方重新组包的计划。未实施 Binary Host，未 Commit / Push。

## 当前执行结论（用户决策覆盖此前候选方案）

完整复制2440个文件、296552738字节，逐文件SHA-256与DEI源目录一致；保留全部模块，不做裁剪。
见 [固定副本说明](../../../third_party/qt-package.md) 与 `third_party/qt-files.json`。
现有显式CMake缓存不自动覆盖，后续构建必须核对实际PAE_QT_ROOT。
用户要求删除 `D:/develop_env/Qt/pae-qt-audit-20260913-120357`，最初删除调用被执行接口安全策略拒绝；
用户随后手动删除，已再次核对目录不存在。下文该目录及材料路径仅为历史记录，不再是可用证据入口。
DEI原包及Qt5.11未修改。

仓库副本已在新建windows-msvc-lab-vendored-qt目录默认选中并完成Debug/Release UI构建，
去除测试子进程外部Qt环境后，窗口烟测两配置各7/7通过；不提升为全部Qt模块或纯净目标机验证。

Qt检查点收尾已完成：2440项Hash、Git换行属性及忽略规则检查通过，没有单文件达到100MiB；
UI OFF并指定不存在的Qt根目录配置成功，生成的vcxproj/props未发现Qt库及工具引用。
仅在子进程PATH加入仓库副本bin/release并清除Qt/QML变量，moc/rcc/uic版本检查全部退出0；
uic另成功生成最小QWidget UI头文件。详见 [固定副本说明](../../../third_party/qt-package.md)。
这关闭当前固定副本的独立uic验证缺口，不解释已删除官方候选包此前的启动停滞原因。
尚未Stage/Commit/Push；许可/来源材料边界仍保留，不将工程检查点收尾等同正式分发审查通过。

## 1. 结论与证据边界

Lab 当前使用外部 Qt 包，PAE 仓库的 `third_party` 尚未包含 Qt。
建议固化 Lab 所需的完整开发依赖闭包，不复制整套 Qt SDK，也不因可接受大文件而引入无关模块。
现有包的来源和分发材料尚未闭合，暂不能作为已批准的随仓依赖。

本地核验入口：

- 当前构建缓存：`out/build/windows-msvc-lab-host-observer/CMakeCache.txt`。
- 消费边界：`cmake/PaeQt513.cmake`、`tools/protocol_lab_ui/CMakeLists.txt`。
- 部署范围：`cmake/DeployProtocolLabUi.cmake`。
- 当前外部包：`F:/PersonalWorkspace/DEI/third_party/windows/qt`。
- 包的相邻 README 仅描述 Qt 5 headers / libs / runtime 用途，不提供获取与构建记录。

外部包核验只提取通用依赖事实；不向 PAE 复制外部项目代码或业务资料。

## 2. 已确认事实

| 项目 | 当前证据 |
|---|---|
| 版本 / 配置 | `include/QtCore/qconfig.h` 为 5.13.0，shared=1、static=-1、debug_and_release=1 |
| 包规模 | 2440 个文件，约 282.81 MiB；本次盘点没有单文件超过 100 MiB |
| 包结构 | 顶层 bin / include / lib；含超出 Lab 直接需要的模块 |
| Lab 直接链接 | Core / Gui / Widgets；Qt 仅属于可选 UI，不进入 PAE Core |
| 消费工具链 | Windows x64、VS generator、v142 14.29.30133、cl 19.29.30159.0；这是 Lab 消费端要求，不证明 Qt 原始构建编译器 |
| 工具 | moc / rcc / uic 的文件版本均为 5.13.0.0 |
| 当前部署 | Debug / Release 分别部署三个 Qt DLL 和 platforms/qwindows[d].dll；开发头文件与工具不进入运行目录 |
| 分发材料 | 本次包内文件名扫描未发现 LICENSE / COPYING / README / SHA256 材料；不能由此推断适用许可证或原始获取方式 |

使用 VS 14.29.30133 的 dumpbin /dependents 对三个工具及两种配置的三个 Qt DLL、qwindows 插件检查：

- Widgets 依赖 Gui / Core，Gui 依赖 Core，qwindows 依赖 Gui / Core。
- Release 依赖 MSVCP140 / VCRUNTIME140 / UCRT API 及 Windows 系统 DLL。
- Debug 依赖 MSVCP140D / VCRUNTIME140D / ucrtbased；开发环境前置条件必须另列。
- uic 直接依赖 Qt5Core.dll；moc / rcc 的本次导入表未列出 Qt DLL。
- 工具位于 bin，而 Release Qt5Core.dll 位于 bin/release。后续必须明确工具运行环境或批准的同目录依赖布局，并在去除外部 Qt PATH 后执行工具验证。

导入表不是运行闭包证明；动态加载插件、图形后端和实际部署启动仍待验证。
有限 Git 路径历史只找到目录整理记录，未据此确认原始安装包、源代码或本地修改情况。

以下 SHA-256 仅标识本次样本，不是完整包锁或上游真实性证明：

| 包内路径 | SHA-256 |
|---|---|
| include/QtCore/qconfig.h | 813C65F965BD39A1D130172C08C826721FEB25BFE56386C9BAAA10F720C68DBB |
| bin/release/Qt5Core.dll | B12770D29184AB3724DFC3480728D4D72E8575E53B3EAE617B946F3F46704578 |
| bin/uic.exe | BCBBB9D5231315DD1F77FB05D7863FA866D575EF4DED6ED7DC415E09E405CE7D |

## 3. 来源与分发门禁

复制和推送前须取得：原始获取渠道、安装包或源包版本与校验值、对应源码、原始工具链与配置、是否本地修改，以及库、工具、插件和内嵌第三方组件的许可材料。
未能确认的字段明确保持 unknown，不以相同版本号或 DLL 文件版本替代来源证据。

Qt 官方说明强调适用开源许可证下的对应源码、许可文本及通知等义务；动态链接本身不足以证明分发合规。
参见 [Qt 官方 GPL/LGPL 义务说明](https://www.qt.io/development/open-source-lgpl-obligations)（2026-09-13 查阅）。
该通用说明不是现有 5.13.0 二进制具体授权的证明；本方案不作法律合规结论。

若无法恢复现有包来源，应另行选择可追溯的同版本包或可复现构建，并重新验证；不静默升级到其他 Qt 版本。

## 4. 拟随仓范围与目录

拟根目录：`third_party/qt/5.13.0/windows-x64/`。
这是待实施方案，本轮不创建该目录，不套用外部项目的目录规则。

1. Core / Gui / Widgets 所需完整传递头文件及配置头；以编译依赖检查确认，不仅按模块文件夹名裁剪。
2. 上述模块 Debug / Release import libraries 和 DLL，以及两种 qwindows 插件。
3. moc / rcc / uic 及经确认的工具运行依赖；如新增 Qt5Core 工具侧副本，显式列入清单并验证 Hash 一致。
4. 可保留与所选二进制匹配的 PDB，单列为调试资产；不声称未核对的 PDB 已匹配。
5. 来源说明、完整文件清单、许可证与 notices、对应源码提供方式及修改记录。

默认不纳入 Designer、Charts、Network、Sql、SerialPort、Svg、OpenGL 等未证明需要的模块、Qt Creator、示例、其他编译器 kit 和构建中间物。
这不是断言这些模块永不需要；发现传递依赖后须更新清单与许可核验。
系统 DLL 不从开发机盲目复制；MSVC runtime 安装前置条件或批准的部署方式单独处理。

开发包与运行包分离：仓库保存可构建的依赖闭包；Release 运行包只携带实际需要且获准分发的运行依赖和通知材料。

## 5. 版本锁与消费策略

拟增加同目录 README.md、dependency.lock.json、SHA256SUMS 及 licenses/。
锁文件至少记录：Qt 精确版本、package revision、来源 URL / archive hash、原始编译器与配置、目标架构、模块、许可材料位置、对应源码、逐文件相对路径/大小/SHA-256、本地修改及更新时间。
源包未知时不生成看似正式的已批准锁；本次样本 Hash 仅留在核验报告。

未来实现要求：

- 可选 Lab UI 默认消费仓库内固定包；显式 PAE_QT_ROOT 外部覆盖仍可保留，但输出实际根目录与版本，不静默回退。
- 配置阶段验证锁及必要文件；不自动下载；UI 关闭时不读取 Qt 或引入 Qt 依赖。
- 版本更新单独审查，不覆盖历史版本而不更新 package revision / Hash / notices。
- Git 普通对象或 LFS 在完整拟纳入清单生成后单独确定；本轮不改 .gitattributes，不迁移历史。

## 6. 下一步与验收

先补齐第3节来源材料，再申请复制及构建调整的独立实施授权。
后续验证计划为：逐文件 Hash 与缺失/篡改负例；干净构建目录离线配置；移除外部 Qt 路径的工具执行；Windows Debug / Release 构建与定向测试；部署启动和最小人工回归；UI OFF 隔离检查。
不能把当前外部包构建历史冒充仓库内包验证，也不能把开发机启动冒充干净目标机部署成功。

本轮仅完成静态文件、配置、导入表核验与文档检查，未执行新构建、运行验收或依赖复制。
Binary Host 六项已确认契约保持有效，实施仍待独立授权，不与本依赖检查点混合提交。

## 7. 来源追查补充（2026-09-13）

用户要求继续推进后，限定补查包所属仓库的依赖声明及 Qt 路径历史：

- 依赖声明将 Qt 5.13 UI runtime 标注为 LGPLv3，并给出 Qt 官网及 5.13 官方归档入口。这是项目声明线索，不是当前二进制与上游包一致性的证明，也不直接覆盖工具及内嵌组件。
- 可见路径历史包含目录整理与基线初始化；没有从这些记录取得原始安装包 Hash、构建参数或修改清单。
- 已知本机 `D:/develop_env/Qt` 下仅看到 Qt5.11.3 目录，不能拿它代替当前 5.13.0 包。未扩大扫描用户全盘。
- [Qt 官方 5.13.0 归档](https://download.qt.io/archive/qt/5.13/5.13.0/)仍列出 Windows 开源安装包（约 3.7G）、single 与 submodules 入口。此次仅查看目录，未下载或运行安装程序；安装器名称中的 x86 不用于推断待选 kit 的目标架构。

后续有两条路径：优先由用户提供现有包的原始安装包或获取/构建记录，进行对应性核验；若确实无法恢复，则单独批准从官方归档获取同版本材料，检查目标 kit、许可证及 Hash，再评估替换与验证成本。
本次追查没有解除来源门禁，不自动启动数 GB 下载、安装、包替换或 Binary Host 实施。

## 8. 官方材料获取（2026-09-13 获准）

用户随后授权从官方归档获取同版本材料核验。下载目录为仓库外
`D:/develop_env/Qt/pae-qt-audit-20260913-120357/`，不运行安装器、不替换现有依赖。

- `qtbase-everywhere-src-5.13.0.zip` 已下载，大小 80,298,006 字节；实测 SHA-256 为 `214af72d583a74fa1d0b9f8d4b4d69430ccdb2cc805119e4fdc6ca37db473906`，与官方 `.sha256` 一致。
- 从源包限定提取了根 LICENSE 文件及 moc/rcc/uic 的 main.cpp。三个工具的源码许可头为 GPL-EXCEPT，指向 LICENSE.GPL3-EXCEPT；后续许可清单必须区分工具、库与内嵌第三方组件，不将工具统一标注为 LGPLv3。
- Windows 安装包已于 2026-09-13 12:21:56 完成大小与 SHA-256 核验。实测大小为 3,955,186,416 字节，SHA-256 为 `99d64cd78176b117089f4896740b44f596c8403ea254b2264b532bd310c747db`，均与官方一致；本地 download-verification.json 保存结果。
- 本地保存官方 installer.sha256、installer.mirrorlist.html、qtbase.sha256；官方 meta4 链接返回404，未将其视为已取得的材料。
- `verify-download.ps1` 仅等待本次下载结束并检查大小 / Hash；通过后将 .part 重命名为 .exe，不执行该文件。结果写入同目录 download-verification.json，缺失结果或 not_verified 均不能视为完成。

官方材料入口：[安装包详情](https://download.qt.io/archive/qt/5.13/5.13.0/qt-opensource-windows-x86-5.13.0.exe.mirrorlist)、[QtBase 源包](https://download.qt.io/archive/qt/5.13/5.13.0/submodules/qtbase-everywhere-src-5.13.0.zip)。
即使安装包 Hash 通过，仍需检查具体 x64 kit、构建元数据与许可/第三方清单；尚未证明原外部包与官方包相同，也未完成替换构建验证。

## 9. 安装包静态检查与现有包样本对应

用户授权继续检查，并要求不影响现有 Qt 5.11。此次没有运行 Qt 安装器或安装脚本，
没有修改 PATH、注册表、Qt Creator 配置、现有 Qt 安装目录或项目构建设置。

在同一仓库外核验目录内，inspect-embedded.ps1 只读扫描已核验 EXE，按内嵌 7z 头边界
复制压缩数据并交由 7-Zip 列表检查；共得到 572 个可列出的压缩包候选。
这是归档检查，不是对安装器全部语义的解析。单独运行 7-Zip 打开整个 EXE 只列出首个
Android 组件，因此没有将该列表误认为完整 kit 清单。

`archive-144.7z` 含 `5.13.0/msvc2017_64` 的 QtBase 内容。解包输出仅位于
kit-review / debug-review 内（分别输出3153个、59个文件），包括所需元数据、头文件和
二进制样本；未解包到 Qt5.11.3，也没有执行任何解包出的 EXE。
内嵌归档临时副本合计约3.93GB，保留用于复核，本轮未清理。

构建信息核验：

- qconfig.pri：Qt 5.13.0、QT_ARCH=x86_64、shared、debug_and_release；原始编译器版本字段为19.15.26730。
- Qt5Core.dll 的 PE machine 为8664（x64），linker version为14.15。
- qconfig.pri 同时包含 QT_EDITION=Enterprise；这是构建元数据，不单独决定分发许可。
  实际来源为已校验的官方 opensource 安装包，许可仍按具体文件与对应来源材料核对。
- Lab 消费端继续锁定原有 v142 工具链；本轮不更改工具链，也不宣称新包已通过该工具链构建验证。

12项样本比较结果写入本地 sample-comparison.json：

| 范围 | 结果 |
|---|---|
| Gui / Widgets Debug与Release DLL、qwindows两配置、moc/rcc/uic、qconfig.h（共10项） | SHA-256与现有外部包完全一致 |
| Qt5Core.dll | 同长度，仅32字节不同，偏移3021292～3021324，落在qt_prfxpath安装前缀区域 |
| Qt5Cored.dll | 同长度，仅36字节不同，偏移7730092～7730129，落在qt_prfxpath安装前缀区域 |

官方两个Core文件均保留构建机安装前缀；现有Debug和Release各指向不同的本地历史安装路径。
这些差异与安装路径重定位相符，但不据此推断修改者或原始安装流程。其他头文件、import libs、
PDB及未选模块还没有完成逐文件对应，不能将12项样本结论提升为全包来源已闭合。

许可材料方面，已读取同版本源包 LICENSE.GPL3-EXCEPT 的 Qt Company GPL Exception 1.0，
并核对三个工具的源码许可头。它不等同于仅附 LGPL 文本；仍需整理库、工具及实际内嵌第三方
组件的通知和源码对应清单。未从构建元数据或工具生成输出例外推导整个开发包的分发结论。

保护性复查：现有 Qt5.11.3 七个 kit 的 qmake.exe SHA-256 在本次归档检查期间两次读取一致。
这属于有限关键文件检查，不声称完成Qt5.11全目录审计或既有项目运行回归。

下一步建议以官方包为候选基线，在仓库外整理最小完整文件清单、许可证/第三方材料和
重定位策略；先完成可追溯包定义，再独立授权随仓复制与Windows Debug/Release验证。
继续保留Qt5.11环境，不安装新全局Qt、不自动升级、不修改系统环境；本轮无Commit/Push。

## 10. 候选清单与重定位草案（2026-09-13）

按用户确认，在仓库外核验目录的candidate-definition中生成候选定义，尚未组装依赖包：
candidate-files.json、SHA256SUMS.proposed、license-source-index.json、root-license-index.json及README.md。
生成脚本prepare-candidate-manifest.ps1保留在同一核验目录，不进入项目构建链。

- 清单1800个唯一目标路径，均有来源、大小与SHA-256；基础/工具/审计1794项共117595803字节，
  图形后端候选6项共49723976字节。基础包含Core/Gui/Widgets完整模块头目录1772项，尚非编译验证后的最小闭包。
- 与原外部包可对应1789项：1782项Hash相同，7项不同。除已知两个Core DLL前缀外，新增发现5个头文件不同：
  qobject.h、qvariant.h、qabstractitemview.h解码文本相同；qobjectdefs.h、qstring.h剔除空白后相同。
  不将这种文本检查升级为语义证明；候选基线采用官方原样，不继承外部格式改动。
- 许可索引覆盖源包src下37份元数据文件的59条声明；已声明LicenseFile路径均存在。
  这是宽范围来源索引，不是59个组件均被当前Windows二进制链接的结论；内联许可及实际适用性仍须收口。
- 当前kit启用动态OpenGL/ANGLE。单凭导入表不能排除图形运行时加载依赖，故libEGL/libGLESv2两配置、
  d3dcompiler_47.dll、opengl32sw.dll单列为未批准候选。额外组件的源码/许可不能由QtBase索引代替。
- 重定位草案保持DLL原始字节，通过工具旁Release Core副本、应用私有qt.conf和验证子进程环境隔离实现；
  qt.conf不解决Windows首次DLL装载，且不修改全局PATH或Qt5.11配置。依据为同版本qlibraryinfo.cpp，未运行验证。

尚未生成正式dependency.lock.json、许可批准记录或可部署包；未混入未匹配PDB。
下一步先收口基础/图形范围与许可附件，再独立授权仓库外组装和隔离验证，之后才决定随仓接线。

## 11. 仓库外基础候选组包与首次隔离检查（2026-09-13）

按用户确认，在 `D:/develop_env/Qt/pae-qt-audit-20260913-120357/candidate-package`
组装 Lab Widgets raster 候选。源代码检索未发现 Lab UI 使用 QOpenGL、QGLWidget、Qt Quick 或 Web
组件；本轮排除6项图形后端候选，不将基础包视为通用Qt SDK或OpenGL部署包。

- 保留1794项基础清单文件，逐项校验复制前后SHA-256；另保留完整官方QtBase源码zip、根许可文本、
  全部src归属元数据及其声明的许可文件、来源索引和工具私有qt.conf。
- 实际清单MANIFEST.json包含1895项、199221176字节（不含清单自身）。源归属目录是保守全集，
  并非实际链接组件的最终清单；完整源码附件不等于分发义务或法律审查已闭合。
- 子进程PATH仅保留Windows目录，清除该子进程Qt/QML变量后，moc/rcc返回5.13.0。
  uic未正常退出，已核对其进程路径并结束本轮启动的该进程；此项验证未通过。
- uic-isolated-modules.json记录其Qt5Core确实从候选包bin加载，MSVC/UCRT从Windows加载；
  因而不能简单将此次停滞归因于缺失Qt5Core。确切原因尚待诊断，未进行补丁绕过。

本轮没有修改Qt5.11、全局环境或项目构建接线，没有运行安装器，没有Commit/Push。
Windows Debug/Release新包构建、QPA加载与Lab烟测尚未执行；先解决uic启动问题，再继续这些验证。
候选包尚不具备随仓替换和正式分发结论。
