# ASCII complete-record host example

This example is a public, from-scratch Schema 0.10 host integration check. The host supplies one
complete record, owns the input/output buffers and `ExecutionWorkspace`, and copies borrowed Decode
bytes before retaining them. It does not frame a stream, open a network endpoint, convert Unicode,
or use Protocol Lab.

The executable loads `examples/config/synthetic_ascii_text_slice.pae.json`, verifies independent
RX and TX vectors, and returns non-zero on any mismatch. Build it only with
`PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON` and `PAE_BUILD_ASCII_TEXT_EXAMPLE=ON`.
