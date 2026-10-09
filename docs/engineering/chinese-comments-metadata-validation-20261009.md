# 第十五批内部 metadata 中文注释验证（2026-10-09）

## 1. 接管、范围及并行例外

接管与最终 HEAD 为 main@0e40741cebb283bf2a3438b940584eeaf68606d9，暂存区为空。
接管时第十四批 Decimal 两源码注释及其验证报告尚未提交，完整保留，不算本批增量。
本批只向 src/config_compiler/protocol_metadata.h 新增 13 行、protocol_metadata.cpp 新增
18 行中文注释，共 31 行；另新增本报告。准确英文保留，没有更改非注释 token、声明、
布局、接口、宏、行为、测试、CMake 或依赖，也没有重复补公开接口已有说明。

用户授权另一任务并行补 Lab 显示注释，双方不写同一文件；本任务独占本轮构建时段，
对方在总控放行前只写注释。本批范围外保护 Hash 明确排除下列 7 个并行候选：

- tools/protocol_lab_ui/description_mapping.h、description_mapping.cpp
- tools/protocol_lab_ui/field_table_model.h、field_table_model.cpp
- tools/protocol_lab_ui/document_tab.cpp、hex_view.cpp
- docs/engineering/chinese-comments-lab-display-validation-20261009.md

排除清单原始保存于 parallel-exceptions.log，不将这些授权变化误判为污染，也不为其
注释正确性或验证结果背书。原保护清单含 Decimal 两源码及报告与其余非例外文件，
共 3244 项，最终逐项 SHA256 不变；未重建 baseline 掩盖差异。

## 2. 注释语义与源码依据

读取 [公开编译 metadata 契约](pae-public-compile-metadata-slice.md)、实际内部类型、
Builder、编译发布链、公开 description 查询与共享状态实现后补充以下说明：

- 单块存储布局为 header、按实际 sizeof/alignof 对齐的描述数组、复制文本区。
  DescriptionStringSpan 为整块起点的字节偏移和长度，不是字符数或字符串区相对偏移。
  CopyString 不附加 NUL，不借用原 JSON/SchemaIr；空文本保留有效游标。
- Message/Field/Enum 按配置顺序展平；begin/count 是全局表区间，不是 raw 值范围。
  索引成员包含在描述对象计费中，因此 index_bytes=0 不表示漏掉索引。
- ProtocolMetadataStorage 是 move-only 自有 owner；返回数组、引用、string_view 为借用，
  不增加引用计数。内部 Protocol/数组访问器要求非空合法存储，operator[] 不做越界检查，
  不能当成公开 facade 的 HasValue/optional 防御接口。移动后重新查询，不能用残留计费值
  判断 moved-from owner 有效。
- Resolve 仅检查空存储、整块范围及加法溢出；Audit 另核对字符串区归属，不把 Resolve
  注释为能检查任意伪造 span 的完整语义验证器。
- Build 首先释放旧 metadata，失败不保留旧视图。通过预算前不分配；有效上限取调用方
  限制与 profile/ABI/字符串预算推导上限的较小值。临时 RAII owner 管理失败回收，
  placement new 构造平凡析构描述对象，复制及游标核验完成后才转移 owner。
- Plan 冻结后 Audit 复算布局、计费、跨度、计数及连续展平区间，再组合发布；总数量相等
  不足以证明字段关联正确。Audit 不执行 Matcher/Codec，也不认证 source_ref 真实性。
  内存报告是逻辑计费，不是编译峰值或进程 RSS 硬上限。

依据位置为两个本批源码中的 DescriptionStringSpan/DescriptionArrayView/Storage、
DescriptionHeader/AddArrayRegion/CalculateLayout/CopyString/Resolve、DerivedLimit、
Build/Audit，以及 src/config_compiler/config_compiler.cpp 的 CompileJsonToPlanWithMetadata
发布链。CompiledProtocolArtifacts 自有 PlanOwner 和 metadata，CompiledState 同时保留
二者；公开查询身份来自 Plan，展示文本来自 metadata。此所有权关系此前已有中文说明，
本批不再修改 config_compiler.h、公开 compiler/description/state 文件。

## 3. 实际专项验证

复用经核对的 out/build/comments-public-20261009/default-repo，未重配扩大矩阵。
工具链 Visual Studio 18 2026/x64/v142 14.29.30133；缓存公共 API 与测试开启，已有
PAE_BUILD_HOST_ENDPOINT_SLICE=ON 如实保留，不表示本次跑了 Host 专项。
CL/_CL_ 未设置，未改 Qt、全局环境或依赖。初始残留 MSBuild 为 nodeReuse worker，
未当成运行中的构建或擅自终止。

开始前递归审计三个目标的生成项目引用，10 个项目的 ClCompile 输入未含 Lab 源码；
没有以并行修改中的 Lab 文件为本批编译依赖。目标和测试名从实际 CMake 注册核对。
从仓库根运行，下方 B 为上述构建根；Debug 的 Build/CTest 全部结束后才开始 Release：

```powershell
ctest --test-dir $B -C Debug -N -R '^(pae\.config_compiler\.ui_description|pae\.public_api\.compile_metadata|pae\.public_api\.consumer_metadata)$'
cmake --build $B --config Debug --target pae_config_compiler_ui_description_tests pae_public_api_tests pae_public_consumer_metadata_tests -- /m:2
ctest --test-dir $B -C Debug -R '^(pae\.config_compiler\.ui_description|pae\.public_api\.compile_metadata|pae\.public_api\.consumer_metadata)$' -V
cmake --build $B --config Release --target pae_config_compiler_ui_description_tests pae_public_api_tests pae_public_consumer_metadata_tests -- /m:2
ctest --test-dir $B -C Release -R '^(pae\.config_compiler\.ui_description|pae\.public_api\.compile_metadata|pae\.public_api\.consumer_metadata)$' -V
```

实际测试清单、两个 Build 与两个 CTest 均退出 0，Debug/Release 各 3/3。

| 测试 | Debug 实际计数 | Release 实际计数 | 代表性覆盖 |
| --- | --- | --- | --- |
| pae.config_compiler.ui_description | 69 PASS / 0 FAIL | 69 PASS / 0 FAIL | Plan-only 不碰描述、单块计费与连续区间、预算 exact/minus-one、布局溢出、分配/Freeze/Audit 故障路径及回收 |
| pae.public_api.compile_metadata | 41 PASS / 0 FAIL | 41 PASS / 0 FAIL | 身份/关联/字段/枚举文本、计费类别、空 owner、结构化诊断、预算、移动后重新查询与越界 |
| pae.public_api.consumer_metadata | 21 PASS / 0 FAIL | 21 PASS / 0 FAIL | 类型/Encode 来源、关联权限与尺寸、Decimal/Computed/Bounded/ASCII、查询无分配、移动/越界 |

实际检查用运行时 Runner::Check 和失败计数决定退出，不依赖 assert；Release 项目为
MaxSpeed/NDEBUG，仍保留这些检查。计数只是本次执行数量，不代表全输入域覆盖率。
没有新增或改写测试、期望或旧指纹。Build 日志未见 warning/error。

一次验证辅助失败单独保留：首次 out 中的项目依赖审计脚本误将绝对 ProjectReference
拼接到当前目录，Resolve-Path 失败；尚未执行 Build/CTest。这是辅助脚本问题，不是产品
编译或测试失败。只修 out 脚本，识别绝对/相对路径，使用 retry- 日志前缀重跑，旧失败
日志保留，没有覆盖。产品源码除注释外不变。

## 4. 等价、格式与证据

本批证据独立位于 out/build/comments-public-20261009/metadata/：

- takeover.ps1、takeover-status.log、baseline/、protected-hashes.json、parallel-exceptions.log：
  原始接管、原路径源码副本、保护及并行例外。
- baseline-format 日志、build-cache-audit.log：原有格式状态与真实构建开关。
- verify-comments.ps1、initial/final-equivalence.log、对应 format/diff-check 日志：
  非注释 token/字面量、预处理指令、宏续行、UTF-8、保护与候选检查。
- validation-transcript.log：首次辅助脚本失败；run-validation.ps1、retry-validation-transcript.log
  及 retry-dependency-projects.log：修正后依赖审计、实际命令与退出。
- retry-test-inventory.log、retry-build/test/cases-{Debug,Release}.log：本次专项原始输出。

两个源码的非注释 token、字面量、指令及宏续行与原 baseline 一致，没有注释尾反斜杠。
等价脚本遇到不支持的 raw string 语法失败关闭；本批源码无该语法。未全文件格式化。

clang-format dry-run 并非全文件通过：原 header 退出 1、有 1 条诊断，原 cpp 退出 1、
有 21 条诊断。修改后相同退出及数量，按不变代码 token 锚点、列号、诊断文本和代码行
逐项比较一致，新增诊断为 0。旧格式问题不在本批顺手修复。
两个源码与本报告 UTF-8 无 BOM，git diff --check 退出 0。范围外原 3244 项 Hash 全不变，
包含第十四批已有未提交内容。候选白名单分开本批 3 项、Decimal 既有 3 项、并行 Lab
最多 7 项；并行项不纳入本批实现/验证成果。定向敏感路径/凭据检查通过，非全面保密审计。
out 保持忽略，不进入源码候选。

## 5. 最终边界与停点

本批没有发现需要越界修复的明确产品问题，注释及三项 Windows 专项已完成；
不是独立完整内存安全、并发或非法内部调用审计。未重跑全矩阵、公开物理/ASCII 专项、
UI/Lab 显示、网络、SDK 包外/搬迁/打包、开关隔离、Linux、真实协议 Golden、硬件或现场；
不新增性能、RSS 或来源认证结论，也不声称对过期借用视图提供运行时保护。

最终分支/HEAD 不变，暂存空。本批增量为两 tracked 注释文件与一 untracked 报告；
其他已有及并行候选以 final-equivalence.log 的实际快照为准，不归本任务所有。
未 Stage、Commit、Push、下载、清理、重打包或继续下一批。
本轮构建已结束并释放时段；完成一次总控交接，停止写入，待总控复核和放行 Lab 构建。
