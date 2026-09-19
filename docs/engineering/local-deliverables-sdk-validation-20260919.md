# `deliverables/sdk/98df5e0` 本地 SDK 归集验证

## 1. 结论与边界

2026-09-19 将已验证 clean-checkpoint SDK 五包从：

`out/sdk-clean-checkpoint/candidate1-20260919/`

原样复制到仓库工作树内的本地交付根：

`deliverables/sdk/98df5e0/`

复制前后完成 provenance、Manifest、包内 Hash、文件集合、长度和逐文件 SHA-256 校验，全部通过。
本次只是 verified bytes 的本地归集：没有修改任一包内文件，没有压缩、重命名或重打包，没有构建、运行、
测试或重新生成 SDK，也没有删除来源候选。

`/deliverables/sdk/` 由总控在复制前加入 `.gitignore`。实际执行前
`git check-ignore -v deliverables/sdk/98df5e0/pae-sdk-source/PROVENANCE.json` 命中该规则；目标根不存在，
来源根及五个包均不是 reparse point。

## 2. 来源身份与目录集合

来源五包均满足：

- `source_head=98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`
- `source_worktree_dirty=false`
- `package_kind`、`configuration` 与目录名一致

目标顶层没有额外文件，只包含以下五个原名目录：

1. `pae-sdk-source`
2. `pae-sdk-static-debug`
3. `pae-sdk-static-release`
4. `pae-sdk-shared-debug`
5. `pae-sdk-shared-release`

没有复制 clean clone、build tree、consumer build、验证日志、Qt Lab、旧 dirty candidate 或整个开发仓库。
顶层导航 README 由总控独占，本轮未创建。

## 3. 复制前后核验结果

| 包 | 文件数 | Bytes | Manifest 条目 | 包内 Hash 条目 | `SHA256SUMS.txt` SHA-256 |
| --- | ---: | ---: | ---: | ---: | --- |
| `pae-sdk-source` | 81 | 1,743,299 | 79 | 80 | `9bf25c6dc043989b179a77443c5cddd099c645e4a1d422445a571ed7f12c09f6` |
| `pae-sdk-static-debug` | 29 | 17,385,845 | 27 | 28 | `95ddd594f4f320430364607910435161cbf27b1f976521f4345a2920b2a02037` |
| `pae-sdk-static-release` | 29 | 7,832,800 | 27 | 28 | `ccaf8417c65b5e3c31e4f957e3bea4e5831d11d707ab22c4dc55f9a15caaec0f` |
| `pae-sdk-shared-debug` | 25 | 2,194,262 | 23 | 24 | `ea3d32b34f2b5cf272f1cdfaf4a382d3dc1dd33918a987d396e7b4f72c5fc35f` |
| `pae-sdk-shared-release` | 25 | 790,364 | 23 | 24 | `e18f49aaffddac82e766ba10a5ccd530a2cda4714c6ec64490bea786f8e603b8` |
| **合计** | **189** | **29,946,570** | **179** | **184** | — |

实际检查顺序：

1. 复制前确认来源顶层目录集合精确等于上述五包，且无根文件。
2. 对五包逐一解析 `PROVENANCE.json`，核对 HEAD、dirty、kind/config。
3. 复算五个 `SHA256SUMS.txt` 文件自身的 SHA-256，与 clean-checkpoint 验证身份一致。
4. 逐条验证来源 `SHA256SUMS.txt` 的 184 个 payload Hash。
5. 逐条验证来源 `MANIFEST.txt` 的 179 个路径和文件长度。
6. 使用 `Copy-Item -LiteralPath <source-package> -Destination <delivery-root> -Recurse` 原样复制五个目录；
   没有使用覆盖或强制参数。
7. 在目标重复步骤 1～5。
8. 比较 source/target 的五组相对文件集合，再对全部 189 个文件逐一比较 SHA-256；0 mismatch。

仅核对包内 `SHA256SUMS.txt` 不足以证明整个复制，因为该文件不包含自己的 Hash；本次额外比较了它自身和
全部 189 个 source/target 文件。

## 4. 格式与使用边界

- source 包继续是 81 文件白名单 SDK，不是 Git checkout：不含 `.git`、`out`、build、tests、tools、
  spikes 或 Qt；`third_party` 仅含 yyjson。
- static/shared 的 Debug、Release 仍为四个独立包，没有合并配置；shared 包内各自保留原匹配
  `bin/pae.dll`。
- 包内 `PAE-SDK-README.md`、`PROVENANCE.json`、`MANIFEST.txt`、`SHA256SUMS.txt` 未改写。
- 本地归集不形成新 package identity。功能证据仍来自
  [`clean-checkpoint SDK 五包与包外消费验证`](pae-sdk-clean-checkpoint-validation.md)。
- 复制通过不等于重新执行六组 consumer、Qt Lab、正式发布、稳定 ABI、Linux、许可、生产或现场验证。
- 来源候选仍保留；其后续删除必须依赖生成物盘点、目标复核和用户精确删除授权。

## 5. Git 与停止点

本次允许的仓库内持久变更仅为本报告及对
[`仓库结构审查`](repository-organization-audit-20260919.md)目标路径的纠正；`deliverables/sdk/` 内容被明确
忽略。`.gitignore` 由总控维护，本任务没有修改该文件。

未 Stage、Commit、Push、正式发布或删除。状态：已完成派发范围，待总控复核。
