# 第四批公开 ASCII adapter 中文注释验证（2026-10-09）

状态：**已完成派发范围，待总控复核**。本批只改注释，不改执行代码、声明布局、断言或 CMake；不是 Unicode token 修复的延续，也不是 A1 预算返修。

## 现场、范围与内容

接管及结束基线为 `main@5765457a78951b019e8d9bad0e79d32f16e7716d`。接管 `git status --short` 为空，暂存区为空；不沿用历史 `3facda7`/`481d51a` 作为当前 HEAD。本批留下四个修改文件及本新增报告待总控复核。

| 文件 | 注释重点 |
| --- | --- |
| `tools/protocol_lab_ascii/public_ascii_offline_adapter.h` | A1 非 Qt 完整记录边界、自有描述/结果与借用引用、调用方原始字段字节、线程串行边界、逻辑预算非 RSS |
| `tools/protocol_lab_ascii/public_ascii_offline_adapter.cpp` | 饱和计费、方向实际计数 reserve、RX 地址范围核验及原子发布、一次 Codec 调用、TX segment 投影独立于 RX |
| `tools/protocol_lab_ascii/public_ascii_host_adapter.h` | 完整记录/CRLF stream 组合、冻结状态与 metadata/view/DTO 四类寿命、Flow 状态、Submit/Continue/Reset 边界 |
| `tools/protocol_lab_ascii/public_ascii_host_adapter.cpp` | 同步回调内有界复制、observer/sink 分工、输入冻结与实际消费 cursor、执行后复制失败不回滚消费、故障与 Reset 新 handle |

四源码相对接管副本共 61 行加入、4 行移除；移除的是两处过时英文注释，其余为局部中文说明，未机械逐行翻译。公开路径的结论不泛化到 legacy/Binary adapter。输入表示转换在上游完成，adapter 执行原始字段/记录字节；Encode 不为展示再 Decode TX。

## 两处英文纠正的实现依据

1. offline 头原“Codec borrows the compiled owner and therefore precedes it”改为“Codec precedes the metadata owner; execution retains frozen state”，保留原来正确的倒序析构事实，但去掉“执行必须借用外部 CompiledProtocol 寿命”的误导。
   - `include/pae/codec.h` 明确创建成功保留冻结状态、外部 CompiledProtocol 可先销毁；`include/pae/compiler.h` 则明确 metadata 的 string_view/字面量指针借用外部 owner。
   - `src/public_api/codec.cpp` 的 `CreateCompleteRecordCodec` 通过 `CompiledProtocolAccess::Acquire` 获得 `CompiledStateRef`，`CompleteRecordCodec::Impl` 持有该引用。adapter 中 `compiled_` 在 `codec_` 前声明，倒序析构仍先销毁 Codec；这不是必须借用的证明。
2. Host 头原“complete-record Host”及“HostEndpoint and all copied results borrow only during synchronous calls”改为支持完整记录与 ASCII CRLF stream，并区分：A1 持有 metadata，Host 保留冻结状态，回调 view 同步借用，复制 DTO 独立自有。
   - `HostAdapter::Create` 查询公开 framing，明确接纳 COMPLETE_RECORD 或 ASCII_CRLF STREAM_CHUNK；`SubmitStreamChunk`/`ContinueStream`/`RunStreamStep` 是实际 stream 路径。
   - `src/public_api/host_endpoint.cpp` 的 `CreateHostEndpoint` Acquire 后保存 `impl->state`；`CopyDecode`/`CopyEncode` 将 frame、字段字节、身份复制到 string/vector；没有把回调指针留在 DTO 中。
   - `Reset` 成功后实际重新 `Find` handle，并清除目标 Flow 的冻结输入/游标/计数；其他 Flow 不被本地清除。`RunStreamStep` 根据真实 `bytes_consumed` 推进，复制失败仍保留消费与 Codec 事实并要求 Reset。

仅纠正文义，没有更改上述执行实现或公共契约。

## 静态等价与保护

新证据根：`out/build/comments-public-20261009/ascii-adapters`，不覆盖前批日志。四文件接管副本保存在 `baseline/`；`protected-before.csv` 保存全部 3240 个已跟踪文件的 SHA-256。

`verify.ps1` 使用能区分普通字符串/字符字面量和注释的扫描，去掉注释及纯空行后比较剩余**完整代码行**，比只比较非注释 token 更严格：四文件代码、原有声明布局均相同；另外逐条比较以反斜杠续行的原始行，宏续行不变。该扫描遇到 raw string 会拒绝而非误判，四文件没有该形状。本批只改变注释，预处理前反斜杠续行无变化。

其他 **3236 个已跟踪文件** Hash 全部不变，包括公共头/CMake/PAE/Qt/其他 UI/教程/旧报告；最终 changed/untracked 集合只能包含授权四文件及本报告。四源码和报告均严格 UTF-8、无 BOM，`git diff --check` 通过，暂存区为空。证据为 `verify-initial.log`、`verify-final.log`、`final-files.csv`、`git-status-final.log`。

`clang-format --dry-run --Werror` 分别检查四文件接管副本状态与修改后状态，均退出 0。日志 `format-{before,after}-<filename>.log`；没有既有格式差异或全文件重排，未运行格式写入。

## 定向 Debug / Release

复用已核对的 `out/build/comments-public-20261009/lab-lifecycle/build`：CMAKE_HOME_DIRECTORY 指向本仓库，A1/A2/public stream/Testing 均 ON，MSVC v142 `14.29.30133`、随仓 Qt。`test-inventory.log` 记录本轮预检的五项注册；当时相关 exe 尚未在该树构建，随后实际构建并执行，不用 `ctest -N` 充当 PASS。无需修改 CMake 或增加配置选项；既有树正常 regeneration 不属于源码配置变更。

仅指定三个直接覆盖目标及其必要依赖闭包：

```powershell
cmake --build out/build/comments-public-20261009/lab-lifecycle/build --config Debug --target pae_protocol_lab_ascii_public_a1_tests pae_protocol_lab_ascii_public_stream_tests pae_protocol_lab_ui_ascii_public_a2_tests --parallel 4
ctest --test-dir out/build/comments-public-20261009/lab-lifecycle/build -C Debug -R '^pae\.(tools\.protocol_lab_ascii\.(public_a1|public_stream|public_stream_noexcept_reject|public_stream_noexcept_candidate)|tools\.protocol_lab_ui\.ascii_public_a2)$' --no-tests=error --output-on-failure -j 1
# Debug 完成后，用相同目标和正则分别执行 --config Release / -C Release。
```

复现入口为 `./out/build/comments-public-20261009/ascii-adapters/run.ps1 -Phase Debug` 或 `-Phase Release`。脚本拒绝覆盖日志，日志首行是展开命令、末行实际退出码。

| 阶段 | 日志（相对证据根） | 结果 |
| --- | --- | --- |
| Debug 三专项目标构建 | `build-Debug.log` | 退出 0 |
| Debug 定向 CTest | `test-Debug.log` | 5/5 PASS，退出 0 |
| Release 三专项目标构建 | `build-Release.log` | 退出 0 |
| Release 定向 CTest | `test-Release.log` | 5/5 PASS，退出 0 |

五项实际测试：

- `pae.tools.protocol_lab_ascii.public_a1`
- `pae.tools.protocol_lab_ascii.public_stream`
- `pae.tools.protocol_lab_ascii.public_stream_noexcept_reject`
- `pae.tools.protocol_lab_ascii.public_stream_noexcept_candidate`
- `pae.tools.protocol_lab_ui.ascii_public_a2`

offline 覆盖独立 DTO、单向/混合关联、范围及结果预算/复制失败；stream 覆盖跨 chunk CRLF、粘连失败候选后恢复、Flow 隔离、工作预算、执行后复制失败的消费事实和 Reset；Host A2 headless 专项直接调用公开 Host 并验证资源边界、RX/TX、回调复制失败及上层 session 接入。这不是全仓或全部 adapter 行为覆盖，也不为证明注释等价新增测试/断言。

A1/stream 两个测试源码显式重定义 `assert` 为始终执行并返回失败的检查；A2 专项重定义为失败退出的 `Check`，另沿用 `/UNDEBUG`。`release-options.log` 核对 A2 生成工程 `Undefine=NDEBUG` 及分列 source/execution UTF-8。Debug/Release 构建 C4819 均 0；Release 有一条 D9025（`/UNDEBUG` 覆盖 `/DNDEBUG`），不宣称零警告，不屏蔽 warning。

## 未验证与停点

未运行全仓测试、Lab app/可见或隐藏 UI smoke、人工 UI、SDK/体验包重制或包外消费、Linux、所有开关组合、硬件/真实协议/现场验证。A2 目标会构建现有 headless 依赖闭包（包含兼容模块），不表示这些模块被修改或其全部行为被单独验收。

未发现本批需要修复的代码问题；未扩到 legacy/Binary/public PAE 或预算体系。未 Stage/Commit/Push、删除、发布或派发其他任务。向总控一次交接后停止写入，等待复核，不自行宣称总控验收通过。
