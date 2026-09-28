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

// A deque's storage, assembled by hand: three blocks, and a map pointing at
// them. This is everything the iterator needs -- it never asks the container
// for anything -- so the tests in section 1 below can run before
// initialise_map, push_back or begin() exist.
template <typename T>
struct ManualMap {
    static constexpr std::size_t block_size = neko::deque_block_size<T>();
    static constexpr std::size_t blocks = 3;

    T storage[blocks][block_size];
    T* map[blocks];

    ManualMap() {
        for (std::size_t b = 0; b < blocks; ++b) map[b] = storage[b];
    }

    // An iterator onto element `off` of block `b`: set_node picks the block,
    // then cur walks to the offset. That is exactly how begin() and end() will
    // position one once they exist.
    neko::deque_iterator<T, T&, T*> iter(std::size_t b, std::size_t off = 0) {
        neko::deque_iterator<T, T&, T*> it;
        it.set_node(&map[b]);
        it.cur = it.first + off;
        return it;
    }
};

// Number every slot with its position in the whole sequence, so an element's
// value alone says which block it is in and where.
void number(ManualMap<int>& m) {
    for (std::size_t b = 0; b < m.blocks; ++b)
        for (std::size_t i = 0; i < m.block_size; ++i)
            m.storage[b][i] = static_cast<int>(b * m.block_size + i);
}

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
// 1b. The iterator, built by hand
//
// No container anywhere below: every iterator is positioned directly on a map
// the test owns. These are the first tests in the file that can go green, and
// they need only set_node, operator*, operator->, ++ and --.
//
// One rule for the hand-built map: never ++ past the last element of the last
// block. A real deque always has a slot for end() to denote, but this map is
// exactly full, so going one further would call set_node on a map entry that
// does not exist.
// ---------------------------------------------------------------------------

NEKO_TEST(set_node_adopts_the_whole_block) {
    ManualMap<int> m;
    neko::deque_iterator<int, int&, int*> it;
    it.set_node(&m.map[1]);

    CHECK_EQ(it.node, &m.map[1]);
    CHECK_EQ(it.first, m.map[1]);
    CHECK_EQ(it.last, m.map[1] + m.block_size);    // one PAST the block
    CHECK_EQ(it.cur, static_cast<int*>(nullptr));  // cur is the caller's job
}

NEKO_TEST(dereference_yields_the_element_at_cur) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(2, 5);
    CHECK_EQ(*it, static_cast<int>(2 * m.block_size + 5));
}

NEKO_TEST(arrow_reaches_a_member_of_the_element) {
    ManualMap<Tracker> m;
    m.storage[0][3].value = 42;

    auto it = m.iter(0, 3);
    CHECK_EQ(it->value, 42);
    CHECK_EQ((*it).value, 42);  // the equivalence -> is required to provide
}

NEKO_TEST(increment_walks_forward_inside_a_block) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(0, 0);
    ++it;
    CHECK_EQ(*it, 1);
    CHECK_EQ(it.node, &m.map[0]);  // no hop: same block
}

NEKO_TEST(increment_hops_to_the_next_block_at_the_boundary) {
    ManualMap<int> m;
    number(m);

    // The last element of block 0. One ++ has to cross into block 1, which is
    // somewhere else in memory entirely -- the case a bare T* cannot handle,
    // and the reason this iterator exists at all.
    auto it = m.iter(0, m.block_size - 1);
    REQUIRE_EQ(*it, static_cast<int>(m.block_size - 1));

    ++it;
    CHECK_EQ(it.node, &m.map[1]);
    CHECK_EQ(it.cur, m.map[1]);
    CHECK_EQ(*it, static_cast<int>(m.block_size));
}

NEKO_TEST(decrement_hops_to_the_previous_block_at_the_boundary) {
    ManualMap<int> m;
    number(m);

    // Mirror image: from the first element of block 1, one -- lands on the
    // LAST element of block 0, not the first.
    auto it = m.iter(1, 0);
    --it;
    CHECK_EQ(it.node, &m.map[0]);
    CHECK_EQ(it.cur, m.map[0] + m.block_size - 1);
    CHECK_EQ(*it, static_cast<int>(m.block_size - 1));
}

NEKO_TEST(increment_and_decrement_undo_each_other_across_a_boundary) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(0, m.block_size - 1);
    auto before = it;
    ++it;
    --it;

    // node as well as cur: operator== only compares cur, so an iterator that
    // hopped forward and left node pointing at the new block would still
    // compare equal here -- and then break on the very next ++.
    CHECK_EQ(it.cur, before.cur);
    CHECK_EQ(it.node, before.node);
    CHECK_EQ(it.first, before.first);
    CHECK_EQ(it.last, before.last);
}

NEKO_TEST(postfix_increment_returns_the_position_it_left) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(0, m.block_size - 1);
    auto old = it++;

    CHECK_EQ(*old, static_cast<int>(m.block_size - 1));  // where it was
    CHECK_EQ(*it, static_cast<int>(m.block_size));       // where it is now
    CHECK_EQ(old.node, &m.map[0]);  // the copy kept the old block too
}

NEKO_TEST(postfix_decrement_returns_the_position_it_left) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(1, 0);
    auto old = it--;

    CHECK_EQ(*old, static_cast<int>(m.block_size));
    CHECK_EQ(*it, static_cast<int>(m.block_size - 1));
    CHECK_EQ(old.node, &m.map[1]);
}

NEKO_TEST(incrementing_visits_every_element_of_every_block_in_order) {
    ManualMap<int> m;
    number(m);
    const int n = static_cast<int>(m.blocks * m.block_size);

    auto it = m.iter(0, 0);
    for (int i = 0; i < n - 1; ++i) {  // stop ON the last one, see the note
        REQUIRE_EQ(*it, i);
        ++it;
    }
    CHECK_EQ(*it, n - 1);
    CHECK_EQ(it.node, &m.map[m.blocks - 1]);
}

NEKO_TEST(decrementing_visits_every_element_of_every_block_in_reverse) {
    ManualMap<int> m;
    number(m);
    const int n = static_cast<int>(m.blocks * m.block_size);

    auto it = m.iter(m.blocks - 1, m.block_size - 1);
    for (int i = n - 1; i > 0; --i) {
        REQUIRE_EQ(*it, i);
        --it;
    }
    CHECK_EQ(*it, 0);
    CHECK_EQ(it.node, &m.map[0]);
}

NEKO_TEST(a_hand_built_iterator_converts_to_a_const_iterator) {
    ManualMap<int> m;
    number(m);

    // The runtime half of iterator_converts_to_const_iterator_but_not_back,
    // which only checks the conversion exists.
    neko::deque<int>::const_iterator cit = m.iter(1, 2);
    STATIC_CHECK(neko::is_same_v<decltype(*cit), const int&>);
    CHECK_EQ(*cit, static_cast<int>(m.block_size + 2));
}

// ---------------------------------------------------------------------------
// 1c. Iterator arithmetic, still on the hand-built map
//
// ++ and -- only ever cross one boundary, and they find it by comparing a
// pointer against `last`. operator+= cannot work that way: n may be any
// distance in either direction, so it has to *compute* which block it lands
// in and re-bind first/last to that block. Every test below is a case where
// that computation can go wrong while stepping stays right.
//
// Section 1b's rule still holds -- the map is exactly full, so nothing here
// may land past the last element of block 2 or before the first of block 0.
// ---------------------------------------------------------------------------

NEKO_TEST(a_default_constructed_iterator_holds_no_position) {
    neko::deque<int>::iterator it;
    CHECK_EQ(it.cur, static_cast<int*>(nullptr));
    CHECK_EQ(it.node, static_cast<int**>(nullptr));
    CHECK(it == neko::deque<int>::iterator{});
}

NEKO_TEST(plus_equals_stays_put_for_a_jump_of_zero) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(1, 4);
    it += 0;
    CHECK_EQ(it.node, &m.map[1]);
    CHECK_EQ(*it, static_cast<int>(m.block_size + 4));
}

NEKO_TEST(plus_equals_moves_within_a_block_without_touching_the_node) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(1, 2);
    it += 3;
    CHECK_EQ(it.node, &m.map[1]);  // the cheap path: no map lookup at all
    CHECK_EQ(it.cur, m.map[1] + 5);
    CHECK_EQ(*it, static_cast<int>(m.block_size + 5));
}

NEKO_TEST(plus_equals_of_exactly_one_block_lands_on_the_next_block) {
    ManualMap<int> m;
    number(m);

    // The boundary: offset == block_size is the smallest offset that is NOT
    // in this block, so it has to hop -- and land on slot 0 of the block
    // after, not slot block_size of anything.
    auto it = m.iter(0, 0);
    it += static_cast<std::ptrdiff_t>(m.block_size);
    CHECK_EQ(it.node, &m.map[1]);
    CHECK_EQ(it.cur, m.map[1]);
    CHECK_EQ(*it, static_cast<int>(m.block_size));
}

NEKO_TEST(plus_equals_crosses_more_than_one_block_in_one_step) {
    ManualMap<int> m;
    number(m);

    // Two whole blocks in a single O(1) operation -- the thing ++ in a loop
    // cannot do, and the reason += exists rather than being sugar for it.
    auto it = m.iter(0, 3);
    it += static_cast<std::ptrdiff_t>(2 * m.block_size);
    CHECK_EQ(it.node, &m.map[2]);
    CHECK_EQ(it.cur, m.map[2] + 3);
    CHECK_EQ(*it, static_cast<int>(2 * m.block_size + 3));
}

NEKO_TEST(plus_equals_accepts_a_negative_distance) {
    ManualMap<int> m;
    number(m);

    // += and -= are one operation with a sign, and the sign is precisely
    // where truncating integer division goes the wrong way.
    auto it = m.iter(2, 5);
    it += -static_cast<std::ptrdiff_t>(m.block_size + 5);
    CHECK_EQ(it.node, &m.map[1]);
    CHECK_EQ(it.cur, m.map[1]);
    CHECK_EQ(*it, static_cast<int>(m.block_size));
}

NEKO_TEST(minus_equals_crosses_backwards_with_a_remainder) {
    ManualMap<int> m;
    number(m);

    // Lands part-way into an earlier block, so the offset is negative and not
    // a multiple of block_size. -1 / block_size truncates towards zero and
    // gives 0, which leaves the iterator a whole block too high unless the
    // division is corrected for the sign.
    auto it = m.iter(2, 1);
    it -= static_cast<std::ptrdiff_t>(m.block_size + 3);
    CHECK_EQ(it.node, &m.map[0]);
    CHECK_EQ(it.cur, m.map[0] + m.block_size - 2);
    CHECK_EQ(*it, static_cast<int>(m.block_size - 2));
}

NEKO_TEST(minus_equals_of_one_matches_a_single_decrement) {
    ManualMap<int> m;
    number(m);

    // The likeliest off-by-one there is: -= 1 from the first slot of a block
    // must land on the LAST slot of the block before, exactly where -- goes.
    auto by_jump = m.iter(1, 0);
    by_jump -= 1;
    auto by_step = m.iter(1, 0);
    --by_step;

    CHECK_EQ(by_jump.cur, by_step.cur);
    CHECK_EQ(by_jump.node, by_step.node);
    CHECK_EQ(*by_jump, static_cast<int>(m.block_size - 1));
}

NEKO_TEST(plus_and_minus_leave_the_original_iterator_alone) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(1, 0);
    auto ahead = it + static_cast<std::ptrdiff_t>(m.block_size);
    auto behind = it - 1;

    CHECK_EQ(it.cur, m.map[1]);  // unmoved: these return a copy
    CHECK_EQ(it.node, &m.map[1]);
    CHECK_EQ(*ahead, static_cast<int>(2 * m.block_size));
    CHECK_EQ(*behind, static_cast<int>(m.block_size - 1));
}

NEKO_TEST(jumping_forward_agrees_with_stepping_for_every_distance) {
    ManualMap<int> m;
    number(m);
    const std::ptrdiff_t n =
        static_cast<std::ptrdiff_t>(m.blocks * m.block_size);

    // The exhaustive version: from the front, `it + k` must be the same
    // iterator as k increments, for every k the map can hold. One loop
    // covers every combination of whole blocks and remainder.
    const auto base = m.iter(0, 0);
    auto stepped = base;
    for (std::ptrdiff_t k = 0; k < n; ++k) {
        const auto jumped = base + k;
        REQUIRE_EQ(jumped.cur, stepped.cur);
        REQUIRE_EQ(jumped.node, stepped.node);
        REQUIRE_EQ(jumped.first, stepped.first);  // a hop that lands right but
        REQUIRE_EQ(jumped.last, stepped.last);    // forgets to re-bind is a
        if (k + 1 < n) ++stepped;                 // bug the next ++ pays for
    }
}

NEKO_TEST(jumping_backward_agrees_with_stepping_for_every_distance) {
    ManualMap<int> m;
    number(m);
    const std::ptrdiff_t n =
        static_cast<std::ptrdiff_t>(m.blocks * m.block_size);

    const auto base = m.iter(m.blocks - 1, m.block_size - 1);
    auto stepped = base;
    for (std::ptrdiff_t k = 0; k < n; ++k) {
        const auto jumped = base - k;
        REQUIRE_EQ(jumped.cur, stepped.cur);
        REQUIRE_EQ(jumped.node, stepped.node);
        REQUIRE_EQ(jumped.first, stepped.first);
        REQUIRE_EQ(jumped.last, stepped.last);
        if (k + 1 < n) --stepped;
    }
}

NEKO_TEST(an_iterator_reached_by_jumping_can_still_be_stepped) {
    ManualMap<int> m;
    number(m);

    // A jump onto the last slot of a block. If += wrote cur without going
    // through set_node, ++ here compares cur against the *old* block's `last`,
    // never matches, and walks off the end of block 1 instead of hopping.
    auto it = m.iter(0, 0);
    it += static_cast<std::ptrdiff_t>(2 * m.block_size - 1);
    REQUIRE_EQ(*it, static_cast<int>(2 * m.block_size - 1));

    ++it;
    CHECK_EQ(it.node, &m.map[2]);
    CHECK_EQ(it.cur, m.map[2]);
    CHECK_EQ(*it, static_cast<int>(2 * m.block_size));
}

NEKO_TEST(subscript_reads_an_element_without_moving_the_iterator) {
    ManualMap<int> m;
    number(m);

    auto it = m.iter(1, 0);
    CHECK_EQ(it[0], static_cast<int>(m.block_size));
    CHECK_EQ(it[3], static_cast<int>(m.block_size + 3));
    CHECK_EQ(it[-1], static_cast<int>(m.block_size - 1));  // back a block
    CHECK_EQ(it[static_cast<std::ptrdiff_t>(m.block_size)],
             static_cast<int>(2 * m.block_size));  // forward a whole block
    CHECK_EQ(it.cur, m.map[1]);                    // and still where it was
}

NEKO_TEST(subscript_yields_a_reference_that_writes_through) {
    ManualMap<int> m;
    number(m);

    // A reference to the element, not a copy of it and not the pointer to it:
    // assigning through the subscript has to reach into the block.
    auto it = m.iter(0, 0);
    STATIC_CHECK(neko::is_same_v<decltype(it[0]), int&>);
    it[static_cast<std::ptrdiff_t>(m.block_size) + 2] = 99;
    CHECK_EQ(m.storage[1][2], 99);
}

NEKO_TEST(equality_compares_position_not_the_route_taken) {
    ManualMap<int> m;
    number(m);

    auto stepped = m.iter(0, m.block_size - 1);
    ++stepped;                   // arrived at block 1 by hopping
    auto placed = m.iter(1, 0);  // put there directly

    CHECK(stepped == placed);
    CHECK(!(stepped != placed));

    ++placed;
    CHECK(stepped != placed);
    CHECK(!(stepped == placed));
}

NEKO_TEST(ordering_follows_the_sequence_across_blocks) {
    ManualMap<int> m;
    number(m);

    // Within a block the elements are adjacent, so cur alone orders them.
    // Across blocks they need not be -- block 1 may sit below block 0 in
    // memory -- so the node is what decides, and cur is only the tie-break.
    const auto early = m.iter(0, 3);
    const auto late_same_block = m.iter(0, 7);
    const auto next_block = m.iter(1, 0);

    CHECK(early < late_same_block);
    CHECK(!(late_same_block < early));
    CHECK(late_same_block < next_block);
    CHECK(!(next_block < late_same_block));
    CHECK(!(early < early));  // irreflexive
}

NEKO_TEST(a_const_iterator_does_the_same_arithmetic) {
    ManualMap<int> m;
    number(m);

    // One class body serves both, so the arithmetic is shared code -- but the
    // Ref/Ptr substitution and the converting constructor are not, and those
    // are the whole of what const_iterator adds.
    using CIt = neko::deque<int>::const_iterator;
    CIt cit = m.iter(0, 1);

    cit += static_cast<std::ptrdiff_t>(m.block_size);
    CHECK_EQ(*cit, static_cast<int>(m.block_size + 1));

    STATIC_CHECK(neko::is_same_v<decltype(cit[0]), const int&>);
    CHECK_EQ(cit[1], static_cast<int>(m.block_size + 2));

    cit -= 2;
    CHECK_EQ(*cit, static_cast<int>(m.block_size - 1));
    CHECK(cit < CIt(m.iter(1, 0)));
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
