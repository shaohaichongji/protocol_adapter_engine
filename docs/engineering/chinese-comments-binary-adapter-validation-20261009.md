# 第五批 Binary H1 公开 adapter 中文注释验证（2026-10-09）

状态：**已完成派发范围，待总控复核**。仅补注释，不改代码 token、声明布局、宏续行或执行语义；H2 UI 映射留待下一批。

## 基线与修改范围

接管/结束现场为 `main@5765457a78951b019e8d9bad0e79d32f16e7716d`，暂存区为空。第四批 ASCII 四源码及 `docs/engineering/chinese-comments-ascii-adapters-validation-20261009.md` 已 dirty，本片不修改、覆盖或更新它们。

本批仅修改：

- `tools/protocol_lab_binary/public_binary_decode.h`：新增 34 行中文注释。
- `tools/protocol_lab_binary/public_binary_decode.cpp`：新增 26 行中文注释。
- 新增本报告。

保留全部正确英文，未发现需要限定纠正的矛盾英文，未修改任何英文注释或异常字符串。未扩到 legacy/prepared/candidate_materializer/resource_budget、H2/UI、公共 PAE、CMake 或测试实现。

## 注释语义与只读依据

- **职责及入口**：文件名 `public_binary_decode` 是历史命名，实际 H1 支持完整记录 Decode、Encode，以及 FIXED_LENGTH/SYNC_FIXED_LENGTH/SYNC_LENGTH_FIELD 三种 Binary stream。`Create` 只编译一次 JSON 并创建一个 DECODE 绑定、两个 Flow；`AdoptCompiled` 移入已有结果并配置多绑定/Encode，不在执行热路径重编译。
- **拥有与借用**：`compiled_` 保留查询 metadata 的 owner，`messages_` 自有复制 Message/Field id；公开 Host 另保留冻结执行状态。`include/pae/host_endpoint.h` 及 `src/public_api/host_endpoint.cpp` 的 Acquire、`impl->state`、`src/public_api/compiled_state_internal.h` 的 Retain/Release 证实执行并非借用外部 CompiledProtocol 对象地址/寿命。倒序析构顺序依旧先 Flow/Host 后 compiled，未改成员布局。
- **结果引用不是独立返回值**：Candidate/Encoded 中 string/vector 已在同步回调内复制，不再依赖借用 view；但 `Decode`/`Encode`/Submit/Continue 返回内部状态或拒绝槽的引用。跨调用/销毁长期保存要复制 DTO/Operation/StreamStep 值对象，不能因字节自有就声称返回引用独立于 Adapter。`State` 同样借用 Adapter 内部状态。
- **索引与单位**：Decode/Reset/State 使用展平 Flow 索引；stream 用 binding 内 Flow 序号，`FlowIndex` 才进行换算。`tools/protocol_lab_ui/binary_host_adapter_public.cpp` 的 DecodeComplete 调用先 FlowIndex 再 Decode，是只读调用方依据。物理范围为完整记录字节偏移，位掩码只使用有效前缀；预算/长度以字节计，work_units 是逻辑量，0 选择编译资源默认值而非无限预算。
- **raw 与 logical**：kind 决定有效逻辑成员，Decimal64 是逻辑十进制；conversion_raw_* 来自当前 Decode 保留的原始整数，未知 Enum 可只有 raw、没有已知条目。EncodeInput 的 Enum 是字段内条目序号，不是 raw/global index。依据 `include/pae/codec.h` 及 Output/Encode 的真实访问器/selector，禁止为展示由 logical 反算 raw。
- **回调与发布**：observer 收到全部候选，成功留给业务 sink 复制，失败 observer 只复制候选帧和诊断，不从 failed flat field 猜 Message。Output 在回调 view 有效期内核对身份、当前帧物理描述、最小复制需求并复核 capacity 后才发布 pending。EncodeOutput 复制当前 TX/输入并解析物理投影，没有额外 RX Decode。异常由公开 Host 标记 CALLBACK_FAILED，不发布半份 DTO。
- **前置与执行后失败**：Decode 前置拒绝返回 rejected 槽、不替换 Flow 旧成功；实际执行或 callback 失败则替换该 Flow 当前结果。Encode 已定位 Flow 后的本地输入拒绝会清当前 TX，非法 binding 则使用公共拒绝槽；不把不同入口概括成同一种清除行为。local OK 不是业务成功，必须看 Host/diagnostic/可选 DTO。
- **冻结与消费**：新 chunk 先有界复制为 Flow 自有输入，有冻结后缀或内部待办须 Continue。RunStreamStep 只 Push 未消费后缀或 Continue 内部工作，STOP 使每步至多一候选，不撤销当前候选消费；复制失败保留 Host 的确认消费并要求目标 Flow Reset，不回滚 cursor、重喂字节或自动重试。
- **Reset/清结果/线程**：Reset 重新 Find 新代次 handle；stream 清逻辑输入、游标、计数/当前结果并复用已计费 capacity，不动其他 Flow。没有概括为所有非 stream 槽位都被完全置零；ClearCurrent 仅清结果，不代替 Reset，也不清 draft/冻结输入。同实例执行、观察、状态读取与销毁须串行，Flow 隔离不等于并发许可。
- **预算**：描述/结果按本实现的逻辑 capacity 计费；admission 还包含冻结计划、Host、Flow 结果/草稿峰值及冻结 Buffer。replacement 计旧实例重叠；溢出不允许 wrap-around。不是 RSS/所有瞬时分配硬上限，未以注释扩建预算体系。

以上依据来自两文件完整阅读、必要公共接口/实际持有实现、只读调用片段及下述专项测试；不把 H1 结论泛化到 legacy 或 H2。

## 等价、保护与格式检查

独立证据根：`out/build/comments-public-20261009/binary-adapter`；脚本拒绝覆盖旧日志，不写第四批证据目录。两源码接管副本保存在 `baseline/`，`protected-before.csv` 为 3240 个已跟踪文件接管 Hash，`untracked-before.csv` 保存既有 ASCII 报告 Hash。

`verify.ps1` 对普通字符串/字符字面量先保护再去注释，去纯空行后比较完整剩余代码行，并逐条比较反斜杠续行。遇 raw string 等本扫描不支持的形状会拒绝而非误判；两文件没有该形状。结果：非注释代码内容/原声明布局/宏续行全部一致，两源码严格 UTF-8/no BOM；其他 **3238 已跟踪文件 Hash 不变**（含 ASCII dirty 四源码），既有 **1 个未跟踪 ASCII 报告 Hash 不变**。

`git diff --check`、空暂存区及授权增量范围检查通过；最终两源码和新报告 Hash/状态见 `final-files.csv`、`git-status-final.log`，最终保护结果见 `verify-final.log`。除本片两文件及新报告外，只保留接管时的 ASCII 五文件变更；本报告严格 UTF-8/no BOM。

格式 dry-run **不是全通过**，保留原有风格不重排：

| 文件 | 修改前 `--dry-run --Werror` | 修改后 | 对照 |
| --- | --- | --- | --- |
| `public_binary_decode.h` | 退出 1，7 条诊断 | 退出 1，7 条诊断 | 原诊断源码/光标内容逐项相同 |
| `public_binary_decode.cpp` | 退出 1，53 条诊断 | 退出 1，53 条诊断 | 原诊断源码/光标内容逐项相同 |

证据为 `format-{before,after}-<filename>.log`、`format-comparison.log` 和 `compare-format.ps1`。行号因新增注释后移，不当作新增格式问题；没有全文件 clang-format 写入。

## 串行 Debug/Release 专项

接管时未发现 cmake/ctest/MSBuild/ninja/cl 构建进程，总控已明确不并行使用构建树。复用并核对 `out/build/comments-public-20261009/lab-lifecycle/build`：源根为本仓库，H1/Testing ON，MSVC v142 `14.29.30133`，现有分列 source/execution UTF-8。H2 虽已配置 ON，本片不选择 H2/UI 目标，不改任何开关或 CMake。

实际只构建两个专项目标和必要非 Qt 依赖，先完成 Debug 再 Release：

```powershell
cmake --build out/build/comments-public-20261009/lab-lifecycle/build --config Debug --target pae_binary_public_h1_tests pae_binary_public_stream_tests --parallel 4
ctest --test-dir out/build/comments-public-20261009/lab-lifecycle/build -C Debug -R '^pae\.protocol_lab_binary\.(public_h1|public_stream)$' --no-tests=error --output-on-failure -j 1
# Debug 结束后，相同目标与正则执行 --config Release / -C Release。
```

执行入口为证据根 `run.ps1 -Phase Debug` / `-Phase Release`；每份日志首行记录展开后的命令、末行实际退出码。

| 阶段 | 证据日志（相对证据根） | 结果 |
| --- | --- | --- |
| Debug 构建 | `build-Debug.log` | 退出 0 |
| Debug CTest | `test-Debug.log` | 2/2 PASS，退出 0 |
| Release 构建 | `build-Release.log` | 退出 0 |
| Release CTest | `test-Release.log` | 2/2 PASS，退出 0 |

实际测试为 `pae.protocol_lab_binary.public_h1`、`pae.protocol_lab_binary.public_stream`。`test-inventory.log` 仅记录构建前注册核对，当时该树尚无这两个 exe；随后实际构建运行通过，不用 -N 代替测试证据。

H1 覆盖 typed RX/TX、raw/Decimal/Enum、ByteRange/bit masks、变长物理范围、完整性失败恢复、Flow 当前结果/草稿、结果预算 callback failure、实例/replacement；stream 覆盖三策略、半帧、粘连候选、失败候选消费后恢复、Flow Reset 隔离、工作预算/内部待办、真实 reserve 与回调分配失败、消费事实及 WRONG_INPUT_KIND 不旁路回退。未增加或修改测试代码。

Release 工程仍定义 NDEBUG（`release-test-options.log`），但两个测试源码均在 `<cassert>` 后 `#undef assert` 并显式重定义为始终求值、失败打印并 `return 1` 的检查；因此 Release 检查不依赖标准 assert，不会因 NDEBUG 关闭。实际 Release 2/2 运行证据独立列明，没有声称通过 /UNDEBUG 才开启。本次构建日志检索编译 warning C/D 编号为 Debug/Release 各 0，见 `warning-counts.log`；不因此宣称全仓零警告。

## 未验证、遗留与停点

未运行 H2/UI 映射专项、Lab/UI smoke/人工界面、全仓测试、全部开关、SDK/体验包重制或包外消费、Linux、真实协议/硬件/现场验证。格式遗留为上述既有 7/53 条，不在本片扩修；本批未确认需新增授权实施的代码缺陷，也不以注释专项作为完整缺陷审计。

未 Stage/Commit/Push、清理删除、发布或派发其他任务。完成一次主动总控交接后停止写入，等待总控复核；第四批 ASCII 仍保留原身份及报告，不被本报告替代。
