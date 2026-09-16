# 公开编译与通用元数据首片

状态：2026-09-14 总控按用户继续推进授权派发实施；不是已完成或已发布 SDK。
总边界见 [PAE / Lab 独立交付边界](pae-lab-delivery-boundary.md)。

## 目标与非目标

建立 experimental 0.x 的 C++17 编译入口、不可拆分编译所有者、只读通用描述和单一 CMake 消费目标 `PAE::pae`。
只包装既有编译语义，不重新解析 JSON、不重建另一套编译器。保留旧内部入口以免同时迁移 Lab。
本片不公开 Core/Framer/Host 执行，不迁移 Lab，不生成正式源码/静态/动态发行包，不宣称 DLL ABI 已验证。

## 接口约束

- 公开文件位于 `include/pae/`，实现位于 `src/public_api/`；公开头自包含，不能包含 src 私有头、SchemaIr、Frozen/ExecutionPlan、Qt/Lab 类型或测试 hook。
- 一个 move-only `CompiledProtocol`（具体命名可依现有命名习惯微调）整体拥有同次编译的 Plan 与元数据。内部可持有原有 artifacts，公开面不提供 TakePlan/TakeDescription 或内部 Plan 指针。
- 采用 opaque/pImpl 所有者，析构和移动操作在库内实现；为后续 DLL 准备明确导出宏，但本片先闭合静态/source-tree 消费。
- 元数据为只读借用 view。文档必须明确 owner 销毁、移动赋值、替换与借用失效规则；首片可保守要求移动后重新取得 view，不承诺持有已失效 view 的运行时保护。
- 空/移动后 owner 的直接查询、越界查询必须安全失败或返回空，不能调用空 sidecar 产生未定义行为。不得用引用返回掩盖未满足的前提。
- 首片描述范围：protocol/pipeline/message/field/enum 的 identity、名称、描述、source_ref、关联索引/范围及枚举语义值；资源报告沿用既有语义并区分 Plan、metadata 与 facade 开销。不是完整物理布局查询 API。
- 复杂位布局、实际 frame length 范围解析、Core/Host handle 及显式 CopySnapshot 留后续切片，不为第一片预先构造全量 Lab DTO。
- 编译诊断保留 stage、错误码、JSON Pointer、可选偏移和资源诊断的结构化信息；公开名称去 UI 化，不以 internal enum 强制类型转换充当稳定映射。
- 公开布局不受消费者 Schema 功能宏影响；本片测试构建启用已有 Schema 0.1～0.11 能力，不改变旧构建默认开关。实际支持范围须可查询或在入口说明中准确列明。
- 明确配置失败走诊断；不要把整个 API 标为绝不抛异常。库内分配失败映射、调用方 STL 分配异常及 noexcept 操作分别说明，不宣称 OOM 全恢复。

## 文件写入范围

《子任务推进》单独负责：`include/pae/`、`include/README.md`、`src/public_api/`、`tests/public_api/`、`examples/public_api_compile/`；根 CMake、CMakePresets 及必要 `cmake/` 接线由该任务独占修改。
仅在必要时修改 `src/config_compiler/` 和对应 `tests/config_compiler/`，不批量改内部名称。若修复空 sidecar，先以针对性测试表达预期，再改实现。
允许新增本片验证报告 `docs/engineering/pae-public-compile-metadata-validation.md`，不编辑其他总控文档。
不得修改 Lab/Host/Core/Framer 实现、Schema 规则、third_party 内容、外层目录；不得丢弃既有未提交变更。

## 相称验证

1. 公开头逐个单独编译；无私有头、UI 类型和消费者宏布局依赖。
2. valid/invalid JSON、结构化诊断、description limit exact/minus-one（按实际接口）、空/越界查询、move/销毁与 Plan/metadata 配对测试；包括现有 Binary 和 ASCII 合成配置，不能只验证 0.9。
3. Windows x64 v142、Debug /MDd 与 Release /MD 串行构建及受影响 compiler/public API 回归；单独记录命令与结果。
4. 仓库外最小 consumer 通过源码 `add_subdirectory` 接入，只 include `<pae/...>`、只 link `PAE::pae`；不额外添加 src 头路径或内部链接目标。这是源码树外消费验证，不是 `find_package`/成品包验证。
5. 无 Qt/Lab 构建与原推荐 Lab 配置的兼容性检查；若未触碰旧执行路径，不要求重复人工截图。不得把静态检查当作实际构建结果。

构建输出集中在新 `out/build/windows-msvc-public-api-stage1`，日志及隔离 consumer 放 `out/public-api-stage1/`；consumer 源目录必须实际位于仓库外，通过临时目录创建且在报告列明，不擅自递归清理。
本片完成后给出实际文件、公开 API 使用示例、验证与未验证项，主动向总控反馈一次并停止。不 Stage/Commit/Push，不继续扩展完整 SDK。
