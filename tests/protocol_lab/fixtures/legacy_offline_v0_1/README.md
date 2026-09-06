# Legacy Offline V0.1 Bundle Fixture

该夹具源自Git基线`35ecbccc`对应程序在2026-09-04生成的公开Synthetic `inspect` Bundle：
`run_20260904T174137Z_b0023c1efb4e175f`。

为避免固化机器路径，`result_summary_v0.1.json`中的`evidence_bundle`被规范化为空字符串；
配置复用仓库公开样例`examples/config/synthetic_lab_exchange_slice.pae.json`，原始Frame保持
`0D00A731563412DEADBEEF0200`不变。测试物化夹具时恢复配置、二进制Frame、空`COMPLETE`和
`SHA256SUMS`；Run Record Payload Hash对应规范化后的静态夹具文件。

该变换不增加V0.1字段，不改变旧Record、Result或Event的结构形状。夹具仅包含从零设计的公开
Synthetic数据，不包含客户协议、生产Endpoint、用户身份或机器绝对路径；原始历史产物不修改。

JSONL使用`.gitattributes`强制LF换行。测试在刷新任何Payload Hash之前，通过完整Bundle读取
核对静态Record中的长度和SHA-256；CRLF转换与等长Event内容篡改均必须被拒绝，不能通过
重新计算Hash掩盖夹具漂移。测试程序支持`--legacy-offline-only`，仅执行旧版兼容性及夹具
负向测试，不创建Socket；运行工作目录为构建树的`tests/protocol_lab`。
