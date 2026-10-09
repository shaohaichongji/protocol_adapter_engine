# YAML 独立测试入口字符集与中文路径验证（2026-10-09）

## 1. 范围、接管与状态

本次是第十三批 YAML 前端中文注释之后的独立测试入口修复，不改变 YAML 准入、
转换、预算、Compiler、Codec、公开 API 或 SDK 字符集契约。接管为
main@c881412ba012143935fbc7e9b81b9649c934a298，暂存区为空。

第十三批四个 YAML 源码注释修改及其未跟踪验证报告完整保留；本次没有回写其历史结果。
本次增量仅为 tests/config_frontend_yaml/CMakeLists.txt、frontend_tests.cpp 和本报告。
public_facade_tests.cpp、根 CMake、库与接口均未修改。

证据根：out/build/comments-public-20261009/yaml-test-entry-fix/。
baseline/、protected-hashes.json、previous-evidence-hashes.json、takeover-status.log
是原始接管证据，未重建。旧第十三批证据仍在相邻 yaml-frontend/，未覆盖。
限定修复与专项验证已完成，待总控复核；不表示获准提交或整个产品验收通过。

## 2. 修复前独立复现及限制

新建 default-before 构建根，使用仓库默认独立测试入口，没有 CMAKE_PROJECT_INCLUDE
注入文件、CL 或 _CL_ 环境覆盖。

- Configure 退出 0；Debug 构建两个测试目标退出 1。
- 公开 YAML 测试缺少 UTF-8 源字符集，包含带中文注释的头文件时出现 C4819，随后
  C2061/C2059/C2143 等编译错误。
- 集成测试虽构建出可执行文件，但执行字符集仍为代码页 936；非 BMP 字符产生 C4566。
  单独运行该 Debug 程序退出 1，记录 failures=12，涉及字符分类、来源位置、所有权、
  Unicode 和 block scalar 等检查。
- 本次修复前构建失败后未运行 CTest，也未执行修复前 Release；不能写成修复前完整矩阵失败。

命令与退出保存在 reproduce.ps1、before-transcript.log、before-configure.log、
before-build-Debug.log；实际单独运行输出保存在 before-integration-run-Debug.log。
原可执行程序位于 default-before/Debug/pae_yaml_frontend_tests.exe。

路径问题需与字符集问题区分：旧 Read 使用窄字符串 ifstream，UTF-8 编码的中文根路径
不应依赖 Windows ANSI 代码页。前批在测试字符集适配后曾记录窄路径失败，这是历史证据；
本次未将其冒充为新做的路径单项修复前复现。本次原默认 Debug 运行仍能读入夹具，
但字符检查失败。修复后中文绝对路径及不同 cwd 的成功是本次实际证据。

## 3. 最小修复及原断言保留

两个 MSVC 测试目标分别追加 PRIVATE /source-charset:utf-8 和 /execution-charset:utf-8。
使用拆分选项与 PAE::pae 已有 source-charset 传播保持兼容，不给库、SDK 消费者或全局环境
追加执行字符集，也不使用 /utf-8 覆盖接口约定。

Read 将 PAE_TEST_SOURCE 与相对夹具路径通过 C++17 filesystem::u8path 转为原生路径，
用 filesystem::path 重载打开二进制流。打开失败、检测到读取 badbit 时抛出明确错误，
main 打印 FAIL fixture_io 并退出 1；不再把打不开的文件作为空内容继续测试。
未硬编码机器路径、相对层级根或复制夹具，PAE_TEST_SOURCE 仍由 CMake 提供真实绝对根。

新增三个常规检查：fixture_root_is_absolute、fixture_read_from_absolute_utf8_root、
missing_fixture_read_throws_explicit_error。增加测试程序专用 --probe-read-failure，
实际缺失文件必须触发指定诊断及非零退出；它不是生产公开接口。

原 BasicCases 与 PublicCases 完整函数体、Check 失败计数逻辑，与原 baseline 在仅统一
CRLF/LF 后逐字一致；原 CMake 入口保持，仅末尾追加目标局部选项。公开测试文件受
原保护 Hash 约束。正常 CTest 不传探针参数，仍执行全部原用例与三个新增检查；
使用运行时 Check，不因 Release/NDEBUG 丢失断言。

## 4. 实际构建、测试与退出结果

工具链为 Visual Studio 18 2026、x64、v142 14.29.30133（MSVC 19.29.30159），
CMake/CTest 4.3.1-msvc1，clang-format 22.1.3。本次使用独立 default-after 新根。
以下命令中的 REPO_ABS 表示现场仓库绝对路径；实际完整参数与路径见 after-transcript.log。
从仓库根运行，B 为证据根中的 default-after：

```powershell
cmake -S tests/config_frontend_yaml -B $B -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" "-DPAE_SOURCE_DIR=$REPO_ABS"
ctest --test-dir $B -C Debug -N -R '^(pae_yaml_frontend_tests|pae_yaml_frontend_public_tests)$'
cmake --build $B --config Debug --target pae_yaml_frontend_tests pae_yaml_frontend_public_tests -- /m:2
ctest --test-dir $B -C Debug -R '^(pae_yaml_frontend_tests|pae_yaml_frontend_public_tests)$' -V
cmake --build $B --config Release --target pae_yaml_frontend_tests pae_yaml_frontend_public_tests -- /m:2
ctest --test-dir $B -C Release -R '^(pae_yaml_frontend_tests|pae_yaml_frontend_public_tests)$' -V
```

validate.ps1 实际按 Debug 完整步骤后再 Release 执行，共享目录没有并发配置测试。
Configure、测试清单、两次 Build 与 CTest 均退出 0；注册两项测试，D/R 各 2/2。
每配置集成程序为 98 PASS、0 FAIL（原 95 加 3），公开程序为 10 PASS、0 FAIL。
这些是实际断言计数，不是覆盖率。两次构建日志没有 warning/error。

该脚本检查 CL/_CL_ 未设置，拒绝非空 PAE_DIR 和 CMAKE_PROJECT*INCLUDE 缓存；
after-cache-audit.log 及生成 vcxproj 可复核绝对中文源根和拆分字符集设置。
本次没有使用前批 out/test-charset.cmake 类临时注入或相对根路径 workaround。

另外从仓库 docs 工作目录直接运行同一 Debug/Release 的两个程序，四次均退出 0。
集成日志打印真实中文绝对 fixture_root_utf8，并通过新增读取检查；不是仅检查文件存在。
从同一 cwd 执行各配置的 --probe-read-failure，两次退出 1（预期 1），精确匹配
FAIL fixture_io: cannot open YAML test fixture: 前缀；不是仅用通用非零退出证明拒绝。

日志映射（均在本次证据根）：

- after-transcript.log、validate.ps1：实际命令、串行顺序与退出检查。
- after-configure.log、after-cache-audit.log、after-test-inventory.log：默认入口及两项注册。
- after-build-{Debug,Release}.log：构建；after-test-{Debug,Release}.log、
  after-cases-{Debug,Release}.log：CTest 及断言原始输出。
- away-cwd-integration-{Debug,Release}.log、away-cwd-public-{Debug,Release}.log：不同 cwd。
- missing-fixture-probe-{Debug,Release}.log：实际缺失文件负例诊断。
- baseline-format.log、finish-checks.ps1、final-checks.log：最终格式、原断言、保护及候选检查。

收尾未修改已通过验证的两个测试文件，没有无故重跑构建或专项矩阵。

## 5. 最终检查、边界与交接

frontend_tests.cpp 接管与最终 clang-format --dry-run --Werror --ferror-limit=0 均退出 0；
未全文件重排。CMake 遵循相邻缩进风格，仅追加目标局部选项。
两个修复文件和本报告严格 UTF-8 无 BOM；git diff --check 退出 0。
候选白名单核对 6 个 tracked 修改与 2 份 untracked 报告，out 保持忽略。
新增候选检查未发现硬编码用户/机器路径、端点、私有密钥或凭据；此定向检查不构成
全面保密审计。原 3248 个保护文件和前批 524 个证据文件全部 Hash 不变。

最终仍为 main@c881412，暂存区为空；本次未 Stage、Commit、Push、清理、重打包或启动下批。
第十三批四源码及历史报告属于原检查点，本次两测试文件及本报告可独立拆分审查。
停止写入，完成一次总控交接后等待复核。

未验证 Linux、其他 MSVC/代码页、所有 I/O 故障类型、长路径/权限/损坏文件的全面组合、
完整产品矩阵、JSON-only/关闭开关组合、SDK 包外/搬迁/打包、Lab/UI、网络、真实协议、
硬件或现场。badbit 防御存在不表示所有读取故障已动态注入验证。
本限定范围未发现剩余实施阻断；是否接受及提交拆分仍由总控决定，不升级产品成熟度结论。
