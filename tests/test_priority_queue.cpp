// Tests for neko/priority_queue.hpp and the heap section of neko/algorithm.hpp.
//
// The two are tested together because the adaptor has no behaviour of its own:
// every member is a call into a heap algorithm. Get the heap functions passing
// first -- the adaptor tests will then mostly pass for free.
#include "test_framework.hpp"

#include "neko/algorithm.hpp"
#include "neko/array.hpp"
#include "neko/functional.hpp"
#include "neko/priority_queue.hpp"
#include "neko/vector.hpp"

namespace {

// A range is a max-heap when no element outranks its parent.
bool is_max_heap(const int* first, const int* last) {
    const std::ptrdiff_t n = last - first;
    for (std::ptrdiff_t child = 1; child < n; ++child)
        if (first[(child - 1) / 2] < first[child]) return false;
    return true;
}

// The detection idiom, used here to assert that something is *absent*.
template <typename T, typename = void>
struct has_begin : neko::false_type {};
template <typename T>
struct has_begin<T, neko::void_t<decltype(neko::declval<T&>().begin())>>
    : neko::true_type {};

}  // namespace

// ---------------------------------------------------------------------------
// The comparators
// ---------------------------------------------------------------------------

NEKO_TEST(less_and_greater_are_strict_weak_orderings) {
    neko::less<int> lt;
    neko::greater<int> gt;
    CHECK(lt(1, 2));
    CHECK(!lt(2, 1));
    CHECK(!lt(1, 1));  // irreflexive: never true for equal arguments
    CHECK(gt(2, 1));
    CHECK(!gt(1, 2));
    CHECK(!gt(1, 1));
}

NEKO_TEST(a_stateless_comparator_costs_nothing) {
    // Why the STL takes a Compare type rather than a function pointer.
    STATIC_CHECK(sizeof(neko::less<int>) == 1);  // empty, not zero-sized
    STATIC_CHECK(std::is_empty_v<neko::less<int>>);
}

// ---------------------------------------------------------------------------
// make_heap / push_heap / pop_heap
// ---------------------------------------------------------------------------

NEKO_TEST(make_heap_establishes_the_invariant) {
    int a[] = {3, 1, 4, 1, 5, 9, 2, 6};
    neko::make_heap(a, a + 8);
    CHECK(is_max_heap(a, a + 8));
    CHECK_EQ(a[0], 9);  // the largest ends up at the root
}

NEKO_TEST(make_heap_on_a_trivial_range_does_nothing_wrong) {
    int a[] = {1};
    neko::make_heap(a, a);
    neko::make_heap(a, a + 1);
    CHECK_EQ(a[0], 1);
}

NEKO_TEST(make_heap_keeps_every_element) {
    int a[] = {3, 1, 4, 1, 5};
    neko::make_heap(a, a + 5);
    int sum = 0;
    for (int v : a) sum += v;
    CHECK_EQ(sum, 14);  // a permutation, not a rewrite
}

NEKO_TEST(push_heap_sifts_the_last_element_into_place) {
    int a[] = {9, 5, 4, 1, 1, 3, 2, 0};
    neko::make_heap(a, a + 7);
    a[7] = 7;  // append, then repair
    neko::push_heap(a, a + 8);
    CHECK(is_max_heap(a, a + 8));
}

NEKO_TEST(push_heap_can_promote_to_the_root) {
    int a[] = {5, 4, 3, 99};
    neko::make_heap(a, a + 3);
    neko::push_heap(a, a + 4);
    CHECK_EQ(a[0], 99);
}

NEKO_TEST(pop_heap_moves_the_largest_to_the_back) {
    int a[] = {3, 1, 4, 1, 5, 9, 2};
    neko::make_heap(a, a + 7);
    neko::pop_heap(a, a + 7);
    CHECK_EQ(a[6], 9);             // the old root, parked at the end
    CHECK(is_max_heap(a, a + 6));  // and the rest is a heap again
}

NEKO_TEST(repeated_pop_heap_sorts_the_range_ascending) {
    int a[] = {3, 1, 4, 1, 5, 9, 2, 6};
    neko::make_heap(a, a + 8);
    for (int n = 8; n > 1; --n) neko::pop_heap(a, a + n);
    // Popping to the back repeatedly is heapsort, and it sorts ascending.
    for (int i = 1; i < 8; ++i) REQUIRE(a[i - 1] <= a[i]);
}

NEKO_TEST(sort_heap_sorts_ascending) {
    int a[] = {3, 1, 4, 1, 5, 9, 2, 6};
    neko::make_heap(a, a + 8);
    neko::sort_heap(a, a + 8);
    for (int i = 1; i < 8; ++i) REQUIRE(a[i - 1] <= a[i]);
}

NEKO_TEST(the_comparator_overloads_invert_the_heap) {
    int a[] = {3, 1, 4, 1, 5, 9, 2};
    neko::make_heap(a, a + 7, neko::greater<int>{});
    CHECK_EQ(a[0], 1);  // greater makes a MIN-heap
    neko::pop_heap(a, a + 7, neko::greater<int>{});
    CHECK_EQ(a[6], 1);
}

// ---------------------------------------------------------------------------
// The adaptor
// ---------------------------------------------------------------------------

NEKO_TEST(priority_queue_is_not_a_container) {
    // No begin/end/operator[] -- the absences are the interface. If you ever
    // add them, this test should start failing.
    using Q = neko::priority_queue<int>;
    STATIC_CHECK(!has_begin<Q>::value);
    STATIC_CHECK(
        has_begin<neko::vector<int>>::value);  // the adapted one has it
    STATIC_CHECK(neko::is_same_v<Q::container_type, neko::vector<int>>);
    STATIC_CHECK(neko::is_same_v<Q::value_compare, neko::less<int>>);
}

NEKO_TEST(a_default_constructed_queue_is_empty) {
    neko::priority_queue<int> q;
    CHECK(q.empty());
    CHECK_EQ(q.size(), 0u);
}

NEKO_TEST(top_returns_the_largest_by_default) {
    // The surprise: less<T> gives a MAX-heap.
    neko::priority_queue<int> q;
    q.push(3);
    q.push(9);
    q.push(1);
    CHECK_EQ(q.top(), 9);
    CHECK_EQ(q.size(), 3u);
}

NEKO_TEST(top_is_const_even_on_a_non_const_queue) {
    neko::priority_queue<int> q;
    STATIC_CHECK(neko::is_same_v<decltype(q.top()), const int&>);
}

NEKO_TEST(pop_removes_the_top_and_reveals_the_next) {
    neko::priority_queue<int> q;
    for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) q.push(v);

    int expected[] = {9, 6, 5, 4, 3, 2, 1, 1};
    for (int e : expected) {
        REQUIRE_EQ(q.top(), e);
        q.pop();
    }
    CHECK(q.empty());
}

NEKO_TEST(greater_turns_it_into_a_min_queue) {
    neko::priority_queue<int, neko::vector<int>, neko::greater<int>> q;
    for (int v : {3, 1, 4, 1, 5}) q.push(v);
    CHECK_EQ(q.top(), 1);
    q.pop();
    CHECK_EQ(q.top(), 1);
    q.pop();
    CHECK_EQ(q.top(), 3);
}

NEKO_TEST(emplace_constructs_in_place) {
    neko::priority_queue<int> q;
    q.emplace(5);
    q.emplace(7);
    CHECK_EQ(q.top(), 7);
}

NEKO_TEST(constructing_from_a_range_heapifies_once) {
    const int src[] = {3, 1, 4, 1, 5, 9, 2};
    neko::priority_queue<int> q(src, src + 7);
    CHECK_EQ(q.size(), 7u);
    CHECK_EQ(q.top(), 9);
}

NEKO_TEST(swap_exchanges_two_queues) {
    neko::priority_queue<int> a;
    neko::priority_queue<int> b;
    a.push(1);
    b.push(2);
    a.swap(b);
    CHECK_EQ(a.top(), 2);
    CHECK_EQ(b.top(), 1);
}
