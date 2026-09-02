# Config Compiler Expected Results

本目录记录`V0.1 DRAFT SLICE / INCOMPLETE`的预期语义和已接入的机器快照。

- `synthetic_lab_exchange_slice.canonical.json`：由稳定ID表达、被CTest逐字节比较的机器权威快照；
- `synthetic_lab_exchange_slice.plan.json`：包含来源标签和区间说明的人工可读语义参考，不参与逐字节比较；
- `diagnostics_v0.1.tsv`：合法/非法语料的人工可读诊断镜像，机器权威是`../config_compiler_contract_tests.cpp`中的断言。

当前Domain负例包含字段越界、字段重叠，以及Field与Fixed Matcher区间并集未覆盖完整帧的`FRAME_NOT_FULLY_DEFINED`；这些配置必须在冻结Plan之前失败，不能推迟到逐帧Encode路径。

当前错误枚举和快照格式都只属于内部草案切片，不是已经冻结数值或结构的公共API（Application Programming Interface，应用程序编程接口）。
