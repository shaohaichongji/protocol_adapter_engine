# MSVC source charset 消费兼容修正验证

日期：2026-10-09。状态：已完成派发范围，待总控复核；不是提交或发布批准。

## 1. 问题与接管

当前仓库 `main@3facda71a53f57d97b2d1b497e19c7624b846676`，暂存区为空。
接管时 32 个已跟踪修改文件，连同展开的未跟踪文件合计 49 个既有文件。
本批保留教程、前两批公开注释及第三批 Lab 注释；不改其执行逻辑或历史报告。
允许重叠的六个既有编码配置/测试/SDK 文档文件只作本轮最小增量，其余 43 个既有文件
最终 SHA256 均与接管一致。根 CMake 原为干净文件，本批仅改内部编码选项。

此前 Lab 的 `lab-lifecycle/build-Debug.log` 与 `build-Release.log` 中 D8016 是历史失败证据。
本次另建独立 OBJECT 最小工程，以同一个中文 consumer fixture 同时传入 `/utf-8` 和
`/source-charset:utf-8`，v142 Debug/Release 均实际返回 D8016，构建退出码 1。
没有把首次公开 API 默认消费通过当作已有 `/utf-8` 宿主的兼容证据。

证据根为忽略目录 `out/build/comments-public-20261009/charset-compat/`，下文路径相对该根。
最小复现工程、配置和失败日志为 `repro/CMakeLists.txt`、`repro-configure.log`、
`repro-Debug.log`、`repro-Release.log`；前两批及 Lab 原失败日志未覆盖。

## 2. 最小机制与宿主要求

没有做全局 flags/环境猜测，采用显式按编译目标声明的消费契约：

- 默认 `PAE::pae` 仅传播 MSVC C++ `/source-charset:utf-8`，不传播 execution charset
  或内部 warning policy。
- 若实际消费目标的 `PAE_MSVC_SOURCE_CHARSET_SUPPLIED` 为真，省略该自动 source 选项。
  属性本身不添加任何编码设置；宿主必须已经提供 UTF-8 源码读取，例如 `/utf-8`。
- 属性设在实际编译目标，不设在 `PAE::pae` 或中间 INTERFACE 库。多个编译目标各自配置，
  不全局关闭其他默认消费者的设置。不提供源码选项却 opt-out 是误用，未承诺自动防止。
- 源码 target 和新安装 `PAEConfig.cmake` 使用相同的条件 generator expression。
  本批没有宣称旧安装包会自动获得新机制。
- 根 `pae::project_options` 将 `/utf-8` 拆为等价的 `/source-charset:utf-8` 和
  `/execution-charset:utf-8`，保持仓库内部源码及窄字符串的 UTF-8 原语义。
  相同 source 选项由 CMake 去重，不借助宿主 flags 猜测或环境覆盖。

宿主已有 `/utf-8` 时的接入形状：

```cmake
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
target_compile_options(my_pae_consumer PRIVATE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>")
set_property(TARGET my_pae_consumer PROPERTY PAE_MSVC_SOURCE_CHARSET_SUPPLIED ON)
```

默认消费无需此属性；公共源码选项仍作用于整个 C++ 编译单元，宿主源文件必须使用 UTF-8。
本批不承诺 GBK 源码与 UTF-8 公共头可混作同一源码编码；保留宿主 execution charset
不等于允许任意 source charset。手工消费可自行只选择 source 设置，或按需求选择 `/utf-8`。

## 3. 本轮增量文件

1. `CMakeLists.txt`：仅拆分内部编码选项，不改依赖、开关或目标结构。
2. `src/public_api/CMakeLists.txt`：默认 source-only 选项增加按消费目标 opt-out 条件。
3. `cmake/PAEConfig.cmake.in`：新 static/shared 安装 target 同样支持该条件。
4. `tests/public_api/CMakeLists.txt`：增加三个 MSVC 编码组合及按配置/语言生成选项观察文件。
5. `tests/public_api/source_charset_consumer.cpp`：保留链接行为，增加独立窄字符串字节断言。
6. `tests/public_api/verify_source_charset.cmake`：精确检查全部 public options、安装配置无泄漏，
   并验证实际消费目标生成的 CXX 选项组合。
7. `docs/sdk/README.md`：解释默认/已有 `/utf-8` 消费契约和属性误用边界。
8. 本独立报告。

未改公开 API、Core、Lab 源码或测试、Qt、全局 PATH、教程、旧报告、原 SDK 或体验包。
不更新第三批报告为通过，也不擅自恢复 Lab 的专项验证；应由总控另行恢复。

## 4. 回归断言与实际结果

`source_charset_consumer.cpp` 的宽字面量逐字符与 Unicode escape 比较，验证源码读取。
窄字符串预期是独立的字节常量，不由被测编码反算：

- UTF-8：`中文` 长度 6 字节，逐字节 `E4 B8 AD E6 96 87`。
- 显式 CP936 execution：长度 4 字节，逐字节 `D6 D0 CE C4`。

构建成功意味着这些 static_assert 已执行编译检查；运行时还实际调用公开入口，
检查空 compiled 的创建失败行为，确保真实链接所选库而非只编译头。
默认消费不强行断言机器默认 execution charset，另用显式 `.936` 合法对照证明 public target
没有覆盖宿主指定的窄字符串语义。

| 消费组合 | 生成的 CXX 编码选项 | 验证 |
| --- | --- | --- |
| 仅 `PAE::pae` 默认 | `/source-charset:utf-8` | 中文源码读取及实际链接 |
| 已有 `/utf-8` + opt-out | `/utf-8`，没有额外 source 选项 | 源码读取、UTF-8 窄字节 |
| 仅 PAE + 显式 `.936` execution | `/execution-charset:.936;/source-charset:utf-8` | CP936 窄字节保留 |
| 仓库 project_options + PAE | 内部 source/execution UTF-8，与同值 source 选项组合 | 原内部 UTF-8 窄字节保留 |

`verify_source_charset.cmake` 检查 public target 的完整接口选项仅为条件 source 设置，
安装模板不泄漏 `/W4`、`/permissive-`、`/utf-8`、`/Zc:__cplusplus` 或 project_options。
内部组合当然仍有自己显式链接的内部 warning 选项，不将其误报为 PAE 公开接口泄漏。

本次最终执行：Visual Studio 18 2026、x64、v142 14.29.30133，Debug/Release 串行，
无 `CL`、`_CL_` 或 `CXXFLAGS` 覆盖，起止环境未变。

| 路径 | Debug | Release |
| --- | --- | --- |
| 当前仓库公开 API 专项 | 14/14 | 14/14 |
| 新源码 SDK consumer | 3/3 | 3/3 |
| 新 static 安装 SDK consumer | 3/3 | 3/3 |
| 新 shared 安装 SDK consumer | 3/3 | 3/3 |

最终配置、构建、安装、测试命令均退出 0。共享 product `ctest -N` 为 0 项；consumer
三个测试是宿主自己的测试，不是安装产品注册的测试。当前仓库另验证 project_options 组合，
外部 SDK 各验证默认、已有 `/utf-8` 和显式 `.936` 三种组合。

第一次新增选项观察文件未按语言区分，配置 Generate 失败，随后 Debug 构建成功但选项测试
13/14、CTest 退出 8；这是本次回归观察机制的实现错误，不是产品编码修正已通过。
已将观察文件输出按 `$<COMPILE_LANGUAGE>` 分离并读取 CXX 结果，最终重新配置及全矩阵通过。
保留 `repo-configure.log`、`repo-build-Debug.log`、`repo-test-Debug.log`、
`validation-transcript.log`，不覆盖失败过程或回溯改写它为有效成功证据。

## 5. 命令与来源证据

复用当前仓库 `out/build/comments-public-20261009/default-repo`，其余使用本次全新根。
新源码 SDK 是工作树白名单快照，provenance 记录上述 HEAD 与 `source_worktree_dirty=true`，
不是该提交逐字节原件，也不是原体验包。root/public CMake 和安装模板 Hash 与本次工作树一致。
static 库由该 source consumer 构建后安装，shared 库由该 source SDK 独立 product 构建后安装。
各安装 consumer 的 `PAE_DIR` 精确对应本次配置专用 SDK；公共头逐个 Hash 与当前源码一致，
shared 消费 executable 旁 DLL 与所选安装 SDK Hash 一致。没有使用前两批旧 SDK 作新证据。

主要命令形状（准确参数、13 个仓库目标及每次退出码见 `run-validation.ps1` 和 transcript）：

```powershell
cmake --preset windows-msvc-public-api-stage1 -B $RepoBuild
cmake --build $RepoBuild --config Debug --target <公开API与编码目标> -- /m:2
ctest --test-dir $RepoBuild -C Debug -R '^pae\.public_api\.' --output-on-failure
# 完成 Debug 后再执行 Release。
pwsh -NoProfile -File scripts/package_sdk_stage3.ps1 -Kind source -PackageRoot $SourceSdk -Configuration source
cmake -S $Harness -B $SourceBuild -DPAE_SOURCE_DIR=$SourceSdk -DFIXTURE_FILE=$Fixture -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
cmake --build $SourceBuild --config Debug -- /m:2
ctest --test-dir $SourceBuild -C Debug --output-on-failure
cmake --install $SourceBuild/pae-source --config Debug --prefix $StaticInstallDebug
cmake -S $SourceSdk -B $SharedBuild -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
cmake --build $SharedBuild --config Debug --target pae_public_api -- /m:2
cmake --install $SharedBuild --config Debug --prefix $SharedInstallDebug
cmake -S $Harness -B $InstalledConsumer -DCMAKE_PREFIX_PATH=$SelectedInstall -DFIXTURE_FILE=$Fixture -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
cmake --build $InstalledConsumer --config Debug -- /m:2
ctest --test-dir $InstalledConsumer -C Debug --output-on-failure
```

每个共享树严格先 Debug 后 Release；四个安装 consumer 使用各自配置专用新根。
源 fixture 来自本次仓库测试文件，外部 harness 在 out 中显式引用，不声称 SDK 含此测试文件。

主要证据：

- `takeover-status.log`、`takeover-dirty-hashes.json`、`baseline/`：接管与增量依据。
- `repro-*`：本次独立最小冲突复现。
- `repo-configure-final.log`、`final-validation-transcript.log`：最终配置、完整命令序列及退出码。
- `final-repo-{build,test}-{Debug,Release}.log`：本次公开专项 14/14。
- `final-source-*`、`final-static-*`、`final-shared-*`：新 SDK 配置、构建、安装及三种消费结果。
- `consumer/CMakeLists.txt`、各 consumer 的 `options/<配置>/CXX/`：精确 interface/evaluated 选项。
- `format-cpp.log`、`diff-check.log`、`candidate-portable.log`、`final-audit.log`：格式与最终保真核对。

## 6. 停点与局限

本批 C++ fixture clang-format dry-run、`git diff --check`、候选便携路径和 UTF-8 无 BOM
检查通过。CMake 使用现有样式并经配置/生成验证，不把它说成执行了不存在的 CMake formatter。
没有新增依赖或改变工具链，没有手改生成工程，也没有设置编译器环境变量来绕过冲突。

已验证路径未发现剩余编码阻断；仍待总控复核。未验证任意全局 flags、所有宿主编码组合、
clang-cl、Linux、稳定 ABI、性能、网络、真实协议或设备。宿主已有 `/utf-8` 必须显式 opt-out，
本批不是“任何未知宿主自动兼容”的保证。尚未重跑 Lab，第三批恢复及结论由总控独立决定。
未执行 Stage、Commit、Push、删除、清理、发布或历史改写。完成交接后停止写入。
