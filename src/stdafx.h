#ifndef CRASH4WIDESCREENFIX_STDAFX_H
#define CRASH4WIDESCREENFIX_STDAFX_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

// ModUtils relies on these arriving transitively, which current MSVC headers no longer guarantee
#include <cstdint>
#include <utility>

// The pinned ModUtils calls stdext::make_checked_array_iterator, which current MSVC STL no longer
// provides. The STL headers are included first so the rename below can't touch an older STL's own
// declaration, and ModUtils then gets a plain pointer instead.
#include <algorithm>
#include <iterator>
#define make_checked_array_iterator widescreenfix_make_array_iterator
namespace stdext
{
    template<typename T>
    T* widescreenfix_make_array_iterator(T* ptr, size_t) { return ptr; }
}
#endif //CRASH4WIDESCREENFIX_STDAFX_H
