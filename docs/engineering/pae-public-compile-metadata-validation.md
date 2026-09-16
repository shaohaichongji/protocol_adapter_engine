# PAE 公开编译与通用元数据首片验证

状态：2026-09-14 已完成派发范围内实现和 Windows 验证，待总控复核；不是正式 SDK、DLL
或生产可用声明。

权威范围见 [公开编译与通用元数据首片](pae-public-compile-metadata-slice.md) 和
[PAE / Lab 独立交付边界](pae-lab-delivery-boundary.md)。

## 1. 实际实现

- 新增 experimental `0.experimental.1` C++17 公开头，位于 `include/pae/`。
- `CompileProtocolJson()` 复用既有 `CompileJsonToPlanWithUiDescription()`，没有第二次解析 JSON
  或复制编译规则。
- move-only `CompiledProtocol` 通过 pImpl 整体持有同次编译产生的冻结 Plan 和 metadata；公开面
  不暴露内部 Plan 指针，也不提供独立拆分所有者的接口。
- 公开 description 覆盖 protocol、pipeline、message、field、enum 的 identity、展示文本、
  `source_ref`、关联索引/范围及 enum raw value；不公开复杂物理布局、Core handle 或 Lab DTO。
- 配置诊断通过显式 `switch` 映射到不受 `PAE_ENABLE_SCHEMA_*` 影响的公开枚举；metadata 资源项
  使用去 UI 化的 `METADATA_ACCOUNTED_MEMORY`。
- 空 owner、移动后 owner 和越界索引返回空值或零；借用的 `string_view` 在 owner 销毁、
  move-assignment 或替换后失效，移动后必须重新取得 view。
- `CompileMemoryReport` 分别报告 Plan accounted bytes、metadata accounted bytes、metadata
  allocation count 和 façade pImpl payload bytes；façade 数值不包含 allocator 实现开销，不能解释为
  进程 RSS 上限。
- 新增单一源码树消费 target `PAE::pae`。Stage 1 开关默认 OFF；启用时固定打开现有 Schema
  0.1～0.11 Compiler 能力，但不构建或公开 Core/Framer/Host。
- 保留既有 `CompiledUiArtifacts` 和 Lab 调用路径，未迁移 Lab、未修改协议或 Schema 执行语义。

最小调用形状：

```cpp
#include <pae/compiler.h>

auto result = pae::CompileProtocolJson(json);
if (!result.Succeeded()) {
  const pae::CompileDiagnostic* diagnostic = result.Diagnostic();
  // 使用 stage/code/json_pointer/byte_offset/resource fields。
} else {
  const auto protocol = result.Compiled()->Protocol();
  // 在 result 持有编译所有者期间使用 protocol 中的借用字符串。
}
```

CMake consumer 只链接：

```cmake
set(PAE_BUILD_PUBLIC_API_STAGE1 ON CACHE BOOL "" FORCE)
add_subdirectory("${PAE_SOURCE_DIR}" pae-source)
target_link_libraries(my_consumer PRIVATE PAE::pae)
```

## 2. 自动化覆盖

`tests/public_api/public_api_tests.cpp` 在 Debug/Release 均完成 41 项精确断言：

- 公开 API 和 Schema 0.1～0.11 支持查询；
- 空 owner、越界 pipeline/message/field/enum 及关联查询安全失败；
- 非法 JSON 保留 `JSON_SYNTAX/JSON_SYNTAX_ERROR` 和 byte offset；
- Schema 0.1 Binary、带 enum 的 Schema 0.1 Binary、Schema 0.10 ASCII、Schema 0.11 ASCII
  stream 均能通过公开入口编译并读取 metadata；
- pipeline/message、message/field、field/enum 的索引关系；
- 合成 enum `mode_idle` 的 id、中文 display name 和 raw value `1`；
- Plan、metadata、façade 资源类别分离；
- metadata exact limit 成功、minus-one 在 `RESOURCE_BUDGET` 阶段以结构化资源诊断失败；
- `CompileResult` 取出、owner move construction/move assignment，以及移动后重新取得 view。

`tests/public_api/header_*.cpp` 分别单独 include 四个公开头；
`verify_public_headers.cmake` 拒绝私有相对路径、Compiler/Plan/UiDescription 名称、Schema 功能宏和
Qt token。consumer 生成工程复核到主程序的 include path 只有仓库 `include`，静态链接闭包由
`PAE::pae` 自动带入 public API、Compiler、yyjson 和 Plan 库。

## 3. 实际命令和结果

工具链：Visual Studio 18 2026 generator、Windows x64、MSVC 19.29.30159、v142
14.29.30133；生成工程为 Debug `/MDd`、Release `/MD`。

### 3.1 Stage 1 构建与测试

```powershell
cmake --preset windows-msvc-public-api-stage1 -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=OFF
cmake --build --preset windows-msvc-public-api-stage1-debug
cmake --build --preset windows-msvc-public-api-stage1-release
ctest --test-dir out/build/windows-msvc-public-api-stage1 -C Debug --output-on-failure
ctest --test-dir out/build/windows-msvc-public-api-stage1 -C Release --output-on-failure
```

结果：配置成功；Debug、Release 构建均退出 0；两种配置均为 9/9 CTest 通过，其中 public API
3 项、Compiler 5 项、yyjson vendor 1 项。最终 target 集合未包含 Core、Lab 或 Qt。

日志：

- `out/public-api-stage1/configure.log`
- `out/public-api-stage1/build-debug.log`
- `out/public-api-stage1/build-release.log`
- `out/public-api-stage1/test-full-debug.log`
- `out/public-api-stage1/test-full-release.log`
- `out/public-api-stage1/public-api-debug-detail.log`
- `out/public-api-stage1/public-api-release-detail.log`

### 3.2 仓库外源码 consumer

consumer 源目录实际位于：

`%LOCALAPPDATA%\Temp\pae-public-api-stage1-consumer-53386a1de5fd415986a0e671203c2c2a`

复制 `examples/public_api_compile/` 后执行独立 configure、Debug/Release build 和运行。Debug 使用
公开 Binary 0.1 样例，输出：

```text
schema=0.1 protocol=synthetic_lab_exchange pipelines=2 messages=2
```

Release 使用公开 ASCII stream 0.11 样例，输出：

```text
schema=0.11 protocol=synthetic_ascii_stream_slice pipelines=3 messages=3
```

两种配置退出码均为 0。MSBuild 对位于系统临时目录的中间目录给出 `MSB8029` 增量构建提示；
本次全量构建和运行成功，该提示不作为成品包或长期构建目录结论。

日志：`out/public-api-stage1/external-*.log`；源目录位置另见
`out/public-api-stage1/external-consumer-path.txt`。未删除该临时目录。
早期构建边界收紧前还创建过
`%LOCALAPPDATA%\Temp\pae-public-api-stage1-consumer-53e31659b08940258b393226a7f5a6c8`；按禁止递归清理边界保留，
不作为上述最终消费证据。

### 3.3 既有 Lab 兼容

为避免覆盖原推荐 build，使用相同 `windows-msvc-pae-lab` preset 并把 binary dir 改到
`out/build/windows-msvc-public-api-stage1-lab-compat`。`PAE_BUILD_PUBLIC_API_STAGE1` 保持 OFF，
Debug/Release 的 `pae_protocol_lab_ui` 目标均构建成功、退出 0；没有启动 UI 或重复人工验收。

日志：

- `out/public-api-stage1/lab-compat-configure.log`
- `out/public-api-stage1/lab-compat-build-debug.log`
- `out/public-api-stage1/lab-compat-build-release.log`

## 4. 明确未完成边界

- 未实现或验证 install/export、`find_package(PAE)`、正式源码包、静态 SDK 或动态 DLL SDK。
- 未公开 Core、Framer、Host、完整物理布局或 CopySnapshot；未迁移 Lab 到新接口。
- `PAE_API` 仅为后续 DLL 准备导出标记；本片实际 target 是 STATIC，不能据此宣称 DLL ABI 已闭合。
- 未验证 Linux、跨编译器/跨工具集 ABI、`/MT`、真实外部生产项目、私有协议、硬件或现场。
- 没有执行人工 Lab 截图或网络测试；既有人工验收不自动扩展到本接口。
- experimental 0.x 不承诺跨版本二进制兼容；正式许可和发布位置仍待后续决定。

本片没有发现需要改变协议/Schema 语义的冲突。是否进入下一片或提交阶段由总控另行复核和授权。
