# yyjson 0.12.0正式最小随仓依赖Windows验证报告

实施及验证日期：2026-09-08；Git换行P2复核：2026-09-08。
接管HEAD：`0c4bc4848b7e61fafe1adb83960126200501fb41`。

## 1. 结论

PAE已按确认范围采用yyjson 0.12.0、Commit
`8b4a38dc994a110abaec8a400615567bd996105f`作为Config Compiler和Protocol Lab的内部JSON
依赖。三份上游文件从既有、已核验本地缓存按字节复制到`third_party/yyjson`，Compiler、Lab、
纯格式测试及yyjson Spike统一从本地源码构建，不再通过FetchContent获取yyjson。

全新短路径Windows x64/MSVC Debug/Release验证通过。该结论不表示整个工程具备系统级断网构建
保证：nlohmann/json和RapidJSON Spike仍保留各自实验下载路径，本轮没有运行它们，也没有执行
UDP、Linux、真实协议Golden、硬件、现场或性能验证。

## 2. 来源、锁和边界

唯一当前锁为`third_party/yyjson/dependency.lock.json`。它记录版本、Tag object、Commit、上游
地址、归档Hash、MIT License、文件长度及Hash、本地修改`none`和更新策略。原
`cmake/yyjson_candidate.lock.json`及候选CMake入口退出；`spikes/json_parser/dependencies.lock.json`
只保留历史选型快照，不再由yyjson消费者读取。

| 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| `LICENSE` | 1,084 | `45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947` |
| `src/yyjson.h` | 322,407 | `175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3` |
| `src/yyjson.c` | 410,813 | `ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a` |

`pae::yyjson`仍是内部静态目标并保持既有四个`YYJSON_DISABLE_*`编译宏。生成工程扫描确认Compiler、
Lab、纯格式测试、Testing-off Lab和yyjson-only Spike均引用`third_party/yyjson`，没有yyjson
`_deps`或codeload引用；Product-only工程没有yyjson、Compiler或Lab目标。源码扫描仍确认
`src/protocol_plan`与`src/protocol_core`不包含yyjson依赖。

## 3. 失败关闭门禁

新增`pae.dependency.yyjson.vendor_contract`。每次先验证正式锁和Vendor文件，再在构建目录独立
副本上验证：

- 锁文件缺失，精确命中`yyjson dependency lock does not exist`；
- `src/yyjson.c`缺失，精确命中`Locked yyjson source file is missing`；
- `src/yyjson.h`追加内容后长度和Hash变化，精确命中`Locked yyjson source file changed`。

负例只修改`out/yv`隔离副本，未修改正式Vendor和原`out/sources/yyjson`缓存。

### 3.1 Git换行P2纠错

修复前静态检查确认`.gitattributes`只使`LICENSE`命中`text=auto`，本机系统Git配置为
`core.autocrlf=true`。随后在`out/yv-eol/pre-true`独立初始化Git仓库，将四个测试文件加入该
隔离仓库Index并使用`checkout-index`实际检出；主仓库Index、Objects、Refs和Git配置均未修改。

修复前`core.autocrlf=true`检出结果为：

- `LICENSE`从1,084字节、21 LF、0 CR变为1,105字节、21 LF、21 CR，SHA-256变为
  `7b14b8632bf3d5cb64c7a5f1ddfa9062e9c6eba38ac495a0897541d6658d3ad2`；
- `.h/.c`因原通用规则已有`eol=lf`，长度和锁定Hash保持不变。

Windows下仅设置`core.autocrlf=false`但保留`text=auto`时，默认`core.eol=native`仍会检出CRLF，
所以它不是保护无扩展名上游文件的充分条件。修复通过三个精确路径`-text`禁用Git文本转换，
没有宽泛修改`third_party`。修复后分别以隔离仓库`core.autocrlf=true`和`false`实际检出，三份
文件均保持原长度、0 CR和锁定SHA-256；`git check-attr`均报告`text: unset`。

依赖合同增加对三条精确属性的检查。修复后Compiler、A阶段和旧Lab三个现有构建目录的
Debug/Release依赖合同各1/1通过；这是本次Git属性专项复核，没有重复无关25/25矩阵。

## 4. Windows实际验证

所有目录均为本轮全新短路径，Debug与Release串行执行。配置基本形式如下，并按矩阵切换开关：

```powershell
cmake -S . -B out/yv/<case> -G "Visual Studio 18 2026" -A x64 <options>
cmake --build out/yv/<case> --config Debug --parallel
ctest --test-dir out/yv/<case> -C Debug --output-on-failure
cmake --build out/yv/<case> --config Release --parallel
ctest --test-dir out/yv/<case> -C Release --output-on-failure
```

| 目录/配置 | 注册数 | Debug | Release |
| --- | ---: | --- | --- |
| `out/yv/c` Compiler | 2 | 2/2 | 2/2 |
| `out/yv/l`旧Lab，排除`udp_exchange` | 26总注册、25离线 | 25/25 | 25/25 |
| `out/yv/a` Lab 0.6纯格式A | 2 | 2/2 | 2/2 |
| `out/yv/p` Product-only | 0 | 构建通过 | 构建通过 |
| `out/yv/l0` Lab-on/Testing-off | 0 | 构建通过 | 构建通过 |
| `out/yv/s` yyjson-only Spike、Testing-off | 0 | 构建通过 | 构建通过 |

Schema 0.5 Compiler与普通Protocol Lab组合使用`out/yv/g`配置，按既有门禁失败，Configure退出1。
未执行该失败目录的构建。所有配置与构建/CTest日志位于`out/yv/logs/`。

Git换行隔离复现和修复后证据位于`out/yv-eol/`；专项依赖合同日志位于
`out/yv/logs/ctest-vendor-*-*.log`。

## 5. 证据限制与重建条件

本轮静态确认CMake中不再存在yyjson FetchContent入口；全新构建又确认生成工程直接引用本地
Vendor。这两类证据不同于操作系统级断网实验，本报告不宣称整个环境完全离线。

重建要求仓库中唯一锁及三份Vendor文件同时存在且长度、Hash匹配，并具备项目既有MSVC/CMake
工具链。若主动开启nlohmann/json或RapidJSON候选，仍可能需要其网络或预置缓存。本轮没有清理
旧构建树或依赖缓存，没有Stage、Commit或Push；当前状态待总控审查。
