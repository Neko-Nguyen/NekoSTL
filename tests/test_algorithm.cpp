// Tests for neko/algorithm.hpp.
//
// Every test here reports TODO until the matching function is implemented --
// NEKO_TODO() throws, and the runner counts that as a work item rather than a
// failure. Work down the file in order; each section matches a section of the
// header.
#include "test_framework.hpp"

#include "neko/algorithm.hpp"
#include "neko/array.hpp"
#include "tracker.hpp"

namespace {

using neko_test::Tracker;

}  // namespace

// ---------------------------------------------------------------------------
// Comparing ranges
// ---------------------------------------------------------------------------

NEKO_TEST(equal_compares_element_wise) {
    const neko::array<int, 4> a{{1, 2, 3, 4}};
    const neko::array<int, 4> b{{1, 2, 3, 4}};
    const neko::array<int, 4> c{{1, 2, 9, 4}};
    CHECK(neko::equal(a.begin(), a.end(), b.begin()));
    CHECK(!neko::equal(a.begin(), a.end(), c.begin()));
}

NEKO_TEST(equal_on_an_empty_range_is_true) {
    const neko::array<int, 1> a{{7}};
    CHECK(neko::equal(a.begin(), a.begin(), a.begin()));
}

NEKO_TEST(equal_with_both_ends_rejects_different_lengths) {
    const int a[] = {1, 2, 3};
    const int b[] = {1, 2, 3, 4};
    CHECK(!neko::equal(a, a + 3, b, b + 4));
    CHECK(neko::equal(a, a + 3, b, b + 3));
}

NEKO_TEST(lexicographical_compare_uses_the_first_difference) {
    const int a[] = {1, 2, 3};
    const int b[] = {1, 2, 4};
    CHECK(neko::lexicographical_compare(a, a + 3, b, b + 3));
    CHECK(!neko::lexicographical_compare(b, b + 3, a, a + 3));
}

NEKO_TEST(lexicographical_compare_treats_a_prefix_as_smaller) {
    const int ab[] = {1, 2};
    const int abc[] = {1, 2, 3};
    CHECK(neko::lexicographical_compare(ab, ab + 2, abc, abc + 3));
    CHECK(!neko::lexicographical_compare(abc, abc + 3, ab, ab + 2));
}

NEKO_TEST(lexicographical_compare_is_not_a_length_comparison) {
    const int two[] = {2};
    const int ones[] = {1, 1, 1};
    CHECK(neko::lexicographical_compare(ones, ones + 3, two, two + 1));
    CHECK(!neko::lexicographical_compare(two, two + 1, ones, ones + 3));
}

NEKO_TEST(equal_ranges_are_not_lexicographically_less) {
    const int a[] = {1, 2, 3};
    CHECK(!neko::lexicographical_compare(a, a + 3, a, a + 3));
}

// ---------------------------------------------------------------------------
// Copying
// ---------------------------------------------------------------------------

NEKO_TEST(copy_writes_the_range_and_returns_the_new_end) {
    const neko::array<int, 4> src{{1, 2, 3, 4}};
    int dst[6] = {0, 0, 0, 0, 0, 0};
    int* out = neko::copy(src.begin(), src.end(), dst);
    CHECK_EQ(out, dst + 4);
    CHECK_EQ(dst[0], 1);
    CHECK_EQ(dst[3], 4);
    CHECK_EQ(dst[4], 0);  // one past the copy is untouched
}

NEKO_TEST(copy_of_an_empty_range_returns_the_destination_unchanged) {
    const int src[] = {1, 2, 3};
    int dst[1] = {42};
    CHECK_EQ(neko::copy(src, src, dst), dst);
    CHECK_EQ(dst[0], 42);
}

NEKO_TEST(copy_returns_a_chaining_point) {
    const int a[] = {1, 2};
    const int b[] = {3, 4};
    int dst[4] = {};
    int* out = neko::copy(b, b + 2, neko::copy(a, a + 2, dst));
    CHECK_EQ(out, dst + 4);
    CHECK_EQ(dst[1], 2);
    CHECK_EQ(dst[2], 3);
}

NEKO_TEST(copy_n_stops_after_count_elements) {
    const int src[] = {1, 2, 3, 4, 5};
    int dst[5] = {};
    int* out = neko::copy_n(src, 3, dst);
    CHECK_EQ(out, dst + 3);
    CHECK_EQ(dst[2], 3);
    CHECK_EQ(dst[3], 0);
}

NEKO_TEST(copy_n_with_zero_writes_nothing) {
    const int src[] = {1, 2};
    int dst[1] = {42};
    CHECK_EQ(neko::copy_n(src, 0, dst), dst);
    CHECK_EQ(dst[0], 42);
}

NEKO_TEST(copy_backward_shifts_an_overlapping_range_right) {
    // The shift vector::insert performs to open a one-element gap. A forward
    // copy() here would smear the first element across the whole buffer.
    int buf[5] = {1, 2, 3, 4, 0};
    int* out = neko::copy_backward(buf + 1, buf + 4, buf + 5);
    CHECK_EQ(out, buf + 2);
    CHECK_EQ(buf[0], 1);
    CHECK_EQ(buf[2], 2);
    CHECK_EQ(buf[3], 3);
    CHECK_EQ(buf[4], 4);
}

NEKO_TEST(copy_backward_into_a_disjoint_range) {
    const int src[] = {1, 2, 3};
    int dst[3] = {};
    CHECK_EQ(neko::copy_backward(src, src + 3, dst + 3), dst);
    CHECK_EQ(dst[0], 1);
    CHECK_EQ(dst[2], 3);
}

// ---------------------------------------------------------------------------
// Moving
// ---------------------------------------------------------------------------

NEKO_TEST(move_moves_rather_than_copies) {
    Tracker::reset();
    Tracker src[3] = {Tracker{1}, Tracker{2}, Tracker{3}};
    Tracker dst[3];
    const int copies_before = Tracker::copies;

    neko::move(src, src + 3, dst);

    CHECK_EQ(Tracker::copies, copies_before);
    CHECK_EQ(Tracker::moves, 3);
    CHECK_EQ(dst[0].value, 1);
    CHECK_EQ(dst[2].value, 3);
    CHECK(src[0].moved_from);
}

NEKO_TEST(move_the_algorithm_coexists_with_move_the_cast) {
    // Three arguments selects the range algorithm; one selects the cast in
    // utility.hpp. Same name, same namespace, told apart by arity alone.
    Tracker::reset();
    Tracker a{1};
    Tracker b = neko::move(a);
    CHECK_EQ(Tracker::moves, 1);
    CHECK_EQ(b.value, 1);
}

NEKO_TEST(move_backward_shifts_right_without_clobbering) {
    Tracker::reset();
    Tracker buf[4];
    buf[0].value = 1;
    buf[1].value = 2;
    buf[2].value = 3;

    neko::move_backward(buf, buf + 3, buf + 4);

    CHECK_EQ(buf[1].value, 1);
    CHECK_EQ(buf[2].value, 2);
    CHECK_EQ(buf[3].value, 3);
}

// ---------------------------------------------------------------------------
// Filling
// ---------------------------------------------------------------------------

NEKO_TEST(fill_writes_every_element) {
    neko::array<int, 4> a{{1, 2, 3, 4}};
    neko::fill(a.begin(), a.end(), 7);
    CHECK_EQ(a[0], 7);
    CHECK_EQ(a[3], 7);
}

NEKO_TEST(fill_of_an_empty_range_does_nothing) {
    int buf[1] = {42};
    neko::fill(buf, buf, 7);
    CHECK_EQ(buf[0], 42);
}

NEKO_TEST(fill_takes_the_value_type_as_a_separate_parameter) {
    // The element is long, the value is an int literal. This only compiles
    // because T is deduced independently of the iterator's value type.
    long buf[3] = {};
    neko::fill(buf, buf + 3, 7);
    CHECK_EQ(buf[2], 7L);
}

NEKO_TEST(fill_n_returns_one_past_the_last_write) {
    int buf[5] = {};
    int* out = neko::fill_n(buf, 3, 9);
    CHECK_EQ(out, buf + 3);
    CHECK_EQ(buf[2], 9);
    CHECK_EQ(buf[3], 0);
}

// ---------------------------------------------------------------------------
// Swapping
// ---------------------------------------------------------------------------

NEKO_TEST(iter_swap_exchanges_what_two_iterators_point_at) {
    int a = 1, b = 2;
    neko::iter_swap(&a, &b);
    CHECK_EQ(a, 2);
    CHECK_EQ(b, 1);
}

NEKO_TEST(swap_ranges_returns_the_end_of_the_second_range) {
    neko::array<int, 3> a{{1, 2, 3}};
    neko::array<int, 3> b{{4, 5, 6}};
    int* out = neko::swap_ranges(a.begin(), a.end(), b.begin());
    CHECK_EQ(out, b.end());
    CHECK_EQ(a[0], 4);
    CHECK_EQ(a[2], 6);
    CHECK_EQ(b[0], 1);
    CHECK_EQ(b[2], 3);
}

NEKO_TEST(swap_ranges_moves_rather_than_copies) {
    Tracker::reset();
    Tracker a[2];
    Tracker b[2];
    const int copies_before = Tracker::copies;
    neko::swap_ranges(a, a + 2, b);
    CHECK_EQ(Tracker::copies, copies_before);
}

// ---------------------------------------------------------------------------
// Minimum and maximum
// ---------------------------------------------------------------------------

NEKO_TEST(min_and_max_pick_the_expected_value) {
    CHECK_EQ(neko::min(2, 5), 2);
    CHECK_EQ(neko::max(2, 5), 5);
    CHECK_EQ(neko::min(5, 2), 2);
    CHECK_EQ(neko::max(5, 2), 5);
}

NEKO_TEST(min_and_max_return_the_first_argument_on_a_tie) {
    // Not pedantry: this is the property a stable sort built on them inherits.
    const int a = 3;
    const int b = 3;
    CHECK_EQ(&neko::min(a, b), &a);
    CHECK_EQ(&neko::max(a, b), &a);
}

// ---------------------------------------------------------------------------
// Signatures
//
// These are compile-time, so they hold even while every body above is a stub.
// ---------------------------------------------------------------------------

NEKO_TEST(algorithms_return_the_iterator_types_they_were_handed) {
    using In = const int*;
    using Out = int*;
    STATIC_CHECK(neko::is_same_v<decltype(neko::copy(neko::declval<In>(),
                                                     neko::declval<In>(),
                                                     neko::declval<Out>())),
                                 Out>);
    STATIC_CHECK(neko::is_same_v<decltype(neko::equal(neko::declval<In>(),
                                                      neko::declval<In>(),
                                                      neko::declval<In>())),
                                 bool>);
    STATIC_CHECK(
        neko::is_same_v<decltype(neko::min(neko::declval<const int&>(),
                                           neko::declval<const int&>())),
                        const int&>);
}
