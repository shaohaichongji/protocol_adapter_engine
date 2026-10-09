# 中文配置教学增补验证（2026-10-02）

## 第二轮教学增补 v2

当前首选为 `deliverables/sdk/adeae30-tutorial-v2/PAE-Lab-Windows-x64-adeae30.zip`，教学身份 `chinese-config-tutorial/2`。下文v1记录保留，不覆盖其哈希与当时结果。

- 新增03构建运行、04常用字段速查、05协议表到SDK练习；统一docs与deliverables入口，加入借用视图等接入提醒。
- 教学程序增加内存故障注入并断言MISSING_PROPERTY、UNKNOWN_REFERENCE；Binary测试增加短记录拒绝。未修改合法配置的磁盘内容。
- 首次增量构建发现MSVC默认代码页不能正确处理新中文注释；目标级添加 `/utf-8` 后构建成功，不改全局环境。
- 仓库教学目录Release测试4/4；再以新包tutorials为源码、新包static Release SDK为依赖，在新根 `out/build/config-tutorial-v2-package-20261002` 配置、构建、CTest，4/4通过。工具链沿用VS18/v142 14.29.30133。
- 新包310个payload哈希逐一核对通过，SDK/Lab原有290文件与基包一致，教程11处Markdown相对链接全部存在；`git diff --check`通过。
- ZIP SHA256：`c2c3edf5c983e5915d670f5655a9a7c59fafdc9f38b00764baa0dbf4cb477e8e`。
- 未进行人工UI验证、Mermaid视觉渲染、Debug或Linux验证，未重建SDK/Lab；未Stage/Commit/Push。本轮未删除旧包。

## 第一轮教学增补 v1

## 变更边界

新增 `docs/experience/tutorials/` 两份完整中文合成配置、三份阅读文档和可执行公开Codec验证程序；补充体验导航及仓库配置入门入口。未改PAE、Lab或原有测试配置。教程说明结构引用、偏移/字节宽度/字节序、收发模板及完整记录与流式的区别，含Mermaid图。

`scripts/package_experience_bundle.ps1` 后续归集包含tutorials；本轮使用 `scripts/package_tutorial_overlay.ps1` 从已有adeae30体验包派生新目录，先验证基包哈希，拒绝覆盖既有目标。原SDK包内provenance和清单不变；新包顶层身份、清单、哈希及ZIP独立生成。

## 实际验证

- 新构建根 `out/build/config-tutorial-20261002`，Windows x64、VS18、v142 14.29.30133，消费原体验包static Release SDK；配置与Release构建成功。
- `ctest --test-dir out/build/config-tutorial-20261002 -C Release --output-on-failure`：2/2通过。
- Binary：配置编译、AA 01 02解析逻辑值258、类型化258组包回AA 01 02、AB 00 07拒绝。
- ASCII：配置编译、PING+CRLF解析成功且0字段、空输入字段组包为PONG+CRLF。
- 对新包内两份配置再次执行验证程序，各输出TUTORIAL PASS。
- 新包307个payload文件SHA256逐一核对通过；SDK/Lab共290个文件与基包逐一哈希一致。
- `git diff --check` 通过；本轮没有Stage/Commit/Push。

产物：`deliverables/sdk/adeae30-tutorial-v1/PAE-Lab-Windows-x64-adeae30.zip`。
ZIP SHA256：`c0b7079ff6da949dbff3f91321aedb81a056774a7cdbef46f524330690dcb7d1`。

## 未验证与限制

未重建SDK/Lab、未做新人工UI操作、Debug、Linux或真实设备验证；既有Qt再分发许可边界不变。
流式章节说明沿用已有配置规则，本轮未重跑流式路径。教程不是完整字段字典，复杂校验与变长规则仍需查对应Schema及专项示例。
本地包目录被Git忽略；脚本与教学源纳入工作树，提交源码并不自动上传ZIP。
