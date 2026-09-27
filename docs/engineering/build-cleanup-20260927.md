# 五个旧构建根清理记录

## 范围与保护

用户于 2026-09-27 授权执行上一轮列出的五根清理。仓库基线 `main@adeae30d942ba42cc518c244c220730b7e462d2c`，已有体验包文档和脚本未提交，全部保留；本轮不 Stage、Commit 或 Push。

精确对象（均相对于仓库根目录）：

- `out/build/lab-yaml-sdk-migration-20260926`
- `out/build/lab-yaml-minimal-entry-20260926`
- `out/build/windows-msvc-lab-compile-diagnostics-20260921`
- `out/build/yaml-sdk-packaging-20260926`
- `out/build/yaml-sdk-component-20260926`

保护 `deliverables/sdk`、`deliverables/lab` 全部文件；当前 adeae30 和前一 b12ad80 的构建根、源码、测试、示例与 third_party 不在删除清单。共享任务状态及本机构建/Lab进程在执行前核对。

## 证据归集方式

本地证据根：`deliverables/evidence/cleanup-five-20260927`（Git 忽略）。

- `inventory.csv`：五根文件相对路径、长度、修改时间；删除前重新比较，变化则停止。
- `files/` 与 `evidence.csv`：保留日志、文本、配置、CMake、JSON/YAML、XML、脚本、差异、清单及哈希文件，复制后 SHA-256 核对；删除前再次核对。
- `protected.csv`：全部现有 SDK/Lab 交付文件 SHA-256；删除前后比较。
- `summary.json`：删除对象文件数及逻辑字节数。

不保留旧 obj、PDB、库、EXE 等完整中间二进制；删除后不能原样恢复旧构建树，需按历史源码和工具链重新构建。历史验证报告中的原路径保持历史身份，不再意味着目录仍存在。

## 执行结果

五个精确根已删除，执行后确认全部不存在。共移除 29,434 个文件，逻辑长度 5,871,173,255 bytes（约 5.468 GiB）。证据归集目录约 44.612 MiB，净减少逻辑文件长度约 5.424 GiB；实际磁盘可用空间受文件系统分配等影响，不作等量承诺。

2,685 份文本证据复制及删除前 SHA-256 核对通过；1,125 份现有 SDK/Lab 交付文件删除前后 SHA-256 全部一致。目标内重解析点检查通过，删除前目录清单未变，三个执行任务 idle 且未发现构建/Lab 进程。最新及前一批构建根均保留。

交付索引已移除退役 dirty 候选的可执行启动指引。未重跑产品构建、功能测试或人工 UI 验收；清理校验不扩大既有产品验证结论。旧中间二进制未备份、未进回收站，不能原样恢复；已归集证据可继续查阅。

后续遵循[两批保留规则](build-artifact-retention.md)。
