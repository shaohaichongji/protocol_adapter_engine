# DEC-042B 精确算术隔离验证报告

日期：2026-09-07。状态：只读审查未发现明确算术实现缺陷；P2测试证据缺口已补齐并通过Windows复核，未接入生产Core。
授权为“隔离算术验证，暂不接入生产Core”，不包含Stage、Commit或Push。

## 1. 实现边界

独立目录为`spikes/decimal_arithmetic`，仅依赖C++17标准库；不改根CMake、src、include、
可执行Schema或Lab格式。固定8个uint32_t表示256位幅值，符号单独保存，零规范为非负。
使用16个uint32_t局部单元保存完整乘积并检查高半部；除法采用固定256步二进制长除法。
参数编译与双向转换使用同一受限整数原语，没有浮点中间值或__int128。

Plan仅为实验内部结构，使用前提是Compile成功且未被修改。错误枚举不是生产CodecStatus。
没有提前冻结生产布局、错误竞争次序、计费口径、Reader字段或兼容性组合。

## 2. 位宽上界推导

以下推导适用于已确认参数域和本候选的具体计算顺序，不是机器检查的形式证明，
也不替代算法实现的独立审查。

1. 分子幅值≤2^63，作者分母在1..10^18；先检查上限再约分。约分分母只含2和5且
   所需十进制位数≤18，因此准确整除10^t。零偏置约分为0/1；比例分子非零。
2. 取共同小数位t≤18，比例=A/10^t、偏置=B/10^t。
   因10^18<2^60，`|A|,|B|<2^123`。参数先转各自小数位再对齐，
   对齐后仍等于分子乘10^t再除分母，故不是两个互不相关的10^18乘数。
3. Decode：raw幅值<2^64，故`|raw*A|<2^187`，`|raw*A+B|<2^188`。
   去除小数尾零仅减小幅值，之后才检查INT64系数范围。
4. Encode：输入C为INT64，s在0..18，u=max(s,t)。
   `|C*10^(u-s)|<2^123`；采用故意放宽且不依赖指数关联的界，
   `|B*10^(u-t)|<2^183`，二者差幅值<2^184；分母`|A*10^(u-t)|<2^183`。
   分母非零且为整数，因此整数商幅值不超过分子幅值；余数必须为零才检查Wire范围。
5. 参数GCD（最大公约数）只操作uint64_t；INT64_MIN幅值用无符号减法取得，
   不执行有符号最小值取负。10的幂最多10^18，uint64_t可容纳。
6. 单乘法累加上界为`(2^32-1)^2+2*(2^32-1)=2^64-1`；
   加法单元上界2^33-1，借位比较使用uint64_t。256×256乘积局部保留512位，
   高256位非零则拒绝，不静默截断。该检查也覆盖超出转换域的辅助原语测试。
7. 长除法保持余数小于除数，移位后的额外高位单独处理；商与余数占固定256位。
   规范化只除以10，符号与幅值分别处理；最终窄化前检查范围，INT64_MIN专门表达。

结论：上述合法转换路径的中间幅值均严格小于2^256，固定256位在容量上有余量。
测试通过不证明所有算法分支正确；仍需独立审查，不能将容量推导写成完整B验收。

## 3. 测试依据

- 独立手算：温度523↔12.3、12.35拒绝、等价小数、负比例、3/6约分、1/3拒绝、
  19位小数拒绝、分母作者上限、UINT64_MAX与INT64_MIN抵消、规范化后才可表示的系数。
- 边界：INT64_MIN约分/取负、18位小数、1～8字节有符号/无符号范围、非法scale、
  超64位公共系数和乘加、输出越界、失败保留实验输出。
- 原语独立预期：`(2^64-1)^2`、`(2^128-1)^2`的单元值、最大256位除3、
  全部256×256个二进制基向量乘积或溢出、逐位进位和除2。
- 3000组确定性受限原生uint64_t计算交叉检查；15组参数×201个raw采用独立小整数公式。
- 200组全宽除法用`q*d+r=n`及`r<d`检查。这是同原语重构不变量，不是独立Oracle。
- 测试程序替换普通new/new[]并先用显式分配确认计数器有效，计数涵盖参数编译及首次转换。
  该监测不覆盖全部CRT/OS分配或单独的对齐分配接口；候选静态检查只使用固定局部对象，
  不能据此宣称整个产品无堆分配或已测得嵌入式峰值栈占用。

## 4. 实际执行

Windows x64；CMake 4.4.3；Visual Studio 18 2026；MSVC 19.51.36256.0；
Windows SDK 10.0.22621；编译选项含`/W4 /WX /permissive- /utf-8`。
在仓库根目录执行（cmake/ctest均使用`D:\develop_env\cmake-4.4.3\bin`中的程序）：

```powershell
cmake -S spikes/decimal_arithmetic -B out/build/windows-msvc-decimal-arithmetic `
  -G "Visual Studio 18 2026" -A x64
cmake --build out/build/windows-msvc-decimal-arithmetic --config Debug
ctest --test-dir out/build/windows-msvc-decimal-arithmetic -C Debug -V `
  --output-log out/review/dec042b-arithmetic/debug-ctest.log
cmake --build out/build/windows-msvc-decimal-arithmetic --config Release
ctest --test-dir out/build/windows-msvc-decimal-arithmetic -C Release -V `
  --output-log out/review/dec042b-arithmetic/release-ctest.log
```

重跑命令前需创建输出日志父目录。首次完整隔离结果（保留历史）：

| 构建 | CTest | 程序断言 | 分配计数增量 |
| --- | --- | --- | --- |
| Debug | 1/1 PASS | 78380，失败0 | 0 |
| Release | 1/1 PASS | 78380，失败0 | 0 |

首次Debug版本为12841断言通过；补充全基向量乘法及极值后得到上表结果；P2补测后结果见第6节。
日志保存在仓库外提交范围的`out/review/dec042b-arithmetic`，不提交生成物。
耗时仅属本机测试执行，不作为吞吐、延迟或实时性基准。

## 5. 未验证与后续门禁

未运行独立高精度Oracle，具体工具仍待单独批准；未做Linux、设备、真实协议Golden、
网络、生产Core回归、栈峰值、压力或性能验收。根产品工程未改，本轮没有重复全切片历史验证。
只读审查未发现明确实现缺陷，已补齐该次审查发现的P2测试缺口；下一步冻结完整接入细节并另行授权；
不能直接将此候选搬入Core或升级Schema能力。未Stage、Commit、Push。

## 6. P2测试证据缺口闭合

审查发现原宽除数转换用例只有零商，缺少大数抵消后的非零成功结果。
本轮只修改测试与相关文档，不修改算术候选、CMake或生产Core。

新增6项断言，独立预期来自`(INT64_MAX+INT64_MIN)/2=-0.5`：

1. 比例INT64_MAX/2、偏置INT64_MIN/2编译成功。
2. 公共小数位为1，A正B负，两个系数幅值均严格大于UINT64_MAX。
3. Decode raw=1得到{-5,1}。
4. Encode {-5,1}得到非零raw=1。
5. Encode等价表示{-50,2}仍得到raw=1。
6. Encode {-4,1}返回RAW_NOT_INTEGRAL，并保持哨兵输出77。

最后一项的反算值为`1 + 1/(5*INT64_MAX)`，不是整数；预期不由候选自身生成。
这组补测不是高精度Oracle，也不把同原语随机重构升级为独立参考。

执行第4节相同Debug/Release构建和CTest命令，分别将`--output-log`改为
`out/review/dec042b-arithmetic/p2-debug-ctest.log`及`p2-release-ctest.log`，保留首次日志。
实际结果：Debug/Release各1/1 PASS，各78386断言、失败0、普通new/new[]计数增量0。
本次P2属于测试证据缺口，不是已复现的算术产品缺陷；补测未触发实现修复。
