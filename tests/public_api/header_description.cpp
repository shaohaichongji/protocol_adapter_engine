#include <pae/protocol_description.h>

int HeaderDescriptionCompiles() { return sizeof(pae::ProtocolDescription) == 0U ? 1 : 0; }
