# 推荐 Lab preset 配置依赖修复

## 原因与修改

基线 `320a3cc`，保留总控此前七份入口文档修改。
`windows-msvc-pae-lab` 开启 Qt UI，但未开启 `PAE_BUILD_PUBLIC_API_STAGE1`。
UI 的 owned presentation 和 ASCII compatibility 目标已链接 `PAE::pae`，
该 target 仅在公开 API 子目录启用后创建，因而清洁配置存在 Generate 失败。

- 推荐 preset 显式添加 `PAE_BUILD_PUBLIC_API_STAGE1=ON`。
- 根 CMake 对 UI 开启而公开 API 关闭的组合提前 FATAL_ERROR，不静默覆盖用户选择。
- 不切换其他 Lab 公开迁移开关，不修改执行语义或系统 Qt。

## 实际验证

在此前不存在的 `out/build/preset-closure-20260922/` 下分别配置：

```powershell
cmake --preset windows-msvc-pae-lab -B out/build/preset-closure-20260922/positive
cmake --preset windows-msvc-pae-lab -B out/build/preset-closure-20260922/negative -DPAE_BUILD_PUBLIC_API_STAGE1=OFF
```

- 正例退出 0：Configuring done、Generating done；MSVC v142 14.29.30133。
- 负例退出 1：精确命中 `PAE_BUILD_PROTOCOL_LAB_UI requires PAE_BUILD_PUBLIC_API_STAGE1=ON`。
- 日志：`out/preset-closure-positive-20260922.log`、`out/preset-closure-negative-20260922.log`。

仅验证 Configure/Generate 与依赖门禁；未编译应用、运行 CTest/Lab、重建 SDK，
未实际操作 VS Code 或验证 IntelliSense 红线消失。未删除用户失败目录、修改已有缓存、
部署或全局环境；无 Stage/Commit/Push。

用户可在 CMake Tools 中重新选择推荐 preset 并执行 Configure。
若仍有 IntelliSense 问题，应继续检查新的 Configure 输出与配置提供者，不据本次结果直接宣称编辑器已恢复。

## 用户反馈与提交收口

2026-09-22 用户确认“目前已经解决了这个问题”，随后授权本轮十个文件本地提交。
该反馈关闭此次用户报告的配置/编辑器问题，不补充未执行的应用构建、全量测试或运行验证。
上文无 Git 写操作为修复验证阶段状态；本次授权仅提交，不推送。
