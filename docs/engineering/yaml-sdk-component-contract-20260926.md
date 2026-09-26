# YAML 可选 SDK 组件：首片实施契约

## 决策与边界

用户同意预检后的下一步，采用独立静态附加组件 `PAE::yaml_frontend`，默认 OFF。
静态和动态 PAE 均可消费它；后者称为“PAE DLL + 静态 YAML”，不产生 YAML DLL，
不承诺跨编译器稳定 ABI。仅在本地形成验证候选，不选择项目许可证、不发布成品。
YAML 是作者源，转换后显式调用原 `CompileProtocolJson`；不得改变 Core、Schema
规则、Profile 语法或 JSON-only 默认行为。人工体验按用户决定延期至统一成品。

## 公开封装

- 独立可选头 `src/config_frontend_yaml/public/pae/yaml_frontend.h`，安装为
  `include/pae/yaml_frontend.h`；命名空间 `pae::yaml`，不安装内部 `frontend.h`。
- move-only 不透明结果 owner，同步借用输入、成功自持 JSON/来源身份/映射。
  构造、移动、销毁实现归属组件；失败不提供部分结果，分配失败安全返回状态。
  view 在 owner 销毁、赋值或移动后不再保证有效；移动后对象可安全查询/销毁。
- 查询状态、原因、JSON、来源身份；来源查询返回值类型，不暴露内部池/Entry指针、
  rapidyaml 类型或测试故障注入。精确查询与最近祖先查询须区分，祖先回退明确近似。
  不猜测语法错误行列，不改变现有编译诊断 ABI。
- 首片固定沿用输入 16 KiB、Parser/辅助各 128 KiB、JSON 32 KiB、512 节点、
  16 层、4 KiB 标量，命名为试用组件资源约束 V0.1，不开放任意调参。
  文档说明这是当前拒绝边界而非生产容量或 RSS 上界；后续改变须显式更新约束版本、
  说明及测试，不静默扩大。本片不修改内部参数值；新增 owner 开销单独说明，不能遗漏计费后
  仍宣称全内存已覆盖。必要时提供只读限额查询，便于 Lab 预读，不暴露 fault 参数。

## 首片实施与验证范围

《子任务推进》独占以下必要改动：

- `src/config_frontend_yaml/` 新公开封装及 CMake；内部转换算法原则上不改。
- `tests/config_frontend_yaml/` 公开接口和生命周期专项测试。
- `cmake/PaeSdkInstall.cmake`、`cmake/PAEConfig.cmake.in` 的条件安装/组件发现；
  根 `CMakeLists.txt` 仅在必要时做最小接线，保留未提交 Lab 修改。
- 新 `examples/yaml_sdk_consumer/` 自包含合成样例及 consumer，复用既有合成向量；
  不依赖 spikes/src/开发仓库绝对路径。
- `docs/sdk/README.md` 追加组件边界，`schema/pae_yaml_profile_v0.1.md` 仅同步状态与
  试用资源约束口径；新增 `docs/engineering/yaml-sdk-component-validation-20260926.md`。

开启时安装静态库、公开头、Profile、随仓第三方许可通知及最小 consumer；关闭时不安装
误导性 YAML 头或 target。显式请求缺失组件应在 Configure 失败，普通 `find_package(PAE)`
行为兼容；组件不自动链接 PAE Core，不泄漏内部源码 include 路径。
保留既有同配置、工具链和 CRT 限制；检查重复 find_package 及先基础后请求组件场景，
避免现有 target 早退绕过缺组件检查。

独占新根 `out/build/yaml-sdk-component-20260926`、`out/evidence/yaml-sdk-component-20260926`，
使用仓库有效 Windows x64 工具链，Debug/Release 串行，不修改系统 Qt/PATH。
专项验证 owner 生命周期/失败/来源映射及原内部前端回归；安装 static/shared PAE D/R，
包外 consumer 只经安装头和 imported targets，运行 YAML→JSON→公开 Compiler→Codec。
检查同包 DLL 来源、配置错配门禁、JSON-only 无组件消费及缺组件请求明确失败。
至少 static/shared Release 搬迁到含空格新根再消费，保留日志；不以 Configure 代替运行。
若矩阵因超范围问题阻塞，报告具体项，不悄悄扩大产品或降低断言。

## 后续片与停点

本片不改 `scripts/package_sdk_stage3.ps1`、Lab/standalone、现用 deliverables、
第三方原件、公开 PAE Core API；源码包白名单/正式归集及 Lab 公共消费迁移后续串行。
本片安装验证不是干净正式 SDK 发布，记录 HEAD + dirty 来源和实际文件证据。
总控独占计划/契约/索引；其余任务停止写入，禁止同构建根并发。
无 Stage/Commit/Push、删除、部署替换、可见 Lab 或外部发布授权。
实现、验证、报告完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次：
实际文件、命令/结果/证据、未验证、风险及 Git 状态；停止写入，等待总控复核。
