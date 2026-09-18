// ============================================================================
// neko/type_traits.hpp  --  WORKED REFERENCE MODULE
//
// This one is written out in full, with comments, as the pattern you will copy
// for everything else. Traits are the foundation: every container, iterator and
// algorithm in the STL is built out of them. Read it top to bottom once, then
// go add the ones listed under "YOUR TURN" at the bottom.
// ============================================================================
#pragma once

#include <cstddef>

namespace neko {

// ---------------------------------------------------------------------------
// integral_constant: the trick that makes the whole library work.
// A *value* is lifted into a *type*, so it can be used in overload resolution
// and partial specialisation. true_type and false_type are the two you see
// everywhere.
// ---------------------------------------------------------------------------
template <typename T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant;

    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; }
};

template <bool B>
using bool_constant = integral_constant<bool, B>;

using true_type = bool_constant<true>;
using false_type = bool_constant<false>;

// ---------------------------------------------------------------------------
// is_same: the archetypal trait. Primary template says "no"; the partial
// specialisation for two identical types is more specialised, so it wins.
// ---------------------------------------------------------------------------
template <typename T, typename U>
struct is_same : false_type {};

template <typename T>
struct is_same<T, T> : true_type {};

template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

// ---------------------------------------------------------------------------
// void_t: maps any pack of types to void. The engine behind SFINAE-based
// detection ("does T have a member called value_type?").
// ---------------------------------------------------------------------------
template <typename...>
using void_t = void;

// type_identity: an identity function for types. Two uses -- it gives you a
// `::type` to inherit, and it blocks template argument deduction (a parameter
// of type type_identity_t<T> is a "non-deduced context").
template <typename T>
struct type_identity {
    using type = T;
};
template <typename T>
using type_identity_t = typename type_identity<T>::type;

// ---------------------------------------------------------------------------
// conditional / enable_if: compile-time if, and the SFINAE gate.
// ---------------------------------------------------------------------------
template <bool B, typename T, typename F>
struct conditional {
    using type = T;
};

template <typename T, typename F>
struct conditional<false, T, F> {
    using type = F;
};

template <bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

// No `type` member when B is false -> substitution fails -> overload silently
// drops out of the candidate set. That "silently" is the whole point.
template <bool B, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> {
    using type = T;
};

template <bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

// ---------------------------------------------------------------------------
// Reference manipulation. Note there is no `T&&&` -- reference collapsing in
// the language handles that; here we just strip and add.
// ---------------------------------------------------------------------------
template <typename T>
struct remove_reference {
    using type = T;
};
template <typename T>
struct remove_reference<T&> {
    using type = T;
};
template <typename T>
struct remove_reference<T&&> {
    using type = T;
};

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

template <typename T>
struct is_lvalue_reference : false_type {};
template <typename T>
struct is_lvalue_reference<T&> : true_type {};
template <typename T>
inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

template <typename T>
struct is_rvalue_reference : false_type {};
template <typename T>
struct is_rvalue_reference<T&&> : true_type {};
template <typename T>
inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

template <typename T>
struct is_reference
    : bool_constant<is_lvalue_reference_v<T> || is_rvalue_reference_v<T>> {};
template <typename T>
inline constexpr bool is_reference_v = is_reference<T>::value;

// add_*_reference must be SFINAE-friendly: `T&` is ill-formed for void and for
// function types with cv/ref qualifiers, but the trait must yield T instead of
// erroring. The overload trick below does exactly that -- if `T&` is invalid,
// substitution into the first overload fails and the `...` overload is picked.
namespace detail {
template <typename T>
auto try_add_lvalue_reference(int) -> type_identity<T&>;
template <typename T>
auto try_add_lvalue_reference(...) -> type_identity<T>;

template <typename T>
auto try_add_rvalue_reference(int) -> type_identity<T&&>;
template <typename T>
auto try_add_rvalue_reference(...) -> type_identity<T>;
}  // namespace detail

template <typename T>
struct add_lvalue_reference : decltype(detail::try_add_lvalue_reference<T>(0)) {
};
template <typename T>
using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

template <typename T>
struct add_rvalue_reference : decltype(detail::try_add_rvalue_reference<T>(0)) {
};
template <typename T>
using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

// ---------------------------------------------------------------------------
// declval: DONE (reference)
//
// Produces a "value" of type T in an unevaluated context, without requiring T
// to be constructible. Never define it -- calling it is a hard error, which is
// exactly what you want: it may only appear inside decltype/sizeof/noexcept.
// Returns T&& (not T) so that decltype(declval<T>()) works for non-movable T.
//
// It lives here rather than in utility.hpp because the detection traits below
// need it, and type_traits.hpp cannot include utility.hpp -- the dependency
// only runs the other way. <type_traits> in the real library does the same.
// ---------------------------------------------------------------------------
template <typename T>
add_rvalue_reference_t<T> declval() noexcept;

// ---------------------------------------------------------------------------
// cv-qualifier manipulation.
// ---------------------------------------------------------------------------
template <typename T>
struct remove_const {
    using type = T;
};
template <typename T>
struct remove_const<const T> {
    using type = T;
};
template <typename T>
using remove_const_t = typename remove_const<T>::type;

template <typename T>
struct remove_volatile {
    using type = T;
};
template <typename T>
struct remove_volatile<volatile T> {
    using type = T;
};
template <typename T>
using remove_volatile_t = typename remove_volatile<T>::type;

template <typename T>
struct remove_cv {
    using type = remove_volatile_t<remove_const_t<T>>;
};
template <typename T>
using remove_cv_t = typename remove_cv<T>::type;

// The workhorse: strip references *then* cv. Order matters -- `const T&`
// must lose the reference before the const is visible.
template <typename T>
struct remove_cvref {
    using type = remove_cv_t<remove_reference_t<T>>;
};
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

template <typename T>
struct is_const : false_type {};
template <typename T>
struct is_const<const T> : true_type {};
template <typename T>
inline constexpr bool is_const_v = is_const<T>::value;

// ---------------------------------------------------------------------------
// Pointer traits.
// ---------------------------------------------------------------------------
namespace detail {
template <typename T>
struct is_pointer_impl : false_type {};
template <typename T>
struct is_pointer_impl<T*> : true_type {};
}  // namespace detail

// Applied to remove_cv_t<T> so that `int* const` still counts as a pointer.
template <typename T>
struct is_pointer : detail::is_pointer_impl<remove_cv_t<T>> {};
template <typename T>
inline constexpr bool is_pointer_v = is_pointer<T>::value;

template <typename T>
struct remove_pointer {
    using type = T;
};
template <typename T>
struct remove_pointer<T*> {
    using type = T;
};
template <typename T>
struct remove_pointer<T* const> {
    using type = T;
};
template <typename T>
using remove_pointer_t = typename remove_pointer<T>::type;

// ---------------------------------------------------------------------------
// Array traits.
// ---------------------------------------------------------------------------
template <typename T>
struct is_array : false_type {};
template <typename T>
struct is_array<T[]> : true_type {};
template <typename T, std::size_t N>
struct is_array<T[N]> : true_type {};
template <typename T>
inline constexpr bool is_array_v = is_array<T>::value;

template <typename T>
struct remove_extent {
    using type = T;
};
template <typename T>
struct remove_extent<T[]> {
    using type = T;
};
template <typename T, std::size_t N>
struct remove_extent<T[N]> {
    using type = T;
};
template <typename T>
using remove_extent_t = typename remove_extent<T>::type;

// ---------------------------------------------------------------------------
// Primary type categories. These are the ones that must be *enumerated* --
// there is no language construct to ask "is this an integer?", so the standard
// library just lists them.
// ---------------------------------------------------------------------------
namespace detail {
template <typename T>
struct is_integral_impl : false_type {};
template <>
struct is_integral_impl<bool> : true_type {};
template <>
struct is_integral_impl<char> : true_type {};
template <>
struct is_integral_impl<signed char> : true_type {};
template <>
struct is_integral_impl<unsigned char> : true_type {};
template <>
struct is_integral_impl<char8_t> : true_type {};
template <>
struct is_integral_impl<char16_t> : true_type {};
template <>
struct is_integral_impl<char32_t> : true_type {};
template <>
struct is_integral_impl<wchar_t> : true_type {};
template <>
struct is_integral_impl<short> : true_type {};
template <>
struct is_integral_impl<unsigned short> : true_type {};
template <>
struct is_integral_impl<int> : true_type {};
template <>
struct is_integral_impl<unsigned int> : true_type {};
template <>
struct is_integral_impl<long> : true_type {};
template <>
struct is_integral_impl<unsigned long> : true_type {};
template <>
struct is_integral_impl<long long> : true_type {};
template <>
struct is_integral_impl<unsigned long long> : true_type {};

template <typename T>
struct is_floating_point_impl : false_type {};
template <>
struct is_floating_point_impl<float> : true_type {};
template <>
struct is_floating_point_impl<double> : true_type {};
template <>
struct is_floating_point_impl<long double> : true_type {};
}  // namespace detail

template <typename T>
struct is_integral : detail::is_integral_impl<remove_cv_t<T>> {};
template <typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

template <typename T>
struct is_floating_point : detail::is_floating_point_impl<remove_cv_t<T>> {};
template <typename T>
inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

template <typename T>
struct is_arithmetic
    : bool_constant<is_integral_v<T> || is_floating_point_v<T>> {};
template <typename T>
inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

template <typename T>
struct is_void : is_same<void, remove_cv_t<T>> {};
template <typename T>
inline constexpr bool is_void_v = is_void<T>::value;

// ---------------------------------------------------------------------------
// Logical combinators. Short-circuiting matters: conjunction must not
// instantiate later traits once one is false.
// ---------------------------------------------------------------------------
template <typename...>
struct conjunction : true_type {};
template <typename B1>
struct conjunction<B1> : B1 {};
template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : conditional_t<bool(B1::value), conjunction<Bn...>, B1> {};
template <typename... B>
inline constexpr bool conjunction_v = conjunction<B...>::value;

template <typename...>
struct disjunction : false_type {};
template <typename B1>
struct disjunction<B1> : B1 {};
template <typename B1, typename... Bn>
struct disjunction<B1, Bn...>
    : conditional_t<bool(B1::value), B1, disjunction<Bn...>> {};
template <typename... B>
inline constexpr bool disjunction_v = disjunction<B...>::value;

template <typename B>
struct negation : bool_constant<!bool(B::value)> {};
template <typename B>
inline constexpr bool negation_v = negation<B>::value;

// ============================================================================
// YOUR TURN
//
// Add these here as you need them. The tests for each are already written in
// tests/test_type_traits.cpp inside `#if 0` blocks -- flip a block to `#if 1`
// and make it compile.
//
//   add_const / add_pointer
//   decay
//       array -> pointer, function -> pointer, otherwise remove_cvref. This is
//       what happens to a by-value function parameter.
//   is_signed / is_unsigned / make_signed / make_unsigned
//   common_type
//   is_convertible                (needs declval + SFINAE)
//   is_base_of                    (needs the __is_base_of intrinsic)
//   is_constructible / is_default_constructible / is_copy_constructible
//   is_trivially_*                (these really do need compiler intrinsics --
//       nothing in the language lets you observe triviality)
//   invoke_result                 (do this after utility.hpp is done)
//
// The three below are declared at the bottom of this header because
// neko::move_if_noexcept needs their names to exist. Define them there.
// ============================================================================

template <typename T, typename = void>
struct is_move_constructible : false_type {};
template <typename T>
struct is_move_constructible<T, void_t<decltype(T(declval<T&&>()))>>
    : true_type {};
template <typename T>
inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;

template <typename T, typename = void>
struct is_copy_constructible : false_type {};
template <typename T>
struct is_copy_constructible<T, void_t<decltype(T(declval<const T&>()))>>
    : true_type {};
template <typename T>
inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

template <typename T, typename = void>
struct is_nothrow_move_constructible : false_type {};
template <typename T>
struct is_nothrow_move_constructible<T, void_t<decltype(T(declval<T&&>()))>>
    : conditional_t<noexcept(T(declval<T&&>())), true_type, false_type> {};
template <typename T>
inline constexpr bool is_nothrow_move_constructible_v =
    is_nothrow_move_constructible<T>::value;

}  // namespace neko
