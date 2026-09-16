# Stage 4 H1：Lab public-only Binary complete Decode 契约

日期：2026-09-15。H1 仅新增非 Qt、非可见的公开 API 消费适配层；既有 Binary UI backend 保持不变。H2 UI 切换、Encode、stream、ASCII 和旧桥清理另行授权。

## 边界与输入

- 使用 `CompileProtocolJson` 一次编译并独占 `CompiledProtocol`；所需描述字符串立即复制。绑定只允许 Binary complete-record Decode Pipeline。`CreateHostEndpoint` 从该 compiled owner 获取同一冻结状态并由 Host 独立保留状态引用，并非借用 owner 对象；Adapter 成员反序析构使 `host_` 先于 `compiled_` 销毁。成功 callback 的物理查询仍使用本 Adapter 的同一 compiled owner。
- 两个 Flow 各持有自己的 Handle、草稿与最近结果。成功 Reset 清本 Flow 草稿/结果后重新 Find，不复用旧 Handle，也不影响另一 Flow；适配层不实现 Qt Tab/reload/关闭事务。
- 生产头仅标准库和 `pae/**`；独立 target 仅显式链接 `PAE::pae` 与通用编译选项，不借私有 backend、Schema、Plan 或 SDK 内部头。

## 同调用观察与发布

- 只在 Host 成功 output callback 中，以该 Host 的同一 compiled owner、`record.MessageIndex()`、本次 `frame.size` 取得 `ResolveMessagePhysical` 和每个 `ResolveFieldPhysical`。将 Frame、字段 ID、typed logical、实际 `ConversionRaw*`、Enum raw/known、BYTES、byte range 或非零 bit masks、integrity/computed storage 复制为自有 DTO；不保存 borrowed view，也不重复 Decode。
- 严格校验 callback 的 action/message/record、Field metadata/index/kind、实际 BYTES 长度与物理长度、range/mask 边界。任何查询或复制不一致整次失败，不能裁剪、猜值或保留前次成功高亮。成功 DTO 在发布前无条件复核最终容量，含理论上的零字段路径；当前 Binary Compiler/Plan 不允许合法零字段 Message。零长度 BYTES 是存在的空 range，不生成高亮字节。
- 失败 candidate 只记录公开确证的 Codec status、field index、conversion error、诊断帧与 Host 计数；没有确证 message 时标未知。协议失败、callback 失败和复制失败均清旧成功字段/高亮。纯本地输入拒绝（未调用 Host）保留当前结果和草稿，报告拒绝状态。发布使用构造完成后的 move/swap，不在发布阶段复制。

## 资源与验证

- 逻辑预算不是 RSS：单实例 128 MiB，替换峰值 256 MiB；分别计 PAE compiled/Host 共享冻结状态与 Lab 自有描述、Flow、两份保留草稿加一份替换期 pending 草稿、结果副本/临时拷贝。加乘溢出失败关闭；准确上界可在 Host 创建后按其报告复核；成功结果以容量计费，等于上限可准入、少一字节拒绝。预算失败不发布半成品。
- 验证 Windows x64 MSVC v142 Debug/Release：typed 六类、enum known/unknown、Decimal 实际 raw、bit/byte、bounded 零/中/最大、integrity/computed、失败清旧与恢复、两 Flow、Reset、所有权和预算边界。新 target 的头/源/link 不依赖私有入口；默认 OFF、缺公开依赖显式失败、Testing-off 零测试；仓库外 static D/R 实际编译调用 H1 adapter。
- 自动证据不代表 UI 人工验收、Golden、Linux、硬件、现场、稳定 ABI 或正式发布。无 Stage/Commit/Push。
