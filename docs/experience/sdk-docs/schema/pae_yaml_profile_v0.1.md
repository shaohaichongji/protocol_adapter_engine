# PAE 受限 YAML 配置入口 Profile V0.1

> 交付版投影：据固定 `9680cf90512f053962949cb55467d6af35959088` 的
> `schema/pae_yaml_profile_v0.1.md` 制作；本文件不是该提交的原始字节。
> 语法、类型、来源和资源规则正文未改。下述 `docs/engineering/` 路径是该
> 固定源码仓库的历史参考，**未随体验包附带，也不是 SDK 包内路径**。

## 状态与作用

原文起于 2026-09-26 YAML 首片的**待总控复核契约候选**。原文当时已有独立非 Qt
前端、仓库内可选 Lab 入口和可选 SDK 静态组件的分片实现及各自限定验证；后两者
不等于人工体验、正式包发布或已批准的稳定产品入口。
上位边界的固定源码仓库历史参考（未随体验包附带）为
`docs/engineering/yaml-entry-plan-20260926.md`：YAML 是作者源，
经非 Qt 前端转换为严格 JSON 后，仍调用既有 `CompileProtocolJson`。JSON 仍是规范编译输入；
Schema、执行语义、公开 ABI、现用 SDK 与 Lab 均不因本 Profile 改变。

## 语法、类型与身份

1. 接受 UTF-8、YAML 1.2 单文档、根 Mapping、字符串键、Mapping/Sequence/基本标量和注释。
   显式版本指令只接受 `%YAML 1.2`。拒绝 BOM、非法 UTF-8、空文档、多个文档、非对象根。
2. 不支持显式 Tag（含自定义及标准显式 Tag）、Anchor、Alias、Merge 扩展、
   `%TAG`、include、环境变量替换及任何外部资源读取。普通带引号字符串键 `"<<"` 不算 Merge；
   Plain Key `<<` 拒绝。禁用构造应在转换前失败，不先展开再检查。
3. Mapping Key 必须解析为字符串。每层按**解码后的完整 Unicode 码点序列**检查重复键，
   在覆盖前拒绝；不做 Unicode normalization。`a` 与 `"\u0061"` 属同一个 Key。
   键顺序不代表覆盖或协议优先级。
4. 双引号、单引号及 Plain 字符串经 YAML 转义/折叠规则解码后输出 JSON String。
   不按目标 PAE 属性隐式把字符串转成数字或布尔。版本号、十六进制字节文本等数值外观字符串应加引号。
   为避免空值歧义，Plain 空标量拒绝；空字符串写 `""`，空值写 `null`。
5. Plain `null`、`true`、`false` 分别输出 JSON Null/Boolean，其他大小写或 YAML 1.1 风格词不隐式识别。
   Plain 十进制整数仅接受 JSON 规范整数词法：`0` 或 `[1-9][0-9]*`，可有前导负号但拒绝 `-0`。
   正数最多 UINT64_MAX，负数最小 INT64_MIN；数字文本直接进入 JSON，不经 `double`。
   数字外观但非此形式的 Plain 标量（例如 `+1`、`01`、`2.0`、`1e3`、`0xFF`）拒绝，
   不自动规范化或降为字符串。需要这些文本时显式加引号。
6. 转换输出必须满足现有 Strict JSON Profile；之后由既有 Schema/Domain/Resource/Plan 链再次判定。
   YAML 转换成功不表示配置编译成功，也不表示两种作者源执行等价。

### 本片词法澄清（不改变上述类型边界）

- “数字外观”指首字节为 ASCII 数字、正负号后紧跟 ASCII 数字或点，或点后紧跟 ASCII 数字；
  `.inf`、`.nan` 的大小写变体及带符号形式也归入拒绝范围。`-foo`、`.well-known` 等
  非数字外观 Plain 标量保持字符串。数字外观必须完整符合第 5 项的规范整数词法及范围。
- `null`、`true`、`false` 仅这三个精确小写 Plain 词转成 JSON Null/Boolean；大小写变体
  拒绝。`yes`、`no`、`on`、`off` 不按 YAML 1.1 隐式转型，作为普通字符串。
- 双引号转义、单引号转义及块标量折叠/截尾由 YAML 解析器解码；JSON writer 对解码后的
  控制字符确定性转义（例如换行写成 `\\u000a`）。前端在写入前再次验证解码后键和值
  均为合法 UTF-8，拒绝代理项码点等非法 Unicode；合法非 BMP 字符可用原生 UTF-8
  或 `\\U` 八位十六进制转义表达。非法 YAML 转义由解析器或该前端门禁拒绝。
- 显式版本指令须为 `%YAML` 后至少一个空格或 Tab，再接 `1.2`；后面仅允许空白或注释。
  前置空行和整行注释允许；`%YAML 1.1`、附加非注释文本和其他指令拒绝。

## 来源定位与资源边界

- 前端应同时维护 YAML 原文来源身份、行/列和 JSON Pointer 关联。传给既有编译器的 JSON Offset
  只表示生成物位置，不能当成 YAML 字节 Offset 或原始行列。不得修改公开 `CompileDiagnostic` ABI。
- 在调用 YAML Parser 前检查原文大小；解析时约束深度、节点和标量；输出期间约束生成 JSON 大小。
  所有加法、乘法及长度比较需防溢出。编译器原有资源预算继续生效。
- **解析阶段上限须覆盖 Parser Scanner、Token/Event 暂存及内部分配**。仅在完整标量 Event
  产生后检查其长度，或在构造完整树后检查节点数，均不足以证明解析峰值受控。
  历史首片 libyaml 探针只验证了输入前置、Event 阶段和输出阶段检查；当时 Parser
  内部分配的上界未证明。后续 rapidyaml 候选通过每调用分配回调预算覆盖其
  Parser/Tree/arena 请求；这不是整个进程 RSS、线程栈或极端嵌套的全域资源证明。
- 具体生产数值限额及异常诊断格式尚未获批。SDK 可选组件首片将现有默认值固定称为
  **试用组件资源约束 V0.1**：输入 16 KiB、Parser 和辅助预算各 128 KiB、输出 JSON
  32 KiB、512 节点、16 层及 4 KiB 单标量。公开只读查询用于调用方预读；不开放任意
  调参或测试故障注入。上述是当前拒绝边界，不是生产容量档位、进程 RSS、CRT/栈
  峰值或完整内存计费承诺；公开结果 owner 还需一次单独分配，失败返回分配失败状态。
  后续调整须明确更新约束版本、说明和测试，不能仅修改默认数值。

## 兼容与验证边界

JSON-only 构建及调用不得强制引入 YAML 依赖。YAML 作者源不要求手工维护第二份 JSON。
历史首片仅有独立解析器探针与两份公开合成配置的结构对照，未调用 PAE 编译器。
原文记录的独立内部前端曾对公开 Binary CRC 与 ASCII 样例执行原 JSON/转换 JSON 的
真实 `CompileProtocolJson` 和已知 Decode/Encode 对照。固定源码仓库历史参考
（未随体验包附带）：
`docs/engineering/yaml-frontend-slice-validation-20260926.md`。这仍不证明所有配置等价。
后续仓库内 Lab 与可选 SDK 安装树的旧阶段限定结果，其固定源码仓库历史参考
（未随体验包附带）分别为
`docs/engineering/lab-yaml-minimal-entry-validation-20260926.md` 和
`docs/engineering/yaml-sdk-component-validation-20260926.md`；不能把前端首片的历史测试
追溯写成这两项已经通过，也不能据此宣称正式产品或人工验收。

后续资源探针的固定源码仓库历史参考（未随体验包附带）为
`docs/engineering/yaml-parser-resource-validation-20260926.md`：
libyaml Event 异常清理已返修，第二候选 rapidyaml 的局部分配预算已作隔离动态验证。
这不改变本 Profile 的待复核状态和正式容量未拍板的边界；内部前端候选的实现状态
以后续独立验证报告为准。
