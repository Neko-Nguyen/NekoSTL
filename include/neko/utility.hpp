// ============================================================================
// neko/utility.hpp
//
// Tests: tests/test_utility.cpp
// ============================================================================
#pragma once

#include "neko/config.hpp"
#include "neko/type_traits.hpp"

namespace neko {

// --- declval: moved ---------------------------------------------------------
// Now declared in type_traits.hpp, which needs it for the detection traits and
// cannot include this header. Still reachable as neko::declval from here.

template <typename T>
constexpr const T& as_const(T& t) noexcept {
    return t;
}
template <typename T>
void as_const(const T&&) = delete;  // would dangle

template <typename T>
constexpr remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<remove_reference_t<T>&&>(t);
}

template <typename T>
constexpr T&& forward(remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}

template <typename T>
constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
    static_assert(!is_lvalue_reference_v<T>,
                  "cannot forward an rvalue as an lvalue");
    return static_cast<T&&>(t);
}

template <typename T>
conditional_t<!is_nothrow_move_constructible<T>::value &&
                  is_copy_constructible<T>::value,
              const T&, T&&>
move_if_noexcept(T& t) noexcept {
    return static_cast<conditional_t<!is_nothrow_move_constructible<T>::value &&
                                         is_copy_constructible<T>::value,
                                     const T&, T &&>>(t);
}

template <typename T>
constexpr void swap(T& a, T& b) {
    T tmp = neko::move(a);
    a = neko::move(b);
    b = neko::move(tmp);
}

template <typename T, typename U = T>
constexpr T exchange(T& obj, U&& new_value) {
    T tmp = neko::move(obj);
    obj = neko::forward<U>(new_value);
    return tmp;
}

}  // namespace neko
