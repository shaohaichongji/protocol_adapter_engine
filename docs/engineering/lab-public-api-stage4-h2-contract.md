# Stage 4 H2：Binary 0.9 UI 切换契约

日期：2026-09-15。状态：H2 接线契约，实施与验证证据另见同片报告。本片只切换现有 Schema 0.9 complete-record Decode UI；旧 0.5–0.8、ASCII、Binary Encode/stream、旧桥清理不进入本片。H1 已完成限定复核，P physical query 是布局事实，不单独证明 Decode 成功。

## 1. 单次编译与 dispatch-only 路由

- 后台 worker 对最多 4 MiB 配置做严格 JSON 根对象检查，只从真实根成员读取已解码的 `schema_version`。分类只选择 public Binary 或既有 private legacy/ASCII 编译器，不解释 Matcher、Field、Plan、布局或协议能力；目标编译器仍完整验证配置。Binary 初次打开和显式 Apply 均调用 `CompileProtocolJson` 一次；旧分支仍调用 `CompileJsonToPlanWithUiDescription` 一次。失败不 fallback、不默认版本、不新增用户选择。
- 不以会静默覆盖重复 key 的 DOM 查找为唯一证据；逐根成员计数，拒绝重复顶层 `schema_version`，包括 JSON 转义等价 key。缺失、错误类型、非法 JSON、超限、未知/不支持版本分类失败，completion 区分 classifier diagnostic 与目标 compiler diagnostic。
- completion 明确为 public `CompiledProtocol` 或 private artifacts 两种 move-only 结果，保留 DocumentId/load revision/config hash/request identity。迟到/cancel/close 依旧按现有六维身份拒绝，不把失败结果发布为 READY。

## 2. Binary 描述等价映射与缺口

| 现有 UI 事实 | public 来源与映射 | 边界 |
| --- | --- | --- |
| Schema、协议、Pipeline、Message、Field、Enum ID/名称/说明/source_ref | `CompiledProtocol::Protocol/Pipeline/Message/Field/Enum`，立即复制字符串 | 不保留借用 `string_view` |
| Pipeline/Message decode 可用性与选择 | `PipelineMessageIndex/Execution` | 不以 UI 解析 Schema 决定支持 |
| logical Type 与只读 Source | `FieldDescription::value_kind/encode_value_source` | Decimal64 显示 logical kind；constant/computed 仅展示只读，不伪装 Decode 结果可编辑 |
| byte range、非零 bit masks、BYTES 上下界、fixed/bounded 记录长度及 integrity/computed storage | `MessagePhysical/FieldPhysical`；成功 callback 只使用 H1 同 owner/message/frame resolved 值 | bitfield 不伪造满容器 byte range；动态 payload 的当前长度/高亮使用本次 owned `actual_range`，零 payload 有空 range、无高亮 |
| conversion 存在与通用说明 | public logical `DECIMAL64` 及本次 `ConversionRaw*` | public 不给具体 linear scale/bias；现有 Binary UI 仅显示通用 conversion 提示，精确参数不猜、不作为输入编辑约束 |
| wire codec/byte order | 当前 public metadata 不逐项暴露 | 现有可见 physical byte/bit/mask 由查询事实承接；不可见内部 enum 不作为执行判断，不展示猜出的 wire/order。若后续 UI 要精确 wire/order 参数，另报公共观察缺口 |

总控按实际调用点核对后确认：原 Binary `wire_codec/byte_order` 只保存在旧内部描述，不存在现有可见精确参数控件；`conversion` 的可见详情只显示通用 Decimal64/Core representability 提示，不打印 linear scale/bias。H2 从 public `ValueKind::DECIMAL64` 保留对应 TYPE/提示/typed/raw/logical，不构造假的 `LinearConversionDescriptor`，也不复制未消费的 wire/order/scale/bias 载荷或新增“未知”显示。这是内部旧 DTO 字段与实际已验收可见能力的区分，不批准 PAE API 扩展或展示削减。

旧共享 `DocumentDescription/FieldTableModel` 仍因 legacy/ASCII 持 private enum；Binary 专属 `binary_host_adapter_public.h` 和 `public_binary_description.h` 仅暴露 public PAE 与 Lab 自有 DTO/前置声明，不依赖 private Plan/Core/Host 类型。映射源在共享展示边界转换 `ValueKind/EncodeValueSource` 供现有 Qt 表格消费，不读取 private Plan 或执行 backend。Binary 专属 H2 target 只声明 H1/PAE 链接，推荐 H2 preset 关闭旧 materializer；旧分支在独立旧构建中回归。Binary 字段 Value 列保持只读，Raw/Logical 只来自本次 Decode，失败清旧。

## 3. 绑定、结果与发布

- Binary 保持未绑定、显式 Apply、多 binding 各自 Pipeline、每 binding 两 Flow 及草稿/当前结果隔离。H1 扩为接收同一次 worker 已编译 owner 的 move-only入口和 bindings；不重新读 JSON、不再次 Compile/Decode、不依赖旧 materializer。成功回调复制 H1 owned DTO、typed/实际 raw、enum known/unknown、Frame、resolved 物理位置；失败只留确证诊断并清旧成功字段/高亮。
- UI 保留 prepare→逻辑预算→确认→move 发布、旧 Session 等待确认期间不变；预算计 compiled/Host/H1 owner、Binary 自有描述、Qt DTO/视图、两 Flow、待发布副本及旧实例共存。预算不是 RSS。Tab/reload/close/两 Tab Yes→No、迟到 completion 和 selector 生命周期沿用现有事务边界。
- 本片不启用 Binary Encode、stream，也不修改旧 backend 行为；新 Binary 可见执行只调用 H1/public Host 一次。

## 4. 验证出口

Windows x64/v142、随仓 Qt，Debug/Release 串行针对 Binary UI、worker/router、受影响 session/展示回归；H1 改动后复验 H1 与 static D/R 包外 consumer。检查 classifier 转义/嵌套/重复/类型/语法/超限/未知及各分支一次编译无 fallback；UI 类型/位置/失败/两 Flow/多 binding/预算、cancel/reload/close/两 Tab；Testing-off、默认/缺依赖门禁。交付可直接启动的 Release 本地部署目录和最多三个动作组的待用户烟测说明，但任务自身不运行人工 UI。无 Stage/Commit/Push、发布、删除或 Qt 全局环境改动。

## 5. Flow 草稿隔离返修（2026-09-15）

人工反馈覆盖初始自动通过结论：Flow 0/1 输入不同帧并反复切换，选中 Flow、输入框与已解码结果发生错配，隔离项未通过。H2 原 `SaveAndSelect` 把当前编辑草稿写入目标槽；既有旧 Binary backend 的语义是先保存当前槽，完成后再选目标槽。H2 改为跟踪当前选择，先完成待保存草稿的 Lab 复制、ASCII/预算确认，再只向当前源槽保存，最后更新选中 binding/Flow；准备或保存失败不得污染源/目标槽或 Session/Qt 选择。目标草稿由 `PrepareBinaryHostFlow` 读取，切换后发布给编辑器，不拿上次 Decode 帧覆盖未提交草稿。

回归必须用两个 Flow 的不同 count/帧反复往返，核对 QPlainTextEdit、Session 草稿、各自结果；另核对首次空 Flow、未 Inspect 草稿、跨 binding、准备/复制/预算失败。未 Inspect 草稿切回后，现有 Session 可恢复同 Flow 上次 Decode 结果；这是“上次结果”，不是对新草稿重新 Decode。本返修不改 H1/PAE、旧 backend 或 ASCII；人工 Flow 复验仍须由用户执行。证据与最新部署哈希见同片 validation 返修附录。
