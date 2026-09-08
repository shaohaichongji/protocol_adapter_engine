# yyjson 0.12.0

本目录保存PAE当前正式采用的yyjson最小随仓源码范围。yyjson只用于内部Config Compiler和
Protocol Lab JSON处理，不进入`pae_protocol_plan`或`pae_protocol_core_slice`依赖链。

## 来源与完整性

- 上游：<https://github.com/ibireme/yyjson>
- 版本：0.12.0
- Commit：`8b4a38dc994a110abaec8a400615567bd996105f`
- Tag object：`7871d321ff4cd8068c1f777c97975dc2fb640ab3`
- License：MIT，原文保存在`LICENSE`
- 本地修改：none（无）
- 唯一当前锁：`dependency.lock.json`

随仓的上游文件仅为`LICENSE`、`src/yyjson.h`和`src/yyjson.c`。它们从已核验的本地上游归档
缓存按字节复制，未经过格式化或源码修改。归档URL和Hash用于来源追溯；正常Configure不会下载
归档，也不读取`out/`缓存。`.gitattributes`为这三个精确路径设置`-text`，避免Git根据
`core.autocrlf`或平台默认换行规则改变锁定的上游字节；README和锁文件仍按仓库文本规则处理。

## 更新规则

升级或替换必须先单独批准版本和兼容性范围，然后复核上游Tag、Commit、License及最小源码
范围，按字节替换文件并更新锁中的长度和SHA-256。完成后重新执行依赖合同、Compiler、离线
Protocol Lab、纯格式专项和产品隔离门禁。不得只修改Hash来接纳未经审查的源码变化。

nlohmann/json和RapidJSON仍只属于`spikes/json_parser`历史对比范围，不在本目录随仓。
