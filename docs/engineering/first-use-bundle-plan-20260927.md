# 首版完整体验包交付计划

## 目标与固定身份

用户要求完整、纯净、附中文说明和示例的独立体验包，再开始真实使用。
产品固定来源为已推送且接管时干净的 `adeae30d942ba42cc518c244c220730b7e462d2c`。
本计划及后续包级中文导航是交付材料，不冒充该提交原文件；分别记录来源与哈希。
JSON 保持 canonical，YAML 为受限作者源。仅 Windows x64 已验证工具链，非生产发布。

## 交付组成

- 一个最终体验包目录及 ZIP，首选 Lab Release，只保留一个默认启动入口。
- SDK Source、含 YAML 的 Static/Shared Debug/Release 五包；JSON-only 对照不重复附入主体验包。
- PAE Source 白名单包含需要的源码和依赖，不要求下载整个仓库；不含 Qt/Lab 或实验目录。
- Lab 带同源 Qt DLL、插件及 JSON/YAML 示例；Qt 不进入 SDK。
- 中文开始说明、Lab 使用、SDK 接入、配置与 Schema 导航、版本及已知限制。
  教程全部以体验包根为工作目录，不依赖原仓库绝对路径或旧 deliverables/out。
- 原始 SDK provenance/manifest/hash 保持不变；包级说明清楚区分固定源码文档和本轮导航。
  总包另设完整清单与 SHA-256，运行产物和原始验证日志均留包外。

最终位置暂定 `deliverables/sdk/adeae30-experience/` 下的独立总包和 ZIP，便于沿用既有忽略规则；
具体路径在归集时核对不存在再创建。旧 b12ad80 及所有现用部署不覆盖、不删除。

## 分工与依赖

1. PAE 任务从本地固定提交独立干净 clone 构建五 SDK，独占
   `out/build/experience-sdk-adeae30-20260927/` 和同名 evidence 根，
   仅新增 `experience-sdk-validation-20260927.md` 工程报告。
   验证包外 JSON/YAML consumer、修复后 Source 嵌入宿主测试保留、清单及共享 DLL 来源。
2. 工程整理并行仅编写独立体验导航草稿 `docs/experience/` 和
   `experience-docs-validation-20260927.md`，不提前复制成品或宣称验证通过。
   包内原 SDK 说明含历史 b12ad80 引用，必须在新导航明确其历史含义，不冒充当前版本。
3. SDK 总控复核后，串行派发 Lab 从同提交消费本轮 SDK，生成一个默认 static Release
   完整部署并跑既有定向自动检查。Lab 未收到下一次派发前不执行。
4. 两类成品复核后工程整理归集总包、检查离线相对链接、制作 ZIP、在新位置解包核对
   全部哈希并实际执行最小 consumer/隐藏 Lab 检查。最终只向用户给一个入口。

## 边界与停点

不改产品执行语义/API/根构建策略/系统 Qt，不新下载依赖，不发布或执行主仓库 Git 写操作。
沿用固定 v142 工具链；失败保留证据，涉及源码或打包规则修复先报告总控，不偷改固定 clone。
项目许可证、Linux、稳定 ABI、真实协议/设备长期运行仍未闭合，不因制包而宣称完成。
每片完成向总控反馈一次，停止写入。此计划落盘不等于成品已生成。
