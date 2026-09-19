# Lab 自有展示类型与输入工具首片验证

## 1. 结论

本片已完成授权范围，待总控复核。

- Lab 的 document/message/field/pipeline 展示 DTO、field enum、`TypedDraft` 与
  `Decimal64` 已收敛到 Lab 自有头文件，该头文件不包含 private PAE Plan 或
  `protocol_lab::v06` 头文件。
- canonical `UINT64`/`INT64` 解析与 `Decimal64` 归一化已移入 Lab 自有工具；
  编辑草稿不再以 private `v06::Decimal64` 为存储类型。
- private Plan/public API/private Binary materializer 到 Lab DTO 的映射保留在边界 `.cpp`；
  旧 `v06` 执行桥只在真实执行边界接收由 Lab `Decimal64` 显式转换的值。
- execution owner、schema dispatch、Host/Codec 调用次数、事务与执行语义未改；
  Binary 0.9 仍为 Decode-only，ASCII A2 与 0.11 stream 路径不变。

## 2. 实现范围

### 2.1 新增 Lab 自有类型与工具

- `tools/protocol_lab_ui/owned_presentation_types.h`
  - `DocumentLayout`、`FieldValueType`、`FieldWireCodec`、`FieldByteOrder`、
    `FieldEncodeSource`。
  - `DocumentDescription`/`PipelineDescriptor`/`MessageDescriptor`/
    `FieldDescriptor`/`EnumDescriptor`。
  - `Decimal64`、`EnumSelection`、`TypedDraft`。
- `tools/protocol_lab_ui/canonical_input.h/.cpp`
  - canonical `UINT64`/`INT64` 解析。
  - lossless `Decimal64` 归一化。
- `pae_protocol_lab_ui_owned_presentation` 是独立 static target，只依赖
  `pae::project_options`；专项测试 target 只链接此 target 和 `pae::project_options`。

### 2.2 边界映射与 UI 消费

- `description_mapping.h` 仅保留 private source 到自有 DTO 的映射声明，DTO 已移出。
- `description_mapping.cpp`、`public_binary_description.cpp` 和旧 private
  `binary_host_adapter.cpp` 在边界处显式映射 enum 及 Decimal 展示能力。
- `document_session.cpp`、`field_table_model.cpp`、`exact_value_delegate.cpp`、
  `document_tab.cpp` 改为消费 Lab 自有类型。
- field type/source、editor 选择、read-only annotation 与 physical highlight 依据未变；
  `description_mapping` 专项显式断言 input/constant/computed 来源、只读标注及 physical mask。

## 3. 测试先行证据

新增 `tests/protocol_lab_ui/owned_presentation_types_tests.cpp` 后，在未有实现时先构建
`pae_protocol_lab_ui_owned_presentation_types_tests`，预期失败：

- 日志：`out/lab-owned-presentation-types/pre-fix-debug-owned-types.log`
- 失败：`fatal error C1083`，缺少 `tools/protocol_lab_ui/canonical_input.h`。

专项覆盖：

- `UINT64`/`INT64` 零值、上下界、溢出、前导零、符号和空白等 canonical
  正反例。
- `Decimal64` 正负数归一化、零值归一化与 scale 18 typed draft 无损保留。
- owned field type/source/source-ref/byte range/decimal flag/read-only annotation。
- `DocumentDescription` nothrow move 性质。

## 4. 验证结果

主构建目录：

- `out/build/windows-msvc-lab-owned-presentation-types`
- Visual Studio 18 2026, x64, `v142,version=14.29.30133`
- 仓内 Qt 5.13.0：`third_party/qt`

### 4.1 Debug / Release 专项及回归

Debug 与 Release 均完成 Lab UI 构建，下列 9 项测试串行全通过：

1. `owned_presentation_types`
2. `description_mapping`
3. `compile_queue`
4. `document_state`
5. `bounded_v08_state`
6. `binary_public_h2`
7. `binary_public_header`
8. `ascii_public_a2`
9. `ascii_stream_session`

日志：

- `out/lab-owned-presentation-types/debug-targeted-build.log`
- `out/lab-owned-presentation-types/debug-targeted-tests.log`
- `out/lab-owned-presentation-types/release-targeted-build.log`
- `out/lab-owned-presentation-types/release-targeted-tests.log`
- 最终 mapping 断言补强复跑：
  `out/lab-owned-presentation-types/final-description-mapping-build-tests.log`

Release 构建日志显式记录 `/DNDEBUG` 被 `/UNDEBUG` 覆盖，测试断言已执行。

### 4.2 Qt 自动 smoke

Debug 与 Release 均串行通过 8 项：旧 v0.5/v0.6、v0.7、v0.8、Binary H2、
ASCII A2、ASCII 单向、0.11 stream 及 stream close/cancel。

- `out/lab-owned-presentation-types/debug-qt-smoke.log`
- `out/lab-owned-presentation-types/release-qt-smoke.log`

这些是自动 smoke 证据，不替代人工 UI 验收。

### 4.3 旧 private Binary 兼容与预算

为编译验证 H2=OFF 时的旧 `binary_host_adapter.cpp` 边界映射，另用：

- `out/build/windows-msvc-lab-owned-presentation-types-private-compat`
- `PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER=ON`
- `PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2=OFF`

`binary_stage1` Debug/Release 均通过：

- `out/lab-owned-presentation-types/private-compat-debug-build.log`
- `out/lab-owned-presentation-types/private-compat-debug-test.log`
- `out/lab-owned-presentation-types/private-compat-release-build.log`
- `out/lab-owned-presentation-types/private-compat-release-test.log`

owned DTO 脱离 feature macro 后结构大小可与旧条件结构不同。H2 和旧 adapter 的账户
函数都直接用当前 `sizeof(DocumentDescription/PipelineDescriptor/MessageDescriptor/
FieldDescriptor/EnumDescriptor)` 计费；两条路径的 description exact/minus-one
预检断言在 Debug/Release 均通过，未将逻辑预算解释为 RSS 上限。

## 5. 边界与未验证项

- 未修改 root CMake、PAE public/src、Core/Plan/Schema/Host/stream 契约或旧执行桥实现。
- 未重打 SDK 五包，未覆盖旧部署，未执行全仓回归。
- 未做 Linux、目标板、硬件、现场或人工 UI 验收。
- 未 Stage、Commit、Push、发布或删除文件。
- 总控计划与既有两份盘点文档保留，本任务未覆盖其内容。
