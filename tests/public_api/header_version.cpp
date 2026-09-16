#include <pae/version.h>

int HeaderVersionCompiles() { return pae::kPublicApiVersion.empty() ? 1 : 0; }
