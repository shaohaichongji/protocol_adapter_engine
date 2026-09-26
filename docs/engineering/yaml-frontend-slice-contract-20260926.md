# YAML 非 Qt 前端最小实现片

## 授权、依据和状态

2026-09-26 用户同意下一步：总控复核资源探针关键源码/日志后，限定实施非 Qt 前端和真实公开 API 对照。
基线 main@6b2e3ee；此前计划、Profile、探针及报告均保留，不提交、不覆盖历史证据。
总控抽查了局部 allocator、语义检查、emit 路径，读取 D/R 语义日志和 Release 结构对照，
现场复算 rapidyaml 单头 SHA256 与报告一致；未重跑探针或声称完整审计。

本片选择 rapidyaml 0.16.0 作为实现候选，不等于正式 SDK 依赖或发布批准。
原始头文件仍使用隔离 out 下已验证副本，通过显式 CMake 路径参数与固定 SHA256 校验消费。
不新增下载回退，不修改上游，不进入正式 third_party；后续打包片再固化随仓依赖和许可。

## 实现位置和依赖边界

- src/config_frontend_yaml/：可选、纯 C++17 内部前端，包含接口头、实现及自身 CMake；不包含 Qt、Windows 或业务代码。
- tests/config_frontend_yaml/：独立 CMake 测试入口、用例和公开合成夹具。
- 独立测试入口允许把仓库作为子项目构建公开 PAE target（关闭 Lab 与无关测试），不修改根 CMake/preset。
- 新前端仅输出严格 JSON 和来源映射，不解析 PAE 领域规则；测试调用现有 CompileProtocolJson 和 Codec。
- 不修改 include/pae、公共 ABI、原 JSON 编译器、Plan/Core/Host/Framer、Lab、现用 SDK 或部署。
- 本片接口是内部候选，不导出、不安装、不冒充 SDK 新能力。禁止直接把 Windows 资源探针当产品实现复制。

## 输入、输出与失败契约

1. 接受借用的 UTF-8 YAML 字节视图，在调用内使用；无文件读取/网络/include/环境变量展开。
2. 遵循 schema/pae_yaml_profile_v0.1.md。实现显式类型分类和有界 JSON writer，不依赖 ryml emitter 的默认推断。
   引号字符串保持字符串；Plain null/true/false 明确转换；数字不用 double；非字符串键与解码后重复键拒绝。
   数字外观判定、大小写布尔/null、.inf/.nan、块标量/转义、版本指令空白/注释的具体接受规则须在 Profile 明确并测试。
   仅作词法澄清，不擅自扩大语法或静默纠正非法配置。无法保持已确认边界时停报。
3. 输出为自有 JSON、来源记录及明确成功/失败状态。失败不得返回可用的部分 JSON/映射，不调用 PAE。
   顶层捕获分配失败与解析失败；错误状态本身不依赖必定成功的动态分配，无跨 C 边界异常、abort 或吞错误。
4. 每个值/容器关联 RFC6901 JSON Pointer 与 YAML 一基行列；键和值位置可区分。
   数组索引、键中 / 与 ~、中文、转义、根节点都须测试。映射为节点起点，不声称逐字符精确定位。
   缺失属性可明确回退最近容器并标明近似，否则未映射；不改原 CompileDiagnostic 或伪造 byte_offset。
5. JSON writer 保持确定性输出，同一原文/选项重复转换字节一致；不要求不同排版 YAML 的配置哈希必然一致。
   来源记录必须自有，不能悬挂在被销毁 Parser/Tree/input 上。

## 资源模型

本片不承诺进程 RSS/CRT/线程栈/异常运行时的精确总峰值。分别约束以下前端可控部分，报告实测而不混同逻辑长度与分配容量：

- 输入字节上限，在 Parser 前检查；调用方已有缓冲不计入前端自有内存。避免无意义双份输入副本。
- 每调用 Parser/Tree/arena 回调硬预算，包含自有计费头的口径须明确，溢出安全、无全局可变计数。
- 辅助结构及来源映射的显式容量/分配预算；reserve、扩容临时峰值不能以 size 冒充，避免无界重复键列表或递归栈。
- JSON 输出增长前检查容量预算，禁止 emitrs_json 完整生成后才拒绝；返回自有数据的资源管理器寿命须安全。
- 独立节点、深度、单标量上限；区分解析后语义门禁与解析期间内存预算，不冒称 Parser 期间深度硬拒绝。

内部 Limits 可调；执行者根据现有公开夹具给出保守默认值及依据，写入报告，暂不作为生产资源档位。
预算失败与真实分配失败区分，测试成功、失败、重复调用和并发实例后的资源恢复。

## 最低验证和交付

1. Windows x64 Debug/Release 串行：前端正负例、UTF-8、类型/重复键/禁用语法、来源映射、预算边界及故障注入。
2. Binary CRC 与 ASCII 至少各一组：原 JSON 和 YAML 转换结果分别调用真实 CompileProtocolJson，
   核对关键公开 metadata 和已知 Decode/Encode 输入输出，不仅比较 PowerShell 对象。
3. 下游 Schema 错误保留原诊断并关联 YAML 节点位置；转换失败不得调用下游。
4. JSON-only 构建不引入前端/ryml；不重跑无关全量、UI 或 SDK 打包。新证据根不覆盖旧日志。
5. 交付 docs/engineering/yaml-frontend-slice-validation-20260926.md，说明构建参数、源码/依赖身份、测试结果、预算口径及未验证。

《子任务推进》独占上述两个代码目录、validation 报告及 Profile 必要澄清；原探针不修改。
总控独占本契约/计划/索引，Lab、工程整理停止。输出独占 out/build/yaml-frontend-slice-20260926 与
out/evidence/yaml-frontend-slice-20260926。公共文档仅相对路径，机器原始日志留 out。
超范围接口/根构建/依赖改动先停报；完成后主动向总控反馈一次完整交接，停止写入等待复核。
无 Stage/Commit/Push、删除、发布或部署替换授权；Lab/可选打包后续单独派发。
