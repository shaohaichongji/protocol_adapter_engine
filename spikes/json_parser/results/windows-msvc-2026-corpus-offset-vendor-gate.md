# Windows MSVC Corpus、offset 与 Vendor 候选门禁报告

## 1. 结论

截至 2026-09-01，JSON Parser（JSON 解析器）Spike（技术探针）在 Windows x64/MSVC Release 下完成第二批机器可读文档语料迁移、诊断位置断言、清单生成器负向门禁和 yyjson 候选来源复核。

实际结果：

- 文档级总用例仍为 100 项，没有增加重复探针或删减既有用例；其中`MANIFEST_AUTHORITY（机器清单权威）`由 14 项增加到 28 项，`LEGACY_EMBEDDED（旧版源码内嵌）`由 86 项减少到 72 项；
- 本批迁移空输入、纯空白、BOM（Byte Order Mark，字节顺序标记）、7 类非法 UTF-8、2 类 UTF-16 输入，以及 2 个根级重复 key；
- 机器清单新增 10 个`EXACT_INPUT_BYTE（精确输入字节）`offset 断言、2 个`PARSER_REPORTED_POSITION（Parser 报告位置）`种类断言，以及 2 个重复 key 的精确 JSON Pointer（JSON 路径指针）和 offset 缺省断言；
- 5 项 Negative Mutation Gate（负向变异门禁）均通过，证明非法 offset 组合、非法 Target Lock（目标锁）和缺失 Inventory（清单）状态会被 Configure（配置生成）拒绝；
- yyjson 的 Archive（归档包）URL 和 SHA-256 由`dependencies.lock.json`直接驱动，下载后实际复核`LICENSE`、`src/yyjson.h`、`src/yyjson.c`的字节数与 SHA-256；当前锁定内容通过；
- 三个候选各 100 个文档级用例仍为`regression_failures=0 / regression_gate=PASS`，独立 Number Token（数字词法单元）55 项仍通过；
- CTest（CMake Test，CMake 测试驱动）为 16/16 通过。

本轮没有执行 Linux GCC/Clang。按当前用户确认的推进顺序，PAE 先在 Windows 上打磨；实现继续避免平台专属 API，并保留标准 C++17 和可迁移 CMake 边界。没有 Linux 实测前，不宣称已经具备 Linux 兼容证据。

## 2. 机器语料迁移

### 2.1 本批 14 项

| 分组 | case_id | 断言重点 |
| --- | --- | --- |
| 空输入 | `empty`、`whitespace_only` | `SYNTAX_ERROR`；只锁定 Parser 报告位置的种类，不统一不同 Parser 的数值 |
| 编码预检 | `bom` | `BOM_NOT_ALLOWED`，精确 offset 0 |
| 非法 UTF-8 | `invalid_utf8`、`truncated_utf8`、`overlong_utf8`、`stray_utf8_continuation`、`utf8_encoded_surrogate`、`utf8_above_unicode_max`、`legacy_five_byte_utf8` | `INVALID_UTF8`及公共预检的精确输入字节位置 |
| 非 UTF-8 输入 | `utf16_little_endian`、`utf16_big_endian` | 首字节 offset 0 即拒绝 |
| 重复 key | `duplicate_root`、`duplicate_escaped_key` | `DUPLICATE_KEY`、Pointer=`/protocol_id`、offset 缺省 |

`invalid_utf8`输入中的非法起始字节`C3`位于 offset 39，但实际违反后继字节规则的是`28`，所以公共预检稳定报告 offset 40。其余本批非法 UTF-8 起始/码点形式报告 offset 39。

清单使用大写 Hex（十六进制）保存原始字节。零字节输入使用单个`-`哨兵；CMake 明确要求该哨兵只能与`expected_input_size=0`组合，Runner 解码后再次核对实际长度。

### 2.2 当前清单统计

```text
strict_json_corpus rows=28
  CONFORMANCE=25
  CHARACTERIZATION=2
  OPEN_DECISION=1

pointer assertions
  ABSENT=16
  EXACT=12

offset assertions
  ABSENT=16
  EXACT=10
  KIND_ONLY=2

strict_json_inventory rows=100
  MANIFEST_AUTHORITY=28
  LEGACY_EMBEDDED=72
```

这仍不是完整生产 Conformance Corpus（一致性语料）。72 个源码内嵌用例及生成型资源边界尚需继续迁移或固定 Generator ID（生成器标识）、参数、字节数和 Hash。

## 3. 清单生成器负向门禁

本轮增加 5 个 CMake-only（仅使用 CMake）的跨平台测试。每个测试复制一份工作清单、注入一条非法记录，再启动独立 CMake 进程；只有失败且命中预期诊断才算通过。

| 测试 | 必须拒绝的组合 |
| --- | --- |
| `pae_json_corpus_reject_exact_parser_reported` | `EXACT + PARSER_REPORTED_POSITION` |
| `pae_json_corpus_reject_exact_outside_input` | 精确 offset 等于输入长度、超出零基有效范围 |
| `pae_json_corpus_reject_kind_only_none` | `KIND_ONLY + NONE` |
| `pae_json_corpus_reject_non_characterization_target` | 非`CHARACTERIZATION`用例设置`target_code/target_lock_candidate` |
| `pae_json_corpus_reject_missing_inventory` | Manifest 用例没有`MANIFEST_AUTHORITY`来源状态 |

这些测试只依赖 CMake 3.25+ 文件和脚本能力，不使用 PowerShell、Windows API 或平台路径语义；因此代码结构可供后续 Linux 工具链直接复用，但本轮未在 Linux 执行。

## 4. yyjson 候选来源与许可证边界

当前仍保持：

```text
selection_status = candidate
local_modifications = none
license = MIT
```

CMake 从`dependencies.lock.json`读取 yyjson Archive URL 和 SHA-256，而不是在`CMakeLists.txt`再维护第二份值。`FetchContent`验证归档包后，还按锁文件逐项复核：

| 文件 | 锁定字节数 | 锁定 SHA-256 |
| --- | ---: | --- |
| `LICENSE` | 1,084 B | `45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947` |
| `src/yyjson.h` | 322,407 B | `175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3` |
| `src/yyjson.c` | 410,813 B | `ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a` |

锁文件内的源码/License路径还必须是非空、正斜杠相对路径；绝对路径、反斜杠和`..`父目录穿越均被拒绝。任一文件缺失、字节数漂移、Hash 漂移、License 名称改变、本地修改状态改变，或在最终拍板前把状态静默改成非`candidate`，Configure 均失败。

这只固定 yyjson 首选候选实验的可重复性。它不表示：

- 最终生产 Parser 已拍板；
- yyjson 源码已经正式 Vendor（随仓库管理）；
- nlohmann/json 或 RapidJSON 已进入生产交付范围；
- Loader 公共接口或资源上限已经冻结。

## 5. Windows 实测证据

### 5.1 工具链与命令

```text
CMake 4.4.3
Visual Studio 18 2026 / MSBuild 18.9.1
Windows SDK 10.0.22621.0
x64 Release
```

实际执行：

```powershell
cmake --preset windows-msvc-spike
cmake --build --preset windows-msvc-spike-release --parallel
ctest --preset windows-msvc-spike-release
```

本轮 Build（构建）使用既有构建目录做增量 Release 构建，不是 Clean Build（干净构建）。Configure 成功本身包含 yyjson 归档来源、源码范围和许可证正向复核。

### 5.2 CTest

```text
100% tests passed, 0 tests failed out of 16
Total Test time (real) = 0.56 sec
```

16 项由既有 11 项加本轮 5 项清单生成器负向门禁组成。三个候选摘要保持：

```text
SUMMARY candidate=yyjson cases=100 inventory_cases=100 inventory_gate=PASS
  regression_failures=0 regression_gate=PASS contract_closure_state=OPEN
  ready_to_promote=2 open_contract_gaps=0
  target_locked_cases=2 target_lock_failures=0

SUMMARY candidate=nlohmann_json cases=100 inventory_cases=100 inventory_gate=PASS
  regression_failures=0 regression_gate=PASS contract_closure_state=OPEN
  ready_to_promote=0 open_contract_gaps=2

SUMMARY candidate=rapidjson cases=100 inventory_cases=100 inventory_gate=PASS
  regression_failures=0 regression_gate=PASS contract_closure_state=OPEN
  ready_to_promote=0 open_contract_gaps=2
```

yyjson Raw Number 自测仍为 8 个 Token、45 个原始词法字节加 7 个测试分隔 NUL 后共 52 字节逐字节一致；allocator（内存分配器）5 个解析期分配点逐点故障注入仍通过。

### 5.3 构建产物快照

| 文件 | 大小 | SHA-256 |
| --- | ---: | --- |
| `pae_json_number_token_corpus.exe` | 40,448 B | `d2125c1aecbf508d84cc41fa5538054789dfd33458d742e137379f2354199dea` |
| `pae_json_spike_yyjson.exe` | 210,432 B | `f42a1a2773c71f8fe75c6e5d79039921715cfc6f02b749135d09ef6b9efd3512` |
| `pae_json_spike_nlohmann.exe` | 198,144 B | `27842c1c8380d707225ca1b0ad8e999f0ee4ceea33fb782ebadfebb6badbec6e` |
| `pae_json_spike_rapidjson.exe` | 131,072 B | `fa3ba9d9938bbae39ddcb972ad544a11b01953e5dbc368aa902b1ed7e1ad3ff6` |

这些 Hash 只对应当前本机增量构建产物，不是发布签名。

## 6. 未关闭边界和下一步

仍未关闭：

- unsigned（无符号）属性是否接受词法`-0`；
- `0e999`、`1e999`等超大 REAL Token 的 Loader/Structural 归属和公共表示；
- Number Token 字节上限，以及与 Desktop/Constrained/Hard Limit 自洽的正式 Loader 资源数值；
- 72 个源码内嵌文档用例及生成型资源边界的机器语料权威；
- 嵌套重复 key 的完整 Pointer、复合错误优先级和统一语法定位边界；
- 最终 Parser 决策与正式 Vendor 复制；
- 正式`pae.schema.json`、ProtocolPlan Execution Semantics（执行语义）和生产 Loader/Core；
- Linux、Sanitizer（运行期检测器）、协议 Golden Vector（黄金测试向量）、硬件和现场验证。

按 Windows-first（Windows 优先）推进顺序，下一步推荐先在 Windows 上完成`-0`、超大 REAL Token 和 Loader 资源 Profile 的编号拍板，再继续迁移高价值静态语料；随后才单独冻结最终 Parser。Linux GCC/Clang 执行暂缓，但任何新增 Core/Loader 代码都继续接受平台无关 API、固定宽度类型、显式字节序、无平台头泄漏和 CMake 可移植性审查。
