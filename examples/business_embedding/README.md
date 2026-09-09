# 最小业务嵌入示例

该目录展示一个非安装、非导出的C++17宿主如何直接调用Config Compiler、不可变Plan和
COMPLETE_RECORD Core。它不调用Protocol Lab或CLI子进程，也不拥有Socket、线程、Qt、重试、
设备生命周期或业务状态机。

## 调用关系

`BusinessAdapter::Initialize`编译配置，按固定业务ID找到RX/TX Pipeline、Message和Field，校验
Decimal64/BOOL业务类型并缓存Plan作用域`FieldRef`。实例持有一份`PlanOwner`以及RX/TX各一份
`ExecutionWorkspace`；Plan的声明顺序保证其晚于Workspace销毁。

`OnReceivedRecord`只接收一条完整记录。Decode成功且缓存引用、类型均匹配时，才同步调用一次
`on_measurement`。`SendCommand`把类型化Decimal64目标值交给Encode；成功并得到完整字节后，才
同步调用一次`on_bytes_ready`。待发送字节只是宿主可复制的数据，不表示已经发送或设备执行成功。

回调参数只保证在回调期间有效。需要排队、异步处理或发送时，宿主必须在回调内复制业务对象或
字节。示例按同一实例串行调用，不承诺并发或回调重入。

## 两份公开合成配置

- `config/synthetic_business_device_a.pae.json`：RX温度比例`1/10`、告警使用LSB0；TX目标比例
  `1/2`并使用大端布局。
- `config/synthetic_business_device_b.pae.json`：保持业务ID和类型不变，改变RX/TX偏移、比例、
  位编号、大小端和常量位置。同一宿主代码无需改变。

配置与测试报文从零人工构造，不来自客户协议、生产端点或现场报文。测试中的预期字节为独立
手算常量，不由被测Codec生成。

## 构建与运行

示例默认关闭。演示程序`main.cpp`目前内置配置A的RX输入和TX预期向量，因此下面的命令行演示
必须使用`synthetic_business_device_a.pae.json`。A/B两份配置在不修改`BusinessAdapter`时保持
业务语义一致的能力，由`tests/business_embedding`合同测试验证；不能据此宣称这个内置演示程序
可任意替换配置后仍完成同一组硬编码向量检查。

启用Schema 0.5 Compiler、Loader切片及示例开关：

```powershell
cmake -S . -B out/business-example -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DPAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE=ON `
  -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF
cmake --build out/business-example
out/business-example/examples/business_embedding/pae_business_embedding_example.exe `
  examples/business_embedding/config/synthetic_business_device_a.pae.json
```

成功输出`BUSINESS_EMBEDDING_EXAMPLE=PASS`。该结果只验证本地合成完整记录调用，不代表网络、
硬件、真实协议Golden、设备执行或生产验收。
