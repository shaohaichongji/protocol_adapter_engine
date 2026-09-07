# DEC-042B 隔离精确算术 Spike（可行性实验）

本目录仅用于验证固定256位幅值与显式符号的候选，不是生产Core、稳定API或通用大整数库。
不被根CMake引入，无第三方依赖，无Schema、Lab、网络或设备接入。

- `arithmetic_candidate.h`：8个32位单元的加减乘除、参数约分、精确Decode/Encode。
- `arithmetic_tests.cpp`：手算向量、受限原生整数参考、全宽算术不变量及分配计数。
- `CMakeLists.txt`：独立C++17测试程序。

Decode/Encode只接受本候选Compile成功产生且未经修改的Plan；辅助函数有内部前置条件，
例如Power10指数不超过18。这不是未来生产冻结Plan、参数防御或资源计费方案。
候选采用局部结果后发布；不能据此推断生产字段槽、输出Buffer或完整报文事务语义。

运行命令、上界推导、实际结果与限制见
[Windows隔离验证报告](../../docs/windows-msvc-2026-dec042b-arithmetic-spike.md)。
