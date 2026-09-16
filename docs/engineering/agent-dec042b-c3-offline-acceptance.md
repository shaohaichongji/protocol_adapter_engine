# DEC-042B C3 代理执行离线验收

日期：2026-09-08。执行方式：代理运行实际CLI并独立核对，不是用户人工操作。

## 结论

`AGENT_OFFLINE_ACCEPTANCE=PASS`：修复Windows CLI路径输入/输出编码边界后，代理使用包含中文及
空格的绝对配置、Values和record-root，完成七项离线验收，35项精确检查全部通过。
`USER_MANUAL_ACCEPTANCE=NOT_EVALUATED`。本结果是代理自动执行证据，不替代用户人工验收，
2026-09-08用户确认以该代理离线验收通过作为本功能检查点收口依据；DEC-042B C3离线功能检查点
已收口。该决定不将用户人工状态改为PASS，也不代表完整PAE V0.1或生产准入通过。

## 基线与证据

- 验证时Git基线：`386a8bb7a1b0e21453f6b80faa3283e513f4a02e`，当时包含未提交修复；
  该修复随后随`df43165`提交并Push。此次收口沿用原验证证据，没有重新运行验收。
- 可执行文件：`out/dec042b-c3-release-msvc/tools/protocol_lab/pae_protocol_lab.exe`。
- EXE SHA-256：`0C213C43EF595980B2DC543A3E03FC259B50C61E0C38AC80E6F18BEF7ABCCE3E`。
- 修复后证据根：`out/agent-c3-acceptance-20260908-212535`，均留在Git之外；`record.json`
  保存命令、退出码和35项检查，`verify-run.log`保存执行日志。
- 修复前原始证据根`out/agent-c3-acceptance-20260908-205122`保持不变：绝对中文路径Encode成功，
  但stdout不是严格UTF-8，返回路径无法按UTF-8原样消费；相对路径复核曾为七项功能提供PARTIAL证据。
- 独立修复前复现位于`out/dec042b-c3-unicode-pre-fix-中文 路径/pre-fix-evidence.json`：stdout
  严格UTF-8解码失败，而按当前Windows ACP解码可恢复真实存在的Bundle路径。该证据将问题限定在
  CLI窄字符串路径输出边界，不追溯改写旧验收记录。

## 七项实际结果（中文及空格绝对路径）

| 项 | 核对结果 |
| --- | --- |
| Encode | 退出0；固定Frame精确一致；temperature coefficient=123、scale=1、raw=523 |
| Inspect | 退出0；全部交付字段与Encode结果一致 |
| Replay两次 | 两次退出0/EQUAL；父子run_id实际连续；指定Pipeline及查询次数1核对通过 |
| 等价Decimal | 原Values文件Hash不同；Compare退出0/EQUAL，零新增Run目录 |
| 改值Compare | 退出6/DIFFERENT，诊断PAE_LAB_C3_COMPARE_DIFFERENT |
| SUM8失败 | 默认退出5；期望匹配退出0；两份Result仍失败、exit_code=5，执行指纹相同 |
| 损坏Bundle | 仅损坏新建副本且不更新Hash；退出3、证据拒绝、发布路径null，Run目录数不变 |

第七项是低层Hash拒绝检查，不冒充自洽语义篡改测试；原始Bundle未修改。所有stdout均按原始
字节执行严格UTF-8解码后再解析JSON，`published_bundle`原样用于Inspect、Replay和Compare，
未通过手工转码或改写路径绕过问题。

## 路径问题及后续

根因是Windows上`std::filesystem::path::generic_string()`把本地代码页字节直接写入JSON，违反
CLI JSON必须为UTF-8的边界；输入侧同时缺少对UTF-8窄参数与PowerShell/CRT本地代码页参数的明确
桥接。修复后路径输出统一为`generic_u8string()`；输入先识别严格UTF-8，否则保留Windows本地
代码页兼容。未修改全局代码页、旧Evidence格式、指纹、Core或网络行为。

未执行网络、UDP、Git写操作、清理、Linux、Oracle、真实协议、硬件、现场或性能验证。
