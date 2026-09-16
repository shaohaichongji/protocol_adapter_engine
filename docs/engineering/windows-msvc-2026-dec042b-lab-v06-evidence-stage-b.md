# PAE-DEC-042B Lab Evidence 0.6 B阶段Windows验证报告

## 1. 结论

2026-09-08在`main`、接管HEAD `1c0617ce123f3c8e92c8bcd8ddbc33778e394651`上完成
默认关闭的Evidence 0.6隔离读写切片。A+B隔离Debug/Release各3/3、旧Lab离线回归各13/13
通过；本轮Reader P2纠错后A+B隔离Debug/Release再次各3/3通过。没有运行UDP或其他网络收发。
当前状态是待总控复核，不表示C运行链、Replay/Compare、
Linux、Golden、硬件或现场通过。

## 2. 实施范围

- `v06_format`增加严格Result 0.6读取，复用既有模型校验和固定20项指纹复算。
- `v06_evidence`实现0.6 Result/Record/Event三件套、原配置/Values、主/TX/RX字节及适用的
  RX Metadata 0.2绑定；Reader失败时清空输出，不交付部分结果。
- Writer采用`.tmp`逐文件关闭/重读复核、`.inprogress`完整Reader预检和最终目录改名；不覆盖
  已存在正式或中间目录，不声称`fsync`/`FlushFileBuffers`级断电持久性。
- 新目标仅依赖本地yyjson、SHA-256和A格式模块；普通Lab、Compiler、Core、Winsock及CLI未接入。

## 3. 自动化覆盖

- 成功、转换失败、`NO_CODEC_REEXECUTION`完整往返，失败无部分字段。
- 两份Decimal原始Values文本Hash不同而规范执行指纹相同。
- 无主Frame（null）与实际零长度主Frame（空字符串/零字节文件）保持不同指纹和存在性。
- 主/TX/RX字节分别绑定Result、Event、Record和实际文件；RX来源绑定Metadata 0.2。
- 创建、写入/关闭确认、重读及最终目录改名故障均不发布正式Bundle；正式目录拒绝覆盖。
- 缺失文件、重复清单、混代额外文件、路径越界、Hash错误、Record重复路径失败关闭。
- Result及Event未知属性负例先同步payload长度/Hash、Record Hash和完整清单Hash，再精确命中语义拒绝。
- Reader早期、中途和晚期失败均检查`StoredBundle`全部可观察成员为默认值；同一输出对象先成功
  后失败也不得残留旧结果。
- 清单文件、负载文件及中间目录的Windows符号链接均由读前门禁拒绝；内部读取观察器精确断言
  三种拒绝均发生在首个内容读取调用之前。

## 4. 实际命令与结果

MSVC环境均通过`Launch-VsDevShell.ps1 -Arch amd64 -SkipAutomaticLocation`初始化。

```text
cmake -S . -B out/b42bd -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DPAE_BUILD_TESTING=ON -DPAE_BUILD_LAB_V06_FORMAT_TESTS=ON
  -DPAE_BUILD_LAB_V06_EVIDENCE_TESTS=ON
cmake --build out/b42bd
ctest --test-dir out/b42bd -C Debug --output-on-failure
结果：3/3通过

同命令改为 out/b42br 与 CMAKE_BUILD_TYPE=Release
结果：3/3通过

cmake --preset windows-msvc-protocol-lab
cmake --build --preset windows-msvc-protocol-lab-debug
ctest --test-dir out/build/windows-msvc-protocol-lab -C Debug --output-on-failure -E udp_exchange
结果：13/13通过

cmake --build --preset windows-msvc-protocol-lab-release
ctest --test-dir out/build/windows-msvc-protocol-lab -C Release --output-on-failure -E udp_exchange
结果：13/13通过
```

隔离门禁：Product-only Release配置/构建成功且0测试；B开启但Testing关闭配置失败；
Schema 0.5 Compiler与普通Lab同时开启仍配置失败。日志位于：

- `out/dec042b-lab-b-debug.log`
- `out/dec042b-lab-b-release.log`
- `out/dec042b-lab-b-old-lab-debug.log`
- `out/dec042b-lab-b-old-lab-release.log`
- `out/dec042b-lab-b-product-only.log`
- `out/dec042b-lab-b-gates.log`

## 5. 边界

B不调用Decode、不产生Replay或Compare结论，也不验证字段ID/索引与真实Frozen Plan对应；这些属于C。
Hash和内部关联用于检测不一致，不能认证来源，也不能检测全面、自洽重写。未运行UDP、Linux、
真实协议Golden、设备、现场或性能测试；未修改历史Bundle或旧指纹算法。

## 6. Reader P2纠错复核（2026-09-08）

### 6.1 独立核实与修复前证据

- 整体交付缺口实际复现：Result未知属性和Event未知属性均命中目标语义诊断，但调用返回失败后
  `StoredBundle`仍保留已读取的配置、Values、Frame或已解析Result/指纹；同一对象先读取合法
  Bundle、再读取非法Event Bundle时也未恢复完整默认状态。
- 读前路径门禁缺口实际复现：隔离测试成功创建清单文件链接、负载文件链接和中间目录链接；
  内部读取观察器均记录到内容读取调用，Reader随后才拒绝链接。
- 修复前Debug专项退出码为8，日志为`out/dec042b-lab-b-reader-p2-before-debug.log`和
  `out/dec042b-lab-b-reader-p2-before-path-debug.log`。前者固定整体交付失败，后者在不改变读取
  顺序的最小观察接缝下固定三类链接的实际读取。

### 6.2 最小修复

- Reader入口仍先清空调用方输出，所有内容只写局部`StoredBundle`候选；Record、Result、Event、
  Metadata、Frame及指纹全部验证成功后才一次性移动交付。
- 任何内容读取前递归枚举Bundle，拒绝符号链接和非普通文件对象；Windows额外检查
  `FILE_ATTRIBUTE_REPARSE_POINT`，因此目录联接等重解析点也按同一门禁拒绝。随后保留原有清单、
  文件集合、长度、Hash及语义校验。
- `LoadEvidenceBundleForTest`只是该隔离内部模块的读取观察接缝，不接普通CLI，也不改变0.6格式、
  指纹或旧代实现。

### 6.3 修复后执行结果

```text
cmake --build out/b42bd
ctest --test-dir out/b42bd -C Debug --output-on-failure
结果：3/3通过，退出码0

cmake --build out/b42br
ctest --test-dir out/b42br -C Release --output-on-failure
结果：3/3通过，退出码0
```

日志：`out/dec042b-lab-b-reader-p2-debug.log`、
`out/dec042b-lab-b-reader-p2-release.log`。本轮没有修改A格式、普通Lab、Core、CMake或依赖关系，
因此未重复旧Lab、UDP、Product-only和隔离门禁历史矩阵；第4节历史结果不作为本轮重跑证据。

### 6.4 限制

动态用例覆盖Windows文件符号链接和目录符号链接；Windows目录联接由同一重解析属性检查静态
覆盖，但本轮未另行创建联接样例。读前检查与后续打开之间仍存在一般文件系统TOCTOU
（check/use竞态）窗口；本切片没有引入句柄级目录隔离或平台安全框架，也不把Hash提升为来源认证。
