# 第十一批：Framer 流式切帧主线中文注释验证

日期：2026-10-09。状态：已完成派发范围，待总控复核；不表示提交批准。

## 1. 接管与增量

现场 `main@829632d7aece20a677d0ffbee9f65d9479863966`，暂存区为空。
接管已有第十批三个源码修改及一份未跟踪报告，全部按接管字节 Hash 保护。
适用外层 AGENTS.md，仓库内未发现更深层规则。未见活动 cmake/ctest；可见 MSBuild 是
`/nodemode:1 /nodeReuse:true` 复用节点，不将其存在当作另一构建正在运行，未终止进程。

本批仅三个源码注释及本报告：

| 文件 | 新增注释行 | 重点 |
| --- | ---: | --- |
| src/protocol_framing/stream_framer.h | 12 | 候选、消费量、状态观察、绑定/借用、缓存和累计统计 |
| src/protocol_framing/stream_framer.cpp | 17 | 预算、交付提交、Binary 恢复、ASCII CRLF、Reset |
| src/public_api/stream_framer.cpp | 11 | 保留 compiled 状态、回调桥、Continue、保护和能力查询 |

保留准确英文，不修改非注释 token、声明、接口、布局、测试、CMake 或其他模块。
公开 include、Core、Host、YAML、Lab、第十批报告和索引没有改动；未新增依赖或功能。

## 2. 注释依据及边界

核对三个实现文件、include/pae/stream_framer.h、冻结切帧描述和 CompiledState 保留关系；
参考 bounded-stream-framing-contract.md、ascii-stream-framing-contract.md 及执行语义中的
流式章节。历史契约的初版时间/能力状态不替代当前源码或本轮证据。

- 内部 Workspace 永久借用同地址 Plan 和一个 Pipeline；公开 Impl 保留 CompiledStateRef，
  析构先释放独立 Workspace，再释放编译状态。Framer 不拥有连接或传输，也不执行 Decode。
- bytes_consumed 是本次输入已接受前缀，不等于缓存量或候选长度。未消费后缀仍归宿主，
  不排队、不预先复制全 chunk；已消费缓存及同步前缀/恢复进度跨调用保留。
- 回调候选借用内部缓存，只在同步 noexcept sink 期间有效。CONTINUE 继续状态机；
  STOP 也确认当前候选已交付，随后重置候选，不在下一次调用重放该帧。
- 工作预算在相应消费/进度提交前检查；预算停止不是 API 错误，已有状态不会回滚。
  回调配额用完时可有 DELIVER_PENDING，输入全消费也不等于内部工作完成。
- 实际存在公开 Continue(sink)，实现为 Push(ByteView{}, sink)；内部使用空输入
  PushStreamChunk。它可推进复制同步头、回扫/搬移、长度头判断或 pending，不补齐半帧。
- Observe 是串行空闲查询，不推进状态；公开 Observe 有实例保护，内部 Observe 不同步。
  Reset 丢弃半帧、pending 和恢复状态，不调用 sink、不改变绑定、不清累计丢弃/畸形统计。
- 回调重入与不同线程占用竞争分别拒绝；原子标志不保证移动、销毁、所有观察可任意并发。
  public 的 thread-local owner 链解释嵌套回调检测，内部仍使用 Workspace 回调标记和占用。
- Binary 使用固定长度、同步固定长度或同步长度字段策略。长度值是总帧字节数；合法后
  收齐该长度，不扫 payload 同步头。仅非法声明长度触发从坏候选下一字节重扫与预算搬移。
- ASCII CRLF 策略候选包含结束符，可跨 chunk；恰好上限结束合法。超长后丢弃至下一
  CRLF，保留末尾 CR 事实，结束符也计入丢弃。Framer 不验证字符或业务 Message。
- 候选交付不等于 Codec、校验或设备业务成功；下游失败不要求 Framer 回扫。
  未把 Lab 的输入冻结/后缀 UI 策略移入内部状态机或创建新的续处理 API。
- 创建时按最大候选一次性准备缓存，并检查 Profile/硬上限、单流内存及最小推进预算；
  public 另列 facade 计费。这是逻辑计费，不是 RSS 或正式性能指标。

## 3. 实际 Windows 验证

复用 `out/build/comments-public-20261009/default-repo`，源根为当前仓库；
VS18 2026/x64/v142，MSVC 工具目录版本 14.29.30133，Schema 0.5 至 0.11 开关开启。
Release `/O2 /Ob2 /DNDEBUG`；CMake/CTest 4.3.1-msvc1、clang-format 22.1.3。
不创建全量构建根，不更改配置、Qt 或全局环境。

依 CMake 核对实际目标后串行执行 Debug build/test → Release build/test，四条命令 exit=0。
每配置 CTest 3/3，实际结果如下：

| CTest | Debug | Release |
| --- | --- | --- |
| pae.protocol_framing.contract | 通过，进程退出 0 | 同左 |
| pae.protocol_framing.ascii_stream_contract | 通过，进程退出 0 | 同左 |
| pae.public_api.stream_framer | 50 passed / 0 failed | 同左 |

内部 Binary/ASCII 使用运行时 Expect，失败写 FAILED 并累积到 ok，末尾返回 0/1；
不输出成功检查计数，因此本报告不填造实际断言总数。public Runner 输出 50 检查并根据
失败数退出；这些不是普通 assert，不被 NDEBUG 取消，static_assert 仍在编译期执行。

```powershell
$Build = 'out/build/comments-public-20261009/default-repo'
cmake --build $Build --config Debug --target pae_protocol_framing_contract_tests pae_ascii_stream_framing_contract_tests pae_public_stream_framer_tests -- /m:2
ctest --test-dir $Build -C Debug -R '^(pae\.protocol_framing\.(contract|ascii_stream_contract)|pae\.public_api\.stream_framer)$' --output-on-failure
# Debug 完成后用相同目标/过滤器执行 Release。
```

既有断言实际涉及分块/粘包、同步头、非法长度恢复、预算残留空提交、STOP 后缀重提及不重复
交付、Reset、归属/重入/忙状态、独立 Workspace、Codec 失败不回扫、预分配及热路径；
ASCII 包括 CRLF 跨 chunk、恰好上限、超长丢弃恢复、pending 观察及空提交；
public 涉及保留 compiled 状态、调用量/内存报告、预算/分配失败、移动保留半帧与观察。
代表性向量通过不等于所有线程交错、所有垃圾序列或全部开关已穷尽证明。

## 4. 等价、格式和保护证据

本批证据根 `out/build/comments-public-20261009/stream-framer/`，未覆盖旧批日志。
三个源文件保留字面量的非注释 token 完全一致，宏续行不变，没有注释尾反斜杠。
raw string 不支持时校验器失败关闭，三个目标未含该语法；源码及报告严格 UTF-8，无 BOM。
范围外 3243 个 tracked 文件加第十批未跟踪报告，共 3244 个文件 SHA256 不变。
最终 git diff --check、候选白名单、机器路径/敏感关键词检查通过，out 不进入候选。

辅助验证中发现两个同名 cpp 的日志名冲突，首轮检查失败关闭；保留 initial-equivalence.log。
改为完整相对路径命名后通过，未放宽 token/Hash 保护。
最初按 basename 保存格式日志不能区分两个 cpp，不能作为两文件独立对照证据；
后续分别验证原始 baseline 副本。扁平改名副本影响 clang-format 主头识别，stdin 管道又引入
额外末尾换行，相关尝试失败日志均保留。这些是验证方式问题，不是源码格式缺陷。
最终将原始字节复制到保留原目录/文件名的 baseline-original-path，用显式仓库格式配置检查，
三个接管副本均 exit=0；三个当前文件 corrected/final 格式检查均 exit=0，未重排源码。

- baseline/、baseline-original-path/、protected-hashes.json、takeover-status.log：接管字节。
- build-cache-audit.log、run-validation.ps1、validation-transcript.log、build/test/cases 的
  Debug/Release 日志：真实命令、退出码和测试输出。
- verify-comments.ps1、initial-equivalence.log、corrected-equivalence.log、final-equivalence.log：
  首次失败和最终等价/保护结果。
- baseline-original-format-*、corrected-format-*、final-format-*：完整路径命名的格式证据；
  baseline-path-format-*、baseline-stdin-format-* 保留辅助尝试失败。
- final-diff-check.log、candidate-check.log、final-git-status.log：最终候选和 Git 核对。

## 5. 停点与局限

本批注释范围未发现需修复的实现阻断；仍待总控复核，不自行宣称批准提交。
未运行全仓、关闭开关组合、长期垃圾序列/计数溢出、穷尽嵌套/线程/生命周期误用；
未新做网络、人工 UI、SDK 包外、重打包、Linux、真实协议、设备或现场验证。
不新增无条件线程安全、性能、完整内存安全或认证承诺。

最终累计六个 tracked 源码修改及两份 untracked 报告；第十批四文件保持接管字节。
分支/HEAD 不变，暂存区为空。未 Stage、Commit、Push、清理、重打包或派发其他任务。
完成一次总控反馈后停止写入。
