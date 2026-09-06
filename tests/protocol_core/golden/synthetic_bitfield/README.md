# DEC-040 公开人工向量

此目录与 `examples/config/synthetic_bitfield_slice.pae.json` 均从零设计，不来自客户协议、生产端点、现场报文或私有资料。

`bitfield_record_001.frame.hex` 由容器整数和位掩码独立手工推导：

- `flags8`: `0xA0 | 1 | (2 << 1) | (1 << 3) = 0xAD`。
- `be_lsb16`: `0x8000 | (0x5A << 4) = 0x85A0`，按 Big Endian 输出 `85 A0`。
- `le_msb16`: `0x0003 | (0x101 << 6) = 0x4043`，按 Little Endian 输出 `43 40`。
- `be_msb32`: `0x80000001 | (0xBEEF << 8) = 0x80BEEF01`。
- `le_lsb64`: `0x0123456789ABCDEF`，按 Little Endian 输出 `EF CD AB 89 67 45 23 01`。
- `tail8`: 基础值 `0x55` 的 MSB0 末位（实际最低位）写 `false`，得到 `0x54`。
