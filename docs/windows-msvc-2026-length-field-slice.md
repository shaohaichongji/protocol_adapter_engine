# 固定完整记录长度字段 Windows 验证报告

日期：2026-09-09。代码基线：`e5a5e4d`，结果来自未提交工作树。
总控定向复核已关闭P2与验收缺口，候选范围和文档链接检查完成；当前准备分组暂存与提交信息。

## 实施范围

当前工作树在显式默认关闭的Schema 0.7能力门下，实现每Message至多一个1/2/4字节UINT64
计算长度字段。Compiler和Builder分别推导/复核frame或region固定字节数；Frozen Plan保存字段
索引、存储、字节序、范围和期望值。Core Encode拒绝业务覆盖，先写长度再生成完整性值并最终
重读；Decode在结构唯一及容量检查后先验证长度。Lab使用Result/指纹0.8、Record 0.9、Event 0.7。

公开样例全部标记`SYNTHETIC_FROM_SCRATCH`，不含真实协议、端点或现场报文。业务嵌入第三份配置
证明宿主不提供长度输入，长度失败不触发业务回调；它不新增网络、Session或稳定公共API。

## 自动化映射

- 正向字节：frame/region，1/2/4字节，2/4字节大/小端；固定Frame预期独立写定，包含
  2字节小端Decode及4字节大端Encode/Decode。
- 范围与顺序：region非空/溢出边界、允许region包含自身存储且按固定范围计数、拒绝同Message
  多个computed length；长度与CRC共存，双错时先报`LENGTH_MISMATCH`，CRC单错仍报
  `INTEGRITY_FAILED`；结构多候选时`decode_calls=0`。
- 配置与冻结：旧Schema拒绝、非法宽度/字节序/未知属性、范围错误、Matcher冲突；Builder对
  派生值、scope枚举、Schema代次及资源数量四类损坏Draft失败关闭；1字节长度255可编译并
  Encode/Decode，256以`VALUE_NOT_REPRESENTABLE`在`/messages/0/fields/0/computed`拒绝。
- 运行失败：computed输入覆盖、长度不符、最终重读故障均零交付；正常成功可在失败后恢复。
- 资源：`total_computed_length_count`精确为3，Plan的冷/热描述均冻结；首次Encode/Decode的
  可替换`new/new[]`增量为0，不增加长度专用Workspace槽。该证据不是正式性能结论。
- Lab：成功Run、失败Run、Replay A、Replay B均实际读取；精确断言格式、模式、主体、状态、
  字段身份、Frame、指纹和比较状态；原Bundle Record/Result Hash在Replay前后不变；跨代Run拒绝。
- P2证据绑定：合法计算字段覆盖失败可Compare并Replay；缺少输入索引、索引越界、索引指向其他
  Values字段的三类副本均先重算Result指纹、Record长度/Hash和清单Hash并通过低层自洽预检，
  再按各自精确语义诊断退出3拒绝。计算字段即使携带错误Values类型也先按禁止覆盖失败，合法
  Bundle仍可Compare，证明修复没有把类型检查提前。

## 实际命令与结果

构建使用VS 18 2026生成器、x64、MSVC 19.51.36256.0。主构建目录为
`out/build/windows-msvc-length-lab`，显式开启Schema 0.5/0.6/0.7 Compiler与相应Lab门。

```powershell
cmake --build out/build/windows-msvc-length-lab --config Debug
ctest --test-dir out/build/windows-msvc-length-lab -C Debug -LE 'udp|loopback|network' --output-on-failure
cmake --build out/build/windows-msvc-length-lab --config Release
ctest --test-dir out/build/windows-msvc-length-lab -C Release -LE 'udp|loopback|network' --output-on-failure
```

初次实现批次Debug、Release最终均39/39通过（不是P2修复后重跑）。该批次最后补充region自包含、重复规则和失败后恢复断言后，长度Core
专项在Debug/Release又各1/1通过。业务嵌入单测在独立目录Debug/Release各1/1通过。Product-only
及Lab-on/Testing-off目录均完成Debug/Release构建，`ctest -N`各显示`Total Tests: 0`。

日志位于`out/validation/length-field-20260909/`：

- `final-debug-build.log`、`final-debug-offline-ctest.log`；
- `final-release-build.log`、`final-release-offline-ctest-rerun.log`；
- `final-targeted-{debug,release}-{build,ctest}.log`；
- `final-business-{debug,release}-{build,ctest}.log`；
- `product-only-*.log`、`lab-no-tests-*.log`。

第一次Debug全矩阵为38/39：旧C3负例把0.5改成当时未知的0.7；Schema 0.7实现后该输入合法。
负例改为真正未知的0.99后复跑39/39。首次Lab-on/Testing-off配置命令遗漏显式Loader/Core开关，
CMake依赖门禁按预期拒绝；补齐开关后双配置构建成功。两者均保留在日志中，不冒充产品失败。
最终Release全矩阵第一次启动两个新生成的测试EXE时遇到Windows瞬时`operation not permitted`；
两个目标单独复跑2/2通过，随后同一Release矩阵完整复跑39/39通过。瞬时失败及复核分别保存在
`final-release-offline-ctest.log`、`final-release-transient-rerun.log`和上述最终全量日志中。

总控P2限定修复没有重跑无关39项矩阵。重建`pae_length_field_contract_tests`和
`pae_protocol_lab`后，串行运行以下受影响离线集合，Debug、Release各4/4通过：

```powershell
ctest --test-dir out/build/windows-msvc-length-lab -C Debug -R '^(pae\.config_compiler\.contract|pae\.protocol_core\.length_field\.contract|pae\.tools\.protocol_lab\.length_schema_contract|pae\.tools\.protocol_lab\.length_cli)$' -LE 'udp|loopback|network' --output-on-failure
ctest --test-dir out/build/windows-msvc-length-lab -C Release -R '^(pae\.config_compiler\.contract|pae\.protocol_core\.length_field\.contract|pae\.tools\.protocol_lab\.length_schema_contract|pae\.tools\.protocol_lab\.length_cli)$' -LE 'udp|loopback|network' --output-on-failure
```

本轮日志为`p2-{debug,release}-build.log`和`p2-{debug,release}-affected-ctest.log`。
补入错误类型优先级对照后，`length_cli`在Debug/Release又各1/1通过，日志为
`p2-{debug,release}-length-cli-final.log`。
修复前独立副本在Debug/Release均退出0/EQUAL；双配置重建后均以
`PAE_LAB_C3_COMPARE_EVIDENCE_INVALID`退出3，证据分别见
`out/review/length-readonly-20260909/recovery-independent-repro.log`、
`p2-after-both-builds-repro.log`及对应`probe-*`目录。中间一次只重建Debug的结果按实际保留，
不作为双配置完成证据。

## 边界

本次没有执行任何网络测试（包括Loopback），也没有执行Linux、真实协议Golden、硬件、现场、
人工Lab或正式性能验证。旧代回归来自同一离线矩阵和既有固定夹具，未重生成旧指纹或改写历史
Evidence Bundle。总控定向复核及51个候选的集中检查已完成，未发现新的范围内提交阻断项。
本次文档收口仅澄清验证批次与命令，不运行测试；当前进入分组暂存与提交信息准备，尚未Commit或Push。
