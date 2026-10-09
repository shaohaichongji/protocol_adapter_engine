# 第十二批：Host 宿主接入主线中文注释验证

日期：2026-10-09。状态：已完成派发范围，待总控复核；不表示提交批准。

## 1. 接管与本批增量

现场为 `main@829632d7aece20a677d0ffbee9f65d9479863966`，暂存区为空。
保留第十、十一批六个源码修改及两份未跟踪报告，未重新生成原始 baseline。
适用外层 AGENTS.md，仓库内未发现更深层规则。本轮恢复时未见活动 cmake/ctest。

本批仅以下源码注释与本报告；保留准确英文，没有修改非注释 token、接口或布局：

| 文件 | 新增注释行 | 重点 |
| --- | ---: | --- |
| src/host_endpoint/host_endpoint.h | 8 | 内部职责、绑定身份、借用输出、候选及空 Push |
| src/host_endpoint/host_endpoint.cpp | 12 | Channel 隔离、索引、忙标志、回调顺序、Reset、逻辑预算 |
| src/public_api/host_endpoint.cpp | 17 | 公开组合链、所有权、句柄、消费量、Continue、Encode 及发布 |

未改公共 include、测试、仓库 CMake、Qt、Lab、其他模块或前批报告，没有新增依赖或功能。
本批脚本、baseline、Hash 清单和日志位于被忽略的
`out/build/comments-public-20261009/host-endpoint/`，不纳入源码候选。

## 2. 注释依据与两层区别

核对三个授权文件、公开 include/pae/host_endpoint.h、CompiledState 与 Codec/Framer 接线，
以及内部和公开 Host 的既有专项测试。以下说明以实际实现为准，不由层名推断调用关系。

- 内部 Session 直接拥有 PlanOwner，并组合 Core/Framing Workspace；公开 Host 保留
  CompiledStateRef，组合公开 CompleteRecordCodec/StreamFramer，不调用内部 Session。
  拥有者的声明顺序使执行对象先于 Plan/编译状态析构，输入和回调 context 仍由调用方拥有。
- 绑定是 endpoint/action 与 Pipeline 的不可变映射，不是 Socket 或业务路由注册。
  每 Channel 有独立可变执行存储；Find 的流序号在绑定内，Handle 的下标在整个实例内。
  Encode 在调用时选择允许 Message，不固化为 Pipeline 第一条。
- 内部普通 bool 忙标志基于调用方串行化约定，阻止回调重入，不提供跨线程互斥。
  公开实例级原子占用和回调 owner 链分别拒绝并发及直接/间接重入；Channel 隔离不等于
  同 Host 的多个 Channel 可以同时执行。不得在回调中销毁对象。
- 每候选只执行一次 Decode，再通知 observer；失败没有有效业务字段或成功 raw 留存。
  observer 正常 STOP 不撤销当前成功业务交付，异常则不再交付并标记故障。
  observer/业务计数只在回调正常返回时增加，包括正常 STOP。
- 内部没有 Continue，空 Push 仅在有内部工作时接受；公开 Continue 是空 Push。
  流消费量来自 Framer，未消费后缀由宿主提交，不重推已消费输入。
  公开完整记录 Decode 已尝试后记录 frame.size 消费量，失败不意味着自动重试。
- 内部 Reset 仅用于 Decode，Handle 不含 generation，Reset 后仍可定位；Encode 回调故障
  不能据此承诺 Reset 恢复。公开 Reset 支持目标 Channel，推进代次及 Codec view epoch，
  旧 Handle 变 stale，需重新 Find，其他 Channel 不受此次 Reset 影响。
- 公开 Encode 的 bytes_produced 是 Codec 已生成事实；随后回调异常不抹去该事实，
  也不代表业务交付完成。失败 Buffer/借用视图不升级为可长期使用的输出。
  多候选聚合不能仅看最后 codec_status；逻辑预算不是 RSS 或完整峰值上限。

## 3. 实际构建与测试

复用 `out/build/comments-public-20261009/default-repo`。源根、VS18 2026/x64/v142、
MSVC 工具目录 14.29.30133 和 Schema 0.5 至 0.11 开关现场核对。
CMake/CTest 4.3.1-msvc1，clang-format 22.1.3；Release 为 `/O2 /Ob2 /DNDEBUG`。

接管缓存的 `PAE_BUILD_HOST_ENDPOINT_SLICE=OFF`，首次 build-Debug 退出 1，
MSB1009 指出内部 Host 测试项目不存在；尚未运行测试，不解释为产品测试失败。
在同一隔离构建目录启用已有 Host 开关，不修改仓库 CMake。
首次配置退出 1：configure-time positive try_compile 未继承项目目标的字符集选项，
出现 C4819 及后续解析错误。仅对配置进程及子进程设置 CL=/utf-8 后配置退出 0。
首次重试构建仍退出 1：CL=/utf-8 与目标已有 /source-charset:utf-8 冲突（D8016）。
恢复临时 CL 后执行正式构建，两配置均成功；未改全局环境或编译器安装。
这些失败日志保留，没有覆盖或充作通过证据。

实际成功顺序为 Debug build/test → Release build/test，四条命令退出码均为 0：

```powershell
$Build = 'out/build/comments-public-20261009/default-repo'
# 配置时进程内 CL 增加 /utf-8，完成后恢复；不将其带入目标构建。
cmake -S . -B $Build -DPAE_BUILD_HOST_ENDPOINT_SLICE=ON
cmake --build $Build --config Debug --target pae_host_endpoint_tests pae_public_host_endpoint_tests -- /m:2
ctest --test-dir $Build -C Debug -R '^(pae\.host_endpoint\.contract|pae\.public_api\.host_endpoint)$' --output-on-failure
# Debug 完成后，相同目标和过滤器执行 Release。
```

| 专项 | Debug | Release |
| --- | --- | --- |
| CTest 两项 Host | 2/2，退出 0 | 2/2，退出 0 |
| pae.public_api.host_endpoint | 56 passed / 0 failed | 56 passed / 0 failed |
| pae.host_endpoint.contract | HOST_ENDPOINT_CONTRACT=PASS | HOST_ENDPOINT_CONTRACT=PASS |

内部 Check 为运行时条件失败抛异常，main 捕获后退出 1；不输出成功检查总数。
其分配失败注入日志为 Debug 0..63/64、Release 0..21/22，这是该次创建的失败点数量，
不是完整测试数或覆盖率。公开 Runner 的 Check/Finish 为运行时判断和失败统计。
两者不是随 NDEBUG 消失的 assert，Release 判断保持有效；不为获取计数修改测试。

既有代表性断言包括：非法/重复绑定、Action/Pipeline/Message 约束、完整记录与 stream
路由、双流半帧隔离、STOP 及未消费后缀、空 Push/Continue、Reset 及句柄身份、候选
成功/失败及 raw 借用、observer/业务异常、重入、逻辑计费精确上限/减一、创建失败原子性、
有限成功热路径分配计数。公开专项还包含 A→B→A 重入和跨线程忙拒绝。
这些是有限合成输入及代表性调用，不是任意生命周期或所有线程交错的证明。

## 4. 等价、格式及保护证据

三个源码的非注释 token（保留字面量）及宏续行对照原始 baseline 一致，
没有注释尾反斜杠；校验器遇不支持的 raw string 语法失败关闭，目标未含该语法。
源码和报告严格 UTF-8、无 BOM，git diff --check 和候选边界检查通过。
保护清单 3245 文件 SHA256 不变，包含范围外 tracked 文件及第十、十一批未跟踪报告。

clang-format 使用同一仓库 style、原目录/文件名的 baseline，并开启完整错误输出。
内部 h/cpp 各有一处接管时已存在的 GetRawInteger 参数续行格式问题：
baseline 和修改后退出码均为 1；按前置非注释 token、列号、诊断和原行内容定位对照，
每文件仍各一处、没有新增诊断。公开 cpp 前后均退出 0。
没有顺带格式化或把内部两文件报告为零格式错误。

证据根下关键文件：

- baseline/、protected-hashes.json、takeover-status.log、build-cache-audit.log：原始接管。
- verify-comments.ps1、initial/final-equivalence.log、对应 baseline-all-format/format 日志：
  token、续行、UTF-8、格式增量及保护 Hash。
- build-Debug.log、validation-transcript.log、configure-host.log、retry-build-Debug.log、
  retry-validation-transcript.log：未通过尝试，保留原始失败。
- retry-configure-host.log、test-inventory.log：成功配置及两项专项注册。
- run-validation.ps1、configure-and-validate.ps1、verified-validation-transcript.log、
  verified-build/test/cases-{Debug,Release}.log：成功执行的实际命令、退出码及用例输出。
- final-diff-check.log、candidate-check.log、final-git-status.log：最终范围与 Git 核对。

## 5. 停点与局限

本批注释及专项范围未发现必须修复的实现阻断，待总控独立复核。
原有两处格式问题保留，不在本批顺带修复。未重跑整个仓库、Lab/UDP、UI、包外 SDK、
关闭开关矩阵或重新打包；未运行网络、Linux、真实协议、硬件及现场验收。
不升级性能、任意并发安全、完整内存安全或生产可用结论，不启动新批次。

最终累计九个 tracked 源码注释修改及三份 untracked 验证报告，其中本批为三源加一报告；
分支/HEAD 不变，暂存区为空，未 Stage、Commit、Push 或清理。
完成一次总控交接后停止写入，等待复核。
