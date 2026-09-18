# PAE 0.11 公开静态 Framing 查询验证

## 1. 结论与范围

基线为 `main@dbf4798f96a45b8d36a4754ad5a01e274680337e`。本片按
`pae-public-ascii-consumption-contract.md` 第 6 节新增只读
`QueryPipelineFramingDescription`，没有改变 Schema、Plan 布局、Core、Host、Framer 执行算法、计费或
旧 capability 的布局/合法状态。没有 Stage、Commit、Push 或正式发布。

查询对 complete Pipeline 返回成功、`COMPLETE_RECORD/COMPLETE_RECORD` 且 M 为空；三种 Binary stream
分别返回 M=3/4/8，ASCII CRLF 返回 M=12（含 CRLF）。运行 `C=4` 与静态 `M=12` 已独立断言。当前未见
本片提交阻断项；内部损坏 Frozen Plan 无现成安全构造入口，本轮没有用 UB 或新增生产测试接口强造该负例。

## 2. 实现文件

- `include/pae/stream_framer.h`：新增固定形状的 input/strategy/query status、纯值 description/result 和
  导出函数；直接包含 `<optional>`。
- `src/public_api/stream_framer.cpp`：从同一 Frozen Plan 做 checked 映射；非法 profile/组合/零值/硬上限/
  `size_t` 表示失败关闭。旧 `QueryStreamFramingCapability` 复用新查询，并把内部契约错误保持映射为旧
  `INVALID_COMPILED_PROTOCOL`。
- `tests/public_api/public_stream_framer_tests.cpp`：新增精确映射、invalid/moved/index、真实 allocation
  observer、C/M 分离及旧 capability 一致性断言。
- `examples/public_api_sdk_consumer/main.cpp`：包外 consumer 实际调用新导出并核对 ASCII M=12。

未采用内部容量 helper：本片查询必须同时映射 public enum，Workspace 现有容量路径保持原样；新增查询用
同一四分支和 hard limit 防御，并由精确测试锁定，没有改变执行接受域。

## 3. 实际验证

构建目录：

- static/tests：`out/build/windows-msvc-public-stream-description`
- Testing-off：`out/build/windows-msvc-public-stream-description-testing-off`
- shared：`out/build/windows-msvc-public-stream-description-shared`
- shared consumer：`out/build/windows-msvc-public-stream-description-consumer{,-debug}`

证据根：`out/public-stream-description/`。

实际结果：

- Windows x64 Debug/Release 全目标构建成功。
- public Framer 可执行程序 Debug/Release 各 `50/50`；新增断言包含
  `description_query_no_allocation`、`description_m_is_distinct_from_runtime_c`。
- 定向 CTest Debug/Release 各 `6/6`：public header self-contained/boundary、public Framer、public Host、
  Binary/ASCII internal Framer。
- Product-only / Testing-off Release 构建 `pae_public_api` 成功，`ctest -N` 为 `Total Tests: 0`。
- shared Debug/Release `pae.dll` 成功；`dumpbin /exports` 同时存在旧
  `QueryStreamFramingCapability` 与新 `QueryPipelineFramingDescription`。
- 独立安装前缀的 shared consumer Debug/Release 均链接并运行成功，输出
  `PAE_SDK_STAGE3_CONSUMER_PASS ...`。
- `clang-format` 已应用于四个源码/测试文件；`git diff --check` 通过。

关键日志：

- `public-stream-framer-debug.log`、`public-stream-framer-release.log`
- `ctest-debug-directed.log`、`ctest-release-directed.log`
- `build-debug-all.log`、`build-release-resume.log`
- `build-testing-off-release-resume.log`、`ctest-testing-off-list.log`
- `build-shared-debug.log`、`build-shared-release.log`
- `consumer-shared-debug-run.log`、`consumer-shared-release-run.log`

首次 configure/build 命令曾因执行窗口 30 秒结束而中止输出，随后在同一隔离目录正常续跑；对应
`configure.log`、`build-release-all.log` 和 `build-testing-off-release.log` 保留，不把它们冒充最终成功日志。

## 4. 边界与剩余风险

- shared 构建仍报告项目既有 C4251 实验 C++ ABI 警告；本片未新增稳定 ABI 承诺。
- 未重打五包/六组 SDK；按总控顺序在接口复核后另派。
- 未执行 Qt/Lab、网络、Linux、硬件、Golden 或人工验收。
- 未修改旧 final6/candidate2 或任何部署目录；本地 install 仅位于 ignored 的本轮证据目录。

状态：**已完成派发范围，待总控复核。**
