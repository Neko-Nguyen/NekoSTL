// Tests for neko/vector.hpp -- your work queue.
//
// Everything here reports TODO until you write the corresponding member. The
// tests are grouped in the order suggested at the top of vector.hpp; run one
// group at a time:
//
//     ./test_vector reserve        # substring match on the test name
//
// Run under ASan (the default build) throughout. Several of these tests are
// really leak and double-destruction tests -- they will pass on the printed
// values and still fail loudly in the sanitizer report if the raw storage is
// mismanaged. That is the point.
#include "test_framework.hpp"

#include <stdexcept>

#include "neko/vector.hpp"
#include "tracker.hpp"

using neko_test::ThrowingMove;
using neko_test::Tracker;

namespace {

// A type that throws on the Nth construction, to check that a half-built
// vector still cleans up after itself.
struct ThrowOnNth {
    static inline int countdown = -1;  // -1 disables throwing
    int value = 0;

    explicit ThrowOnNth(int initial = 0) : value(initial) { maybe_throw(); }
    ThrowOnNth(const ThrowOnNth& o) : value(o.value) { maybe_throw(); }

    static void maybe_throw() {
        if (countdown < 0) return;
        if (countdown-- == 0) throw std::runtime_error("boom");
    }
};

// Fills a vector with 0..n-1 so a test does not repeat the loop each time.
template <typename V>
void fill_iota(V& v, int n) {
    for (int i = 0; i < n; ++i) v.push_back(i);
}

}  // namespace

// ===========================================================================
// 1. default ctor, size/capacity/empty, destructor
// ===========================================================================

NEKO_TEST(vector_publishes_the_standard_member_types) {
    using V = neko::vector<int>;
    STATIC_CHECK(neko::is_same_v<V::value_type, int>);
    STATIC_CHECK(neko::is_same_v<V::size_type, std::size_t>);
    STATIC_CHECK(neko::is_same_v<V::reference, int&>);
    STATIC_CHECK(neko::is_same_v<V::const_reference, const int&>);
    STATIC_CHECK(neko::is_same_v<V::iterator, int*>);
    STATIC_CHECK(neko::is_same_v<V::const_iterator, const int*>);
}

NEKO_TEST(vector_default_ctor_allocates_nothing) {
    neko::vector<int> v;
    CHECK_EQ(v.size(), 0u);
    CHECK_EQ(v.capacity(), 0u);
    CHECK(v.empty());

    // A default-constructed vector must not allocate: it is the common case,
    // and an empty vector should cost three null pointers and nothing else.
    CHECK(v.data() == nullptr);
}

NEKO_TEST(vector_sized_ctor_value_initialises) {
    neko::vector<int> v(3);
    CHECK_EQ(v.size(), 3u);
    CHECK(v.capacity() >= 3u);
    CHECK_EQ(v[0], 0);
    CHECK_EQ(v[2], 0);
}

NEKO_TEST(vector_fill_ctor_copies_the_value) {
    neko::vector<int> v(4, 7);
    CHECK_EQ(v.size(), 4u);
    for (std::size_t i = 0; i < v.size(); ++i) CHECK_EQ(v[i], 7);
}

NEKO_TEST(vector_destructor_destroys_every_element) {
    Tracker::reset();
    {
        neko::vector<Tracker> v;
        v.push_back(Tracker{1});
        v.push_back(Tracker{2});
        v.push_back(Tracker{3});
        CHECK_EQ(Tracker::alive, 3);
    }
    // Destroy [begin_, end_), then release the buffer. Destroying the whole
    // *capacity* would run destructors on objects that were never constructed.
    CHECK_EQ(Tracker::alive, 0);
}

// ===========================================================================
// 2. reserve + push_back -- the growth logic
// ===========================================================================

NEKO_TEST(vector_push_back_appends) {
    neko::vector<int> v;
    v.push_back(1);
    CHECK_EQ(v.size(), 1u);
    CHECK_EQ(v[0], 1);

    v.push_back(2);
    CHECK_EQ(v.size(), 2u);
    CHECK_EQ(v[0], 1);
    CHECK_EQ(v[1], 2);
    CHECK(!v.empty());
}

NEKO_TEST(vector_push_back_grows_geometrically) {
    neko::vector<int> v;
    std::size_t reallocations = 0;
    std::size_t last_cap = v.capacity();

    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
        if (v.capacity() != last_cap) {
            ++reallocations;
            last_cap = v.capacity();
        }
        REQUIRE_EQ(v.size(), static_cast<std::size_t>(i) + 1);
        CHECK(v.capacity() >= v.size());
    }

    // Doubling gives O(log N) reallocations for N push_backs, which is what
    // makes them amortised O(1). Growing by a constant would give ~100 here.
    CHECK(reallocations <= 10);

    for (int i = 0; i < 100; ++i) REQUIRE_EQ(v[static_cast<std::size_t>(i)], i);
}

NEKO_TEST(vector_push_back_from_empty_does_not_double_zero) {
    // The classic off-by-one: capacity 0 * 2 is still 0, so the first
    // push_back must special-case it (libstdc++ starts at 1).
    neko::vector<int> v;
    REQUIRE_EQ(v.capacity(), 0u);
    v.push_back(42);
    CHECK(v.capacity() >= 1u);
    CHECK_EQ(v.size(), 1u);
    CHECK_EQ(v[0], 42);
}

NEKO_TEST(vector_reserve_sets_capacity_without_changing_size) {
    neko::vector<int> v;
    v.reserve(10);
    CHECK(v.capacity() >= 10u);
    CHECK_EQ(v.size(), 0u);
    CHECK(v.empty());

    fill_iota(v, 10);
    CHECK_EQ(v.size(), 10u);
}

NEKO_TEST(vector_reserve_never_shrinks) {
    neko::vector<int> v;
    v.reserve(10);
    const std::size_t big = v.capacity();

    v.reserve(5);
    CHECK_EQ(v.capacity(), big);
    v.reserve(0);
    CHECK_EQ(v.capacity(), big);
}

NEKO_TEST(vector_reserve_preserves_the_elements) {
    neko::vector<int> v;
    fill_iota(v, 5);
    v.reserve(100);

    REQUIRE_EQ(v.size(), 5u);
    for (int i = 0; i < 5; ++i) CHECK_EQ(v[static_cast<std::size_t>(i)], i);
}

NEKO_TEST(vector_reserve_invalidates_iterators_and_references) {
    // Not a bug -- a documented guarantee. Relocation means the old buffer is
    // gone, so anything pointing into it dangles. ASan catches use-after-free
    // here if the old buffer is not actually released.
    neko::vector<int> v;
    v.push_back(1);
    const int* before = v.data();

    v.reserve(1000);
    CHECK(v.data() != before);
    CHECK_EQ(v[0], 1);
}

NEKO_TEST(vector_reserve_moves_when_the_move_ctor_is_noexcept) {
    Tracker::reset();
    {
        neko::vector<Tracker> v;
        v.reserve(4);
        for (int i = 0; i < 4; ++i) v.emplace_back(i);

        const int moves_before = Tracker::moves;
        const int copies_before = Tracker::copies;

        v.reserve(64);  // forces a relocation of all 4 elements

        CHECK_EQ(Tracker::copies - copies_before, 0);
        CHECK_EQ(Tracker::moves - moves_before, 4);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_reserve_copies_when_the_move_ctor_can_throw) {
    // The strong guarantee: if a move throws halfway through relocation, the
    // already-moved elements are wrecked and there is no way back. Copying
    // leaves the original buffer intact, so a throw is recoverable.
    // (std::move_if_noexcept is exactly this decision.)
    neko::vector<ThrowingMove> v;
    v.reserve(2);
    v.push_back(ThrowingMove{1});
    v.push_back(ThrowingMove{2});

    v.reserve(16);
    REQUIRE_EQ(v.size(), 2u);
    CHECK_EQ(v[0].value, 1);
    CHECK_EQ(v[1].value, 2);
}

NEKO_TEST(vector_shrink_to_fit_releases_spare_capacity) {
    neko::vector<int> v;
    v.reserve(100);
    fill_iota(v, 3);

    v.shrink_to_fit();
    CHECK_EQ(v.capacity(), 3u);
    REQUIRE_EQ(v.size(), 3u);
    for (int i = 0; i < 3; ++i) CHECK_EQ(v[static_cast<std::size_t>(i)], i);
}

// ===========================================================================
// 3. element access and iterators
// ===========================================================================

NEKO_TEST(vector_subscript_returns_a_reference) {
    neko::vector<int> v;
    fill_iota(v, 3);

    v[1] = 20;
    CHECK_EQ(v[1], 20);

    const neko::vector<int>& c = v;
    STATIC_CHECK(neko::is_same_v<decltype(c[0]), const int&>);
    CHECK_EQ(c[1], 20);
}

NEKO_TEST(vector_at_is_the_checked_accessor) {
    neko::vector<int> v;
    fill_iota(v, 3);

    CHECK_EQ(v.at(0), 0);
    v.at(0) = 10;
    CHECK_EQ(v.at(0), 10);

    CHECK_THROWS_AS(v.at(3), std::out_of_range);
    CHECK_THROWS_AS(v.at(99), std::out_of_range);
    CHECK_THROWS_AS(neko::as_const(v).at(3), std::out_of_range);

    // The bound is size(), not capacity: reserved-but-unconstructed slots are
    // out of range even though the memory exists.
    v.reserve(100);
    CHECK_THROWS_AS(v.at(50), std::out_of_range);
}

NEKO_TEST(vector_front_back_and_data) {
    neko::vector<int> v;
    fill_iota(v, 3);

    CHECK_EQ(v.front(), 0);
    CHECK_EQ(v.back(), 2);
    CHECK_EQ(v.data(), &v[0]);
    CHECK_EQ(v.data()[2], 2);

    v.front() = 100;
    v.back() = 300;
    CHECK_EQ(v[0], 100);
    CHECK_EQ(v[2], 300);
}

NEKO_TEST(vector_iterators_form_a_half_open_range) {
    neko::vector<int> v;
    fill_iota(v, 4);

    CHECK_EQ(v.end() - v.begin(), 4);
    CHECK_EQ(v.begin(), v.data());
    CHECK_EQ(*v.begin(), 0);
    CHECK_EQ(*(v.end() - 1), 3);

    int sum = 0;
    for (int x : v) sum += x;
    CHECK_EQ(sum, 6);

    // An empty vector's range is empty, and begin() == end() even when both
    // are null. A loop over it must simply not run.
    neko::vector<int> e;
    CHECK_EQ(e.begin(), e.end());
    int count = 0;
    for (int x : e) {
        (void)x;
        ++count;
    }
    CHECK_EQ(count, 0);
}

// ===========================================================================
// 4. copy ctor / copy assignment
// ===========================================================================

NEKO_TEST(vector_copy_ctor_makes_an_independent_copy) {
    neko::vector<int> a;
    fill_iota(a, 3);

    neko::vector<int> b = a;
    REQUIRE_EQ(b.size(), 3u);
    CHECK_EQ(b[0], 0);
    CHECK_EQ(b[2], 2);

    // Deep, not shallow: separate buffers.
    CHECK(b.data() != a.data());
    b[0] = 99;
    CHECK_EQ(a[0], 0);
}

NEKO_TEST(vector_copy_ctor_copies_elements_not_capacity) {
    Tracker::reset();
    {
        neko::vector<Tracker> a;
        a.reserve(64);
        for (int i = 0; i < 3; ++i) a.emplace_back(i);

        const int copies_before = Tracker::copies;
        neko::vector<Tracker> b = a;

        // Exactly 3 copies -- the 61 unused slots hold no objects to copy.
        CHECK_EQ(Tracker::copies - copies_before, 3);
        CHECK_EQ(b.size(), 3u);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_copy_assignment_replaces_the_contents) {
    neko::vector<int> a;
    fill_iota(a, 3);
    neko::vector<int> b;
    fill_iota(b, 10);

    b = a;
    REQUIRE_EQ(b.size(), 3u);
    CHECK_EQ(b[0], 0);
    CHECK_EQ(b[2], 2);
    CHECK(b.data() != a.data());
}

NEKO_TEST(vector_copy_assignment_handles_self_assignment) {
    neko::vector<int> a;
    fill_iota(a, 3);

    // The naive "clear then copy" implementation destroys the source here.
    // Guard against it, or use copy-and-swap, which is immune by construction.
    a = a;

    REQUIRE_EQ(a.size(), 3u);
    CHECK_EQ(a[0], 0);
    CHECK_EQ(a[2], 2);
}

NEKO_TEST(vector_copy_assignment_destroys_the_old_elements) {
    Tracker::reset();
    {
        neko::vector<Tracker> a;
        for (int i = 0; i < 2; ++i) a.emplace_back(i);
        neko::vector<Tracker> b;
        for (int i = 0; i < 5; ++i) b.emplace_back(i);

        b = a;  // b's 5 elements must be destroyed, not leaked
        CHECK_EQ(b.size(), 2u);
        CHECK_EQ(Tracker::alive, 4);  // 2 in a, 2 in b
    }
    CHECK_EQ(Tracker::alive, 0);
}

// ===========================================================================
// 5. move ctor / move assignment
// ===========================================================================

NEKO_TEST(vector_move_ctor_steals_the_buffer) {
    neko::vector<int> a;
    fill_iota(a, 3);
    const int* const buffer = a.data();

    neko::vector<int> b = neko::move(a);

    // The same allocation, not a copy of it: that is what makes moving O(1).
    CHECK_EQ(b.data(), buffer);
    REQUIRE_EQ(b.size(), 3u);
    CHECK_EQ(b[2], 2);

    // The source must be left empty and, crucially, safe to destroy -- the
    // exchange idiom does both at once.
    CHECK_EQ(a.size(), 0u);
    CHECK_EQ(a.capacity(), 0u);
    CHECK(a.data() == nullptr);
}

NEKO_TEST(vector_move_ctor_does_not_touch_the_elements) {
    Tracker::reset();
    {
        neko::vector<Tracker> a;
        for (int i = 0; i < 5; ++i) a.emplace_back(i);

        const int moves_before = Tracker::moves;
        const int copies_before = Tracker::copies;

        neko::vector<Tracker> b = neko::move(a);

        // Moving a *vector* moves three pointers. The elements are untouched:
        // no per-element move, no per-element copy.
        CHECK_EQ(Tracker::moves - moves_before, 0);
        CHECK_EQ(Tracker::copies - copies_before, 0);
        CHECK_EQ(b.size(), 5u);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_move_assignment_releases_the_target_first) {
    Tracker::reset();
    {
        neko::vector<Tracker> a;
        for (int i = 0; i < 2; ++i) a.emplace_back(i);
        neko::vector<Tracker> b;
        for (int i = 0; i < 5; ++i) b.emplace_back(i);

        b = neko::move(a);

        CHECK_EQ(b.size(), 2u);
        CHECK_EQ(a.size(), 0u);
        // b's original 5 must be destroyed here, not leaked with the buffer.
        CHECK_EQ(Tracker::alive, 2);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_move_assignment_handles_self_assignment) {
    neko::vector<int> a;
    fill_iota(a, 3);

    a = neko::move(a);  // must not free the buffer it is about to keep

    // Any valid state is acceptable by the standard; not crashing and not
    // leaking is the real requirement. ASan is the judge here.
    CHECK(a.size() == 3u || a.size() == 0u);
}

// ===========================================================================
// 6. push_back(T&&) and emplace_back
// ===========================================================================

NEKO_TEST(vector_push_back_rvalue_moves) {
    Tracker::reset();
    {
        neko::vector<Tracker> v;
        v.reserve(2);

        Tracker t{1};
        v.push_back(neko::move(t));
        CHECK_EQ(Tracker::moves, 1);
        CHECK_EQ(Tracker::copies, 0);

        Tracker u{2};
        v.push_back(u);  // lvalue: must select the copying overload
        CHECK_EQ(Tracker::copies, 1);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_emplace_back_constructs_in_place) {
    Tracker::reset();
    {
        neko::vector<Tracker> v;
        v.reserve(2);

        v.emplace_back(42);

        // No temporary at all: one construction, zero copies, zero moves.
        // That is the difference between emplace_back and push_back(T{...}).
        CHECK_EQ(Tracker::ctors, 1);
        CHECK_EQ(Tracker::copies, 0);
        CHECK_EQ(Tracker::moves, 0);
        CHECK_EQ(v[0].value, 42);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_emplace_back_returns_a_reference_to_the_new_element) {
    neko::vector<int> v;
    int& r = v.emplace_back(7);
    CHECK_EQ(r, 7);
    CHECK_EQ(&r, &v.back());
    r = 8;
    CHECK_EQ(v[0], 8);
}

NEKO_TEST(vector_emplace_back_forwards_multiple_arguments) {
    struct Pair {
        int a;
        double b;
        Pair(int a_, double b_) : a(a_), b(b_) {}
    };
    neko::vector<Pair> v;
    v.emplace_back(1, 2.5);
    CHECK_EQ(v.size(), 1u);
    CHECK_EQ(v[0].a, 1);
    CHECK(v[0].b == 2.5);
}

// ===========================================================================
// 7. pop_back, clear, resize
// ===========================================================================

NEKO_TEST(vector_pop_back_destroys_only_the_last_element) {
    Tracker::reset();
    {
        neko::vector<Tracker> v;
        for (int i = 0; i < 3; ++i) v.emplace_back(i);
        const std::size_t cap = v.capacity();

        v.pop_back();

        CHECK_EQ(v.size(), 2u);
        CHECK_EQ(Tracker::alive, 2);
        CHECK_EQ(v.back().value, 1);
        // pop_back never reallocates, so capacity is unchanged.
        CHECK_EQ(v.capacity(), cap);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_clear_empties_but_keeps_capacity) {
    Tracker::reset();
    {
        neko::vector<Tracker> v;
        for (int i = 0; i < 5; ++i) v.emplace_back(i);
        const std::size_t cap = v.capacity();

        v.clear();

        CHECK_EQ(v.size(), 0u);
        CHECK(v.empty());
        CHECK_EQ(Tracker::alive, 0);  // all 5 destroyed...
        CHECK_EQ(v.capacity(), cap);  // ...but the buffer is retained

        // And the vector is immediately reusable.
        v.emplace_back(9);
        CHECK_EQ(v.size(), 1u);
        CHECK_EQ(v[0].value, 9);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_resize_grows_and_shrinks) {
    neko::vector<int> v;
    fill_iota(v, 3);

    v.resize(5);  // new elements are value-initialised
    REQUIRE_EQ(v.size(), 5u);
    CHECK_EQ(v[2], 2);
    CHECK_EQ(v[3], 0);
    CHECK_EQ(v[4], 0);

    v.resize(2);  // shrinking destroys the tail
    REQUIRE_EQ(v.size(), 2u);
    CHECK_EQ(v[0], 0);
    CHECK_EQ(v[1], 1);

    v.resize(2);  // no-op
    CHECK_EQ(v.size(), 2u);
}

NEKO_TEST(vector_resize_with_a_fill_value) {
    neko::vector<int> v;
    fill_iota(v, 2);

    v.resize(5, 9);
    REQUIRE_EQ(v.size(), 5u);
    CHECK_EQ(v[1], 1);  // existing elements untouched
    CHECK_EQ(v[2], 9);
    CHECK_EQ(v[4], 9);
}

NEKO_TEST(vector_resize_down_destroys_the_removed_elements) {
    Tracker::reset();
    {
        neko::vector<Tracker> v;
        for (int i = 0; i < 5; ++i) v.emplace_back(i);

        v.resize(2);
        CHECK_EQ(Tracker::alive, 2);
        CHECK_EQ(v.size(), 2u);
    }
    CHECK_EQ(Tracker::alive, 0);
}

// ===========================================================================
// 8. insert and erase -- the fiddly ones
// ===========================================================================

NEKO_TEST(vector_insert_at_the_middle_shifts_the_tail) {
    neko::vector<int> v;
    fill_iota(v, 4);  // 0 1 2 3

    neko::vector<int>::iterator it = v.insert(v.begin() + 2, 99);

    REQUIRE_EQ(v.size(), 5u);
    CHECK_EQ(v[0], 0);
    CHECK_EQ(v[1], 1);
    CHECK_EQ(v[2], 99);
    CHECK_EQ(v[3], 2);
    CHECK_EQ(v[4], 3);

    // Returns an iterator to the inserted element.
    CHECK_EQ(*it, 99);
    CHECK_EQ(it, v.begin() + 2);
}

NEKO_TEST(vector_insert_at_the_ends) {
    neko::vector<int> v;
    fill_iota(v, 2);  // 0 1

    v.insert(v.begin(), 7);
    REQUIRE_EQ(v.size(), 3u);
    CHECK_EQ(v[0], 7);
    CHECK_EQ(v[2], 1);

    v.insert(v.end(), 8);  // end() is a valid insertion point
    REQUIRE_EQ(v.size(), 4u);
    CHECK_EQ(v.back(), 8);

    // ...and into an empty vector, where begin() == end() == nullptr.
    neko::vector<int> e;
    e.insert(e.begin(), 1);
    REQUIRE_EQ(e.size(), 1u);
    CHECK_EQ(e[0], 1);
}

NEKO_TEST(vector_insert_reallocates_when_full) {
    neko::vector<int> v;
    v.reserve(4);
    fill_iota(v, 4);
    REQUIRE_EQ(v.size(), v.capacity());

    // `pos` is an iterator into the buffer that is about to be freed. Convert
    // it to an index *before* reallocating -- forgetting this is the standard
    // insert bug, and ASan reports it as a use-after-free.
    v.insert(v.begin() + 1, 99);

    REQUIRE_EQ(v.size(), 5u);
    CHECK_EQ(v[0], 0);
    CHECK_EQ(v[1], 99);
    CHECK_EQ(v[2], 1);
    CHECK_EQ(v[4], 3);
}

NEKO_TEST(vector_erase_removes_one_element) {
    neko::vector<int> v;
    fill_iota(v, 4);  // 0 1 2 3

    neko::vector<int>::iterator it = v.erase(v.begin() + 1);

    REQUIRE_EQ(v.size(), 3u);
    CHECK_EQ(v[0], 0);
    CHECK_EQ(v[1], 2);
    CHECK_EQ(v[2], 3);

    // Returns an iterator to the element that followed the erased one.
    CHECK_EQ(*it, 2);
    CHECK_EQ(it, v.begin() + 1);
}

NEKO_TEST(vector_erase_the_last_element_returns_end) {
    neko::vector<int> v;
    fill_iota(v, 3);

    neko::vector<int>::iterator it = v.erase(v.end() - 1);
    CHECK_EQ(v.size(), 2u);
    CHECK_EQ(it, v.end());
}

NEKO_TEST(vector_erase_a_range) {
    neko::vector<int> v;
    fill_iota(v, 6);  // 0 1 2 3 4 5

    neko::vector<int>::iterator it = v.erase(v.begin() + 1, v.begin() + 4);

    REQUIRE_EQ(v.size(), 3u);
    CHECK_EQ(v[0], 0);
    CHECK_EQ(v[1], 4);
    CHECK_EQ(v[2], 5);
    CHECK_EQ(it, v.begin() + 1);

    // An empty range is a no-op, not an error.
    v.erase(v.begin(), v.begin());
    CHECK_EQ(v.size(), 3u);
}

NEKO_TEST(vector_erase_destroys_exactly_the_erased_elements) {
    Tracker::reset();
    {
        neko::vector<Tracker> v;
        for (int i = 0; i < 5; ++i) v.emplace_back(i);

        v.erase(v.begin() + 1, v.begin() + 3);

        // Shift the survivors down, then destroy the now-vacated tail slots.
        // A count of 3 alive here means the erase did not leave stale objects
        // constructed past the new end_.
        CHECK_EQ(v.size(), 3u);
        CHECK_EQ(Tracker::alive, 3);
        CHECK_EQ(v[0].value, 0);
        CHECK_EQ(v[1].value, 3);
        CHECK_EQ(v[2].value, 4);
    }
    CHECK_EQ(Tracker::alive, 0);
}

// ===========================================================================
// 9. swap and comparisons
// ===========================================================================

NEKO_TEST(vector_swap_exchanges_the_buffers_in_constant_time) {
    neko::vector<int> a;
    fill_iota(a, 3);
    neko::vector<int> b;
    fill_iota(b, 5);

    const int* const a_data = a.data();
    const int* const b_data = b.data();

    a.swap(b);

    CHECK_EQ(a.size(), 5u);
    CHECK_EQ(b.size(), 3u);
    // Unlike array::swap, the elements never move -- only three pointers do.
    CHECK_EQ(a.data(), b_data);
    CHECK_EQ(b.data(), a_data);
}

NEKO_TEST(vector_swap_does_not_touch_the_elements) {
    Tracker::reset();
    {
        neko::vector<Tracker> a;
        for (int i = 0; i < 3; ++i) a.emplace_back(i);
        neko::vector<Tracker> b;
        for (int i = 0; i < 4; ++i) b.emplace_back(i);

        const int ops_before = Tracker::moves + Tracker::copies;
        a.swap(b);
        CHECK_EQ(Tracker::moves + Tracker::copies - ops_before, 0);
    }
    CHECK_EQ(Tracker::alive, 0);
}

NEKO_TEST(vector_equality_compares_sizes_then_elements) {
    neko::vector<int> a;
    fill_iota(a, 3);
    neko::vector<int> b;
    fill_iota(b, 3);
    neko::vector<int> c;
    fill_iota(c, 4);

    CHECK(a == b);
    CHECK(!(a == c));  // different size, short-circuits before comparing

    b[2] = 99;
    CHECK(!(a == b));

    // Capacity is not part of the value: two equal vectors need not have the
    // same capacity.
    neko::vector<int> d;
    d.reserve(100);
    fill_iota(d, 3);
    CHECK(a == d);
}

// ===========================================================================
// Exception safety -- the reason all of the above is harder than it looks
// ===========================================================================

NEKO_TEST(vector_cleans_up_when_an_element_ctor_throws) {
    // Halfway through filling a buffer, a constructor throws. The elements
    // already built must still be destroyed and the buffer released, or this
    // leaks -- ASan will say so even though no CHECK fails.
    ThrowOnNth::countdown = -1;
    neko::vector<ThrowOnNth> v;
    v.reserve(8);
    for (int i = 0; i < 3; ++i) v.emplace_back(i);

    ThrowOnNth::countdown = 2;  // the 3rd construction throws
    CHECK_THROWS_AS(v.resize(10), std::runtime_error);
    ThrowOnNth::countdown = -1;

    // The strong guarantee: on failure the vector is unchanged.
    CHECK_EQ(v.size(), 3u);
    CHECK_EQ(v[0].value, 0);
    CHECK_EQ(v[2].value, 2);
}
