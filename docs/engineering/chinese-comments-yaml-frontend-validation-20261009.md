# 第十三批：YAML 前端中文注释验证

日期：2026-10-09。状态：已完成派发范围，待总控复核；不表示提交批准。

## 1. 接管与本批增量

现场 `main@c881412ba012143935fbc7e9b81b9649c934a298`，工作区干净、暂存区为空。
第十至十二批源码及报告已在该基线，不沿用旧 dirty 状态推断。适用外层 AGENTS.md，
仓库内未发现更深层规则。可见 MSBuild 为 nodeReuse 复用节点，未终止；未见活动 cmake/ctest。

本批仅四文件注释及本报告，保留准确英文：

| 文件 | 新增注释行 | 重点 |
| --- | ---: | --- |
| src/config_frontend_yaml/frontend.h | 10 | 内部职责、预算单位、来源池、结果借用及 identity |
| src/config_frontend_yaml/frontend.cpp | 18 | 准入顺序、整数保真、Writer、位置复制、清理与预算 |
| src/config_frontend_yaml/public_facade.cpp | 7 | 公开 owner、状态映射边界、移动及祖先回退 |
| src/config_frontend_yaml/public/pae/yaml_frontend.h | 6 | 可选组件、试用约束、来源语义及公开借用期 |

公共可选头只增注释，没有更改声明、API 或布局；其余三文件同样没有非注释 token 变化。
未修改测试、仓库 CMake、第三方、Core/Lab、其他公开头、旧报告或交付包。
未新增依赖、下载、Qt 或全局环境变更。

## 2. 注释依据及行为边界

完整阅读四文件、Schema YAML Profile、内部前端首片契约及可选 SDK 组件契约，核对
独立 tests/config_frontend_yaml 入口及两个测试程序。历史状态不作为本次通过证据。

- YAML 是作者源。本前端只转换 strict JSON 和来源映射，不识别 PAE 领域属性，
  不自动调用 CompileProtocolJson。转换成功与配置编译成功、协议执行成功分别判断。
- 输入大小、BOM、UTF-8、前导版本指令在 Parser 前检查；辅助容量乘加和零容量检查
  先于自有数组分配。Parser/Tree/arena 使用每调用独立 allocator 回调及错误清理边界。
  树生成后检查单文档、根 Mapping、节点/深度及禁用构造，不把后置检查说成解析期间
  深度硬门禁，也不宣称接受完整 YAML 或执行外部 include/环境变量展开。
- 引号标量保留字符串；Plain 仅精确小写 null/true/false 与规范整数显式转型。
  整数范围由 from_chars 验证，原 Token 直接输出，不经 double、不纠正 -0 或前导零。
  解码后键完整比较，重复在写入该键前拒绝，不覆盖、不做 Unicode normalization。
  JSON String 转义与 RFC6901 Pointer 转义分开。
- Parser 请求计费包含对齐 Header，预算/溢出在 malloc 前检查；辅助预算覆盖 Entry
  数组、工作 Path 和持久 Pointer 池，source identity 占用持久池前部。JSON 按上限
  预分配，逐次写入前检查，不先生成完整输出再拒绝。公开 facade 的 owner 还有一次
  独立分配，不将这些拒绝边界描述为 RSS、CRT、栈或完整峰值内存证明。
- 临时解析器和树在 Convert 返回前销毁；结果自持 JSON、identity 和来源池。
  内部 Find 返回借用 Entry，公开查询复制 SourceLocation；JSON/identity view 仍借用
  owner，不应跨移动、赋值或销毁继续使用。移动后的公开对象可安全查询且无成功 payload。
- 位置是一基节点起点，0 表示无位置；无法精确定位时标 approximate。内部最近祖先
  查询只返回已记录 Entry，公开 facade 额外标 ancestor_fallback 及两侧近似，不伪造
  缺失属性位置，也不把生成 JSON 的 offset 当作 YAML 原文 offset。
- 只有整棵 Mapping 成功转换才发布 OK。任何失败统一撤销部分 JSON/来源池，保留
  原因及计费观察；公开 owner 分配失败同样不发布可用 payload。

## 3. 实际独立构建及失败过程

新建本批独立构建根 `out/build/comments-public-20261009/yaml-frontend/slice-build`，
源码入口明确为 tests/config_frontend_yaml，PAE_SOURCE_DIR 为当前仓库。
未复用旧 PAE_DIR/package cache 或改动前批 default-repo。独立入口关闭仓库常规测试、
启用 public API/YAML 及其所需 Schema 0.5 至 0.11；未启用 Lab/UI/网络。
VS18 2026、x64、v142 version=14.29.30133，MSVC 编译器 19.29.30159；
CMake/CTest 4.3.1-msvc1、clang-format 22.1.3，Release /O2 /Ob2 /DNDEBUG。

保留以下实际失败，不将它们充作通过结果：

1. 初始 configure 退出 0，build-Debug 退出 1（D8016）：隔离字符集设置给集成测试
   增加 /utf-8，与 PAE::pae 已传播的 /source-charset:utf-8 冲突，尚未运行测试。
2. 撤销该重复设置后 retry configure/build 退出 0，但 Debug CTest 退出 8、1/2：
   集成测试 12 个字符相关断言失败；C4566 显示执行字符集仍为 CP936。公开测试通过。
3. 仅补集成测试 /execution-charset:utf-8 后 verified configure/build 退出 0，
   字符断言通过，但 Debug CTest 退出 8、1/2：UTF-8 中文绝对 PAE_TEST_SOURCE 不能由
   现有 Windows 窄字符文件接口打开，夹具长度为零，public_fixtures_convert 失败。

最终只在忽略 out 的 test-charset.cmake 使用现有 CMake project include/deferred
target 配置：集成测试继承 PAE 的 UTF-8 source charset 并补 UTF-8 execution charset；
YAML-only 公开测试使用 /utf-8。已有 PAE_TEST_SOURCE 定义改为从测试工作目录出发的
ASCII 相对根 ../../../../../，现场解析到同一仓库；不复制或修改夹具、测试源码及断言。
设置仅用于本隔离测试目标，未改变库目标选项或全局环境。
这属于测试编译/路径配置适配，不是前端产品代码修复；不宣称原默认测试配置已因此修好。

## 4. 最终实际通过证据

最终 configure、test-inventory、Debug build/test、Release build/test 六条命令退出码均为 0。
Debug 全部完成后才执行 Release，未并发共享测试目录。

```powershell
$Evidence = 'out/build/comments-public-20261009/yaml-frontend'
$Build = "$Evidence/slice-build"
cmake -S tests/config_frontend_yaml -B $Build -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' -DPAE_SOURCE_DIR=<当前仓库绝对路径> -DCMAKE_PROJECT_PaeYamlFrontendSlice_INCLUDE=<本批test-charset.cmake绝对路径>
cmake --build $Build --config Debug --target pae_yaml_frontend_tests pae_yaml_frontend_public_tests -- /m:2
ctest --test-dir $Build -C Debug -R '^(pae_yaml_frontend_tests|pae_yaml_frontend_public_tests)$' -V
# Debug 完成后，相同目标和过滤器执行 Release。
```

| 实际输出 | Debug | Release |
| --- | --- | --- |
| CTest 两项专项 | 2/2，退出 0 | 2/2，退出 0 |
| pae_yaml_frontend_tests | 95 条 PASS，0 条 FAIL，failures=0 | 同左 |
| pae_yaml_frontend_public_tests | 10 条 PASS，0 条 FAIL，failures=0 | 同左 |

数量按各测试输出精确行统计，含循环/重复检查名，不当作不同场景数量或覆盖率。
两程序 Check 为运行时条件和 failures 累计，main 依据 failures 返回非零，不使用会
随 NDEBUG 消失的 assert。因此 Release 保留这些判断。最终构建日志未见 warning/error。

代表性断言包括：类型和整数范围、解码重复键、禁用构造、BOM/非法 UTF-8/Unicode、
块标量及 Pointer 转义、原文销毁后的自有存储、源位置/近似/祖先回退、预算精确值/减一、
Parser 和四个前端分配点故障恢复、并发实例隔离、公开移动/失败/固定试用约束。
既有两个公开合成夹具均转换成功，并分别对原 JSON/转换 JSON 执行真实 Compiler、
metadata 及已知 Decode/Encode 对照；前端实现本身并未因测试调用而承担这些职责。
Binary CRC 夹具输入1899/JSON1700字节、70来源节点、Parser峰值30027字节；
ASCII 输入1296/JSON1175字节、46节点、Parser峰值15480字节，与有限历史夹具口径一致。
这不证明所有 YAML/JSON 配置执行等价或所有资源边界安全。

## 5. 等价、格式、候选与停点

四源码非注释 token（保留字面量）与原目录 baseline 一致；宏续行一致，未增加注释尾
反斜杠。校验器遇不支持的 raw string 语法失败关闭，目标未含该语法。
四文件接管及修改后 clang-format dry-run 均退出 0，未全文件格式化。
源码和报告严格 UTF-8 无 BOM，git diff --check、候选白名单及新增候选检查通过。
范围外 3245 个 tracked 文件 SHA256 不变，含前批源码及报告；out 不进入源码候选。

本批证据根为 out/build/comments-public-20261009/yaml-frontend/：

- baseline/、protected-hashes.json、takeover-status.log、baseline-format 日志：原始接管。
- verify-comments.ps1、initial/final-equivalence.log、对应 format/diff-check 日志：最终等价与保护。
- validation-transcript.log、build-Debug.log、retry/verified 测试日志及 failed-cases：失败尝试。
- run-validation.ps1、test-charset.cmake、final-validation-transcript.log、final-configure.log、
  final-build/test/cases-{Debug,Release}.log：最终实际配置、构建、测试和原始输出。
- final-build-cache-audit.log、assertion-counts.log、candidate-check.log、final-git-status.log：
  工具链/源根、实际计数及最终候选依据。旧失败日志未覆盖。
- 交接前将一处注释澄清为“来源记录不借用树”，不暗示 Writer 遍历期间即可销毁树；
  review-equivalence.log 及 review-candidate-check/assertion-counts/final-git-status.log
  复核当前四源码和报告。仅文字澄清，非注释 token 仍相同，不重复运行未变的测试。

本批注释及适配后专项范围未发现必须修复的产品实现阻断，仍待总控独立复核；
原默认独立测试的字符集/窄路径限制已如实记录，没有改仓库构建来收口它。
未重跑全仓、JSON-only隔离、SDK包外/搬迁/打包、Lab/UI、关闭开关组合、网络、Linux、
不可信输入全域审计、真实协议、硬件或现场验证；不升级性能/RSS/完整内存安全结论。

最终分支/HEAD 不变，四个 tracked 注释修改、一份 untracked 报告，暂存区为空。
未 Stage、Commit、Push、清理或启动下批。完成一次总控交接后停止写入，等待复核。
