#include <pae/stream_framer.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<pae::StreamFramer>);
static_assert(std::is_nothrow_move_constructible_v<pae::StreamFramer>);

int HeaderStreamFramerCompiles() { return 0; }
