# Spike Results

人工复核后的实验结论和环境清单放置于此。原始生成输出位于`generated/`并被 Git 忽略，避免机器路径和临时日志进入版本历史。

- [Windows/MSVC 首轮证据报告](windows-msvc-2026-initial.md)：三个候选的严格性、资源边界、合成协议形态、体积、工作集和耗时对比；最终选型仍保持 OPEN。
- [Windows/MSVC 第二轮门禁报告](windows-msvc-2026-parser-gate.md)：yyjson 解析期固定内存池、逐分配点故障注入、稳定 JSON Pointer 和更新后的 Windows 证据；Linux 门禁仍未完成。
- [Windows/MSVC Strict JSON Profile 门禁报告](windows-msvc-2026-strict-profile.md)：100 个定向用例、Unicode 代理项公共预检、2 个 Number Token 已知契约缺口，以及重新执行的构建、CTest、体积和 Benchmark 证据。
- [Windows/MSVC Number Token 与机器语料门禁报告](windows-msvc-2026-number-token-corpus.md)：55 个独立 Token 用例、首批 14 个清单化文档用例、Raw Number 逐字节保真和候选目标锁。
- [Windows/MSVC Corpus、offset 与 Vendor 候选门禁报告](windows-msvc-2026-corpus-offset-vendor-gate.md)：第二批 14 个文档用例迁移、精确 offset、5 项清单生成器负向门禁和 yyjson 候选来源/许可证复核；Linux 实测按当前顺序暂缓。
