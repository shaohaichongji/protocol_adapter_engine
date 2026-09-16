# Lab Binary 自有描述与物理范围基础件验证

日期：2026-09-13。用户在候选物化子片之后授权继续推进。
本次为独立非Qt基础件，不是完整Binary Host Adapter或Lab人工验收。
PAE raw接口和上一物化子片的证据分别保留在对应历史验证记录中。

## 本次改动

- 新增`tools/protocol_lab_binary/owned_description.h/.cpp`：从同一个
  `CompiledUiArtifacts`复制Schema 0.9协议、Pipeline、Message、字段及枚举的ID、显示文本、
  描述和来源；输出只含自有字符串、容器和数值，不保存Plan/sidecar借用引用。
- 完整遍历结构，校验索引、连续sidecar区间、字段/枚举计数、执行字段布局和长度范围。
  位mask从冻结执行Plan映射到物理字节，支持大小端及跨字节位域，不从逻辑值猜位置。
- 固定帧必须长度精确相等；有界帧必须同时满足frame/payload上下界及header/trailer范围。
  payload为零时返回length=0，不高亮；动态校验存储位置按实际payload末尾解析。
  字段范围查询拒绝非当前Message容器内的字段引用。
- `IsActualFrameValid`是无分配的位高亮长度前置检查，不证明Core成功或候选身份。
  `ResolveActualFieldRange`对bit字段返回nullopt，bit高亮读取自有`physical_bits`。
- 更新该内部目标CMake，显式链接非QtCompiler；新增独立描述测试目标。
  继续复用默认OFF的`PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER`门禁，不打开UI。

## 局部预算与所有权边界

描述默认上限4MiB，全部Message合计字段上限1024、帧上限65536字节、身份串上限256字节；
调用者只能收紧。复制前做完整结构及受检加乘预算预检，字符串保守计费`2*size+32`；
复制完成后按DTO对象、vector capacity及string capacity+1核算，超限整次抛异常。
SSO字符串也计费，因此存在有意保守重复计费；预检可能拒绝实际可放入的小额预算。

这不是进程RSS或绝对分配峰值证明：不含源Artifacts、allocator/debug proxy、调用者保留的旧DTO、
Plan、Session及冻结后缀。尚未闭合128MiB实例/256MiB重绑预算。
仅消费Compiler正常产生的成对Artifacts；不声称检测人为用公开工厂拼接的同形异源Plan/sidecar。
描述应视为冻结自有数据，不支持调用者任意修改后仍保持校验保证。

## 验证结果

环境：Windows x64，VS 2026生成器，v142 14.29.30133工具集。
目录：`out/build/windows-msvc-binary-materializer-noqt`，缓存确认UI=OFF、TESTING=ON。
新增`pae.protocol_lab_binary.description`覆盖：

1. 在内存将公开v05/v06/v07/v08合成fixture的Schema版本替换为0.9后编译；原JSON不改。
2. Compiler Artifacts销毁后，自有协议文本、字段source_ref、枚举ID/显示文本/raw仍可读。
3. BE lsb0相邻及跨字节mask、LE msb0跨字节mask、固定byte范围及外部字段拒绝。
4. CRC-16存储范围、computed length范围；0至3字节payload的全部实际长度及动态SUM8位置。
5. 固定/有界帧过短或过长拒绝，bit映射实际长度前置检查，零payload空范围。
6. 描述/字段/帧/身份限额过小、放大硬上限、错误Schema、缺Plan和缺sidecar拒绝。

最终Debug与Release各6/6通过：描述、候选物化、Host契约、Core小数转换、Binary Framer、ASCII Framer。
使用显式Check而非assert，Release不取消断言。最终构建无本次新增编译警告。

复验命令（仓库根目录，两个配置分别执行）：

```powershell
cmake --build out/build/windows-msvc-binary-materializer-noqt --config Debug --target pae_binary_owned_description_tests pae_binary_candidate_materializer_tests pae_host_endpoint_tests pae_decimal_conversion_contract_tests pae_protocol_framing_contract_tests pae_ascii_stream_framing_contract_tests --parallel 4
ctest --test-dir out/build/windows-msvc-binary-materializer-noqt -C Debug -R '^(pae.protocol_lab_binary.(materializer|description)|pae.host_endpoint.contract|pae.protocol_core.decimal_conversion.contract|pae.protocol_framing.(contract|ascii_stream_contract))$' --output-on-failure
```

将两处Debug替换为Release复验Release。另在
`out/build/windows-msvc-binary-materializer-production`（UI=OFF、TESTING=OFF）构建
Release `pae_protocol_lab_binary_materializer`成功；这只是库构建，不是生产运行或UI验证。

## 下一停点与未验证事项

以下为本子片完成时的边界快照；后续准入、转换说明与完整记录联合关联进展见
[准备与关联验证](lab-binary-prepared-validation.md)。

下一步先闭合绑定前完整接受域扫描和候选/描述的同Plan、Message及修订身份关联，补转换说明元数据。
当前枚举显示文本在描述中，尚未自动合入Candidate DTO；范围也尚未与候选BYTES长度进行联合校验。
上述完成后，再串行实现双流Submit/Continue/Reset、Encode及总预算报告，最后接UI与人工验收。
本片未执行第二次Decode，未触碰Qt副本或本机Qt环境，未改Core/Framer协议语义。
未进行UI、硬件、网络或整配置全量验证；未Stage、Commit或Push。
