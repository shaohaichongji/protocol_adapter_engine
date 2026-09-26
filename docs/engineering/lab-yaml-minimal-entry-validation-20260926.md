# Lab YAML 最小接入验证（2026-09-26）

状态：**已完成派发范围，待总控复核**。基线 `main@4da54b9`；本片仅改变仓库内可选 Qt Lab 入口，不代表 installed SDK、standalone 或现用部署支持 YAML。总控同步修改的 `docs/engineering/yaml-entry-plan-20260926.md` 与新增契约不属于本任务写入。

## 实施范围

- 根 `CMakeLists.txt` 增加默认 `OFF` 的 `PAE_BUILD_PROTOCOL_LAB_YAML_ENTRY`，显式要求 UI、内部 YAML 前端、公开 API 和至少一个公开 Lab 编译路由；`tools/protocol_lab_ui/CMakeLists.txt` 仅在 ON 时链接 `pae_yaml_frontend`。JSON-only 不编译该前端。
- `tools/protocol_lab_ui/application_window.cpp`、`document_tab.cpp/.h`：ON 时文件对话框接受 JSON/YAML，文件后缀大小写无关识别 `.yaml`、`.yml`；其他后缀保留 JSON 行为。原路径/标题不变；原 YAML 读取按前端 16 KiB 输入限额预检。重载确认后清旧生成物，取消仍保留旧状态；关闭清映射。
- `tools/protocol_lab_ui/compile_worker.cpp/.h`：在原有文档 ID/revision/最多两个 pending 的后台 worker 内转换；前端 `Limits` 不调高（输入 16 KiB、生成 JSON 32 KiB、解析与辅助各 128 KiB 等）。仅路由到构建中实际启用的公开编译器，私有路由明确拒绝。`config_sha256` 继续针对生成 JSON；成功结果携带内存 JSON/来源映射，Host 重新准备提交同一生成 JSON。JSON 请求原链路不变。
- 下游结构化诊断仍保留 JSON Pointer、生成 JSON byte offset；前端 `Find/FindNearest` 给出 YAML 行列，容器回退标记“近似”，无位置标记“未映射”。当前前端转换错误没有行列，只显示状态/reason 与“未提供源码位置”；不扩公共诊断 ABI。PlainText 展示不变。
- `tests/protocol_lab_ui/CMakeLists.txt`、`yaml_entry_tests.cpp`、`yaml_ui_smoke_tests.cpp` 新增定向测试。Qt 测试不显示窗口；覆盖原文件标题/路径、大小写后缀、未知后缀不回退、重载、失败清理、超限预检、转回 JSON、Host 生成文本复用。Worker 测试覆盖 Binary Schema 0.6 公开 legacy 与 ASCII 0.10 的真实 Codec Decode/Encode、JSON 原路径、哈希、公开路由拒绝、前端失败、精确/近似/未映射诊断以及关闭/旧 revision 隔离。

## 实际命令与结果

所有构建/证据位于 `out/build/lab-yaml-minimal-entry-20260926/` 和 `out/evidence/lab-yaml-minimal-entry-20260926/`。使用 `windows-msvc-pae-lab` preset、Visual Studio 2026 的 v142 `14.29.30133`、仓库 Qt 5.13；ON 组合显式启用内部 YAML 前端、H2/A2 与公开 legacy，未更改系统 Qt 或全局 PATH。

| 验证 | 命令要点 | 结果与日志 |
| --- | --- | --- |
| ON 配置 | `cmake --preset windows-msvc-pae-lab -S . -B out/build/lab-yaml-minimal-entry-20260926 -DPAE_BUILD_YAML_FRONTEND=ON -DPAE_BUILD_PROTOCOL_LAB_YAML_ENTRY=ON`，另显式开启 H1/H2、A1/A2、公开 legacy | Configure/Generate 成功；`configure-on-public-legacy.log`。初次未启公开 legacy 的成功配置保留于 `configure-on.log`，不能用于 Schema 0.6 测试身份。 |
| ON Debug/Release 构建 | `cmake --build out/build/lab-yaml-minimal-entry-20260926 --config Debug/Release --target pae_protocol_lab_ui_yaml_entry_tests pae_protocol_lab_ui_yaml_ui_smoke_tests pae_protocol_lab_ui_compile_queue_tests pae_protocol_lab_ui_document_state_tests pae_protocol_lab_ui_schema_dispatch_tests pae_protocol_lab_ui --parallel 4` | 两配置均成功；最终源码对应 `build-final-Debug.log`、`build-final-Release.log`，Qt smoke 末次编译另见 `build-ui-json-return-{Debug,Release}.log`。Release 测试目标日志明确 `/UNDEBUG` 覆盖 `/DNDEBUG`。 |
| ON 定向 CTest | `ctest --test-dir out/build/lab-yaml-minimal-entry-20260926 -C Debug/Release -R <五项 Lab 专项名称正则> --output-on-failure` | Debug **5/5**、Release **5/5**；最终源码对应 `ctest-delivery-final-Debug.log`、`ctest-delivery-final-Release.log`。正则及每项名称见原始日志。 |
| JSON-only 独立配置/构建 | 同 preset，`-B out/build/lab-yaml-minimal-entry-20260926/json-only -DPAE_BUILD_YAML_FRONTEND=OFF -DPAE_BUILD_PROTOCOL_LAB_YAML_ENTRY=OFF`；Debug 构建 UI、compile_queue、document_state | 配置/构建成功，既有 CTest **2/2**；`configure-json-only.log`、`build-json-only-final-Debug.log`、`ctest-json-only-final-Debug.log`。缓存两项均 OFF、无 `src/config_frontend_yaml` 目标目录。 |
| 缺前端负例 | ON 入口、OFF 前端，独立 `missing-frontend` 构建目录 | Configure 退出 1，明确提示需要 UI/YAML frontend/public API；`configure-missing-frontend.log`。 |
| 无公开路由负例 | ON 入口/前端，不启公开 Lab 路由，独立 `missing-public-route` 目录 | Configure 退出 1，明确提示至少一个公开 Lab compiler route；`configure-missing-public-route.log`。 |

历史失败仍保留：`ctest-yaml-Debug.log` 是中文路径测试夹具误用窄字符 `std::filesystem::path` 的修复前失败，后改 `u8path`；`build-ui-smoke-Debug.log` 是新增 Qt 测试首次缺 `PAE_UI_FIXTURE_DIR` 定义的构建失败。它们不是最终源码的通过证据，不删除或覆盖。

## 验证边界与风险

- 自动证据只覆盖上述仓库内目标与合成配置。没有运行全仓测试、正式 `--ui-smoke` 应用矩阵、SDK 包外消费、standalone、真实协议或人工可见窗口体验；当前 `out` 候选不是发布物，未覆盖现用部署。
- Binary 正例使用公开 legacy Schema 0.6；H2/Schema 0.9 构建成功但本片未增加独立 0.9 YAML 夹具。ASCII 正例为公开 Schema 0.10。其他 Schema/路由若未启用，按当前组合拒绝；不能由这两个正例推断全部协议支持。
- 前端语法错误仍缺原 YAML 行列，这是既有前端接口边界；下游来源映射可精确或容器近似，不能将生成 JSON offset 当作原 YAML offset。后续若要求语法级定位，应先由前端单独提供可靠位置，不在 Lab 猜测。
- 取消重载沿用原有确认前早退路径，本片未制造带半帧/待确认状态的 YAML 交互用例；该特定场景仍需后续短体验或专项补证。旧 completion 使用真实 YAML 转换与公开编译结果做了 revision 拒绝测试，关闭后待处理结果清理亦有专项断言。
- UI 保留的生成 JSON 和来源映射为有界内存对象；未将逻辑限额声称为 RSS 上界。更多并发/低内存压力与真实用户配置仍待后续专项。未更改 SDK 安装规则、standalone 子目录或部署配置白名单。

本片未 Stage、Commit、Push、删除、发布。交接后停止写入，等待总控核对差异与证据。
