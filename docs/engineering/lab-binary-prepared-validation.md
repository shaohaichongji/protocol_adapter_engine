# Lab Binary 绑定前准入与完整记录身份关联验证

日期：2026-09-13。基线main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc，
保留此前PAE raw、候选物化和自有描述的未提交变更。
用户同意下一步：绑定前接受域检查、候选与描述身份关联；不接UI、不动Qt、不提交推送。

## 实施范围

- `tools/protocol_lab_binary/binary_admission.h/.cpp`：Schema 0.9全Plan扫描，
  包括未绑定Message和所有framing profile/pipeline；校验类型/codec/转换/EncodeSource白名单，
  字段、BYTES最大执行宽度、帧及身份串限额。Compiler仍负责协议参数与组合的合法性，
  Host注册继续复核workspace和Framer能力，不复制CRC/Framer算法。
- `owned_description.h/.cpp`：按受检conversion_index复制自有LinearConversionDescriptor，
  包括精确比例、偏置及冻结系数；不从逻辑小数反算raw。
- `prepared_binary.h/.cpp`：准备对象消费同一个Compiler Artifacts，先完成扫描和描述复制，
  再移交唯一PlanOwner给其私有Session。公开只读描述，不暴露生产Session或可取走Plan的接口。
  绑定检查最多64项/64通道、唯一endpoint+action、存在的pipeline及Encode单通道。
  准备异常只销毁新对象，不对旧对象执行替换。
- 最小`Decode(binding,flow,frame)`只接受完整记录。内部先Observe取得generation，
  再由真实Host调用的同步回调生成身份；只执行一次Host Decode，不经旧ExecutionBridge。
  提供Observe/Reset直通入口；没有Push、冻结后缀、Continue、Encode执行或UI重绑发布逻辑。

## 身份与结果边界

Candidate本身没有binding/flow/operation。因此不能靠给任意Candidate补标量验证来源。
本入口不接受外部Candidate：绑定/flow来自实际Find及Decode调用，generation在调用前取得，
operation由实例内单调计数生成，instance由进程内不回绕的原子计数生成。
tab/load/session修订仍为调用者给出的元数据，尚未与document_tab/UI生命周期接线。
这些计数不是跨进程持久化身份，也不是并发Session承诺；操作仍要求串行。

回调关联先检查精确live Plan指针，再检查Message属于绑定Pipeline，失败候选若有Message索引
同样检查成员关系。未知Message可保留原帧诊断，不生成字段或成功高亮。
已知ENUM按字段索引、项ID/raw关联显示文本，存于`presentation.enum_display_text`；
底层`CandidateResult`仍保留其独立物化语义，未知ENUM显示文本保持空。
成功高亮先检查实际帧长，再联合核对字段ID及BYTES长度；返回自有byte range/bit mask和动态校验位置。
结果不含Plan/FieldRef等借用引用，销毁准备对象后仍可读。

复制或关联异常由Host回调失败路径处理：不发布候选、抑制本次成功sink计数并要求Reset。
正常观察STOP仍保留一次成功sink交付；这是内部无业务副作用sink，不代表外部系统处理完成。
测试专用TestPlan/TestAssociate受TEST_HOOKS编译门禁控制，生产配置不提供；探针本身不是Host调用。

## 预算

沿用既有默认OFF非Qt目标及局部硬限额。新增展示容器、mask、ENUM显示文本纳入关联结果容量账本；
复制前检查合并字符串保守预算与总预算，复制后按实际capacity复核。
独立审查发现的“显示文本仅复制后核验string上限”已补复制前检查，并由收紧预算测试覆盖。

Create通过不等于整体内存准入通过：尚未累计Plan、Session、bindings、自有描述、源sidecar共存、
旧新实例与UI副本。`copy_peak_bytes`不含allocator/debug proxy、调用者optional槽位和全部调用栈，
不是RSS/OOM保证。128MiB实例及256MiB重绑峰值仍为下一阶段门禁，不可据此宣称闭合。
输入Artifacts必须来自Compiler正常编译，不保证识别人为拼接的同形异源Plan/sidecar。

## Windows验证

环境：VS 2026生成器、x64、v142工具集14.29.30133。
`out/build/windows-msvc-binary-materializer-noqt`缓存确认UI=OFF、TESTING=ON。
新增`pae.protocol_lab_binary.prepared`覆盖：

1. 完整记录只观察一个候选并保留一次成功sink计数；转换说明、已知ENUM显示及byte/bit联合映射。
2. 两个Flow同为generation=0仍有不同真实flow身份；单Flow Reset后的generation递增及另一Flow不变。
3. 同JSON独立编译Plan拒绝、同Plan跨Pipeline失败Message拒绝、generation不符拒绝（测试探针）。
4. 未知Message和已匹配SUM8失败均无成功高亮；已知/未知ENUM显示区分。
5. 新实例身份不同；结果在原实例/Plan销毁后仍可读。
6. 未选Message超字段/BYTES预算仍拒绝准备；重复绑定、缺pipeline、ASCII Schema拒绝。
7. 三种Binary流策略的全Plan扫描及Host注册通过；完整记录入口对流绑定返回WRONG_INPUT_KIND。
8. 有界payload长度0至3、动态校验位置及BYTES长度联合检查。
9. 收紧string预算导致CALLBACK_FAILED、无候选发布、无成功sink交付，并要求Reset。

最终Debug/Release各7/7通过：准备关联、描述、物化、Host、Core小数转换、Binary Framer、ASCII Framer。
最后一次构建未产生本次新增编译警告。测试使用显式Check，不依赖Release会取消的assert。
另在`out/build/windows-msvc-binary-materializer-production`（UI=OFF、TESTING=OFF）构建
Release `pae_protocol_lab_binary_materializer`成功，测试专用接口不进入该配置。

仓库根目录复验（Release将两处Debug替换）：

```powershell
cmake --build out/build/windows-msvc-binary-materializer-noqt --config Debug --target pae_binary_prepared_tests pae_binary_owned_description_tests pae_binary_candidate_materializer_tests pae_host_endpoint_tests pae_decimal_conversion_contract_tests pae_protocol_framing_contract_tests pae_ascii_stream_framing_contract_tests --parallel 4
ctest --test-dir out/build/windows-msvc-binary-materializer-noqt -C Debug -R '^(pae.protocol_lab_binary.(materializer|description|prepared)|pae.host_endpoint.contract|pae.protocol_core.decimal_conversion.contract|pae.protocol_framing.(contract|ascii_stream_contract))$' --output-on-failure
```

本轮未做整配置全量测试、UI人工验收、网络或硬件验证；未Stage/Commit/Push。
下一步：先闭合总预算，再串行补双流Submit/Continue/冻结后缀与Encode；复核稳定后才接UI。
