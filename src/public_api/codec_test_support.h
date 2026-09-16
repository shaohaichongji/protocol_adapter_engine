#pragma once

namespace pae::public_api_internal::test_only {

using CodecEnteredHook = void (*)(void*) noexcept;

// Non-installed test seam. The hook runs after the facade busy guard is acquired and before any
// result generation, slot, input-map, or raw-value state is touched.
void SetCodecEnteredHook(CodecEnteredHook hook, void* context) noexcept;

}  // namespace pae::public_api_internal::test_only
