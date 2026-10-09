# 公开接口与最小示例中文注释：优先切片验证

日期：2026-10-09。状态：已完成本批派发范围，待总控复核；不是提交批准或正式发布。

## 1. 接管与范围

接管及交付核对均为 `main@3facda71a53f57d97b2d1b497e19c7624b846676`，暂存区为空。
接管时已有六个已跟踪变更：`deliverables/README.md`、`docs/README.md`、
`docs/experience/03-配置与边界.md`、`docs/experience/README.md`、
`docs/guides/04-协议配置入门.md`、`scripts/package_experience_bundle.ps1`。
已有未跟踪入口：`docs/engineering/config-tutorial-validation-20261002.md`、
`docs/experience/tutorials/`、`scripts/package_tutorial_overlay.ps1`。本批未编辑这些文件。

初始授权仅为中文注释及验证。发现消费编码阻断后，先报告并暂停受影响的验证；总控先批准
临时进程参数诊断，随后转达用户明确授权最小永久编码修正。没有将临时验证冒充默认构建闭合。
未修改 Lab、Core/Plan 的执行逻辑、公共声明或布局、根 CMake、原有交付包及本机环境。

## 2. 注释内容与实现依据

本批仅完成四个优先文件，不宣称全部公开头已有中文注释：

- `include/pae/compiler.h`：编译入口、失败阶段、诊断位置/单位、owner 转移、借用 metadata、
  Pipeline 关联序号与全局 Message 索引、全局字段索引、物理查询边界及逻辑内存计费。
- `include/pae/codec.h`：类型化输入与 BYTES 借用、字段/枚举选择器、容量、独立 Workspace、
  保留的冻结状态、结果代次/owner 移动、失败交付与 Buffer 副作用、未知枚举 taint。
- `examples/getting_started/main.cpp`：配置到执行的顺序、样例固定假设、独立字节预期、
  数值复制时机与退出码；不声称执行了 StreamFramer 或网络。
- `examples/public_api_codec/main.cpp`：Binary 常量/动态值、枚举索引与借用 payload，
  ASCII 独立 TX/RX 模板、上界容量与有效长度；明确 Binary 往返检查本身的覆盖局限。

保留原英文注释，不做全文件重排。尚未补中文注释的公开头为 `protocol_description.h`、
`host_endpoint.h`、`stream_framer.h`、`export.h`、`version.h`；按总控要求不继续扩边。

所有权核对依据：

- `src/public_api/codec.cpp` 中 `Impl::state` 与 `CreateCompleteRecordCodec()` 使用
  `CompiledStateRef`/`Acquire()`；外部 `CompiledProtocol` 可先释放，执行对象保留冻结状态。
- `src/public_api/host_endpoint.cpp` 的持有状态与创建路径同样保留冻结状态，仅作只读核对。
  未复制旧 Lab 注释中“执行对象借用外部 compiled owner”的说法，也未修改 Lab 文件。
- `src/public_api/compiler.cpp` 的 metadata 查询及 `CompileResult::TakeCompiled()`；
  外部字符串视图仍按 owner 生命周期使用，不能因执行对象保留状态而继续使用过期视图。
- `src/public_api/codec.cpp` 的 `BeginGuardedCall()`、移动 epoch、视图访问与结果发布；
  `HasValue()` 不能安全探测已销毁 owner，复制视图不等于复制结果/字节。
- `tests/public_api/public_codec_tests.cpp` 覆盖外部 compiled 释放后调用、移动/代次失效、
  Workspace 忙、独立实例、容量失败、首次及重复调用分配、保留未知枚举的成功 taint。

## 3. 实际发现与永久修正

首次默认 Debug 消费构建失败：新增无 BOM UTF-8 公共头被 MSVC 按代码页 936 读取，
出现 C4819 及后续语法错误。内部 `pae::project_options` 带 `/utf-8`，但公共库仅 PRIVATE
链接该目标，消费者未继承源码编码设置。源码内容比对相同不能单独证明该编译条件安全。

最小修正：

- `src/public_api/CMakeLists.txt` 对公开目标增加唯一 INTERFACE 编译选项
  `$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/source-charset:utf-8>`。
- `cmake/PAEConfig.cmake.in` 为手工构造的 static/shared imported `PAE::pae` 提供相同选项。
  安装配置不是直接导出源码目标，故两处均需落实。
- 不将 `pae::project_options` 整体 PUBLIC 传播；不使用全局编译选项、不改变 execution charset、
  不新增 BOM、不写用户或系统 `CL`。源码包白名单已包含以上文件，无需修改打包脚本。
- 首次 source-only 编码复核的 Debug 测试为 10/11：既有 metadata 断言把普通窄字符串
  `"空闲模式"` 与 UTF-8 metadata 比较。改为明确的 C++17 `u8"空闲模式"` 独立期望，
  不通过全局 execution charset 改变来掩盖差异；保留失败日志。
- `docs/sdk/README.md` 说明 CMake/手工消费的源码编码要求及整个消费编译单元的影响。
  不允许将宿主 GBK 源码和 UTF-8 公共头混作同一源码编码；旧包没有自动升级。

## 4. 新增回归与无行为改动核对

- `tests/public_api/source_charset_consumer.cpp` 只链接 `PAE::pae`；中文宽字符串与 Unicode
  escape 做编译期精确对照，验证源码读取而不依赖窄字符串 execution charset。
- `tests/public_api/verify_source_charset.cmake` 精确检查源码目标的完整 INTERFACE 选项集合，
  以及生成的 Debug/Release SDK 配置；拒绝内部警告选项、`/utf-8` 等泄漏。
- `tests/public_api/CMakeLists.txt` 注册以上两个检查。所有现有公开 API 用例继续执行。
- 四个注释文件与 HEAD 的去注释 token 序列相同，字符串/字符字面量保留比较，
  raw string 未出现；脚本遇到 raw string 会拒绝而不是误剥离。结合实际 diff 审查确认
  未改声明、函数体、数值或布局。编码 CMake 和测试期望是另行授权的真实增量。
- 严格 UTF-8、未引入 BOM、续行数量及注释尾反斜杠检查通过；六个修改/新增 C++ 文件
  的 clang-format dry-run 和 `git diff --check` 通过。

## 5. 本次 Windows 执行结果

工具链：Visual Studio 18 2026、x64、v142 14.29.30133；复用现有 public-api preset。
所有产物在忽略的 `out/build/comments-public-20261009/`。下表是永久修正后、无临时
`CL`/`CXXFLAGS` 编码覆盖的实际执行，Debug/Release 串行；全部命令退出码为 0。

| 路径 | Debug | Release | 检查内容 |
| --- | --- | --- | --- |
| 当前仓库 `default-repo` | 11/11 | 11/11 | 公开 API、独立头编译、编码及选项 |
| 新源码 SDK `source-consumer` | 3/3 | 3/3 | 中文编码 fixture、最小程序、Binary/ASCII 示例 |
| 新 static 安装 SDK | 3/3 | 3/3 | 同上，分别消费配置专用安装树 |
| 新 shared 安装 SDK | 3/3 | 3/3 | 同上，并核对相邻 DLL 与所选 SDK Hash 一致 |

源码 SDK 由本次工作树白名单复制，`source_head` 为上述 HEAD、`source_worktree_dirty=true`，
不是该提交的逐字节原件。公共头与两处编码 CMake 文件逐文件 Hash 对应当前工作树。
static 安装树来自源码包消费构建；shared 来自独立 product 构建，均关闭 PAE 测试 hooks。
shared product 的 `ctest -N` 为 0 项；consumer 自身的三个测试不属于产品测试注册。
四个安装树的新增公共头与当前工作树 Hash 相同；各消费工程的选项与 `PAE_DIR` 已核对。
没有回用现成 SDK，没有重制原五包、Lab 或现用 deliverables。

主要命令形状（精确参数数组及所有退出码见 transcript，勿在已存在的包根重复制包）：

```powershell
cmake --preset windows-msvc-public-api-stage1 -B $Root/default-repo
cmake --build $Root/default-repo --config Debug --target <公开测试与示例目标> -- /m:2
ctest --test-dir $Root/default-repo -C Debug -R '^pae\.public_api\.' --output-on-failure
# Release 串行使用相同目标与测试过滤。
pwsh -NoProfile -File scripts/package_sdk_stage3.ps1 -Kind source -PackageRoot $Root/source-sdk -Configuration source
cmake -S $Root/encoding-consumer -B $Root/source-consumer -DPAE_SOURCE_DIR=$Root/source-sdk -DPACKAGE_ROOT=$Root/source-sdk -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
cmake --install $Root/source-consumer/pae-source --config Debug --prefix $Root/static-install-Debug
cmake -S $Root/source-sdk -B $Root/shared-product -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
cmake --build $Root/shared-product --config Debug --target pae_public_api -- /m:2
cmake --install $Root/shared-product --config Debug --prefix $Root/shared-install-Debug
cmake -S $Root/encoding-consumer -B $Root/shared-consumer-Debug -DCMAKE_PREFIX_PATH=$Root/shared-install-Debug -DPACKAGE_ROOT=$Root/shared-install-Debug -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
cmake --build $Root/shared-consumer-Debug --config Debug -- /m:2
ctest --test-dir $Root/shared-consumer-Debug -C Debug --output-on-failure
```

证据均相对上述 `$Root`：

- `build-Debug.log`：首次默认编译失败，退出 1。
- `validation-transcript.log`、`build-Debug-utf8.log`：获准临时进程 `CL=/utf-8` 的诊断验证；
  D/R 公开测试各 9/9、两个示例通过，finally 恢复环境。仅为历史过程，不充作最终默认证明。
- `default-source-only-metadata-failure.log`、`default-validation-first-attempt.log`：
  source-only 下原窄字符串断言失败，CTest 退出 8；后续未覆盖这份失败证据。
- `default-validation-transcript.log`、`run-default-validation.ps1`：永久修正后全部真实命令、
  参数数组、退出码、串行次序和 `environment_unchanged=TRUE`。
- `default-repo-test-{Debug,Release}.log`、`source-consumer-test-{Debug,Release}.log`、
  `{static,shared}-consumer-test-{Debug,Release}.log`：最终各路径测试结果。
- `*-configure*.log`、`*-build*.log`、`{static,shared}-install-*.log`：各新根配置/构建/安装输出。
- `comment-equivalence.log`、`format-final.log`、`diff-check-final.log`、`candidate-portable.log`、
  `final-audit.log`：源码与编码、格式、候选文件及最终消费关联检查。

## 6. 增量与剩余边界

本批增量文件为四个注释文件，加 `src/public_api/CMakeLists.txt`、`cmake/PAEConfig.cmake.in`、
`tests/public_api/CMakeLists.txt`、`tests/public_api/public_api_tests.cpp`、
`tests/public_api/source_charset_consumer.cpp`、`tests/public_api/verify_source_charset.cmake`、
`docs/sdk/README.md` 及本报告，共 12 个候选文件。out 中脚本/fixture/SDK/日志被忽略，不纳入候选。

未发现本批已验证路径的剩余构建阻断，但仍待总控独立复核；未宣布全部公开头注释完成。
未验证 clang-cl、Linux、非 MSVC 编译器、任意宿主源码/执行字符集组合、稳定 ABI、正式分发许可、
真实协议、网络、设备或现场。本批未执行任何网络收发，也未重新运行 Lab 或全切片矩阵。
源码读取配置对宿主整个 C++ 编译单元生效，是本次明确授权的构建政策变化，不是只对头局部生效。
产品行为、Schema/证据版本及原历史包未改变。未执行 Stage、Commit、Push 或任何清理/回退。
