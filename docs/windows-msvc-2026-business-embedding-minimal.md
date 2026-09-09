# 最小业务嵌入示例Windows验证报告

日期：2026-09-08

状态：`IMPLEMENTED / WINDOWS OFFLINE VERIFIED / PENDING CONTROLLER REVIEW`

基线：`main` / `df43165b5bf83d725bbcc327ee56e1f6783c3aed`

## 1. 实施结论

默认关闭的`PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE`新增一个非安装、非导出的C++17宿主示例。
示例直接使用`CompileJsonToPlan`、不可变`PlanOwner`、Plan作用域`FieldRef`、两份独立
`ExecutionWorkspace`及`DecodeCompleteRecord`/`EncodeCompleteRecord`，没有调用Lab或CLI子进程。

初始化一次性按字符串ID绑定并校验`measurement_rx/measurement_record/temperature/alarm`和
`command_tx/command_record/target`，运行时只使用缓存索引、引用及类型化Measurement/Command。
Decode/Encode失败不调用成功回调；成功回调为同步借用，异步保存或发送必须由宿主复制。

两份公开配置保持业务ID和类型不变，但改变比例、偏移、大小端、位编号、常量位置及记录布局；
同一宿主实现均通过。没有修改Core、Lab、Schema、Evidence、指纹或既有格式，也没有引入新依赖。
命令行演示`main.cpp`内置配置A的RX/TX向量，只应以配置A运行；配置A/B适应性由合同测试验证，
不宣称内置演示可任意替换配置完成硬编码向量检查。

## 2. 八组验收映射

| 组 | 自动化证据 |
| --- | --- |
| 初始化 | 合法配置成功；在可编译配置中改名`alarm`或将BOOL改为UINT64，均在建立实例前拒绝 |
| 正常RX | 独立固定A帧`A1 02 08 01 AC`得到温度12、scale 0、alarm=true，回调一次 |
| 动态TX | A配置下12.5生成`B2 00 19 5A 25`，-5生成`B2 FF F6 5A 01`，各回调一次 |
| 方向隔离 | 合法TX帧投入RX入口得到UNKNOWN_MESSAGE，测量回调不增加 |
| 配置适应性 | B配置独立RX帧`A1 80 01 04 33 59`仍得到相同业务值；两组TX分别为`B2 5A 32 00 3E`、`B2 5A EC FF F7` |
| 失败不交付 | SUM8损坏、未知报文、非法Decimal scale及不可精确反算均返回既有Codec错误，回调数不变 |
| 状态恢复 | 成功→多种失败→再次成功，回调计数只随成功增加，旧结果不冒充新结果 |
| 生命周期 | 回调内复制的Measurement和首个TX字节在后续调用及失败后保持不变 |

预期字节和业务数值直接写在测试中，没有由被测Encode/Decode生成；本检查点没有把往返自洽当作
独立正确性证据。

## 3. Windows实际验证

Debug与Release使用独立Ninja目录并由Visual Studio 2026 Developer PowerShell初始化MSVC。

| 验证 | 结果 | 日志 |
| --- | --- | --- |
| Debug构建 | PASS | `out/be-debug-final-build.log` |
| Debug专项 | 1/1 PASS | `out/be-debug-final-targeted.log` |
| Debug/Release示例运行 | 各PASS | `out/be-debug/example-final-run.log`、`out/be-release/example-final-run.log` |
| Release构建 | PASS | `out/be-release-final-build.log` |
| Release专项 | 1/1 PASS | `out/be-release-final-targeted.log` |
| Testing-off Release构建 | PASS | `out/be-testing-off-release-final-build.log` |
| Testing-off注册 | 0测试 | `out/be-testing-off-release-final-registered.log` |
| Testing-off示例运行 | PASS | `out/be-testing-off-release-final-run.log` |
| Testing-off依赖/目标扫描 | 未发现Lab、Winsock或Qt | `out/be-testing-off-release-final-imports.log`、`out/be-testing-off-release-final-targets.log` |
| 默认关闭目标隔离 | 未注册业务示例Target | `out/be-default-off-release-targets.log` |

主要命令：

```powershell
cmake -S . -B out/be-<config> -G Ninja -DCMAKE_BUILD_TYPE=<Debug|Release> `
  -DPAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE=ON -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=OFF `
  -DPAE_BUILD_PROTOCOL_LAB=OFF -DPAE_BUILD_TESTING=ON -DBUILD_TESTING=ON
cmake --build out/be-<config>
ctest --test-dir out/be-<config> -R '^pae\.examples\.business_embedding\.contract$' `
  --output-on-failure
```

Testing-off使用相同功能开关并将两个Testing开关设为OFF，随后直接运行示例可执行文件。

## 4. 证据边界

- 示例字节回调只表示“待发送数据已生成”，没有传输成功或设备执行语义。
- 宿主实例按串行调用验证；并发、回调重入、通用Session、路由、队列和重试未实现。
- 没有运行Protocol Lab、UDP或其他网络测试；只新增示例和测试，因此未重复无关Lab全矩阵。
- 未验证Linux、Oracle、真实协议Golden、硬件、现场、Qt集成或正式性能，不作生产可用声明。
- 公开配置和预期均为从零合成资料，不包含客户资料、生产端点或现场报文。
