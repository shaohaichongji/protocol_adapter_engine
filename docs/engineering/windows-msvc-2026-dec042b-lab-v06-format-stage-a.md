# PAE-DEC-042B Lab 0.6纯格式A阶段Windows验证报告

初次实施日期：2026-09-07；P2纠错更新：2026-09-08。
代码基线：`90c51655768874479106e9a565248ad58df4f7f2`。

## 1. 结论

本阶段在默认关闭的独立测试入口中实现Values 0.4解析、Decimal结果与失败模型、Result 0.6
内存序列化样本、FieldsCanonical和`pae.lab.fingerprint/0.6`固定编码。Windows x64 MSVC
Debug/Release隔离测试各1/1通过；初次实现时旧Protocol Lab离线回归各12/12通过。本轮P2纠错
重新执行针对性Debug/Release各1/1，未重复无关旧矩阵。

此结论只覆盖纯格式模块。普通Protocol Lab仍不接受Values 0.4，不发布或读取0.6 Evidence
Bundle，不调用Schema 0.5 Core，不执行Replay/Compare或网络。B证据读写与C运行链仍未实现，
现有Schema 0.5+Protocol Lab配置拒绝继续生效。

## 2. 实现与契约映射

- Values 0.4：严格对象属性、重复键、类型和版本；Decimal coefficient使用规范INT64字符串，
  scale使用0..18精确JSON整数Token；等价表示在解析模型中规范化，原始输入文本不改写。
- Result字段：Decimal固定为`id/kind/decimal64/raw_kind/raw_value`，不混入`logical_value`或
  `enum_known`；旧类型保持五项字段语义。
- 失败事务：固定四种`conversion_error`并联合校验Codec状态；成功清空失败信息，失败不允许
  部分字段；内部错误、最终复核和未执行Codec使用null转换原因。
- 指纹：严格使用N/S/I/B/A编码和固定20项数组；UTF-8长度按字节；null与空字符串、无报文与
  实际零长度报文分离；Decimal数学值、raw类型/值、失败原因和失败身份参与。
- 追溯分离：等价Decimal原始Values文本具有不同文件Hash，但规范执行指纹相同；Values Hash
  不进入0.6执行指纹。本阶段没有文件写入器，因此未声称Bundle追溯链已验证。

## 3. 自动化断言

隔离测试使用独立手写的255字节固定canonical payload和固定SHA-256预期，未由被测编码器生成
预期。覆盖：

- UTF-8中文、换行、冒号、竖线、空字符串和null；
- INT64_MIN、UINT64_MAX及字段数组数量；
- Decimal等价表示、不同数学值、规范零、scale边界、非法词法、越界、null、未知和重复属性；
- raw类型和值差异；
- conversion_error与状态组合、失败字段和值索引、内部/最终复核/NO_CODEC null原因；
- `NONE/NONE`、`ENCODE_TX/TX`、`DECODE_RX/RX`及真正的NO_CODEC正例；模式、主体、状态、诊断、
  字段和失败身份矛盾组合；
- 已知字段身份的Message/ID/索引关系、Decode拒绝Encode专用输入索引，以及未知字段时仅保留
  Encode输入索引的合法路径；
- 失败无部分结果；默认开关关闭、Testing关闭拒绝及Schema 0.5+普通Lab继续拒绝。

纯格式模块不读取Plan，因此字段顺序由调用方按配置顺序提供；本阶段不把测试模型顺序宣称为
真实Pipeline排序验证。

## 4. 实际命令与结果

主要配置：

```powershell
cmake -S . -B out/build/v06-a -G "Visual Studio 18 2026" -A x64 `
  -DPAE_BUILD_TESTING=ON -DBUILD_TESTING=ON -DPAE_BUILD_LAB_V06_FORMAT_TESTS=ON
```

结果：配置成功。随后分别构建Debug、Release并串行执行CTest，各1/1通过。

隔离门禁：

- 默认不开启`PAE_BUILD_LAB_V06_FORMAT_TESTS`：配置成功，`ctest -N`为0项；
- 格式开关ON且`PAE_BUILD_TESTING=OFF`：按预期配置失败；
- Schema 0.5 Compiler与普通Protocol Lab同时开启：仍按预期配置失败。

旧代必要回归使用现有`out/build/windows-msvc-protocol-lab`，Debug/Release各排除唯一UDP测试后
串行执行12/12通过；这是A初次实现证据。本轮P2只改变隔离模型和测试，未重跑旧矩阵。

P2修复前，在只增加审查断言、尚未修改`ValidateResult`时执行Debug测试，结果0/1、CTest退出8，
精确失败为`NO_CODEC must reject a false OK result with delivered fields`。P2-2由修复前源码和原测试
对象静态确认：校验仅比较字段ID/索引是否同时出现，且原合法失败对象没有Message。修复后
Debug/Release针对性测试各1/1通过。

2026-09-08总控只读复核确认两项P2实现修复关闭。空Message字符串由源码检查确认会拒绝，
当前没有独立的空Message动态断言，不将其表述为专项测试覆盖。本次提交前收口仅修订Markdown，
没有重跑构建或测试；上述运行结果分别保留其原执行批次。

日志：

- `out/build/v06-a/build-debug-final.log`
- `out/build/v06-a/ctest-debug-final.log`
- `out/build/v06-a/build-release-final.log`
- `out/build/v06-a/ctest-release-final.log`
- `out/build/v06-gates/default-configure.log`
- `out/build/v06-gates/default-ctest-n.log`
- `out/build/v06-gates/testing-off-reject.log`
- `out/build/v06-gates/schema-lab-reject.log`
- `out/build/v06-gates/legacy-lab-ctest-debug-offline.log`
- `out/build/v06-gates/legacy-lab-ctest-release-offline.log`
- `out/build/v06-a/p2-review/pre-fix-ctest-debug.log`
- `out/build/v06-a/p2-review/post-fix-ctest-debug.log`
- `out/build/v06-a/p2-review/post-fix-ctest-release.log`

## 5. 边界

未执行Linux、Oracle、真实协议Golden、硬件、现场、性能或网络验证。未实现Evidence 0.6三件套、
Reader语义门禁、Core raw复制、结构优先Inspect、Replay/Compare及跨代矩阵。A阶段已完成总控限定
复核，当前待提交；不表示B/C已实现或整个DEC-042B完成，Commit及Push仍须分别授权。
