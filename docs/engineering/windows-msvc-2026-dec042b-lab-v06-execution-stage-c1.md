# PAE-DEC-042B Lab 0.6执行桥接C1阶段Windows验证报告

日期：2026-09-08
状态：`IMPLEMENTED / VERIFIED IN ISOLATION / PENDING REVIEW`

## 1. 范围

本批次在接管基线`761ff7fb2f11c66089f0d8385d92c3022d3b911b`上实现默认关闭的C1内部执行
桥接。它连接Schema 0.5 Compiler、Frozen Plan、Complete Record Core与A阶段Result 0.6内存模型，
不连接B阶段Evidence文件读写、普通Protocol Lab CLI、Replay/Compare、Winsock或网络。

本报告记录本次实际Windows执行；此前契约静态复核和A/B历史测试不充作C1运行证据。

## 2. 实现和断言

- 准备层：严格编译配置并限定Schema 0.5；Values 0.4继续使用A阶段严格解析器，C1单独按历史规则
  解析0.1～0.3；绑定Pipeline、Message、Field、Enum。配置、版本、语法和未知引用失败均断言
  主Codec为0次、无Result、无Frame。
- Core层：已知引用保持作者顺序交给Core。常量覆盖、重复、缺失、类型及数值不在桥接层重排；测试
  精确检查失败status、conversion error、field/value identity及空字段。
- Inspect：逐Pipeline只做结构匹配；零候选和跨Pipeline多候选均为0次Decode且无Result，Pipeline
  顺序变化不影响歧义；唯一候选只Decode一次。
- Encode：主Encode成功后使用独立Workspace对实际输出Decode一次，随后复制0.6字段及raw；主调用
  失败不复核。复核Decode失败、Message不一致、raw关联失败和内部物化失败均不交付Result/Frame，
  同时保留主Encode实际为OK的内部事实。
- 数据与生命周期：公开固定34字节向量独立断言Encode和Inspect；覆盖规范Decimal、等价Decimal、
  负比例、宽数抵消、INT64/UINT64边界、SUM8先于转换、RAW_NOT_INTEGRAL、RAW_OUT_OF_RANGE、
  LOGICAL_OUT_OF_RANGE、非法scale，以及无转换0.5中的ENUM/INT64/UINT64/BOOL/BYTES。字段按冻结
  顺序，自有Result在Workspace复用和桥接销毁后不变。

动态输出容量不足没有通过C1入口制造：桥接按Frozen Plan精确分配完整槽和Frame。该错误顺序、首次
调用无分配及共享Plan并发由本次19项矩阵中的既有Core测试覆盖；不把这些证据升级为Lab全链零分配
或性能结论。

## 3. 实际命令和结果

Debug与Release使用不同构建目录并严格串行。共同配置要点：

```text
cmake -S <repo> -B <out>/dec042b-lab-c1-{debug|release} -G Ninja
  -DCMAKE_BUILD_TYPE={Debug|Release}
  -DPAE_BUILD_TESTING=ON
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON
  -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON
  -DPAE_BUILD_LAB_V06_FORMAT_TESTS=ON
  -DPAE_BUILD_LAB_V06_EXECUTION_TESTS=ON
cmake --build <build>
ctest --test-dir <build> -R ^pae\.tools\.protocol_lab\.v06_execution$ --output-on-failure
ctest --test-dir <build> --output-on-failure
```

结果：

| 配置 | C1专项 | 隔离完整矩阵 |
| --- | --- | --- |
| Debug | 1/1通过 | 19/19通过 |
| Release | 1/1通过 | 19/19通过 |

最终日志位于被忽略的`out`目录：

- `out/dec042b-lab-c1-debug-targeted-final.log`
- `out/dec042b-lab-c1-debug-ctest-final.log`
- `out/dec042b-lab-c1-release-targeted-final.log`
- `out/dec042b-lab-c1-release-ctest-final.log`
- 对应`*-configure.log`、`*-build.log`和`*-rebuild.log`

隔离构建：

- Product-only Release：Configure/Build通过，`ctest -N`为0测试。
- 普通Lab-on/Testing-off Release：Configure/Build通过，`ctest -N`为0测试；仅构建，未执行Lab。
- C1启用但Testing关闭：Configure按预期失败。
- C1启用但Loader/Core/Schema 0.5条件缺失：Configure按预期失败。
- Schema 0.5与普通Lab同时启用：既有门禁按预期失败。

对应日志：`out/dec042b-c1-product-only-*`、`out/dec042b-c1-lab-on-testing-off-*`和
`out/dec042b-c1-gate-*.log`。

## 4. 边界

- 未实现或验证C2 Evidence复现、C3专用CLI、Replay/Compare及事件语法；未改B Reader/Writer。
- 未执行任何网络或UDP测试；未验证Linux、独立高精度Oracle、真实协议Golden、硬件或现场。
- 测试只使用公开从零合成配置和既有公开Core向量，不含私有协议、客户信息、生产端点或原始证据。
- C1内部桥接及测试目标不构成稳定公共API，整体DEC-042B尚未取得提交或生产审批。

## 5. C1两项P2审查纠错

### 5.1 修复前证据

P2-1已动态复现。将最终兼容正例加入原实现后，Debug专项为0/1、CTest退出8；Values 0.1、0.2、
0.3三个Schema 0.5合法正例均未获得Result。日志：
`out/dec042b-c1-p2-values-before.log`。根因是C1文本入口无条件调用仅接受0.4的A解析器。

P2-2为静态确认的测试缺口：原测试只有非法Pipeline导致的展示Decode失败，以及Decode成功后的
物化故障，不能证明展示Decode自身的INTERNAL_ERROR语义；没有把该静态判断写成运行复现。

### 5.2 限定修复和精确断言

- 新增C1内部兼容解析器。0.4仍调用A解析器；0.1～0.3独立按历史语法读取，不修改输入版本文本，
  不链接旧Lab。合法正例分别覆盖共同的UINT64/BYTES/ENUM、0.2 BOOL和0.3 INT64；Result仍由
  Schema 0.5决定为0.6。负例精确断言0.1 BOOL、0.2 INT64、三代DECIMAL64、未知版本、JSON Number
  及重复ID的准备诊断，且Codec调用为0。
- 在C1测试专用Hook中，主Encode成功后调用既有Core `FailNextDecimalConversionOnce()`，使故障只在
  展示Decode发生。精确断言主状态OK、展示状态INTERNAL_ERROR、Encode/展示Decode各1次、无Result
  和Frame；非法Pipeline对照的展示状态为INVALID_ARGUMENT。Core产品目标和公开接口未修改。

### 5.3 本轮实际复核

```text
cmake --build out/dec042b-lab-c1-{debug|release} --target pae_protocol_lab_v06_execution_tests
ctest --test-dir out/dec042b-lab-c1-{debug|release}
  -R ^pae\.tools\.protocol_lab\.v06_execution$ --output-on-failure
ctest --test-dir out/dec042b-lab-c1-{debug|release} --output-on-failure
```

| 配置 | C1专项 | 隔离矩阵 |
| --- | --- | --- |
| Debug | 1/1通过 | 19/19通过 |
| Release | 1/1通过 | 19/19通过 |

本轮最终构建日志：`out/dec042b-c1-p2-debug-build-final.log`、
`out/dec042b-c1-p2-release-build-final.log`。最终测试日志：
`out/dec042b-c1-p2-debug-targeted-final.log`、
`out/dec042b-c1-p2-debug-full-final.log`、`out/dec042b-c1-p2-release-targeted-final.log`、
`out/dec042b-c1-p2-release-full-final.log`。A纯格式测试包含在19项矩阵中并通过；旧Lab解析器未修改，
未重跑其离线矩阵。静态链接命令确认C1目标未引入旧`protocol_operations`、B Evidence或Winsock。
