#pragma once

// HGLOBAL/HLOCAL layout: the handle is the data pointer (GlobalLock(h) == h, so GMEM_FIXED
// callers may use the handle directly), preceded by a GlobalHeader.

#include "mfcwx/win32.h"

#include <cstddef>

namespace mfcwx {

struct GlobalHeader {
    unsigned magic;
    unsigned flags;
    size_t size;
    long lockCount;
    size_t reserved;
};

static_assert(sizeof(GlobalHeader) % 16 == 0, "data must stay 16-byte aligned");

constexpr unsigned kGlobalMagic = 0x474C4D58u;

inline GlobalHeader* GlobalHeaderOf(HGLOBAL h) {
    if (!h)
        return nullptr;
    auto* header = reinterpret_cast<GlobalHeader*>(static_cast<char*>(h) - sizeof(GlobalHeader));
    return header->magic == kGlobalMagic ? header : nullptr;
}

// Allocates an HGLOBAL holding a copy of data (size bytes; data may be null for zeroed memory).
HGLOBAL GlobalFromData(const void* data, size_t size, UINT flags = GMEM_MOVEABLE);
bool GlobalIsValid(HGLOBAL h);
inline void* GlobalData(HGLOBAL h) { return GlobalHeaderOf(h) ? h : nullptr; }
inline size_t GlobalDataSize(HGLOBAL h) {
    GlobalHeader* header = GlobalHeaderOf(h);
    return header ? header->size : 0;
}

} // namespace mfcwx
