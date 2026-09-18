// Tests for neko/array.hpp -- the worked reference container.
//
// These pass out of the box (array.hpp is fully implemented). Read them as the
// shape every other container's tests will take: member types, the const and
// non-const access pairs, the half-open iterator range, checked vs unchecked
// access, and comparisons.
//
// The one exception is array::swap, which calls neko::swap -- it reports TODO
// until you finish utility.hpp.
#include "test_framework.hpp"

#include "neko/array.hpp"
#include "tracker.hpp"

namespace {

// Note the doubled braces. `array` has a single member (a C array), so the
// outer pair initialises the aggregate and the inner pair the member. A single
// pair works too via brace elision, but doubling it keeps -Wmissing-braces
// quiet and shows the real structure.
constexpr neko::array<int, 5> make_ints() { return {{1, 2, 3, 4, 5}}; }

}  // namespace

// ---------------------------------------------------------------------------
// Interface conventions
// ---------------------------------------------------------------------------

NEKO_TEST(array_publishes_the_standard_member_types) {
    using A = neko::array<int, 3>;
    STATIC_CHECK(neko::is_same_v<A::value_type, int>);
    STATIC_CHECK(neko::is_same_v<A::size_type, std::size_t>);
    STATIC_CHECK(neko::is_same_v<A::difference_type, std::ptrdiff_t>);
    STATIC_CHECK(neko::is_same_v<A::reference, int&>);
    STATIC_CHECK(neko::is_same_v<A::const_reference, const int&>);
    STATIC_CHECK(neko::is_same_v<A::pointer, int*>);
    STATIC_CHECK(neko::is_same_v<A::iterator, int*>);
    STATIC_CHECK(neko::is_same_v<A::const_iterator, const int*>);
}

NEKO_TEST(array_is_an_aggregate) {
    // No user-declared constructors, so aggregate initialisation applies and
    // the type stays trivially copyable and usable at compile time.
    STATIC_CHECK(std::is_aggregate_v<neko::array<int, 3>>);
    STATIC_CHECK(std::is_trivially_copyable_v<neko::array<int, 3>>);

    neko::array<int, 3> a{{1, 2, 3}};
    CHECK_EQ(a[0], 1);
    CHECK_EQ(a[2], 3);

    // Missing initialisers are value-initialised, not left indeterminate.
    neko::array<int, 3> partial{{7}};
    CHECK_EQ(partial[0], 7);
    CHECK_EQ(partial[1], 0);
    CHECK_EQ(partial[2], 0);
}

NEKO_TEST(array_is_usable_at_compile_time) {
    constexpr neko::array<int, 5> a = make_ints();
    STATIC_CHECK(a.size() == 5);
    STATIC_CHECK(a[0] == 1);
    STATIC_CHECK(a.front() == 1);
    STATIC_CHECK(a.back() == 5);
    STATIC_CHECK(!a.empty());
}

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------

NEKO_TEST(array_capacity_is_fixed_and_known_from_the_type) {
    neko::array<int, 3> a{{1, 2, 3}};
    CHECK_EQ(a.size(), 3u);
    CHECK_EQ(a.max_size(), 3u);
    CHECK(!a.empty());

    // Size is a property of the type, so it costs nothing at runtime.
    STATIC_CHECK(sizeof(neko::array<int, 4>) == 4 * sizeof(int));
}

// ---------------------------------------------------------------------------
// Element access
// ---------------------------------------------------------------------------

NEKO_TEST(array_subscript_returns_a_reference) {
    neko::array<int, 3> a{{1, 2, 3}};
    a[1] = 20;
    CHECK_EQ(a[1], 20);

    // The const overload yields a const reference, so this must not compile
    // as an assignment target.
    const neko::array<int, 3>& c = a;
    STATIC_CHECK(neko::is_same_v<decltype(c[0]), const int&>);
    CHECK_EQ(c[1], 20);
}

NEKO_TEST(array_at_is_the_checked_accessor) {
    neko::array<int, 3> a{{1, 2, 3}};
    CHECK_EQ(a.at(0), 1);
    a.at(0) = 10;
    CHECK_EQ(a.at(0), 10);

    CHECK_THROWS_AS(a.at(3), std::out_of_range);
    CHECK_THROWS_AS(a.at(99), std::out_of_range);
    CHECK_THROWS_AS(neko::as_const(a).at(3), std::out_of_range);

    // a[3] is *not* checked -- it is undefined behaviour, and deliberately so.
    // That is the fast-by-default / safe-on-request split.
}

NEKO_TEST(array_front_back_and_data) {
    neko::array<int, 3> a{{1, 2, 3}};
    CHECK_EQ(a.front(), 1);
    CHECK_EQ(a.back(), 3);
    CHECK_EQ(a.data(), &a[0]);
    CHECK_EQ(a.data()[2], 3);

    a.front() = 100;
    a.back() = 300;
    CHECK_EQ(a[0], 100);
    CHECK_EQ(a[2], 300);

    // Storage is contiguous: that is what makes data() meaningful.
    CHECK_EQ(a.data() + 1, &a[1]);
}

// ---------------------------------------------------------------------------
// Iterators
// ---------------------------------------------------------------------------

NEKO_TEST(array_iterators_form_a_half_open_range) {
    neko::array<int, 4> a{{1, 2, 3, 4}};

    CHECK_EQ(a.end() - a.begin(), 4);
    CHECK_EQ(a.begin(), a.data());
    CHECK_EQ(*a.begin(), 1);
    CHECK_EQ(*(a.end() - 1), 4);  // end() itself is never dereferenced

    int sum = 0;
    for (neko::array<int, 4>::iterator it = a.begin(); it != a.end(); ++it)
        sum += *it;
    CHECK_EQ(sum, 10);

    // begin()/end() are all a range-based for loop needs.
    int product = 1;
    for (int v : a) product *= v;
    CHECK_EQ(product, 24);
}

NEKO_TEST(array_const_iterators) {
    const neko::array<int, 3> a{{1, 2, 3}};
    STATIC_CHECK(neko::is_same_v<decltype(a.begin()), const int*>);
    STATIC_CHECK(neko::is_same_v<decltype(a.cbegin()), const int*>);

    int sum = 0;
    for (int v : a) sum += v;
    CHECK_EQ(sum, 6);

    // cbegin/cend give const iterators even from a non-const array.
    neko::array<int, 3> b{{1, 2, 3}};
    STATIC_CHECK(neko::is_same_v<decltype(b.cbegin()), const int*>);
    CHECK_EQ(b.cend() - b.cbegin(), 3);
}

// No test for array<T, 0> here, deliberately: the header declares `T elems_[N]`
// and a zero-length array is not legal ISO C++. std::array handles it with a
// partial specialisation for N == 0 whose begin() == end() and whose size() is
// 0. Adding that specialisation is a good small exercise -- write it, then add
// the test.

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------

NEKO_TEST(array_fill_assigns_every_element) {
    neko::array<int, 4> a{{1, 2, 3, 4}};
    a.fill(9);
    for (int v : a) CHECK_EQ(v, 9);
}

NEKO_TEST(array_swap_is_elementwise) {
    // Unlike vector::swap, this one is O(N) and moves the elements themselves:
    // an array owns its storage inline, so there are no pointers to exchange.
    // Needs neko::swap -- reports TODO until utility.hpp is done.
    neko::array<int, 3> a{{1, 2, 3}};
    neko::array<int, 3> b{{4, 5, 6}};

    int* const a_data = a.data();
    a.swap(b);

    CHECK_EQ(a[0], 4);
    CHECK_EQ(b[0], 1);
    CHECK_EQ(a[2], 6);
    CHECK_EQ(b[2], 3);

    // The addresses did not move -- only the values did.
    CHECK_EQ(a.data(), a_data);
}

// ---------------------------------------------------------------------------
// Comparisons
// ---------------------------------------------------------------------------

NEKO_TEST(array_equality_compares_elementwise) {
    neko::array<int, 3> a{{1, 2, 3}};
    neko::array<int, 3> b{{1, 2, 3}};
    neko::array<int, 3> c{{1, 2, 4}};

    CHECK(a == b);
    CHECK(!(a == c));
    CHECK(a != c);
    CHECK(!(a != b));

    // Different N is a different type, so `a == array<int,4>{}` would not
    // even compile. Size mismatches are caught at compile time.
}

NEKO_TEST(array_less_than_is_lexicographical) {
    neko::array<int, 3> a{{1, 2, 3}};
    neko::array<int, 3> b{{1, 2, 4}};
    neko::array<int, 3> c{{2, 0, 0}};

    CHECK(a < b);  // decided at the last element
    CHECK(a < c);  // decided at the first
    CHECK(!(b < a));
    CHECK(!(a < a));            // irreflexive
    CHECK((a < b) && (b < c));  // transitive
}

NEKO_TEST(array_holds_non_trivial_types_correctly) {
    using neko_test::Tracker;
    Tracker::reset();
    {
        neko::array<Tracker, 3> a{};
        CHECK_EQ(Tracker::ctors, 3);
        CHECK_EQ(Tracker::alive, 3);

        a[0].value = 1;
        CHECK_EQ(a[0].value, 1);
    }
    // Every element destroyed exactly once when the array went out of scope.
    CHECK_EQ(Tracker::alive, 0);
    CHECK_EQ(Tracker::dtors, 3);
}
