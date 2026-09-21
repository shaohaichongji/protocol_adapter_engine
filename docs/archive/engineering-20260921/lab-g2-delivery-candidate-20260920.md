# Lab G2 交付候选归集记录（2026-09-20）

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

## 1. 范围与基线

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 基线：`main@fa81329563dd3aea9bb167ef9bd34526a2606161`，派发时该检查点已推送。
- 归集源：
  `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-g2-c\common\out\protocol_lab_ui\Release`
- 候选根：
  `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\lab\fa81329`
- 运行闭包：`deliverables/lab/fa81329/common-release`

开始时候选根不存在；旧 `deliverables/lab/98df5e0` 和全部源构建/部署目录均保留。
本批未删除、移动、重建或覆盖旧文件。检查时发现总控正在并行修改
`docs/engineering/pae-execution-delivery-organization-plan.md`，本任务未触碰该文件。

## 2. 归集结果

`common-release` 共 17 个文件、21,481,610 bytes：

- `pae_protocol_lab_ui.exe`
- `Qt5Core.dll`、`Qt5Gui.dll`、`Qt5Widgets.dll`
- `platforms/qwindows.dll`
- `configs/` 下 12 个 V05-V11/Binary 样例配置

候选根新增：

- `README.md`：中文身份、实际 cache 开关、能力与边界说明。
- `manifest-sha256.txt`：17 个运行文件的 SHA-256 清单。

首次复制命令使用 `Copy-Item -LiteralPath ...\*`，PowerShell 不展开 literal wildcard，因此候选根
创建后仍为空，没有部分覆盖。随后在确认目标仍为空后改用逐项 `Copy-Item -LiteralPath`
完成复制。失败与修正后证据均保留，不隐藏首次失败。

## 3. 候选身份与可用能力

该 EXE 是 G2-C 既有 common Release 仓库内构建。其实际 cache 开关为：

- `PAE_BUILD_PROTOCOL_LAB_UI=ON`、`PAE_BUILD_PUBLIC_API_STAGE1=ON`、
  `PAE_BUILD_HOST_ENDPOINT_SLICE=ON`
- Binary：H1/H2/Binary UI 为 ON，private Binary materializer 为 OFF。
- ASCII：private adapter、ASCII stream observer、Host observer 为 ON；public A1、A2、
  public stream、public stream UI 均为 OFF。
- public legacy complete 为 OFF；V05-V08 仍为既有私有兼容路径。

因此可用边界是：

- 包含 Binary complete public H2 和 G2 Binary 流式 Session/UI 工作台。
- 包含 common 组合的旧 ASCII adapter/stream/Host 路径；不是 ASCII A1/A2/
  `ASCII_PUBLIC_STREAM_UI` 公开路径。
- 包含已部署的 V05-V08、ASCII、Binary complete 和 Binary stream 样例配置。

源码身份记录为 `fa81329`；但二进制来自 G2-C 既有构建，最后仅 root CMake A2
组合配置门禁变更后未重编。这不影响已构建的运行源码身份，但必须作为候选来源边界
保留，不得将本次复制写成“从 `fa81329` 重新构建”。

## 4. SDK 独立身份

本候选不是 installed-SDK standalone，也未实际消费已安装 SDK：

- common build cache 中没有 `PAE_DIR`、`PAE_SOURCE_DIR` 或 `CMAKE_PREFIX_PATH` SDK 消费记录。
- 运行闭包没有 PAE DLL，EXE 来自同仓构建树。
- installed-SDK static/shared 及包外 Lab 证据是其他已有验证边界，不能转移成
  本候选的实际 SDK 来源证据。

## 5. 哈希与来源核验

三层核验均通过：

1. 源 Release 闭包与候选 `common-release` 文件数均为 17，相对路径及逐文件 SHA-256 全一致。
2. 候选 EXE 与 common `bin/Release` 一致；Qt DLL/qwindows 与仓库 Qt release 输入一致；
   12 个 configs 与各自 canonical 或 common generated 源一致。
3. 候选 `manifest-sha256.txt` 共 17 条，与实际运行文件逐项复算一致。

证据：

- `out/validation/lab-binary-stream-g2-c/delivery-candidate-fa81329-copy-hash-verification.txt`
  （保留的首次失败）
- `out/validation/lab-binary-stream-g2-c/delivery-candidate-fa81329-copy-hash-verification-fixed.txt`
- `out/validation/lab-binary-stream-g2-c/delivery-candidate-fa81329-canonical-hash-verification.txt`
- `out/validation/lab-binary-stream-g2-c/delivery-candidate-fa81329-manifest-verification.txt`
- `out/validation/lab-binary-stream-g2-c/delivery-candidate-fa81329-common-cache.txt`

## 6. 未验证与状态

- 本次未重新编译、未运行测试、未启动可见或隐藏 UI。
- 没有新增模块运行时加载路径快照；Qt 和 fixture 来源以逐文件哈希证明。
- 未重验 installed-SDK standalone、SDK 包外、Linux、硬件、现场或正式发布。
- 未修改现有 README/指南入口；等待 PAE 审计汇合后由总控统一同步。
- `deliverables/lab/` 被仓库 `.gitignore` 忽略，因此候选目录不出现在普通 `git status`；
  本报告和绝对路径是其本地存在与身份的显式记录。
- 未 Stage、Commit、Push、发布或删除。

当前状态：`deliverables/lab/fa81329` 已完成限定归集和静态哈希复核，作为待总控
审查的仓库构建候选，不是正式发布或 installed-SDK standalone 交付。
