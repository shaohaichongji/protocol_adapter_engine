# CMake 辅助模块

这里保存已经出现真实复用需求的项目级CMake模块。当前包括正式yyjson本地锁读取、
来源/License/源码复核和Spike/Compiler/Lab共享内部静态目标创建。唯一当前锁位于
`third_party/yyjson/dependency.lock.json`；缺失或字节/Hash漂移在Configure阶段失败关闭，
不通过FetchContent联网补齐。

V0.1不预建复杂宏体系；通用编译选项仍直接定义在顶层`CMakeLists.txt`中。
