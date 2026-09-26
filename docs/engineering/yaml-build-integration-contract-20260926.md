# YAML 可选依赖与构建入口固化

## 授权与目标

2026-09-26 用户同意继续前端之后的依赖/构建片。此前内部前端返修已限定复核，
本片消除构建对隔离 out 头文件的依赖；不代表 Lab、SDK 或正式公开 YAML API 接入。
现场基线需重查，保留此前全部变更，无 Stage/Commit/Push 授权。

## 实施边界

- 将已验证的 rapidyaml 0.16.0 单头原件归入 `third_party/rapidyaml/`，不修改上游。
  SHA256 固定为 `0D0B8076174CF62F034406B03529FDA542EBC9A17506D3BD6D949AEDC4BFA6AB`。
  附版本、官方来源、文件哈希、上游及内嵌依赖适用版权/许可全文；逐项查实际原件，
  不凭记忆写许可，不替 PAE 选择许可证。必要的官方许可获取须记录来源。
- 根 CMake 增加 `PAE_BUILD_YAML_FRONTEND`，默认 OFF，只显式 ON 时建立独立
  `pae_yaml_frontend`；不自动启用公开 PAE、Lab 或 Qt，不将 ryml 加入 Core 依赖链。
- 前端默认从仓库相对位置消费固定依赖，不保留必须传机器绝对路径的入口，
  无 configure/build 自动联网、下载回退；错误版本或缺失文件明确失败。
- 调整独立测试 CMake，避免根构建与测试入口重复定义目标。测试所需公开 PAE
  开关限测试配置，不改现有默认。前端实现和类型语义保持不变。
- 本片不安装/导出 YAML 头和 target，不改 SDK 安装规则、公开 ABI、已有包或部署。
  源码交付白名单的后续适配如有缺口，只报告，不擅自修改。

## 写入分工

《子任务推进》独占根 `CMakeLists.txt`、两个 YAML 目录内的 CMake、
`third_party/rapidyaml/`、`third_party/README.md`，以及
`docs/engineering/yaml-build-integration-validation-20260926.md`。
如确需测试入口调整，仅限本片 YAML 测试；不修改前端运行语义及旧探针。
总控维护本契约、计划和索引；Lab、工程整理不派实施，避免共享根 CMake 冲突。

## 验证与停止条件

1. 新根 `out/build/yaml-build-integration-20260926`，证据
   `out/evidence/yaml-build-integration-20260926`；不覆盖旧日志/构建。
2. Windows x64 Debug/Release 串行构建可选前端并运行既有专项（95 项断言基线）。
   实际参数、结果、依赖哈希均留证；不是完整产品回归。
3. 独立 JSON-only 配置和公开 PAE Release 构建，不检查或编译 ryml，不依赖 YAML target。
   另验证只有 YAML 开关时可独立构建前端，不要求 Qt/公开 PAE target。
4. 在隔离验证目录进行依赖缺失/哈希不符负例；不得重命名、修改或删除共享依赖原件。
   验证搬迁后的源码路径可配置构建，至少含空格；不依赖旧 out、下载或外部绝对路径。
   可复制最小验证闭包到本片输出根，但不复制整个历史 out/Qt，不改现用部署。
5. 无删除、发布、Stage/Commit/Push。需要扩大实现、公共接口、SDK或依赖版本范围时停报。
   完成后主动向总控交接一次，说明许可归集事实、验证命令/证据、未验证和 Git 状态，
   停止写入，等待复核。
