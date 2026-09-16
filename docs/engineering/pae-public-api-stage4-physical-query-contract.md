# PAE 公开 API 阶段 4 P：Binary 物理布局查询契约

日期：2026-09-15。状态：已完成派发范围的实现与 Windows 验证，待总控复核。本契约只定义阶段 4 P 首片，
不包含 Lab/Qt 迁移、Encode conversion raw、TLV、数组、嵌套、Schema 新能力或稳定 ABI 承诺。

## 1. 目标与事实来源

P 首片把冻结 Plan 中已经存在的 Binary 记录布局以应用无关的公开纯值查询暴露给消费者：

- Message 的 `BINARY` / `ASCII_TEXT` 表示类别；
- Binary 固定或有界记录长度；
- 普通字段的固定/最大 byte range；
- bitfield 在实际 Frame 字节上的非零 mask（最多 8 个）；
- BYTES 值的固定或有界长度；
- integrity 与 computed-length 的存储位置；
- 给定记录长度时，有界 payload 和动态 integrity 的实际范围。

事实只来自成功编译的同一个 `CompiledProtocol` 所持冻结 Plan。查询不解析 Schema，不读取 Frame
内容，不执行 Matcher、Gate、Integrity、Decode 或 Encode，也不证明候选或 Decode 成功。

## 2. 公开纯值类型

`protocol_description.h` 增加：

- `RecordRepresentation { BINARY, ASCII_TEXT }`；
- `PhysicalQueryStatus { OK, INVALID_COMPILED_PROTOCOL, INDEX_OUT_OF_RANGE,
  FRAME_SIZE_MISMATCH, REPRESENTATION_NOT_SUPPORTED, INTERNAL_CONTRACT_VIOLATION }`；
- `ByteRange { offset, length }`、`ByteLengthBounds { minimum, maximum }`；
- `PhysicalBitMask { frame_byte_index, mask }`；
- `kMaximumFieldPhysicalBitMasks == 8`；
- `FieldPhysicalKind { BYTE_RANGE, BIT_MASKS }`；
- Message/Field 静态描述、按长度解析描述及带状态的结果结构。

所有返回结构均为不含私有 Plan 指针、Qt 类型或借用字符串的纯值。可选存储使用
`std::optional<ByteRange>`：`nullopt` 表示不存在，`ByteRange{offset, 0}` 表示存在且长度为零，二者
不得混淆。字段 bit masks 只填充非零 mask，按实际 Frame byte index 升序排列；bitfield 不伪造
“占满容器字节”的 `ByteRange`。

## 3. 查询面

`CompiledProtocol` 增加五个 `noexcept` 查询：

1. `MessageRepresentation(message_index)`：对 Binary 和 ASCII 都可成功，返回表示类别；
2. `MessagePhysical(message_index)`：只支持 Binary，返回记录长度边界、最大/固定 integrity 存储、
   integrity 是否随记录长度移动、computed-length 固定存储；
3. `FieldPhysical(flat_field_index)`：只支持 Binary，返回 byte range 或 bit masks；BYTES 额外返回
   值长度边界；
4. `ResolveMessagePhysical(message_index, frame_size)`：在当前 Message 长度规则下解析实际
   integrity/computed 存储；
5. `ResolveFieldPhysical(flat_field_index, frame_size)`：解析实际 byte range，或原样返回实际物理
   bit masks。

静态描述中的 byte range 是固定范围或有界 payload 的最大范围。动态 integrity 的静态存储范围按
最大记录长度解析，同时以布尔量标明其 offset 依赖记录长度。computed-length 存储在当前能力中是
固定范围；若内部 Plan 不满足该不变量，查询以 `INTERNAL_CONTRACT_VIOLATION` 失败关闭。

## 4. 状态、失败与零长度

- moved-from/空 owner：`INVALID_COMPILED_PROTOCOL`；
- Message 或 flat Field 索引越界：`INDEX_OUT_OF_RANGE`；
- ASCII 的 Message/Field 物理查询：`REPRESENTATION_NOT_SUPPORTED`；表示类别查询仍返回
  `ASCII_TEXT`；
- 固定 Binary 记录必须精确匹配长度；有界记录必须位于 min/max 且满足 header/payload/trailer
  算术：否则 `FRAME_SIZE_MISMATCH`；
- 冻结 Plan 的字段、容器、范围、位宽、存储或 metadata 关联若不自洽：
  `INTERNAL_CONTRACT_VIOLATION`；
- 非 `OK` 结果不包含部分有效 value；消费者不得使用默认构造值猜测布局。

有界 payload 的最小长度允许为零。成功解析零 payload 时返回存在的
`ByteRange{header_length, 0}`。这不是字段缺席，也不产生 Hex 高亮字节。

## 5. 位映射与有界布局

PlanBuilder 已把 `lsb0/msb0` 成员偏移归一为 numeric `bit_mask`；P 查询不再次解释
`bit_numbering`。它只按容器 `byte_order` 把 numeric byte 映射到 Frame byte：little-endian 使用
同序索引，big-endian 使用反序索引。每个非零 byte mask 形成一个 `PhysicalBitMask`，最大 8 项。

有界记录的实际 payload 长度为：

`frame_size - header_length - trailer_length`

计算采用减法式边界检查，先验证 header/trailer，再做减法，禁止整数环绕。动态 integrity storage
按 `frame_size - trailer_length` 锚定；固定字段、固定 integrity 和 computed-length 均须完整落在
本次记录中。

## 6. 所有权、身份与执行关联

查询值可在返回后独立复制，但其中的 Message/Field index 只属于产生它的
`CompiledProtocol`。本片不新增 runtime identity token。消费者必须在同一个成功 Host callback 内，
使用创建该 Host 的同一 compiled owner、callback 的 `message_index` 和 candidate `frame.size` 查询并
复制布局；失败 candidate 不得借布局查询制造成功字段或成功高亮。

查询不改变 compiled owner，也不改变已有字符串 view 寿命：owner move/销毁后应重新获取描述。
调用者不得与同一 owner 的 move/销毁并发。纯值查询自身无隐藏缓存和无堆分配。

## 7. 资源、操作量与模块边界

- 每次 Field 查询最多扫描 Message/Field metadata 和 8 个容器字节；不分配堆内存；
- bit mask 使用固定 `std::array<PhysicalBitMask, 8>`，不新增 Plan、Workspace 或 metadata Arena；
- 不新增逐帧持久状态、Decode、转换或哈希操作；
- 不改变 `CompileMemoryReport`，因为冻结 Plan、sidecar 与 facade 存储均未增加；
- 只修改公开 description/compiler facade 及其测试/示例/文档；不修改 Core、Plan、Loader、Schema、
  Codec、Host、Lab 或 Qt。

## 8. 验证出口

自动化必须使用独立预期覆盖：大小端、`lsb0/msb0`、跨字节和 64-bit masks；fixed 与 bounded
payload 的零/中/最大实际范围；固定/动态 integrity 与 computed storage；无 integrity/computed；
非法 owner/index/长度和 ASCII；重复查询零分配。Host 成功 callback 中查询同 owner/message/frame，
并以 Codec entered hook 证明查询没有触发第二次 Decode。

随后串行验证 Windows x64/v142 Debug/Release 定向 public 回归、Testing-off 与公开头边界，并用新的
`out/sdk-stage4-p/` 候选完成 static/shared D/R 包外消费及一次源码包脱离开发仓库消费。旧 stage 3
final6 不覆盖、不改写；新日志只写入 `out/stage4-p-validation/`。

## 9. 延期和非声明

Encode actual raw、完整数值/Decimal/ASCII 输入约束、Lab H1/H2、旧桥删除、TLV/数组/嵌套、
Runtime/Session、Transport、C ABI/稳定 ABI 均延期。本片 Windows 结果不代表 Linux、真实协议、
Golden、硬件、现场、性能或正式发布通过。
