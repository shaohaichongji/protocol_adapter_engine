#include <pae/compiler.h>

int HeaderCompilerCompiles() { return sizeof(pae::CompileOptions) == 0U ? 1 : 0; }
