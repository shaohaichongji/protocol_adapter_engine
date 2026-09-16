# ASCII 公开事实首片 Windows 验证

总控限定复核（2026-09-15）：已读取公开查询、切片注释、Codec 匹配身份校验、Host 同源透传、新专项测试及外部 consumer，并核对 final D/R 专项与 CTest 日志、shared 运行/导出日志；本片在上述范围完成限定技术复核。总控未重复构建测试，未验证新 SDK 安装包或 Lab 迁移。查询使用冻结数据与固定栈临时空间，不新增持有缓存；重复查询无分配证据不等于整体性能或 RSS 证明。下文“待总控复核”为任务交付时点。执行任务停止；下一片候选是新 SDK 消费验证，尚未派发，不覆盖历史候选或扩大本片结论。

日期：2026-09-15。状态：已完成派发范围的本地实现和定向验证，**待总控复核**；未 Stage、Commit、Push，未发布或覆盖既有 SDK 候选。依据：[ASCII 公开消费契约](pae-public-ascii-consumption-contract.md)第 2、3、5、7 节。本记录只描述本次落盘及执行证据，不回写该契约制定时的历史状态。

## 实现范围

- `CompiledProtocol` 对冻结 ASCII Plan 提供 Decode/Encode 动作、按序片段及 BYTES 字段的只读查询。literal 用明确长度的借用字节视图，查询失败不交付描述；Binary 物理查询的接受域未扩大。查询未新增 facade 持有缓存、Workspace 槽或逐帧分配。
- 公开头明确：成功 ASCII BYTES Decode 视图是本次输入连续子区间，零长度字段仍保留实际起点，尾端可为 one-past-end；字段视图遵守现有 Codec epoch，字节还依赖输入寿命。
- `DecodeResult::matched_message_index` 只透传本次 Core 已唯一选出的消息，并检查所属 compiled owner、Pipeline 和 allowed 集合；Host candidate observer 同源透传。未知、歧义和早拒绝不猜身份；失败仍无有效 record，也不进入成功业务 sink。
- 未修改 Core、Plan、Schema、Qt/UI、Framer 或 TX 实际 range 结果。本片不是 Lab 迁移、流式静态描述或正式 SDK 交付。

## 测试先行与本次结果

先新增公开测试，再构建：修复前新测试目标构建退出 1，缺少 `AsciiAction` 等拟新增公开类型/查询，见 [preimplementation/build-debug.log](../../out/stage4-ascii-facts/preimplementation/build-debug.log)。此失败证明测试能检测接口缺席，不证明每项运行语义都已独立红测。

使用 Visual Studio 18 2026、x64、v142 14.29.30133，静态构建目录 `out/build/windows-msvc-stage4-ascii-facts`。定向目标构建 Debug/Release 均退出 0；直接运行 `pae_public_ascii_facts_tests.exe` 各 **34/34**，日志分别在 `out/stage4-ascii-facts/final-Debug-direct.log`、`final-Release-direct.log`。串行执行公开 API 标签 CTest：

```powershell
ctest --test-dir out/build/windows-msvc-stage4-ascii-facts -C Debug -L public_api --output-on-failure
ctest --test-dir out/build/windows-msvc-stage4-ascii-facts -C Release -L public_api --output-on-failure
```

各 **9/9**，见 `out/stage4-ascii-facts/final-Debug-public-ctest.log`、`final-Release-public-ctest.log`。包含公开头自包含/边界、编译元数据、Codec、Binary 物理查询、Framer、Host 等受影响回归；不是全仓测试矩阵。

34 个断言点覆盖：双向及单向/literal-only 动作、独立 RX/TX 片段顺序和字段引用、固定/变长界限、显式长度及嵌入 NUL literal、无效 selector/非 ASCII 拒绝、128 次重复查询无分配、成功输入位置、末尾及中间零长度位置、旧 view epoch 失效、编译 owner move 后旧查询失效、唯一匹配后字符/容量失败的诊断身份与零交付、真实可达的未知/歧义无身份、Host observer 与业务 sink 同源身份及早拒绝无 candidate。测试断言位于 `tests/public_api/public_ascii_facts_tests.cpp`；测试数量不是覆盖率证明。

## 共享导出与包外消费

将 `examples/public_api_sdk_consumer` 作为独立顶层 CMake 项目，以 `PAE_SOURCE_DIR` 链接本工作树 public target，`BUILD_SHARED_LIBS=ON`，Testing-off，不用旧 final6/candidate2 包冒充含新 API 的 SDK。实际命令：

```powershell
cmake -S examples/public_api_sdk_consumer -B out/build/windows-msvc-stage4-ascii-facts-shared -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" -DPAE_SOURCE_DIR="F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine" -DBUILD_SHARED_LIBS=ON -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_JSON_PARSER_SPIKE=OFF
cmake --build out/build/windows-msvc-stage4-ascii-facts-shared --config Debug --target pae_sdk_stage3_consumer --parallel 4
cmake --build out/build/windows-msvc-stage4-ascii-facts-shared --config Release --target pae_sdk_stage3_consumer --parallel 4
```

Configure、两个构建均退出 0；Debug/Release 消费者各以公开合成 Binary/ASCII 配置运行，退出 0，输出 `ascii_facts=1`。日志在 `out/stage4-ascii-facts/shared-configure.log`、`shared-Debug-build.log`、`shared-Release-build.log`、`shared-Debug-run.log`、`shared-Release-run.log`。`dumpbin /exports` 对两个新 DLL 均退出 0，确认 `CompiledProtocol::AsciiAction`、`AsciiSegment`、`AsciiField` 实际导出，日志 `shared-Debug-exports.log`、`shared-Release-exports.log`。共享构建仍有既存 C4251 STL 成员 DLL 警告；本次链接和运行成功不等于全部 ABI 兼容风险已关闭。

## 边界与待复核

没有运行 Qt Lab、人工 UI、非 Loopback、真实协议 Golden、硬件、Linux，也没有覆盖已验收部署或重打源码/静态/动态 SDK 五包。成功 BYTES 借用视图仍要求调用方保持输入有效；public-only 消费者不承担 UI 长期所有权。TX 展示投影、控制字节列表、流式 framing 静态事实均是其他片。冻结查询重复无分配及定向 Codec 回归不能提升为完整资源峰值或性能结论。

本次产物位于被 Git 忽略的 `out/stage4-ascii-facts` 和独立构建目录；原有共享工作树变更均保留。提交候选与已有其他任务文件必须由总控分离核查。本片完成后的状态是“待总控复核”，不是整体 ASCII 迁移或发布批准。
