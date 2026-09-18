// Tests for neko/deque.hpp.
//
// Grouped in the order of the YOUR TURN list in the header, so working down
// the file is a workable implementation order. Everything reports TODO until
// the matching member is written.
#include "test_framework.hpp"

#include "neko/deque.hpp"
#include "tracker.hpp"

namespace {

using neko_test::Tracker;

}  // namespace

// ---------------------------------------------------------------------------
// 1. Interface conventions and the block size
// ---------------------------------------------------------------------------

NEKO_TEST(deque_publishes_the_standard_member_types) {
    using D = neko::deque<int>;
    STATIC_CHECK(neko::is_same_v<D::value_type, int>);
    STATIC_CHECK(neko::is_same_v<D::size_type, std::size_t>);
    STATIC_CHECK(neko::is_same_v<D::difference_type, std::ptrdiff_t>);
    STATIC_CHECK(neko::is_same_v<D::reference, int&>);
    STATIC_CHECK(neko::is_same_v<D::const_reference, const int&>);
}

NEKO_TEST(deque_iterator_is_not_a_pointer) {
    // The property that separates deque from array and vector, and the reason
    // this module needs iterator_traits.
    using D = neko::deque<int>;
    STATIC_CHECK(!neko::is_same_v<D::iterator, int*>);
    STATIC_CHECK(!neko::is_same_v<D::const_iterator, const int*>);
    STATIC_CHECK(neko::is_same_v<D::iterator::reference, int&>);
    STATIC_CHECK(neko::is_same_v<D::const_iterator::reference, const int&>);
}

NEKO_TEST(iterator_converts_to_const_iterator_but_not_back) {
    // std:: here on purpose -- neko::is_constructible is still on the YOUR
    // TURN list in type_traits.hpp. Swap it over once you have written it.
    using D = neko::deque<int>;
    STATIC_CHECK(std::is_constructible_v<D::const_iterator, D::iterator>);
    STATIC_CHECK(!std::is_constructible_v<D::iterator, D::const_iterator>);
}

NEKO_TEST(block_size_is_at_least_one_element) {
    struct Huge {
        char pad[4096];
    };
    STATIC_CHECK(neko::deque_block_size<int>() > 1);
    STATIC_CHECK(neko::deque_block_size<Huge>() == 1);
}

// ---------------------------------------------------------------------------
// 2. Construction, begin/end, size
// ---------------------------------------------------------------------------

NEKO_TEST(a_default_constructed_deque_is_empty) {
    neko::deque<int> d;
    CHECK(d.empty());
    CHECK_EQ(d.size(), 0u);
    CHECK(d.begin() == d.end());
}

NEKO_TEST(sized_construction_value_initialises) {
    neko::deque<int> d(3);
    CHECK_EQ(d.size(), 3u);
    CHECK_EQ(d[0], 0);
    CHECK_EQ(d[2], 0);
}

NEKO_TEST(fill_construction_copies_the_value) {
    neko::deque<int> d(4, 7);
    CHECK_EQ(d.size(), 4u);
    CHECK_EQ(d[0], 7);
    CHECK_EQ(d[3], 7);
}

// ---------------------------------------------------------------------------
// 3. push_back / push_front
// ---------------------------------------------------------------------------

NEKO_TEST(push_back_appends) {
    neko::deque<int> d;
    d.push_back(1);
    d.push_back(2);
    CHECK_EQ(d.size(), 2u);
    CHECK_EQ(d.front(), 1);
    CHECK_EQ(d.back(), 2);
}

NEKO_TEST(push_front_prepends) {
    neko::deque<int> d;
    d.push_front(1);
    d.push_front(2);
    CHECK_EQ(d.size(), 2u);
    CHECK_EQ(d.front(), 2);
    CHECK_EQ(d.back(), 1);
}

NEKO_TEST(the_two_ends_interleave) {
    neko::deque<int> d;
    d.push_back(1);
    d.push_front(0);
    d.push_back(2);
    d.push_front(-1);
    CHECK_EQ(d.size(), 4u);
    CHECK_EQ(d[0], -1);
    CHECK_EQ(d[1], 0);
    CHECK_EQ(d[2], 1);
    CHECK_EQ(d[3], 2);
}

NEKO_TEST(pop_front_and_pop_back_remove_one_element) {
    neko::deque<int> d;
    for (int i = 0; i < 4; ++i) d.push_back(i);
    d.pop_front();
    d.pop_back();
    CHECK_EQ(d.size(), 2u);
    CHECK_EQ(d.front(), 1);
    CHECK_EQ(d.back(), 2);
}

NEKO_TEST(emplace_back_constructs_in_place) {
    Tracker::reset();
    neko::deque<Tracker> d;
    d.emplace_back(7);
    CHECK_EQ(Tracker::copies, 0);
    CHECK_EQ(Tracker::moves, 0);
    CHECK_EQ(d.back().value, 7);
}

// ---------------------------------------------------------------------------
// 4. Growth across blocks -- the part that exercises the map
// ---------------------------------------------------------------------------

NEKO_TEST(push_back_crosses_block_boundaries) {
    // Several blocks' worth, so the map itself has to grow.
    const int n = static_cast<int>(neko::deque_block_size<int>()) * 3 + 5;
    neko::deque<int> d;
    for (int i = 0; i < n; ++i) d.push_back(i);
    REQUIRE_EQ(d.size(), static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) REQUIRE_EQ(d[static_cast<std::size_t>(i)], i);
}

NEKO_TEST(push_front_crosses_block_boundaries) {
    const int n = static_cast<int>(neko::deque_block_size<int>()) * 3 + 5;
    neko::deque<int> d;
    for (int i = 0; i < n; ++i) d.push_front(i);
    REQUIRE_EQ(d.size(), static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        REQUIRE_EQ(d[static_cast<std::size_t>(i)], n - 1 - i);
}

NEKO_TEST(iterating_visits_every_element_in_order) {
    const int n = static_cast<int>(neko::deque_block_size<int>()) * 2 + 3;
    neko::deque<int> d;
    for (int i = 0; i < n; ++i) d.push_back(i);

    int expected = 0;
    for (neko::deque<int>::iterator it = d.begin(); it != d.end(); ++it)
        REQUIRE_EQ(*it, expected++);
    CHECK_EQ(expected, n);
}

NEKO_TEST(iterating_backwards_visits_every_element) {
    const int n = static_cast<int>(neko::deque_block_size<int>()) * 2 + 3;
    neko::deque<int> d;
    for (int i = 0; i < n; ++i) d.push_back(i);

    neko::deque<int>::iterator it = d.end();
    int expected = n;
    while (it != d.begin()) {
        --it;
        REQUIRE_EQ(*it, --expected);
    }
    CHECK_EQ(expected, 0);
}

NEKO_TEST(iterator_difference_is_the_element_count) {
    const int n = static_cast<int>(neko::deque_block_size<int>()) * 2 + 3;
    neko::deque<int> d;
    for (int i = 0; i < n; ++i) d.push_back(i);
    CHECK_EQ(d.end() - d.begin(), static_cast<std::ptrdiff_t>(n));
}

NEKO_TEST(iterator_jumps_by_more_than_one_block_at_a_time) {
    const int bs = static_cast<int>(neko::deque_block_size<int>());
    const int n = bs * 3 + 5;
    neko::deque<int> d;
    for (int i = 0; i < n; ++i) d.push_back(i);

    neko::deque<int>::iterator it = d.begin() + (bs * 2 + 1);
    CHECK_EQ(*it, bs * 2 + 1);
    it -= bs + 1;  // backwards across a boundary, with a negative remainder
    CHECK_EQ(*it, bs);
}

// ---------------------------------------------------------------------------
// 5. The invalidation guarantee that makes deque worth having
// ---------------------------------------------------------------------------

NEKO_TEST(growth_does_not_move_existing_elements) {
    // vector cannot promise this: reallocation moves everything. deque grows
    // the map, never the blocks, so the address of an element is stable.
    neko::deque<int> d;
    d.push_back(1);
    const int* addr = &d.front();

    const int n = static_cast<int>(neko::deque_block_size<int>()) * 3;
    for (int i = 0; i < n; ++i) d.push_back(i);
    CHECK_EQ(&d.front(), addr);

    for (int i = 0; i < n; ++i) d.push_front(i);
    CHECK_EQ(&d[static_cast<std::size_t>(n)], addr);
}

NEKO_TEST(growth_does_not_copy_or_move_existing_elements) {
    Tracker::reset();
    neko::deque<Tracker> d;
    const int n = static_cast<int>(neko::deque_block_size<Tracker>()) * 3;
    for (int i = 0; i < n; ++i) d.emplace_back(i);
    CHECK_EQ(Tracker::copies, 0);
    CHECK_EQ(Tracker::moves, 0);
}

// ---------------------------------------------------------------------------
// 6. Access, copy, move, swap, lifetime
// ---------------------------------------------------------------------------

NEKO_TEST(at_is_bounds_checked_and_subscript_is_not) {
    neko::deque<int> d(3, 1);
    CHECK_EQ(d.at(2), 1);
    CHECK_THROWS_AS(d.at(3), std::out_of_range);
}

NEKO_TEST(const_access_yields_const_references) {
    neko::deque<int> d(2, 5);
    const neko::deque<int>& c = d;
    STATIC_CHECK(neko::is_same_v<decltype(c[0]), const int&>);
    STATIC_CHECK(neko::is_same_v<decltype(c.front()), const int&>);
    CHECK_EQ(c[1], 5);
}

NEKO_TEST(copying_produces_an_independent_deque) {
    neko::deque<int> a;
    for (int i = 0; i < 5; ++i) a.push_back(i);
    neko::deque<int> b = a;
    b[0] = 99;
    CHECK_EQ(a[0], 0);
    CHECK_EQ(b[0], 99);
    CHECK_EQ(b.size(), 5u);
}

NEKO_TEST(moving_leaves_the_source_empty_and_usable) {
    neko::deque<int> a;
    for (int i = 0; i < 5; ++i) a.push_back(i);
    neko::deque<int> b = neko::move(a);
    CHECK_EQ(b.size(), 5u);
    CHECK(a.empty());
    a.push_back(1);  // moved-from, but still a valid deque
    CHECK_EQ(a.size(), 1u);
}

NEKO_TEST(swap_exchanges_contents_without_touching_elements) {
    Tracker::reset();
    neko::deque<Tracker> a;
    neko::deque<Tracker> b;
    a.emplace_back(1);
    b.emplace_back(2);
    const int moves_before = Tracker::moves;
    const int copies_before = Tracker::copies;

    a.swap(b);

    CHECK_EQ(Tracker::moves, moves_before);
    CHECK_EQ(Tracker::copies, copies_before);
    CHECK_EQ(a.front().value, 2);
    CHECK_EQ(b.front().value, 1);
}

NEKO_TEST(every_element_is_destroyed_exactly_once) {
    Tracker::reset();
    {
        neko::deque<Tracker> d;
        const int n = static_cast<int>(neko::deque_block_size<Tracker>()) * 2;
        for (int i = 0; i < n; ++i) d.emplace_back(i);
        d.pop_front();
        d.pop_back();
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(clear_destroys_the_elements_and_leaves_a_usable_deque) {
    Tracker::reset();
    neko::deque<Tracker> d;
    for (int i = 0; i < 10; ++i) d.emplace_back(i);
    d.clear();
    CHECK_EQ(Tracker::alive, 0);
    CHECK(d.empty());
    d.emplace_back(1);
    CHECK_EQ(d.size(), 1u);
}
