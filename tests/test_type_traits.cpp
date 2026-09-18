// Tests for neko/type_traits.hpp.
//
// The traits already written in the header are checked against <type_traits>:
// if neko::X and std::X ever disagree, that is the bug. Everything under
// "YOUR TURN" in the header has a test waiting inside an `#if 0` block at the
// bottom -- flip one to `#if 1` and make it compile.
#include "test_framework.hpp"

#include <type_traits>  // the reference implementation to check against

#include "neko/type_traits.hpp"

namespace {

struct Incomplete;  // never defined: traits must not require completeness
struct Base {};
struct Derived : Base {};
enum Color { red, green };
using Fn = int(double);

// The void_t detection idiom, used below to ask "does this type have a ::type
// member?" without that question being an error when the answer is no. The
// partial specialisation is only viable when `typename T::type` names
// something; otherwise substitution fails and the primary template wins.
template <typename T, typename = void>
struct has_type_member : neko::false_type {};
template <typename T>
struct has_type_member<T, neko::void_t<typename T::type>> : neko::true_type {};

// Instantiating this is a hard error. Naming it as a template argument is not
// -- which is exactly the difference conjunction/disjunction have to respect.
template <typename T>
struct never_instantiate {
    static_assert(sizeof(T) == 0, "combinator failed to short-circuit");
    static constexpr bool value = true;
};

// Asserts neko::TRAIT<T...> and std::TRAIT<T...> agree on the value.
#define SAME_AS_STD_VALUE(TRAIT, ...)                                          \
    STATIC_CHECK(neko::TRAIT##_v<__VA_ARGS__> == std::TRAIT##_v<__VA_ARGS__>)

// Asserts neko::TRAIT<T...>::type and std::TRAIT<T...>::type are the same type.
#define SAME_AS_STD_TYPE(TRAIT, ...)                                           \
    STATIC_CHECK(neko::is_same_v<neko::TRAIT##_t<__VA_ARGS__>,                 \
                                 std::TRAIT##_t<__VA_ARGS__>>)

}  // namespace

// ---------------------------------------------------------------------------
// The foundations
// ---------------------------------------------------------------------------

NEKO_TEST(integral_constant_lifts_a_value_into_a_type) {
    using two = neko::integral_constant<int, 2>;
    STATIC_CHECK(two::value == 2);
    STATIC_CHECK(neko::is_same_v<two::value_type, int>);
    STATIC_CHECK(neko::is_same_v<two::type, two>);

    // The conversion operator and operator() are what let a trait be used
    // directly as a bool and as a callable tag.
    STATIC_CHECK(two{} == 2);
    STATIC_CHECK(two{}() == 2);

    STATIC_CHECK(neko::true_type::value);
    STATIC_CHECK(!neko::false_type::value);
    STATIC_CHECK(neko::is_same_v<neko::bool_constant<true>, neko::true_type>);
}

NEKO_TEST(is_same_distinguishes_cv_and_ref) {
    STATIC_CHECK(neko::is_same_v<int, int>);
    STATIC_CHECK(!neko::is_same_v<int, const int>);
    STATIC_CHECK(!neko::is_same_v<int, int&>);
    STATIC_CHECK(!neko::is_same_v<int, long>);

    // Works on types that are never defined.
    STATIC_CHECK(neko::is_same_v<Incomplete, Incomplete>);
}

NEKO_TEST(void_t_maps_anything_to_void) {
    STATIC_CHECK(neko::is_same_v<neko::void_t<>, void>);
    STATIC_CHECK(neko::is_same_v<neko::void_t<int, char*, Base>, void>);
}

NEKO_TEST(conditional_selects_a_branch) {
    STATIC_CHECK(neko::is_same_v<neko::conditional_t<true, int, char>, int>);
    STATIC_CHECK(neko::is_same_v<neko::conditional_t<false, int, char>, char>);
    SAME_AS_STD_TYPE(conditional, true, int, char);
}

NEKO_TEST(enable_if_has_a_type_member_only_when_true) {
    STATIC_CHECK(neko::is_same_v<neko::enable_if_t<true, int>, int>);
    STATIC_CHECK(neko::is_same_v<neko::enable_if_t<true>, void>);

    // And the SFINAE behaviour it exists for: `enable_if<false>::type` must
    // be *absent*, and asking about it must be a substitution failure rather
    // than a compile error. That is what makes an overload drop out silently.
    STATIC_CHECK(has_type_member<neko::enable_if<true, int>>::value);
    STATIC_CHECK(!has_type_member<neko::enable_if<false, int>>::value);
}

// ---------------------------------------------------------------------------
// References
// ---------------------------------------------------------------------------

NEKO_TEST(remove_reference_strips_one_reference) {
    SAME_AS_STD_TYPE(remove_reference, int);
    SAME_AS_STD_TYPE(remove_reference, int&);
    SAME_AS_STD_TYPE(remove_reference, int&&);
    SAME_AS_STD_TYPE(remove_reference, const int&);

    // It strips the reference, not the const: `const int&` -> `const int`.
    STATIC_CHECK(
        neko::is_same_v<neko::remove_reference_t<const int&>, const int>);
}

NEKO_TEST(reference_categories) {
    SAME_AS_STD_VALUE(is_lvalue_reference, int);
    SAME_AS_STD_VALUE(is_lvalue_reference, int&);
    SAME_AS_STD_VALUE(is_lvalue_reference, int&&);
    SAME_AS_STD_VALUE(is_rvalue_reference, int&);
    SAME_AS_STD_VALUE(is_rvalue_reference, int&&);
    SAME_AS_STD_VALUE(is_reference, int);
    SAME_AS_STD_VALUE(is_reference, int&);
    SAME_AS_STD_VALUE(is_reference, int&&);
}

NEKO_TEST(add_reference_is_sfinae_friendly) {
    SAME_AS_STD_TYPE(add_lvalue_reference, int);
    SAME_AS_STD_TYPE(add_rvalue_reference, int);

    // Reference collapsing: adding & to int&& gives int&.
    STATIC_CHECK(neko::is_same_v<neko::add_lvalue_reference_t<int&&>, int&>);
    STATIC_CHECK(neko::is_same_v<neko::add_rvalue_reference_t<int&>, int&>);

    // The point of the try_add_*_reference overload trick: `void&` is
    // ill-formed, so the trait must yield void rather than fail to compile.
    SAME_AS_STD_TYPE(add_lvalue_reference, void);
    SAME_AS_STD_TYPE(add_rvalue_reference, void);
    SAME_AS_STD_TYPE(add_lvalue_reference, const void);
}

// ---------------------------------------------------------------------------
// cv-qualifiers
// ---------------------------------------------------------------------------

NEKO_TEST(cv_removal) {
    SAME_AS_STD_TYPE(remove_const, const int);
    SAME_AS_STD_TYPE(remove_volatile, volatile int);
    SAME_AS_STD_TYPE(remove_cv, const volatile int);

    // A const pointer is not a pointer to const: only the former loses const.
    STATIC_CHECK(neko::is_same_v<neko::remove_const_t<int* const>, int*>);
    STATIC_CHECK(neko::is_same_v<neko::remove_const_t<const int*>, const int*>);

    // remove_const does *not* see through a reference -- that is why
    // remove_cvref removes the reference first.
    STATIC_CHECK(neko::is_same_v<neko::remove_const_t<const int&>, const int&>);
}

NEKO_TEST(remove_cvref_strips_both) {
    STATIC_CHECK(neko::is_same_v<neko::remove_cvref_t<const int&>, int>);
    STATIC_CHECK(neko::is_same_v<neko::remove_cvref_t<volatile int&&>, int>);
    STATIC_CHECK(neko::is_same_v<neko::remove_cvref_t<int>, int>);
    SAME_AS_STD_TYPE(remove_cvref, const volatile int&);
}

NEKO_TEST(is_const_looks_at_the_top_level_only) {
    SAME_AS_STD_VALUE(is_const, const int);
    SAME_AS_STD_VALUE(is_const, int);
    SAME_AS_STD_VALUE(is_const, const int*);  // pointer itself is mutable
    SAME_AS_STD_VALUE(is_const, int* const);  // this one is the const
    SAME_AS_STD_VALUE(is_const, const int&);  // references are never const
}

// ---------------------------------------------------------------------------
// Pointers and arrays
// ---------------------------------------------------------------------------

NEKO_TEST(pointer_traits) {
    SAME_AS_STD_VALUE(is_pointer, int*);
    SAME_AS_STD_VALUE(is_pointer, int);
    SAME_AS_STD_VALUE(is_pointer, int* const);  // remove_cv_t makes this true
    SAME_AS_STD_VALUE(is_pointer, const int*);
    SAME_AS_STD_VALUE(is_pointer, int&);  // a reference is not a pointer

    SAME_AS_STD_TYPE(remove_pointer, int*);
    SAME_AS_STD_TYPE(remove_pointer, int* const);
    SAME_AS_STD_TYPE(remove_pointer, const int*);
    SAME_AS_STD_TYPE(remove_pointer, int);
    SAME_AS_STD_TYPE(remove_pointer, int**);  // strips one level only
}

NEKO_TEST(array_traits) {
    SAME_AS_STD_VALUE(is_array, int[3]);
    SAME_AS_STD_VALUE(is_array, int[]);
    SAME_AS_STD_VALUE(is_array, int);
    SAME_AS_STD_VALUE(is_array, int*);

    SAME_AS_STD_TYPE(remove_extent, int[3]);
    SAME_AS_STD_TYPE(remove_extent, int[]);
    SAME_AS_STD_TYPE(remove_extent, int);
    // One dimension at a time: int[2][3] -> int[3].
    SAME_AS_STD_TYPE(remove_extent, int[2][3]);
}

// ---------------------------------------------------------------------------
// Primary categories
// ---------------------------------------------------------------------------

NEKO_TEST(is_integral_enumerates_the_integer_types) {
    SAME_AS_STD_VALUE(is_integral, bool);
    SAME_AS_STD_VALUE(is_integral, char);
    SAME_AS_STD_VALUE(is_integral, signed char);
    SAME_AS_STD_VALUE(is_integral, unsigned char);
    SAME_AS_STD_VALUE(is_integral, char16_t);
    SAME_AS_STD_VALUE(is_integral, wchar_t);
    SAME_AS_STD_VALUE(is_integral, short);
    SAME_AS_STD_VALUE(is_integral, int);
    SAME_AS_STD_VALUE(is_integral, unsigned long long);
    SAME_AS_STD_VALUE(is_integral, const int);  // cv-stripped first
    SAME_AS_STD_VALUE(is_integral, float);
    SAME_AS_STD_VALUE(is_integral, int&);   // a reference is not
    SAME_AS_STD_VALUE(is_integral, Color);  // nor is an enum
}

NEKO_TEST(is_floating_point_and_is_arithmetic) {
    SAME_AS_STD_VALUE(is_floating_point, float);
    SAME_AS_STD_VALUE(is_floating_point, double);
    SAME_AS_STD_VALUE(is_floating_point, long double);
    SAME_AS_STD_VALUE(is_floating_point, const double);
    SAME_AS_STD_VALUE(is_floating_point, int);

    SAME_AS_STD_VALUE(is_arithmetic, int);
    SAME_AS_STD_VALUE(is_arithmetic, double);
    SAME_AS_STD_VALUE(is_arithmetic, int*);
    SAME_AS_STD_VALUE(is_arithmetic, Base);
}

NEKO_TEST(is_void_ignores_cv) {
    SAME_AS_STD_VALUE(is_void, void);
    SAME_AS_STD_VALUE(is_void, const void);
    SAME_AS_STD_VALUE(is_void, void*);  // a pointer to void is not void
    SAME_AS_STD_VALUE(is_void, int);
}

// ---------------------------------------------------------------------------
// Logical combinators
// ---------------------------------------------------------------------------

NEKO_TEST(conjunction_and_disjunction_short_circuit) {
    using T = neko::true_type;
    using F = neko::false_type;

    STATIC_CHECK(neko::conjunction_v<>);
    STATIC_CHECK(neko::conjunction_v<T, T, T>);
    STATIC_CHECK(!neko::conjunction_v<T, F, T>);
    STATIC_CHECK(!neko::disjunction_v<>);
    STATIC_CHECK(neko::disjunction_v<F, T, F>);
    STATIC_CHECK(!neko::disjunction_v<F, F>);

    STATIC_CHECK(neko::negation_v<F>);
    STATIC_CHECK(!neko::negation_v<T>);

    // They inherit from the *deciding* argument, not from bool_constant --
    // which is how `conjunction<is_integral<T>...>` keeps its value_type.
    STATIC_CHECK(neko::is_same_v<neko::conjunction<T, F, T>::type, F>);
    STATIC_CHECK(neko::is_same_v<neko::disjunction<F, T, F>::type, T>);

    // Short-circuiting is the real requirement, and it is load-bearing: the
    // false_type already decides the answer, so the trait after it must never
    // be instantiated. If either of these compiles only by instantiating
    // never_instantiate, the static_assert inside it fires instead.
    STATIC_CHECK(!neko::conjunction_v<F, never_instantiate<int>>);
    STATIC_CHECK(neko::disjunction_v<T, never_instantiate<int>>);
}

// ============================================================================
// YOUR TURN -- flip an `#if 0` to `#if 1`, then implement the trait until this
// file compiles again. These are compile-time tests, so an unimplemented trait
// is a *build* error, not a TODO: only open one block at a time.
// ============================================================================

#if 0  // add_const / add_pointer
NEKO_TEST(add_const_and_add_pointer) {
    SAME_AS_STD_TYPE(add_const, int);
    SAME_AS_STD_TYPE(add_const, const int);   // already const: unchanged
    SAME_AS_STD_TYPE(add_const, int&);        // references cannot be const
    SAME_AS_STD_TYPE(add_pointer, int);
    SAME_AS_STD_TYPE(add_pointer, int&);      // int& -> int*
    SAME_AS_STD_TYPE(add_pointer, void);
    SAME_AS_STD_TYPE(add_pointer, Fn);        // function -> function pointer
}
#endif

#if 0  // decay -- what happens to a by-value function parameter
NEKO_TEST(decay) {
    SAME_AS_STD_TYPE(decay, int);
    SAME_AS_STD_TYPE(decay, const int&);      // -> int
    SAME_AS_STD_TYPE(decay, int&&);           // -> int
    SAME_AS_STD_TYPE(decay, int[3]);          // -> int*      (array-to-pointer)
    SAME_AS_STD_TYPE(decay, const char[6]);   // -> const char*
    SAME_AS_STD_TYPE(decay, Fn);              // -> int(*)(double)
    SAME_AS_STD_TYPE(decay, Fn&);
}
#endif

#if 0  // signedness
NEKO_TEST(signedness) {
    SAME_AS_STD_VALUE(is_signed, int);
    SAME_AS_STD_VALUE(is_signed, unsigned);
    SAME_AS_STD_VALUE(is_signed, double);     // true: not just integers
    SAME_AS_STD_VALUE(is_signed, Base);
    SAME_AS_STD_VALUE(is_unsigned, unsigned);
    SAME_AS_STD_VALUE(is_unsigned, bool);     // true: bool is unsigned
    SAME_AS_STD_VALUE(is_unsigned, int);

    SAME_AS_STD_TYPE(make_unsigned, int);
    SAME_AS_STD_TYPE(make_unsigned, long);
    SAME_AS_STD_TYPE(make_signed, unsigned);
    SAME_AS_STD_TYPE(make_unsigned, const int);   // preserves cv
}
#endif

#if 0  // common_type
NEKO_TEST(common_type) {
    SAME_AS_STD_TYPE(common_type, int);
    SAME_AS_STD_TYPE(common_type, int, long);
    SAME_AS_STD_TYPE(common_type, int, double);
    SAME_AS_STD_TYPE(common_type, int, int, long);
    SAME_AS_STD_TYPE(common_type, Derived*, Base*);
    // The definition is literally "the type of the ternary operator":
    //   decay_t<decltype(false ? declval<T>() : declval<U>())>
    SAME_AS_STD_TYPE(common_type, const int&, int&);  // -> int, decayed
}
#endif

#if 0  // is_convertible -- needs declval + SFINAE
NEKO_TEST(is_convertible) {
    SAME_AS_STD_VALUE(is_convertible, int, long);
    SAME_AS_STD_VALUE(is_convertible, Derived*, Base*);
    SAME_AS_STD_VALUE(is_convertible, Base*, Derived*);  // false: downcast
    SAME_AS_STD_VALUE(is_convertible, int, Base);
    SAME_AS_STD_VALUE(is_convertible, void, void);       // true, special case
    SAME_AS_STD_VALUE(is_convertible, int, void);        // also true
}
#endif

#if 0  // is_base_of -- needs the __is_base_of intrinsic
NEKO_TEST(is_base_of) {
    SAME_AS_STD_VALUE(is_base_of, Base, Derived);
    SAME_AS_STD_VALUE(is_base_of, Derived, Base);
    SAME_AS_STD_VALUE(is_base_of, Base, Base);   // true: a class is its own base
    SAME_AS_STD_VALUE(is_base_of, int, int);     // false: not a class type
    // Unlike is_convertible, this ignores access: it is true even for a
    // private base. That is why it needs a compiler intrinsic.
}
#endif

#if 0  // constructibility
NEKO_TEST(constructibility) {
    SAME_AS_STD_VALUE(is_default_constructible, int);
    SAME_AS_STD_VALUE(is_default_constructible, Base);
    SAME_AS_STD_VALUE(is_default_constructible, int&);
    SAME_AS_STD_VALUE(is_copy_constructible, Base);
    SAME_AS_STD_VALUE(is_move_constructible, Base);
    SAME_AS_STD_VALUE(is_constructible, int, double);
    SAME_AS_STD_VALUE(is_constructible, Base, int);
}
#endif

#if 0  // the intrinsic-only traits: the lesson is that these CANNOT be written
       // in pure C++ -- there is no expression that observes triviality.
NEKO_TEST(trivial_and_nothrow) {
    SAME_AS_STD_VALUE(is_trivially_copyable, int);
    SAME_AS_STD_VALUE(is_trivially_destructible, Base);
    SAME_AS_STD_VALUE(is_nothrow_move_constructible, int);
    // This is the one vector::reserve must ask before it dares to move:
    SAME_AS_STD_VALUE(is_nothrow_move_constructible, neko_test::ThrowingMove);
}
#endif

#if 0  // invoke_result -- do this after utility.hpp is done
NEKO_TEST(invoke_result) {
    auto lambda = [](int, char) { return 3.5; };
    STATIC_CHECK(neko::is_same_v<neko::invoke_result_t<decltype(lambda), int, char>, double>);
    STATIC_CHECK(neko::is_same_v<neko::invoke_result_t<Fn*, double>, int>);
}
#endif
