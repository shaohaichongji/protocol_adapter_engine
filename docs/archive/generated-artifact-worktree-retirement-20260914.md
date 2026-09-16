# 旧 Lab worktree 退役记录（2026-09-14）

> 历史状态提示：用户随后已永久删除外层 `工程整理归档`；下述三个外层 JSON 路径现已失效，文件名和 Hash 仅供追溯当时操作。

## 结果

- 已通过 Git 退役唯一目标 `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine_lab_ui`；退役后该路径不存在，`git worktree list` 只剩主仓 `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`。
- 退役前旧 worktree 为 `feat/lab-ui-c1@8bfe50fcb71a2e572bdc907b7392803038e6afdb`，该提交是 `main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc` 的祖先；tracked 与普通 untracked 状态为空，ignored 仅有 `out/`，全树 reparse point 为 0，未发现依赖该路径的构建或 Lab 进程。
- 使用 `git worktree remove --force` 是因为唯一未提交内容为已核验的 ignored 构建树。没有手工移动 `.git`，没有删除、改写或合并分支。
- 旧 `out/` 共 4747 个文件、2,443,134,288 逻辑字节（2.275 GiB），随 worktree 永久移除，不在回收站；这不是磁盘物理可用空间的测量。

## 证据保存

- 前三轮已归档的旧 worktree 证据为 41 项、191,733 字节，退役前后均重新核对目标大小和 SHA-256。
- 本轮补充归档 18 项、2,616,839 字节：10 个此前未归档日志、1 个人工提交说明、1 个独立最大复现输入，以及 6 个没有找到 tracked 同内容的运行/测试配置副本；复制与目标端 SHA-256 校验 18/18 通过，冲突 0。
- 构建树中另有 252 个测试/配置副本与保留分支 tracked 文件字节一致，没有重复复制为新证据；未发现截图、源码压缩包、脚本或 packet capture。

归档与退役证据：

- 补充归档结果 `old-worktree-retirement-archive-result.json`，SHA-256：`DD7F5A8FE9D13F1AC97E59343DA862FFA82FA55281A22EACC3A79908902ADE4F`；原外层路径现已失效。
- 退役前检查 `old-worktree-retirement-preflight.json`，SHA-256：`52DDD925C3F073E17971BB73B6BA1B0E3377F4CFB8ADD873D9A8CCA39460E12B`；原外层路径现已失效。
- 退役结果 `old-worktree-retirement-result.json`，SHA-256：`DE395D96025B8062D22E2D2A813F24EEB87028DD396BEDD0456CBDEE050C05B9`；原外层路径现已失效。

## 当前入口与历史路径

当前日常工作、构建和启动统一使用主仓：

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`；
- Windows preset：`windows-msvc-pae-lab`；
- Release 启动：`out\build\windows-msvc-pae-lab\out\protocol_lab_ui\Release\pae_protocol_lab_ui.exe`。

历史验证文档中的旧 worktree 路径保留原样，作为当时证据定位，不做全局替换；其文件映射由上述归档清单追溯。

## 分支与恢复

本地 `feat/lab-ui-c1` 与远端 `origin/feat/lab-ui-c1` 均保留在 `8bfe50fcb71a2e572bdc907b7392803038e6afdb`。若确需恢复源码 worktree，应先选择新的明确路径，再执行类似：

```powershell
git -C F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine worktree add <new-path> feat/lab-ui-c1
```

该命令只恢复分支源码；ignored 构建树需要重新生成。当时历史证据可从本地归档读取，但该外层归档后续已由用户永久删除，现只能保留本文中的映射和 Hash 记录。本轮未构建、测试、启动 Lab、Stage、Commit 或 Push。
