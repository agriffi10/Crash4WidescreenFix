#ifndef CRASH4WIDESCREENFIX_STDAFX_H
#define CRASH4WIDESCREENFIX_STDAFX_H

// Must precede every include: the standard headers below pull in <cmath>, which only defines M_PI
// if this is already set
#define _USE_MATH_DEFINES
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

// Everything the pinned ModUtils uses from the standard library. It was written against MSVC
// headers that included each other more freely than current ones do, so it doesn't include all
// of these itself.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// The pinned ModUtils also calls stdext::make_checked_array_iterator, which current MSVC STL no
// longer provides. The STL headers are included above so this rename can't touch an older STL's
// own declaration, and ModUtils then gets a plain pointer instead.
#define make_checked_array_iterator widescreenfix_make_array_iterator
namespace stdext
{
    template<typename T>
    T* widescreenfix_make_array_iterator(T* ptr, size_t) { return ptr; }
}
#endif //CRASH4WIDESCREENFIX_STDAFX_H
