# PAE-DEC-042B Core 双向转换 Windows 验证报告

日期：2026-09-07。状态：第二段 Core 已实现并完成限定 Windows 验证，待总控复核。

## 1. 范围

本段在已提交的 Schema 0.5/编译冻结基础上实现 COMPLETE_RECORD Core 的精确双向转换、
失败优先级、Workspace 原始整数诊断、Encode 原始值缓存和最终重读复核。未实现 Values 0.4、
Result/Record/Event 0.6、Protocol Lab Reader、Replay 或指纹；Schema 0.5 与 Protocol Lab 的
CMake 组合继续失败关闭。默认开关仍为 OFF。

## 2. 实现与契约映射

- A1：Core 私有固定 256 位有符号幅值实现乘、加减、除和规范化；不引用 `spikes`，不使用
  浮点、`__int128`、第三方大整数或新依赖。
- A2/A4：执行字段保存转换表索引和消息内转换槽；Workspace 按每 Message 最大转换数预分配
  Decode Decimal 暂存、原始诊断及 Encode raw 缓存。raw kind实际存储改为与计费一致的
  `ValueType`；估算使用实际元素 `sizeof`，并受既有Session memory limit门禁约束。
- A3：转换字段只接受/交付 `Decimal64`。Decode 仅在整条记录成功后发布连续原始整数诊断；
  任一后续 Workspace 调用先使其失效。普通字段不保存 raw 副本。
- B1/B2：Schema 0.1～0.4沿用原路径；Schema 0.5先按输入顺序完成引用、常量覆盖和重复检查，
  再按配置顺序检查必填、类型、Decimal表示、精确反算和Wire范围，最后检查输出与写入。
- B3：Decode保持结构唯一、输出容量/别名安全、SUM8、字段顺序转换、整体交付；失败字段数为0，
  原始诊断不可读。
- B4：数值失败通过`conversion_error`区分scale、非整数、Wire越界和逻辑值越界；scale超出
  0..18为`INVALID_ARGUMENT`。合法Plan内部算术异常（包括最终重读转换）为`INTERNAL_ERROR`，
  只有最终字节或数学值不一致为`FINAL_REVIEW_FAILED`。
- D1：本次只交付第二段，不接入Lab。D2中的非整数与输出容量、SUM8与转换越界、配置字段顺序、
  中途失败不交付字段、混合与无转换Plan已有Core专项断言；raw诊断失效的失败Encode/Decode
  状态迁移已补齐独立前置。Schema至Lab Replay全链和D3证据Reader检查属于第三段。

## 3. 专项自动化

`pae.protocol_core.decimal_conversion.contract`使用公开、从零设计的34字节合成记录和独立手算字节，
并增加一个无转换Schema 0.5对照Plan：

- `raw=523`经`raw/10-40`得到规范`{123,1}`；等价输入`{1230,2}`编码为同一字节。
- `UINT64_MAX + INT64_MIN = INT64_MAX`覆盖宽中间值抵消。
- 负比例/偏置把`INT64_MIN`映射为`INT64_MAX`，覆盖符号和极值。
- 覆盖反算非整数、Decimal scale非法、Wire越界、Decode逻辑值越界、转换字段类型错误、
  输入顺序翻转、SUM8与转换错误并存时SUM8优先。
- 断言失败不交付字段/原始诊断，成功诊断携带正确raw kind、值和字段身份。
- 混合Plan中的旧UINT64常量保持原语义；无转换Plan的执行槽为0且可正常Encode。
- 操作观察器断言Encode每字段预检一次、最终重读转换一次；Decode每字段转换一次。
- 内部故障注入分别验证`INTERNAL_ERROR`和实际字节被改写后的`FINAL_REVIEW_FAILED`。

`pae.protocol_core.complete_record_codec.first_decimal_allocation`在Plan及Workspace建立后，对首次
Encode和首次Decode包围全局`new/new[]`计数，增量均为0。

### 3.1 总控审查纠错增量

- P2-1独立核实成立：原实现把所有非内部转换失败统一映射为`VALUE_NOT_REPRESENTABLE`；专项旧断言
  也固化了该错误结果。现按契约把scale -1、19以及coefficient=0的两种非法scale均精确断言为
  `INVALID_ARGUMENT / DECIMAL_SCALE_OUT_OF_RANGE`，同时核对失败字段、输入索引、操作次数、
  `required_size=0`及`bytes_written=0`。
- P2-2静态确认成立：旧`VerifyFields`只有bool结果，最终重读转换的内部故障会与值不一致一起落入
  `FINAL_REVIEW_FAILED`。现使用三态内部结果区分MATCH、MISMATCH和INTERNAL_ERROR；专用故障注入
  断言4次预检转换完成后，第5次最终重读发生故障，返回`INTERNAL_ERROR`、无conversion reason、
  `bytes_written=0`。实际改写字节的对照仍为`FINAL_REVIEW_FAILED`。
- 收口检查：Core入口增加与编译开关一致的Schema能力门禁；能力判断函数测试覆盖缺省构建拒绝0.5、
  专用构建接受0.5、两者拒绝0.6，入口调用由源码复核确认；没有构造不兼容二进制混链负例。
  raw诊断的成功Decode发布、后续成功Encode失效、失败Encode/Decode失效和越界读取均已有断言。
- 资源边界：两档Profile按最保守的每消息最大输入字段、位容器及转换槽同时取上界计算，载荷仍低于
  各自Session memory limit；因此合法配置不能单独触发“仅Workspace超过Session”的边界，先由
  字段/容器数量门禁拒绝。Plan内存仍有exact与limit-minus-one动态测试；本结论不含allocator容器
  对象开销、调用方输出槽或局部算术栈。

### 3.2 提交前总控收口（2026-09-07）

两项P2产品修复已完成源码与专项断言核对。总控随后发现一项P2测试证据缺口：原专项唯一成功
Decode之后先调用成功Encode清空raw诊断，再执行失败Encode和失败Decode，因此当时后续零数量断言
不能独立证明“有已发布诊断时，下一次失败调用使其失效”。该限制保留为历史发现，不追溯改写。

本轮只修改专项测试，补齐两个独立状态迁移：每条均使用固定`expected`报文重新成功Decode，断言
`field_count=5`、raw数量4、索引0的Plan/Message/Field身份、INT64 kind和值523均可读；随后分别
紧接非法scale Encode或SUM8失败Decode。失败Encode精确断言`INVALID_ARGUMENT`及
`DECIMAL_SCALE_OUT_OF_RANGE`、字段/输入索引0、`required_size=0`、`bytes_written=0`；失败Decode
断言`INTEGRITY_FAILED`和`field_count=0`。两条后置均断言raw数量0且索引0不可读。未修改产品源码。

针对性构建及测试使用原专用Core目录，Debug/Release严格串行，测试注册名均为
`pae.protocol_core.decimal_conversion.contract`，各1/1通过。日志：

- `out/dec042b-raw-lifecycle-debug-build.log`
- `out/dec042b-raw-lifecycle-debug-ctest.log`
- `out/dec042b-raw-lifecycle-release-build.log`
- `out/dec042b-raw-lifecycle-release-ctest.log`

修复前P2独立运行记录未完整落盘；修复前依据限于旧源码和旧测试断言，不作为磁盘运行证据。
此前已落盘16/16与25/25、隔离构建和Lab配置拒绝记录属于历史验证，本轮未重跑，也未改写。

## 4. Windows执行结果

专用目录：`out/build/windows-msvc-dec042b-core-resume`，显式开启Loader、Core、Testing和
`PAE_ENABLE_SCHEMA_V05_COMPILER`，关闭Protocol Lab。本轮纠错后Debug/Release串行构建和CTest：
各16/16通过，其中DEC-042B专项2项。日志：

- `out/dec042b-review-core-debug-build.log`
- `out/dec042b-review-core-debug-ctest.log`
- `out/dec042b-review-core-release-build.log`
- `out/dec042b-review-core-release-ctest.log`

旧代全切片目录：`out/b/d42bcf`，Schema 0.5开关关闭并同时启用Loader、Core和Protocol Lab。
本轮纠错后Debug/Release各25/25通过；UDP测试仅既有本机Loopback。日志：

- `out/dec042b-review-full-debug-build.log`
- `out/dec042b-review-full-debug-ctest.log`
- `out/dec042b-review-full-release-build.log`
- `out/dec042b-review-full-release-ctest.log`

隔离构建：Product-only目录`out/b/d42bcp`、Lab-on/Testing-off目录`out/b/d42bcl0`均完成
Debug/Release构建，`ctest -N`均为0测试。Schema 0.5与Lab组合在`out/b/d42b-review-gate`配置失败，
诊断明确指出Lab证据段尚未实现。相关日志均以`out/dec042b-review-product-*`、
`out/dec042b-review-lab0-*`、
`out/dec042b-review-isolation-ctest-list.log`及`out/dec042b-review-lab-gate.log`命名。

源码已使用VS 2026随附clang-format 22.1.3格式化；`git diff --check`通过。

## 5. 边界

- 未实现或验证Values 0.4、Lab 0.6证据、Reader、Replay、Compare及新指纹域；不能用Core结果
  宣称D03或完整D08～D10已完成。
- 未执行本轮未授权的独立高精度Oracle；固定向量、既有隔离算术测试与静态边界分析不等价于
  全输入空间形式证明。
- 未执行Linux、真实协议Golden、非Loopback、硬件、现场或正式性能验证。
- 当前能力开关会改变内部C++类型布局；不同开关构建产物混用属于不支持的ABI组合。本轮只证明
  各自完整重建后的运行时代际拒绝，不声称能保护已发生ABI错配的二进制混链。
- 本报告不改变旧指纹和历史证据；未Stage、Commit、Push。
