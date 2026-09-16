# Lab 表示切换 UX：实施与验证

## 范围与当前状态

2026-09-13用户授权规划并推进下一阶段，基线`main@ff72894`且开始时工作区干净。
本轮首先完成路线图的表示切换提示小闭环，不修改PAE、字节转换、输入准入或流状态语义。
未Stage、Commit、Push；用户已确认本次人工复验符合预期且Lab已关闭。
本次结论来自本次复验，不沿用此前检查点的人工通过。

## 改动

- `tools/protocol_lab_ui/document_session.cpp`：表示转换失败统一说明源/目标格式、仍保留的格式、
  解析失败原因、草稿与流状态保留，以及修正或复制/清空后切换的方法。保留原诊断ID。
- `tools/protocol_lab_ui/document_tab.cpp`：中文下拉框帮助文本；真实Qt控件smoke覆盖非法Hex草稿
  切ASCII时回退、文本保留、诊断可见和流未提交。
- `tests/protocol_lab_ui/host_session_tests.cpp`：双向非法草稿、有效字节转换、跨Flow格式恢复、
  清空恢复及错误消除；失败明确输出并退出，避免Windows未捕获异常对话阻塞CTest。

不自动清空草稿，不把非法Hex当作ASCII重新解释；不对输入作第二次Decode。

## 验证证据

先增加Flow 1 Hex草稿`ON`的断言，Debug host_session构建成功但测试1/1失败，
证明旧提示缺少转换方向及恢复说明。初次未捕获异常阻塞，终止本次测试进程后改为明确失败退出，
重跑得到CTest的断言失败结果，再修复实现。

复用`out/build/windows-msvc-lab-host-observer`的既有Qt/MSVC同套工具链：

- Debug与Release重新构建成功。
- `ctest -C Debug/Release -R 'pae.tools.protocol_lab_ui\.' --output-on-failure`分别18/18通过。
- 包含11项headless和7项Qt smoke；覆盖Host开关开启时的新路径及旧Binary/ASCII UI回归。
- 日志：`out/lab-representation-ux-debug-tests.log`、`out/lab-representation-ux-release-tests.log`。

未声称重跑全部52项；本轮没有修改Core/Framer。Host关闭配置未在本轮重新构建；
Linux、硬件、网络、性能和现场均不在本次验证范围内。

## 人工复验（已通过，步骤保留供回归）

EXE：
```text
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-host-observer\out\protocol_lab_ui\Release\pae_protocol_lab_ui.exe
```

配置：
```text
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\examples\config\synthetic_ascii_stream_slice.pae.json
```

1. 加载配置，Apply默认绑定表，选择Decode绑定和Flow 1，确认Representation为Hex。
2. 输入`ON`，不Submit；选择ASCII (escaped)。应回退Hex并保留ON，底部说明
   `Hex -> ASCII (escaped)`、当前仍是Hex、失败原因及`copy/clear`恢复方法；buffered/step仍为0。
3. 悬停Representation，检查中文帮助提示；清空输入后切ASCII，应成功且旧错误消失。
4. 输入`ON`并Submit，buffered=2；切Flow 0再切回，ASCII表示、ON草稿和buffered=2均保留。
5. 替换为`LY\r\n`并Submit，应解出ONLY、零字段成功。截图底部提示与成功结果后关闭Lab。

### 本次人工证据

用户提交三张截图并明确确认“本次复验符合预期，Lab已关闭”。
- 图1：Flow 1仍为Hex，ON草稿保留，buffered=0、step=0；底部完整显示转换方向、
  保留状态、非法Hex原因和copy/clear恢复指引。
- 图2：Flow 1为ASCII (escaped)，ON草稿及buffered=2、step=1、NEED_MORE可见。
- 图3：LY分片补齐后匹配decode_only、零字段成功，buffered=0、step=2，
  candidates/decode_ok/observed/business均为1；原始帧为4F 4E 4C 59 0D 0A。

悬停中文帮助、清空后切换、跨Flow往返及关闭操作的全过程由用户确认，
不把单张状态截图视作这些动作的完整录像。本次只更新文档，无代码修改或重复构建测试。

## Binary Host 下一项源码评估（非实现完成声明）

已核对：`src/host_endpoint/host_endpoint.h`的Candidate同时提供借用frame、DecodeResult、
Plan和成功字段；`tests/host_endpoint/host_endpoint_tests.cpp::Binary`已有fixed、sync_fixed、
sync_length分片及UINT64的宿主测试。因此不应先假定需要增加PAE接口。

Core的DecodedFieldSlot具有类型化逻辑值；现有UI
`description_mapping.cpp::BuildPhysicalMapping`按冻结执行Plan处理大小端位容器与物理mask。
ASCII的BYTES专用适配器不能直接覆盖Binary类型及位范围，也不应在UI里再次Decode。

建议下一契约按六项冻结：
1. 明确首片支持的Schema、三种已有Framer及字段类型，未覆盖类型显式拒绝。
2. 复用现有Candidate观察；回调内复制类型化值/枚举身份/原帧，禁止悬空Plan引用和二次Decode。
3. 固定字段位范围取冻结Plan，动态范围取本次结果；证据不足不展示猜测高亮。
4. 复用每流单候选STOP、严格后缀续提及Reset/替换隔离；失败候选仅诊断、禁止业务成功交付。
5. 首先冻结自有DTO及所有共存副本的准入预算，不能将ASCII容量公式未经复核搬到Binary。
6. 先非Qt适配和独立向量，再串行UI接线、Windows Debug/Release及人工验收。

冻结前仍需核对：各Schema/字段完整支持矩阵、动态范围与枚举拷贝、旧完整记录执行层可复用边界，
以及失败观察/预算极限向量。当前是有源码依据的规划，不是已确认的Binary实施契约。
通信连接、自动转发、持久化仍不在本轮实施范围。
