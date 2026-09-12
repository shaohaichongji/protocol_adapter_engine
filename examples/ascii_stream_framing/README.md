# ASCII CRLF Stream Host Example

This public synthetic example compiles the Schema 0.11 sample, creates one framing and one Core
workspace for a logical stream, submits `RX A!OK\r` and `\n` as separate chunks, and copies the
borrowed frame/field result in the synchronous callback. Its bounded drive loop resubmits only the
unconsumed suffix and stops if no progress is possible.

It does not open a socket, own a transport, append TX terminators, or provide Lab/Evidence/UI
integration.
