# PAE 公开 COMPLETE_RECORD Codec 首片 Windows 验证

状态：2026-09-14 已完成授权范围实现与本机 Windows x64 Debug/Release 验证，
待总控复核。

## 1. 实施范围

- 新增公开 `pae/codec.h`，通过 `PAE::pae` 提供 COMPLETE_RECORD Decode/Encode。
- 编译 owner 与 Codec 共享内部不可变 `CompiledState`；每个 Codec 独占 Core
  `ExecutionWorkspace`，owner 移动或销毁后 Codec 仍可执行。
- Decode 只在成功时发布短期 borrowed record/field view；BYTES 额外借用输入 Frame。
  下一次成功获得 facade guard 的 Decode/Encode 使旧 view 失效，失败不发布旧成功结果。
- Encode 使用调用者 Buffer，接受 UINT64/INT64/BOOL/BYTES/已知 ENUM/Decimal64
  的显式 typed value，不做隐式数值转换。
- facade busy guard 先于 generation、slot、raw 状态和 Encode 映射缓存取得；竞争调用立即
  返回 `WORKSPACE_BUSY`，不改写活动调用的发布状态。
- Decode slot 和 Encode 映射采用创建时精确容量数组，不依赖实现可能超额的
  `vector::capacity()`。Encode 输入数超过 Plan 的 `max_fields_per_message` 时，在读取输入前返回
  `INPUT_VALUES_TOO_MANY`。
- 公开适配层显式映射 Core status/conversion error；Decimal 保持精确 coefficient/scale，
  转换 raw integer 直接读取 Core workspace 发布值，不从 logical value 反算。

本片未修改 Schema、Plan 数据形状、Core 协议逻辑、Framer/Host/Lab 行为或历史 evidence 格式。

## 2. 测试覆盖

`tests/public_api/public_codec_tests.cpp` 在 Debug/Release 各执行 47 个精确断言，覆盖：

- owner 销毁后执行、空 owner 拒绝、同 Plan 多 Codec/workspace 隔离；
- Binary 和 ASCII 独立预期 Encode/Decode；UINT64、INT64、BOOL、BYTES、ENUM、Decimal64；
- 已知枚举 selector、枚举 raw/已知索引、Decimal logical/raw 直接发布；
- Decode slot 不足、Encode Buffer 不足、typed mismatch，以及失败时零交付；
- 下一次调用使 view 失效、BYTES 指向输入、其他 workspace 不使 view 失效；
- 创建资源报告分类精确求和、上限精确通过/少 1 byte 拒绝，以及 Encode 映射容量超限
  先于 null 输入检查返回，证明该路径不读取输入项；
- 创建后首次及重复执行无 heap allocation；可确定的同 Codec 竞争先于缓存改写返回 busy。
- move-construction 后来源 view 失效，move-assignment 后来源和目标的 record/field view
  均失效；moved-from owner 返回 `INVALID_COMPILED_PROTOCOL`，复用时旧 view 不复活，
  移交后的 Codec 可发布新 view。

公开头测试另检查 `codec.h` 单独 include 和私有路径边界。示例和仓库外
consumer 均只 include `pae/*`、只链接 `PAE::pae`。

## 3. 实际命令与结果

主验证目录：`out/build/windows-msvc-public-codec-stage1`。使用 Visual Studio 18 2026、
x64、`v142,version=14.29.30133`：

```powershell
cmake -S . -B out/build/windows-msvc-public-codec-stage1 `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON -DPAE_BUILD_JSON_PARSER_SPIKE=OFF `
  -DPAE_BUILD_PUBLIC_API_STAGE1=ON
cmake --build out/build/windows-msvc-public-codec-stage1 --config Debug -- /m:1
ctest --test-dir out/build/windows-msvc-public-codec-stage1 -C Debug --output-on-failure
cmake --build out/build/windows-msvc-public-codec-stage1 --config Release -- /m:1
ctest --test-dir out/build/windows-msvc-public-codec-stage1 -C Release --output-on-failure
```

- Debug：全矩阵 32/32 PASS；公开 Codec 最终直接运行 47/47 PASS。
- Release：全矩阵 32/32 PASS；公开 Codec 最终直接运行 47/47 PASS。
- Binary/ASCII 示例：Debug/Release 均 `PUBLIC_CODEC_EXAMPLE gate=PASS`。

断电前曾在只构建公开 API 定向 target 后直接运行全量 CTest；当次 32 项中 24 项
`Not Run`，另1项 wrapper 因 Core 可执行文件不存在报失败。原日志
`out/public-codec-stage1/ctest-debug-full.log` 保留为构建前置不满足证据，不计作代码回归。
完整构建后同配置的最终日志为：

- `out/public-codec-stage1/build-{debug,release}-all-after-recovery.log`；
- `out/public-codec-stage1/ctest-{debug,release}-full-after-build.log`；
- `out/public-codec-stage1/public-codec-{debug,release}-direct-final.log`；
- `out/public-codec-stage1/stage1-{debug,release}-example-with-inputs.log`。

### 3.1 move 后旧 view 复活 P2 增量复核

总控复核后增加 A/B 两个 Codec 各自首次 Decode、`generation == 1` 的定向场景。修复前
`CompleteRecordCodec` 使用默认 move-assignment，view 只保存 owner 指针和 Impl generation；
`A = std::move(B)` 后 A 的旧 view 会把 B 的同代次结果当成自己的结果。复用 moved-from B
时同样可复活旧 B view。修复前 Debug 精确结果为 43 PASS / 2 FAIL：

- `move_assignment_invalidates_source_and_destination_views` FAIL；
- `moved_from_reuse_does_not_revive_old_views` FAIL。

证据为 `out/public-codec-stage1/move-view-pre-fix-{build-debug,debug}.log`。

修复在 facade owner 中增加不分配的 `view_epoch_`，record/field view 同时捕获 owner epoch
与 Core generation。move-construction 推进来源 epoch；move-assignment 推进来源和目标 epoch；
因此 Impl generation 相同也不能使旧 view 重新通过 `HasValue()`。没有延长 view 到 owner
销毁之后，view 读取与 owner move/执行仍要求消费者同步。

修复后 Debug/Release 各 47/47 PASS，包括 record 与 field、move-construction、move-assignment、
moved-from 空状态、复用不复活和新 view 正常发布。公开 API 相关 CTest 在两配置各
4/4 PASS。日志：

- `out/public-codec-stage1/move-view-final-{build-debug,build-release,debug,release}.log`；
- `out/public-codec-stage1/move-view-final-public-api-{debug,release}.log`。

公开头和类布局发生变化，因此另增量重编译仓库外 consumer 和 Product-only 示例；
Debug/Release 均编译成功并运行退出 0，日志为
`move-view-external-{build,run}-*.log` 与 `move-view-product-{build,run}-*.log`。

该 P2 只修改公开 facade/头文件及公开测试，未改 Core 协议逻辑。因此本轮没有重跑
无关 Lab 全构建；上文 32/32 是首片主实施的当次最终证据，P2 修复后的增量证据是公开 API
4/4 和直接 47/47，不将前者追溯改写为后者本次重跑。

独立 consumer 源文件位于已忽略的 `out/public-codec-stage1/external-consumer-src`，单独
CMake project 以 `add_subdirectory` 消费源码树的 `PAE::pae`，没有私有 include 路径。Debug/Release
编译并运行退出 0：

- `out/public-codec-stage1/external-build-{debug,release}-after-recovery.log`；
- `out/public-codec-stage1/external-run-{debug,release}-after-recovery.log`。

Product-only 配置启用公开 API，关闭两个 testing 开关和 Lab；Debug/Release 构建成功，
CTest 均注册 0 项，示例均运行通过。生成 target 包含公开 API/示例，不包含 Lab/Qt。
Lab-on/Testing-off 使用独立旧 Lab 配置，不启用本公开 API；Debug/Release 构建成功且
CTest 均注册 0 项，证明根 CMake 接线未破坏该隔离边界。日志：

- `out/public-codec-stage1/product-only-*`；
- `out/public-codec-stage1/lab-no-tests-*`；
- `out/public-codec-stage1/isolation-target-scan.log`。

全部相关 C++ 文件通过 Visual Studio 随附 `clang-format --dry-run --Werror`；已跟踪差异
通过 `git diff --check`，未跟踪候选文件的等价 `--no-index --check` 无空白错误（三个
CMakeLists 仅有 Git 的 LF/CRLF 工作区提示）。`CMakePresets.json` 通过 `cmake --list-presets`
解析；全部新增候选文件的本机绝对路径、内网端点、凭据和客户关键词扫描为 0 匹配。
证据日志及独立 consumer 目录均由根 `.gitignore` 的 `/out/` 规则忽略。

## 4. 证据边界与未验证项

- 本次是仓库源码树/静态目标的 Windows x64 验证；尚未实现或验证 install/export 包、
  独立静态包、DLL 导出与 ABI，它们属于后续交付阶段。
- 未执行 Linux、真实协议 Golden、真实设备、硬件、现场、非 Loopback 网络或性能验收。
  本片未发起任何网络收发。
- `ExecutionMemoryReport` 是已分类的逻辑预算，不是进程 RSS 硬上限；不包含共享且已在
  编译 owner 中计费的不可变 Plan/metadata 存储，仅单列其 facade 保活成本，避免重复计费。
  Core workspace 部分沿用 Plan 的 `estimated_workspace_bytes`；新增 Decode/Encode 数组按元素
  `sizeof * capacity` 精确计费。不包含通用 allocator 元数据。
- status 与 conversion error 是显式 `switch` 映射；自动化覆盖首片关键可达路径，
  不将 47 个 case 数量解释为所有内部 status 均已独立注入。
- Lab 尚未迁移为消费该公开 Codec；Lab 全量物理范围和 Encode 同调用观察字段是后续
  消费侧复核项，本片未通过重复 Decode 或 raw 反算规避。

本次未 Stage、Commit 或 Push，当前结论为“已完成派发范围，待总控复核”。
