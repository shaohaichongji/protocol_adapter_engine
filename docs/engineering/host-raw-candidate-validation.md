# PAE raw候选观察首片验证

日期：2026-09-13。基线：`dfb08f351cdb22a9b50c9e64e688e3b666e669bc`。
授权：独立实施PAE最小raw候选观察接口及Windows Debug/Release验证。
本记录不代表Commit/Push、Lab Binary接线或人工验收完成。

## 实现边界

- `Candidate::RawIntegerCount()`返回转换整数字段的raw条目数，不是全部字段数。
- `GetRawInteger(index, output)`从已有Core工作区受检拷出`RawIntegerValue`；越界返回false，output不变。
- 仅成功候选且存在observer时绑定私有const工作区指针；失败候选、默认Candidate和无转换结果不暴露旧raw。
- 访问器及返回值中的FieldRef仅在本次回调有效；不提供工作区公开访问、不承诺并发或公共ABI。
- 不改Core/Framer协议语义、Session预算、Lab、Qt或现有配置文件；不增加raw数组或第二次Decode。
  单次Core调用结论来自`DecodeCandidate`源码检查：仍只有原有`DecodeCompleteRecord`调用，两个访问器仅转发读取。

## 回归证据

先将新访问器临时实现为空，运行新增测试，Debug以`FAIL: raw candidate values and bounds`失败；
接入真实只读访问后通过，空实现未保留。

Host测试复用Core公开合成decimal向量，在内存中将Schema 0.5改为Host接受的0.9，
再构造fixed_length流版本；未修改原fixture，也未扩大Host实际Schema接受域。
输入字节为独立固定向量，不通过Encode生成期望。

覆盖：有符号523转换为12.3、UINT64_MAX、INT64_MIN及负比例转换、无转换Binary、ASCII与零字段、
Plan/Message/Field关联、one-past/max-size越界、失败读取不改输出、非法Handle早拒绝、
校验失败屏蔽旧raw、转换溢出屏蔽部分raw、STOP一次业务交付、异常抑制业务、流式34字节消费与后缀、Reset恢复。
现有Host回归同时覆盖注册/预算/重入/多流及ASCII观察语义。

在预建Session上以测试现有`operator new/new[]`计数检查：完整记录无observer、有observer读取、
流式有observer读取均为0次分配。此为针对性分配证据，不是全平台性能认证。

## Windows结果与复现

复用`out/build/windows-msvc-lab-vendored-qt`配置，仅构建以下PAE测试目标及其依赖，未启动Lab。
工具链：Visual Studio 18 2026生成器，x64，v142目录14.29.30133，MSVC 19.29.30159.0。

| 测试 | Debug | Release |
| --- | --- | --- |
| pae.host_endpoint.contract | PASS | PASS |
| pae.protocol_core.decimal_conversion.contract | PASS | PASS |
| pae.protocol_framing.contract | PASS | PASS |
| pae.protocol_framing.ascii_stream_contract | PASS | PASS |

每种配置4/4通过；不视为仓库全量测试。
在仓库根目录分别以Debug、Release替换`<CONFIG>`运行：

```powershell
cmake --build out/build/windows-msvc-lab-vendored-qt --config <CONFIG> --target pae_host_endpoint_tests pae_decimal_conversion_contract_tests pae_protocol_framing_contract_tests pae_ascii_stream_framing_contract_tests --parallel 4
ctest --test-dir out/build/windows-msvc-lab-vendored-qt -C <CONFIG> -R '^(pae.host_endpoint.contract|pae.protocol_core.decimal_conversion.contract|pae.protocol_framing.contract|pae.protocol_framing.ascii_stream_contract)$' --output-on-failure
```

尚未验证：Lab DTO物化/预算/UI、Binary新门禁、人工UI、Linux、硬件及现场。
下一停点：另行授权非Qt Binary适配与自有DTO，先闭合raw关联和内存预算，再推进UI。
