# Lab Binary 完整记录 Encode 首片契约

2026-09-19：用户确认最小展示方案，授权实施。基线 `19c5247`；现场已有两份盘点及总控计划变更须保留。

## 产品范围

- 为当前 Binary 0.9 complete-record public Host 路径补齐 Encode，不在本片扩展 Binary stream、Schema 或 PAE 公共 API。
- 由真实 Pipeline/Message execution metadata 决定可选动作与消息；一次用户操作只调用一次 public Host Encode，编译状态保持单一来源。
- 六类 typed 输入保持精度；Decimal64 使用 coefficient/scale，不经 double。修正现有 Decimal64 编辑映射，保留 NOT_REFERENCED 参与性；只允许 CALLER_INPUT 编辑，constant/computed/未参与字段只读。
- 成功展示：明确标注的调用方输入值、最终输出字节、按实际输出长度解析的公开物理范围/bit mask、生成字段说明。
- 未公开的生成字段 raw、转换轨迹、integrity algorithm/coverage 不展示为已知事实；不重复 Decode、不由 logical 反算 raw、不访问 private Plan。
- 失败不发布缓冲区或保留旧成功结果；展示公开状态、失败字段/value、conversion error 等事实，布局不是独立校验通过证明。
- binding/Flow 的草稿、结果与切换独立；Apply/Reload 的陈旧结果拒绝发布。回调内有界复制，所有 DTO/缓冲与替换峰值按现有预算约定计费。

## 实施边界与责任

《Lab应用推进》独占：`tools/protocol_lab_binary/public_binary_decode.{h,cpp}`、`tools/protocol_lab_ui/` 内 public Binary adapter/description、owned DTO、document session/tab、必要 field model/editor 文件及对应局部 CMake；`tests/protocol_lab_binary/`、`tests/protocol_lab_ui/` 内本片专项与合成 fixture；standalone 输入白名单/局部构建接线。限定本片相关修改，不大范围移动重命名。

仅写一份实施验证报告 `docs/engineering/lab-binary-encode-g1-validation.md`。不得改 `include/pae`、`src`、Schema、根 CMake、原 SDK 或98df5e0部署；出现公共能力缺口或写入冲突先停报总控。

## 验证与交付

- 先补映射/失败语义针对性断言，再实施。non-Qt 与 Qt headless Debug/Release，覆盖六类型、单向/混合 association、精度/越界/错误输入、生成字段只读、失败清旧结果、Flow隔离和预算失败；Release断言不得被NDEBUG关闭。
- 新构建根 `out/build/windows-msvc-lab-gui-g1`；独立消费验证根 `out/validation/lab-gui-g1`，其下static/shared、Debug/Release各自独立，不并发操作同一目录。
- 复用既有98df5e0 SDK，验证 installed-SDK static/shared D/R 消费与必要部署模块来源；不重打SDK，不做无关全仓回归。
- 新部署留在上述新验证根，不覆盖 `deliverables/lab/98df5e0`、用户私有配置或本机Qt；不得启动可见窗口打断真实验证。自动检查完成后最多建议两组简短人工观察，不将全部历史验收重做。
- 记录真实命令/日志/基线/dirty来源/未验证项；无Stage/Commit/Push/正式发布/额外删除授权。完成主动反馈总控并停止。

## UI 优化与中文化并行准备

用户授权后续直观性、展示性与中文化优化。《PAE工程整理》只读盘点当前界面并参考工具官方设计，仅写独立建议报告，不与本片并发修改UI。

面向用户的新按钮/提示优先中文，必要时括注英文术语；协议ID、字段ID、API、稳定错误码、用户数据保持原样。既有自动化定位保持稳定，不在本片机械全局替换字符串。后续按任务区域、输入/结果分离、状态与错误可见性统一设计，而不是增加API按钮墙。
