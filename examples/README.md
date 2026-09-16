# Examples

只允许放置脱敏、人工构造或已确认可分发的协议示例。真实客户协议和现场数据默认禁止进入 Git。

## 阅读入口与职责

- [`public_api_codec`](public_api_codec/README.md)：当前 Stage 1/1B 推荐的公开消费示例，只 include
  `pae/*` 并链接 `PAE::pae`；先用 metadata 查询动作、逻辑类型、Encode 来源和输出容量，再执行
  Binary Encode/Decode 与 ASCII Encode。
- [`public_api_framer`](public_api_framer/README.md)：阶段 2A 公开流式消费示例；查询能力并创建
  `StreamFramer`，每个同步候选仅调用一次公开 Codec Decode。其 `standalone/` 是 Testing-off 的
  public-only CMake consumer。
- [`public_api_host`](public_api_host/README.md)：阶段 2B 公开 Host 示例；以同一 endpoint 分别绑定
  Decode/Encode Pipeline，同一 Encode Handle 在调用期选择两个 Message，并演示分块输入、候选观察、
  成功交付、Reset 后重新 Find。其 callback 输出是借用数据，不是 Transport 发送。
- [`public_api_compile`](public_api_compile/)：独立 CMake consumer，演示以 `PAE_SOURCE_DIR` 引入源码树，
  只执行配置编译和通用 metadata 查询；它不替代 Codec 示例。
- `ascii_text/`、`ascii_stream_framing/`、`stream_framing/`、`host_endpoint/` 和
  `business_embedding/` 是内部能力或消费侧的工程示例，不是推荐给外部消费者的稳定 SDK 接入面。
- 配置样例集中在 `config/`。先读 [`../include/README.md`](../include/README.md)理解公开边界；
  需要内部实现或 Lab 示例时，再进入相应目录。

- [`config/synthetic_lab_exchange_slice.pae.json`](config/synthetic_lab_exchange_slice.pae.json)：从零设计的人工实验台双向配置切片；状态为`V0.1 DRAFT SLICE / INCOMPLETE`。
- [`config/synthetic_sum8_slice.pae.json`](config/synthetic_sum8_slice.pae.json)：从零设计的Schema 0.3
  SUM8完整记录样例，配套Values与独立计算Frame；不来自真实协议或现场报文。
- [`business_embedding`](business_embedding/README.md)：默认关闭的最小C++17业务宿主示例，使用两份
  从零合成配置验证类型化RX/TX、配置适应、失败不交付及回调复制生命周期。

该样例只用于当前及后续 Loader/Compiler 和 Plan快照验证：

- 所有`source_ref`均为`SYNTHETIC_FROM_SCRATCH（从零人工设计）`；
- 标识、长度、常量、字段值、大小端和完整字节布局均不来自客户协议、生产代码或现场报文；
- 两个方向故意使用13/11字节记录、Little Endian（小端）、三字节整数、BYTES和ENUM组合，以覆盖当前引擎能力；
- 早期Synthetic样例中的`record_length`仍使用`encode.source=input`，只验证普通动态字段；
  `synthetic_length_slice.pae.json`专门验证Schema 0.7的自动长度回填和Decode一致性检查。

该样例由C++ Loader/Compiler生成与机器Golden Snapshot逐字节一致的Plan，并配套独立Engine Vector；这只验证人工协议范围内的引擎行为，不代表完整PAE V0.1、任何生产协议、硬件或现场验证通过。
