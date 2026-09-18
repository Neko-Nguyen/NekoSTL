// Tests for neko/utility.hpp -- start here.
//
// The STATIC_CHECKs below already pass: they only inspect the *declared*
// signatures, and those are written for you. They are the specification. The
// runtime tests report TODO until you fill the bodies in.
//
// Work in this order: move -> forward -> swap -> exchange.
#include "test_framework.hpp"

#include "neko/utility.hpp"
#include "tracker.hpp"

using neko_test::Tracker;

namespace {

// Records which overload a forwarded argument actually reached. This is the
// only way to *observe* forwarding: the value is unchanged either way, so the
// question is always "which function got called?".
enum class Overload { none, lvalue, const_lvalue, rvalue };

Overload probe(int&) { return Overload::lvalue; }
Overload probe(const int&) { return Overload::const_lvalue; }
Overload probe(int&&) { return Overload::rvalue; }

// A perfect-forwarding wrapper -- the entire reason forward exists.
template <typename T>
Overload forwarding_wrapper(T&& arg) {
    return probe(neko::forward<T>(arg));
}

// The bug forward prevents: without it, `arg` is a named variable and so is
// always an lvalue, no matter what was passed in.
template <typename T>
Overload naive_wrapper(T&& arg) {
    return probe(arg);
}

}  // namespace

// ===========================================================================
// declval and as_const -- already implemented, here as worked examples
// ===========================================================================

NEKO_TEST(declval_produces_a_value_in_an_unevaluated_context) {
    // No object is ever created and no constructor is required: declval is
    // only ever *named*, never called.
    struct NoDefaultCtor {
        NoDefaultCtor() = delete;
        int f();
    };
    STATIC_CHECK(
        neko::is_same_v<decltype(neko::declval<NoDefaultCtor>().f()), int>);

    // It returns T&&, so for a non-reference T the expression is an xvalue.
    STATIC_CHECK(neko::is_same_v<decltype(neko::declval<int>()), int&&>);
    STATIC_CHECK(neko::is_same_v<decltype(neko::declval<int&>()), int&>);
}

NEKO_TEST(as_const_adds_const_without_copying) {
    int x = 7;
    STATIC_CHECK(neko::is_same_v<decltype(neko::as_const(x)), const int&>);
    CHECK_EQ(&neko::as_const(x), &x);  // same object, no copy
    CHECK_EQ(neko::as_const(x), 7);

    // The rvalue overload is deleted because the returned reference would
    // dangle immediately. There is no portable compile-time test for that:
    // naming a deleted function is ill-formed rather than a substitution
    // failure, so `requires { neko::as_const(5); }` is a hard error on GCC
    // rather than evaluating to false. Uncomment to watch it fail to compile.
    //
    //     neko::as_const(5);
}

// ===========================================================================
// move
// ===========================================================================

NEKO_TEST(move_has_the_right_return_type) {
    int x = 0;
    const int cx = 0;

    // T deduces to int&, and remove_reference_t<int&>&& is int&&.
    STATIC_CHECK(neko::is_same_v<decltype(neko::move(x)), int&&>);
    STATIC_CHECK(neko::is_same_v<decltype(neko::move(5)), int&&>);

    // move on a const lvalue gives const int&& -- which binds to a *copy*
    // constructor, not a move constructor. This is why `move` on a const
    // object silently copies, one of the classic performance traps.
    STATIC_CHECK(neko::is_same_v<decltype(neko::move(cx)), const int&&>);

    (void)x;
    (void)cx;
}

NEKO_TEST(move_does_not_modify_its_argument) {
    // move is a cast: on its own it moves nothing at all.
    int x = 42;
    int&& r = neko::move(x);
    CHECK_EQ(x, 42);
    CHECK_EQ(r, 42);
    CHECK_EQ(&r, &x);  // it is still the same object
}

NEKO_TEST(move_selects_the_move_constructor) {
    Tracker::reset();
    {
        Tracker a{5};
        Tracker b{neko::move(a)};
        CHECK_EQ(b.value, 5);
        CHECK(a.moved_from);
    }
    CHECK_EQ(Tracker::moves, 1);
    CHECK_EQ(Tracker::copies, 0);
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(move_on_a_const_object_falls_back_to_copying) {
    Tracker::reset();
    {
        const Tracker a{5};
        Tracker b{neko::move(a)};  // const Tracker&& -> binds to const&
        CHECK_EQ(b.value, 5);
    }
    CHECK_EQ(Tracker::moves, 0);
    CHECK_EQ(Tracker::copies, 1);
}

// ===========================================================================
// forward
// ===========================================================================

NEKO_TEST(forward_has_the_right_return_type) {
    int x = 0;

    // Called with T = int (a non-reference), it produces an rvalue...
    STATIC_CHECK(neko::is_same_v<decltype(neko::forward<int>(x)), int&&>);
    // ...and with T = int& it produces an lvalue. That collapsing is the
    // whole mechanism.
    STATIC_CHECK(neko::is_same_v<decltype(neko::forward<int&>(x)), int&>);
    STATIC_CHECK(neko::is_same_v<decltype(neko::forward<int&&>(x)), int&&>);

    (void)x;
}

NEKO_TEST(forward_preserves_value_category) {
    int x = 0;
    const int cx = 0;

    CHECK(forwarding_wrapper(x) == Overload::lvalue);
    CHECK(forwarding_wrapper(cx) == Overload::const_lvalue);
    CHECK(forwarding_wrapper(5) == Overload::rvalue);
    CHECK(forwarding_wrapper(neko::move(x)) == Overload::rvalue);
}

NEKO_TEST(without_forward_everything_arrives_as_an_lvalue) {
    // The contrast that motivates forward: a named rvalue reference is an
    // lvalue, so the rvalue overload is never reached.
    int x = 0;
    CHECK(naive_wrapper(x) == Overload::lvalue);
    CHECK(naive_wrapper(5) == Overload::lvalue);
}

NEKO_TEST(forward_does_not_copy_or_move_by_itself) {
    Tracker::reset();
    {
        Tracker a{1};
        [](auto&& t) { (void)neko::forward<decltype(t)>(t); }(neko::move(a));
    }
    CHECK_EQ(Tracker::copies, 0);
    CHECK_EQ(Tracker::moves, 0);
}

// ===========================================================================
// swap
// ===========================================================================

NEKO_TEST(swap_exchanges_two_values) {
    int a = 1, b = 2;
    neko::swap(a, b);
    CHECK_EQ(a, 2);
    CHECK_EQ(b, 1);
}

NEKO_TEST(swap_uses_moves_not_copies) {
    Tracker::reset();
    {
        Tracker a{1};
        Tracker b{2};
        neko::swap(a, b);
        CHECK_EQ(a.value, 2);
        CHECK_EQ(b.value, 1);

        // One temporary plus two move-assignments: three moves, no copies.
        CHECK_EQ(Tracker::moves, 3);
        CHECK_EQ(Tracker::copies, 0);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(swap_handles_self_swap) {
    int a = 1;
    neko::swap(a, a);
    CHECK_EQ(a, 1);
}

NEKO_TEST(swap_works_on_arrays_of_elements) {
    int a[3] = {1, 2, 3};
    int b[3] = {4, 5, 6};
    for (int i = 0; i < 3; ++i) neko::swap(a[i], b[i]);
    CHECK_EQ(a[0], 4);
    CHECK_EQ(b[2], 3);
}

// ===========================================================================
// exchange
// ===========================================================================

NEKO_TEST(exchange_returns_the_old_value) {
    int x = 1;
    const int old = neko::exchange(x, 2);
    CHECK_EQ(old, 1);
    CHECK_EQ(x, 2);
}

NEKO_TEST(exchange_accepts_a_different_type) {
    // U is a separate template parameter, so this must compile: the new value
    // only has to be assignable to T.
    long x = 1;
    const long old = neko::exchange(x, 2);  // int literal -> long
    CHECK_EQ(old, 1);
    CHECK_EQ(x, 2);
}

NEKO_TEST(exchange_is_the_backbone_of_a_move_constructor) {
    // The idiom vector's move constructor will use: take the pointer and
    // leave a null behind, in one expression.
    int storage = 99;
    int* p = &storage;
    int* taken = neko::exchange(p, nullptr);
    CHECK_EQ(taken, &storage);
    CHECK(p == nullptr);
}

NEKO_TEST(exchange_moves_rather_than_copies) {
    Tracker::reset();
    {
        Tracker a{1};
        Tracker b{2};
        Tracker old = neko::exchange(a, neko::move(b));
        CHECK_EQ(old.value, 1);
        CHECK_EQ(a.value, 2);
        CHECK_EQ(Tracker::copies, 0);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(exchange_does_not_steal_from_an_lvalue) {
    // new_value is a *forwarding* reference, so its value category is part of
    // what the caller said. Passing a named variable is a promise that the
    // caller still owns it afterwards -- exchange may read it, not gut it.
    Tracker::reset();
    {
        Tracker a{1};
        Tracker b{2};
        Tracker old = neko::exchange(a, b);  // b is an lvalue

        CHECK_EQ(old.value, 1);
        CHECK_EQ(a.value, 2);

        CHECK(!b.moved_from);  // b is still the caller's
        CHECK_EQ(b.value, 2);
        CHECK_EQ(Tracker::copies, 1);  // a <- b must be a *copy* assignment
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(exchange_accepts_a_const_lvalue) {
    // The same call with a const source. This one already works, but only by
    // accident of const&& binding to const& -- it is here so that whatever
    // makes the test above pass does not break it.
    Tracker::reset();
    {
        const Tracker b{2};
        Tracker a{1};
        Tracker old = neko::exchange(a, b);
        CHECK_EQ(old.value, 1);
        CHECK_EQ(a.value, 2);
        CHECK_EQ(b.value, 2);
        CHECK_EQ(Tracker::copies, 1);
    }
    CHECK_EQ(Tracker::alive, 0);
}

// ===========================================================================
// noexcept
//
// These are CHECKs rather than STATIC_CHECKs on purpose: a static_assert here
// would stop the file compiling and take every other test in it down with it.
// `noexcept(expr)` is still answered entirely at compile time either way.
// ===========================================================================

NEKO_TEST(move_and_forward_are_noexcept) {
    int x = 0;

    // A static_cast between reference types cannot throw, and the standard
    // says all three are noexcept.
    CHECK(noexcept(neko::move(x)));
    CHECK(noexcept(neko::forward<int>(x)));
    CHECK(noexcept(neko::forward<int&>(x)));

    CHECK(noexcept(neko::as_const(x)));  // the worked reference, for contrast

    (void)x;
}

NEKO_TEST(noexcept_of_move_propagates_into_a_move_construction) {
    // Why the above matters: this is the question a container asks before it
    // reallocates -- "can I move these elements without risking a throw?".
    // Tracker's move constructor is noexcept, so the answer must be yes.
    STATIC_CHECK(noexcept(Tracker(neko::declval<Tracker&&>())));
    CHECK(noexcept(Tracker(neko::move(neko::declval<Tracker&>()))));

    // ThrowingMove's is not, and must stay not -- the point is that move
    // reports what the type does, not a blanket answer.
    CHECK(!noexcept(neko_test::ThrowingMove(
        neko::move(neko::declval<neko_test::ThrowingMove&>()))));
}

// ===========================================================================
// constexpr
//
// Unlike noexcept, constexpr-ness cannot be tested softly: calling a
// non-constexpr function in a constant expression is a build error, not a
// failed CHECK. If either of these stops compiling, the keyword went missing.
// ===========================================================================

NEKO_TEST(swap_works_at_compile_time) {
    constexpr int first_after_swap = [] {
        int a = 1, b = 2;
        neko::swap(a, b);
        return a;
    }();
    STATIC_CHECK(first_after_swap == 2);
}

NEKO_TEST(exchange_works_at_compile_time) {
    constexpr int old = [] {
        int x = 1;
        return neko::exchange(x, 2);
    }();
    STATIC_CHECK(old == 1);
}
