# 公开接口中文注释：第二批验证

日期：2026-10-09。状态：已完成派发范围，待总控复核；不是提交批准或正式发布。

## 1. 接管与增量

接管与最终核对：`main@3facda71a53f57d97b2d1b497e19c7624b846676`，暂存区为空。
保留第一批及教程、体验包相关工作树差异。本批接管快照列出 15 个已跟踪修改文件，
未跟踪目录展开后，与已修改文件合计 30 个既有文件；最终逐文件 SHA256 未变化。
不能把这些已有差异归为本批增量，也没有用 HEAD 覆盖接管内容。

本批仅新增中文注释与本报告，共六个候选文件：

- `include/pae/protocol_description.h`：33 行注释。
- `include/pae/stream_framer.h`：24 行注释。
- `include/pae/host_endpoint.h`：40 行注释。
- `include/pae/export.h`：2 行注释。
- `include/pae/version.h`：3 行注释。
- `docs/engineering/chinese-comments-public-batch2-validation-20261009.md`：独立验证报告。

保留原英文注释，本批没有发现需要纠正的英文说明。没有修改声明、布局、枚举、函数体、
测试代码、CMake、第一批报告、教程、索引、Lab、既有 SDK 或体验包。
验证脚本、接管副本及日志只放在被忽略的 `out/build/comments-public-20261009/batch2/`。

## 2. 注释事实与依据

### 协议描述

区分逻辑类型与 Wire 宽度、Encode 来源与 Decode 约束、容量上界与有效输出长度。
说明全局 Pipeline/Message/Field/Enum 索引、消息内字段/枚举序号与 Pipeline 关联序号，
不把逻辑 direction 当网络端点。metadata 字符串及 ASCII literal 借用外部 compiled owner，
复制描述并不复制字符串；物理布局描述的数值则是副本，但索引仍依赖来源实例。

物理查询不执行 Matcher、校验或 Decode；明确帧长相关存储位置、实际 Resolve 范围、
位掩码有效项以及 ASCII 模板引用不等同 Pipeline 方向权限。

依据：`src/public_api/compiler.cpp` 的 `PipelineMessageExecution()`、`Field()`、
`AsciiSegment()`、`AsciiField()`、`MessagePhysical()`、`ResolveMessagePhysical()`、
`FieldPhysical()`、`ResolveFieldPhysical()`、`Enum()` 与 `MemoryReport()`。

对应现有断言：`public_consumer_metadata_tests.cpp` 的 `CheckBinary()`、`CheckComputedAndBounded()`、
`CheckAscii()`；`public_physical_query_tests.cpp` 的 `CheckBitfields()`、`CheckBounded()`、
`CheckFixedIntegrity()`、`CheckAsciiAndErrors()`；`public_ascii_facts_tests.cpp` 的
`CheckDescription()`、`CheckZeroAndTail()`、`CheckNulAndOneWay()`、`CheckOtherOwners()`。
这些断言覆盖代表性查询、边界与 owner 移动，不证明任意 owner 失效后的非法访问安全。

### Framer

说明独立流状态与保留冻结 compiled 状态、同步 noexcept sink 的候选借用寿命、
消费前缀与未保留后缀、预算停点、空 Push 与 Continue 等价、Reset 丢弃状态。
候选交付不代表 Decode 成功；STOP 确认当前候选，只停止继续推进，不触发重放。
即便本次输入已全部消费，也可能存在待交付内部候选；半帧缓存不必然能无输入继续。
说明空闲移动、同实例重入/忙保护、Observe 非并发快照，以及逻辑计费不等于 RSS。

依据：`src/public_api/stream_framer.cpp` 的 `Push()`、`Continue()`、`Reset()`、`Observe()`、
`CreateStreamFramer()` 与状态保留；`src/protocol_framing/stream_framer.cpp` 的待交付循环，
在 sink 返回后递增交付数并 `ResetCandidate()`，随后才处理 STOP。

对应现有断言：`public_stream_framer_tests.cpp` 的 `stop_exact_prefix`、`stop_suffix_no_replay`、
`empty_push_advances_pending_once`、`continue_equivalent_and_idle`、`independent_streams`、
`reset_discards_half_only`、`move_preserves_stream_and_invalidates_source`、
`callback_reentry_rejected`、`indirect_callback_loop_rejected_as_reentrant`、
`concurrent_operations_busy`、`work_budget_exact_suffix_progress`、
`framer_codec_retain_compiled_state` 与 `one_public_decode_per_candidate`。

### Host

说明 `(endpoint_key, action)` 精确绑定与全局 Pipeline 索引、各 channel 的独立状态、
非拥有句柄及 Reset 代次、成功 Reset 后重新 Find、同 Host 操作串行而非任意并行安全。
端点 key 按长度和字节比较，不是 Socket 地址，不执行大小写或文本归一化。

候选 observer 在实际 Decode 后调用，Codec 失败仍可观察，但不会交付业务 sink。
observer STOP 不撤销当前成功候选的业务回调；回调异常才使目标 channel 故障并要求 Reset。
视图不拥有候选/输出字节；需要长期保存时复制数据。聚合 Host 状态与最后一次 Codec 状态
分别解释，generation 不是逐帧序号，正常回调返回计数不是业务接收确认。

依据：`src/public_api/host_endpoint.cpp` 的 `EndpointEquals()`、`Resolve()`、`InvokeObserver()`、
`InvokeBusiness()`、`DecodeCandidate()`、`Find()`、`Reset()`、`Decode()`、`Push()`、`Encode()`
与 `CreateHostEndpoint()`。`DecodeCandidate()` 在 observer 返回 STOP 后仍调用当前成功结果
的业务 sink，然后合并两者的继续意愿；未通过注释把该停点重定义为接收门禁。

对应现有断言：`public_host_endpoint_tests.cpp` 的 `CheckBindingAndBudget()`、
`CheckCompleteRecordAndHandles()`、`CheckBinaryStreamAndEncode()`、`CheckAsciiStream()`、
`CheckGuards()`，包括 `observer_stop_keeps_current_success_and_suffix`、
`unconsumed_suffix_resubmitted_once`、`stream_codec_failure_not_delivered_or_replayed`、
`observer_exception_exact_consumption_and_channel_isolation`、
`stream_fault_reset_stales_handle` 与 `business_exception_requires_target_reset`。
`public_physical_query_tests.cpp` 的 `host_callback_query_uses_same_identity_without_second_decode`
另核对回调中查询物理信息未重复 Decode。

### 导出与版本

`export.h` 仅简述静态/DLL 导入导出宏职责；`version.h` 区分实验 API 标识与 Schema/产品版本，
说明 Schema 精确白名单查询不是任意配置可编译或稳定 ABI 的承诺。宏及版本值未变。

## 3. 本次实际验证

复用第一批 `out/build/comments-public-20261009/default-repo/`，未重新配置或制包。
工具链为既有 Visual Studio 18 2026、x64、v142 14.29.30133。
单进程脚本按 Debug 构建 → Debug 测试 → Release 构建 → Release 测试推进。
起止 `CL`、`CXXFLAGS` 均不存在，没有临时编码覆盖或全局环境修改。

本次四条构建/测试命令退出码均为 0，Debug 与 Release 各 11/11 通过。
CTest 范围仅为 `^pae\.public_api\.`：源码编码、编码选项、编译 metadata、独立头编译、
Codec、消费 metadata、物理查询、ASCII facts、Framer、Host 和公开头依赖边界。
没有运行包含 UDP 的整个 Lab 或全切片矩阵。

实际命令（`$Build` 为上述 default-repo，`$Batch` 为上述 batch2，目标数组保存在脚本）：

```powershell
& "$Batch/verify-batch2.ps1"
clang-format --dry-run --Werror include/pae/protocol_description.h include/pae/stream_framer.h include/pae/host_endpoint.h include/pae/export.h include/pae/version.h
cmake --build $Build --config Debug --target <10个公开测试与示例目标> -- /m:2
ctest --test-dir $Build -C Debug -R '^pae\.public_api\.' --output-on-failure
cmake --build $Build --config Release --target <相同10个目标> -- /m:2
ctest --test-dir $Build -C Release -R '^pae\.public_api\.' --output-on-failure
git diff --check
```

五个头逐一与接管时的原始副本比较，保留字符串/字符字面量后剥离注释，非注释 token
序列完全相同。验证器遇到 raw string 会拒绝而不误处理；这五个头未使用 raw string。
同时检查宏续行数量不变且无注释尾反斜杠，并结合 diff 审查确认只有新增注释。
五个头及新报告为严格 UTF-8、无 BOM；五个头 clang-format dry-run 和最终 diff 检查通过。
新报告及本批头的候选路径检查通过，没有加入私有协议、原始证据或 out 产物。

以下证据均在 `$Batch`，与第一批日志分离且未覆盖其证据：

- `baseline/`、`takeover-status.log`、`takeover-dirty-hashes.json`：实际接管副本及状态。
- `verify-batch2.ps1`、`comment-equivalence.log`、`final-comment-equivalence.log`：
  token、UTF-8、宏续行、既有 30 文件 Hash 及环境检查。
- `run-batch2.ps1`、`validation-transcript.log`：完整目标数组、命令参数、串行次序及退出码。
- `build-Debug.log`、`test-Debug.log`、`build-Release.log`、`test-Release.log`：本次构建/CTest。
- `format.log`、`diff-check.log`、`candidate-portable.log`、`final-audit.log`：格式、候选和最终核对。

第一批的 SDK 消费及编码修正是历史证据，不充作本批 SDK 再验证；本批只复核当前仓库
公开 API 专项，不宣称已重新验证或更新安装包、体验包和运行中的 Lab。

## 4. 停点与局限

未发现本批注释及已执行专项路径的阻断，仍待总控独立复核。
现有测试是代表性动态证据，注释同时以源码核对为依据，不宣称所有寿命误用、参数组合、
并发调度或资源边界均已被穷尽证明。没有新增测试或生产测试接口。

未验证 Linux、其他编译器、稳定 ABI、任意宿主字符集、真实协议、性能、网络、设备、
硬件或现场；没有执行任何网络收发，也没有重制 SDK/Lab、修改教程或升级产品能力声明。
未执行 Stage、Commit、Push、清理、回退或历史改写。完成交接后停止写入，等待总控复核。
