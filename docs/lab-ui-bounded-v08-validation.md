# Protocol Lab UI Schema 0.8 有界变长检查点验证记录

日期：2026-09-10

基线：`4a04afb13d8dc38ea59bd7e7aa3cc3bba370c301`，分支 `feat/lab-ui-c1`

构建目录：`out/ui-v08-v142-check`

## 1. 构建配置

Windows x64、MSVC v142 `14.29.30133`、Qt `5.13.0`。Release 与 Debug 使用同一
multi-config 构建树和以下关键开关：

```text
PAE_BUILD_PROTOCOL_LAB_UI=ON
PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON
PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=OFF
PAE_BUILD_PROTOCOL_LAB=OFF
```

Release 原批次和 Debug 本批次均成功构建 7 个 UI headless 测试目标与
`pae_protocol_lab_ui`。Debug 部署输出位于
`out/ui-v08-v142-check/out/protocol_lab_ui/Debug`。

## 2. 自动验证结果

Release 原批次命令：

```powershell
ctest --test-dir out/ui-v08-v142-check -C Release --output-on-failure -R "^pae\.tools\.protocol_lab_ui\."
```

结果：10/10 通过，其中 7 个 headless 测试、3 个真实 shown-window Qt smoke。

Debug 本批次命令：

```powershell
ctest --test-dir out/ui-v08-v142-check -C Debug --output-on-failure -R "^pae\.tools\.protocol_lab_ui\."
```

结果：10/10 通过，其中 7 个 headless 测试、3 个真实 shown-window Qt smoke；本批次
CTest 日志保存于 `out/ui-v08-v142-check/Testing/Temporary/LastTest.log`。Debug 验证在
Release 原批次之后按序执行，没有因无代码变化重复运行 Release。

Schema 0.8 专项覆盖：

- 空 payload：Core 输出 `A5 03 A8`，computed length 为 3，payload 结果为空；
- 最大三字节 payload：Core 输出 `A5 06 00 FF 01 AB`；
- 两字节 payload：Core 输出 `A5 05 10 20 DA`，payload 高亮为 byte 2、3，动态
  integrity storage 为 byte 4；
- 直接调用 Field model 时，四字节输入被长度边界拒绝；非法 Hex 被 canonical 词法规则拒绝，
  二者错误原因不同；
- 动态 integrity 失败不显示未经有效结果证明的 trailer 位置，随后合法 Inspect 可恢复实际
  payload 高亮；
- 描述映射验证复制后的 0..3 payload 边界、动态 range/storage 锚点和实际范围解析；
- Schema 0.5、0.6、0.7 的既有 headless 与 shown-window smoke 全部通过。

人工发现详情陈旧问题后，只重建受影响的 UI 目标并运行 Schema 0.8 shown-window 专项：

```powershell
ctest --test-dir out/ui-v08-v142-check -C Release --output-on-failure -R "^pae\.tools\.protocol_lab_ui\.qt_smoke_v08$"
ctest --test-dir out/ui-v08-v142-check -C Debug --output-on-failure -R "^pae\.tools\.protocol_lab_ui\.qt_smoke_v08$"
```

Release 1/1、Debug 1/1 通过。专项保持 payload 选择不变，依次验证空载荷、两字节、三字节
Encode 及 `GG` 失效后的右侧详情、表格、实际 Frame 和高亮同步。日志分别保存为：

- `out/ui-v08-v142-check/Testing/Temporary/v08-detail-refresh-release.log`
- `out/ui-v08-v142-check/Testing/Temporary/v08-detail-refresh-debug.log`

## 3. 证据边界

已验证的是离线合成配置下的 Debug/Release 编译、状态模型、Core 调用结果、窗口控件路径
和 Hex 高亮；限定人工检查见第4、5节。未验证完整视觉布局、真实协议配置、网络、硬件、
长稳、部署或现场验收；没有把 shown-window 自动 smoke 记为人工通过。

## 4. 人工实际检查记录

状态：`PARTIAL / DEFECT_FOUND_AND_FIXED_IN_CANDIDATE`。以下是用户修复前实际检查的文字记录；
外部临时截图仅用于核对，没有复制进仓库。

1. 空 payload Encode 实际得到 `A5 03 A8`、长度 3、表格 `length=0` 且无 payload
   高亮；右侧详情错误地保留 `current range unavailable`。
2. `1020` Encode 实际得到 `A5 05 10 20 DA`；重新选择 payload 后，右侧正确显示 payload
   `2 + 2`、integrity `4 + 1`。
3. 保持 payload 选择，改为 `00FF01` 后实际得到 `A5 06 00 FF 01 AB`、长度 6、高亮
   byte 2..4，但右侧错误地保留旧的 `2 + 2` / `4 + 1`。
4. 随后提交 `GG`，`UI_INPUT_INVALID` 正确报告 canonical Hex 错误，Frame、Raw、Logical、
   高亮和表格实际范围均清空；右侧仍错误地保留旧实际范围。
5. Inspect `A5 05 10 20 DB` 正确进入 `INTEGRITY_FAILED` 且无动态尾高亮；改为
   `A5 05 10 20 DA` 后恢复长度 5、payload `1020`、byte 2..3 高亮和 `2 + 2` / `4 + 1`
   详情。该 Inspect 人工路径通过，无需重复。

根因是 Encode 结果变化只刷新表格与 Hex，右侧详情仅在选择变化时重算。候选修复让每次
Encode 成功或失效都基于当前选择重算详情，同时避免详情与预览刷新互相递归。

四字节越界没有由人工编辑器路径验证。BYTES 编辑器按最大三字节设置 `maxLength=6`，因此
键入或粘贴 `01020304` 时，超过六个字符的部分不会进入 Field model；自动化中的四字节
负例属于 model 层验证，不能记为人工 UI 验证。该静默截断存在误解输入已完整提交的风险；
建议后续单独决策是否增加显式超长提示或整次粘贴拒绝，本检查点不改变输入策略。

## 5. 修复后人工确认（2026-09-10）

状态：`TARGETED_MANUAL_FIX_CONFIRMATION_PASS`，仅覆盖下述截图可核实结果，不升级完整人工矩阵。

1. 用户按不重新选行的检查步骤回传 `00FF01` Encode 截图：Frame为
   `A5 06 00 FF 01 AB`，长度6，payload高亮byte 2..4；右侧已同步为 actual
   `2 + 3`、integrity `5 + 1`，不再保留旧范围。
2. 随后 `GG` 截图显示 `UI_INPUT_INVALID`；Frame、Raw、Logical、高亮和右侧旧actual
   范围均清空，右侧显示 `current range unavailable`。失败清理修复获人工确认。
3. 修复后空载荷详情没有单独截图确认，保留专项自动化证据；此前Inspect人工通过记录不变。

用户已关闭Lab。本次只记录实际人工证据，未重跑测试、未更改源码，未执行Git写操作。
第4节超长输入截断风险仍保留，四字节越界未由人工编辑器路径验证。

## 6. 提交前格式收口

总控只读格式检查发现部分候选C++排版不符合仓库clang-format配置，随后仅对候选C++文件
执行格式化，`clang-format --dry-run --Werror`及`git diff --check`通过。未修改业务逻辑，
未重跑测试；上述Debug/Release证据属于格式整理前的功能验证批次。未Stage/Commit/Push。
