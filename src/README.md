# Source

当前源码包含三个相互分层的内部目标：

- `pae_protocol_plan`保存PAE（Protocol Adapter Engine，协议适配引擎）自有的内部草案载荷、`BudgetedPlanDraft（已预算计划草案）`、`PlanBuilder（计划构建器）`、不可变`PlanBundle`及其类型化冷元数据和热执行描述符。最终Plan的`FrozenString/FrozenArray`使用单一`PlanStorageBlock`，`PlanArena`精确分类计费，`PlanOwner`负责move-only（仅移动）所有权；`PlanBuilder`只接受不可伪造的`BudgetedPlanDraft`，`PlanBundle`不可复制或移动；该目标已经从`config_compiler`抽离，不依赖yyjson或其他JSON Parser（解析器）；
- `pae_config_compiler`把严格JSON依次编译为`SchemaIr → ValidatedSchemaIr → BudgetedSchemaIr → BudgetedPlanDraft`，完成结构、领域和资源校验后生成完整`PlanBundle`；ResourceBudget阶段在产生Budgeted能力前完成精确单Plan内存准入并携带批准报告与Limit；三种能力对象只能移动，不能默认构造或复制，移动后的对象不能成功重复进入后续阶段；
- `pae_protocol_core_slice`是首个`COMPLETE_RECORD（完整记录）`Codec（编解码器）内部切片，直接消费`PlanBundle`，不读取JSON，也不依赖`config_compiler`或yyjson。

当前依赖方向为：

```text
pae_config_compiler ──→ pae_protocol_plan ←── pae_protocol_core_slice
         │
         └──→ yyjson（仅配置加载私有依赖）
```

`pae_protocol_core_slice`的入口使用`PlanBundle + ExecutionWorkspace（执行工作区）+ pipeline_index`选择有向Pipeline（管线）。当前支持确定性Matcher、`UINT64`、字节对齐补码`INT64`、固定长度`BYTES`、`ENUM`和位成员`BOOL`，以及整数`input/constant`。INT64使用独立类型和`std::int64_t`，不与UINT64隐式转换。字段和Enum引用绑定原Plan作用域；完成输入预检、写入、完整性及最终复核后才交付结果。

当前Codec入口声明为`noexcept`，逐帧调用不创建或扩容容器；Plan和Workspace内存由初始化阶段提前建立，输出槽位和输出Buffer由宿主提供。Workspace永久绑定并借用一个Plan，Plan必须比Workspace存活更久；每个并发或重入调用必须独占一个Workspace，复用正在使用的Workspace会返回`WORKSPACE_BUSY`。失败时Decode不交付部分字段，Encode的`bytes_written`保持为零；若错误发生在最终复核阶段，调用方仍必须把输出Buffer内容视为不可交付数据。

`PAE_BUILD_TESTING（PAE测试构建开关）`控制测试、配置编译器测试依赖和操作计数版Core。PAE作为子项目嵌入时该选项默认关闭，不会因为宿主启用了通用`BUILD_TESTING`而自动引入yyjson、测试Runner或instrumented（带计数）Core；产品Core不保存或清零操作计数。

上述三个目标均不安装、不导出，不属于稳定公共API（Application Programming Interface，应用程序接口）。当前切片不包含`STREAM_CHUNK（流式字节块）`Framer（切帧器）、Integrity（完整性校验）、Receive Gate（接收门禁）、Mapping（映射）、Session（会话）、Runtime（运行时）注册、Transport（传输层）、C ABI（Application Binary Interface，应用二进制接口）或字符串键值接口。

JSON Parser Spike（技术探针）与生产切片隔离，位于`spikes/json_parser`。
