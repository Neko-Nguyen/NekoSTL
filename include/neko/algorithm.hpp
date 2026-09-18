// ============================================================================
// neko/algorithm.hpp
//
// Tests: tests/test_algorithm.cpp
//
// The first module that is not a container. Everything here is a free function
// template over *iterators*, never over a container -- which is the whole point
// of the STL's shape: one copy() works on neko::array, neko::vector, a raw C
// array and a pointer into the middle of any of them, because none of them is
// named in the signature.
//
// Conventions that every function below follows:
//
//   [first, last)   half-open, as everywhere else. `last` is not dereferenced.
//                   An empty range is first == last, never a special case.
//
//   parameter names are the iterator category required: InputIt reads once
//                   forward, ForwardIt can be re-read, BidirIt can step back,
//                   RandomAccessIt can jump. Nothing enforces this yet -- the
//                   names are the contract until the module grows real
//                   iterator_traits and concepts.
//
//   no allocation, no bounds checks. A destination is an iterator, not a range:
//                   the caller guarantees there is room. This is exactly why
//                   back_inserter exists in the real library.
//
//   return values are load-bearing, not decoration. copy() hands back the new
//                   end of the destination so a second copy() can start there.
//
// Stubs use NEKO_TODO() and so are deliberately *not* marked constexpr or
// noexcept -- a constexpr function that can only ever throw is ill-formed.
// Add both back as you implement each one; array.hpp's comparison operators
// are constexpr and will need these to be too.
// ============================================================================
#pragma once

#include "neko/config.hpp"
#include "neko/type_traits.hpp"
#include "neko/utility.hpp"

namespace neko {

// ---------------------------------------------------------------------------
// Comparing ranges
//
// These two are what array.hpp's hand-written operator== and operator< become
// once this header works.
// ---------------------------------------------------------------------------

// Element-wise ==. The three-argument form trusts the caller that the second
// range has at least as many elements as the first; it cannot check, because
// it was never told where the second range ends.
template <typename InputIt1, typename InputIt2>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
    NEKO_TODO();
}

// The four-argument form knows both ends, so ranges of different length are
// simply not equal rather than undefined behaviour. C++14 added it for that
// reason alone.
template <typename InputIt1, typename InputIt2>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2) {
    NEKO_TODO();
}

// Dictionary order: the first position where the ranges differ decides, and if
// one range runs out first it is the smaller. Note what this must *not* do --
// compare lengths first. {2} is greater than {1, 1, 1}.
//
// Only `<` may be used on the elements. Deriving the other five comparisons
// from `<` alone is the same discipline neko::swap relies on: require the
// minimum from T.
template <typename InputIt1, typename InputIt2>
bool lexicographical_compare(InputIt1 first1, InputIt1 last1, InputIt2 first2,
                             InputIt2 last2) {
    NEKO_TODO();
}

// ---------------------------------------------------------------------------
// Copying
// ---------------------------------------------------------------------------

// Returns the end of the written range: d_first + (last - first).
template <typename InputIt, typename OutputIt>
OutputIt copy(InputIt first, InputIt last, OutputIt d_first) {
    NEKO_TODO();
}

// Count-driven rather than end-driven. Size is a template parameter so a plain
// int, a std::size_t or a difference_type all work without a conversion
// warning at the call site.
template <typename InputIt, typename Size, typename OutputIt>
OutputIt copy_n(InputIt first, Size count, OutputIt d_first) {
    NEKO_TODO();
}

// Copies right-to-left into a range that *ends* at d_last, and returns the
// beginning of what it wrote. The direction is not a style choice: it is the
// only way to shift elements towards higher addresses without overwriting
// source elements you have not read yet. vector::insert needs this.
template <typename BidirIt1, typename BidirIt2>
BidirIt2 copy_backward(BidirIt1 first, BidirIt1 last, BidirIt2 d_last) {
    NEKO_TODO();
}

// ---------------------------------------------------------------------------
// Moving
//
// Careful: `neko::move` already names the cast in utility.hpp. These are a
// different function with the same name, distinguished only by arity -- one
// argument is the cast, three is the range algorithm. The standard library has
// exactly this collision between <utility> and <algorithm> and lives with it.
//
// After these run, the source range is valid but unspecified: the elements are
// still there and still destructible, but you may not assume what they hold.
// ---------------------------------------------------------------------------

template <typename InputIt, typename OutputIt>
OutputIt move(InputIt first, InputIt last, OutputIt d_first) {
    NEKO_TODO();
}

template <typename BidirIt1, typename BidirIt2>
BidirIt2 move_backward(BidirIt1 first, BidirIt1 last, BidirIt2 d_last) {
    NEKO_TODO();
}

// ---------------------------------------------------------------------------
// Filling
// ---------------------------------------------------------------------------

// T is deduced independently of whatever the iterators point at, so filling a
// range of long with the literal 7 compiles. Taken by const& rather than by
// value so a heavy T is not copied once per call.
template <typename ForwardIt, typename T>
void fill(ForwardIt first, ForwardIt last, const T& value) {
    NEKO_TODO();
}

template <typename OutputIt, typename Size, typename T>
OutputIt fill_n(OutputIt first, Size count, const T& value) {
    NEKO_TODO();
}

// ---------------------------------------------------------------------------
// Swapping
// ---------------------------------------------------------------------------

// Two iterators, not two values -- the indirection is the point. The two
// iterator types may differ as long as the elements are swappable.
template <typename ForwardIt1, typename ForwardIt2>
void iter_swap(ForwardIt1 a, ForwardIt2 b) {
    NEKO_TODO();
}

// Returns one past the last element swapped in the second range. array::swap
// is this function.
template <typename ForwardIt1, typename ForwardIt2>
ForwardIt2 swap_ranges(ForwardIt1 first1, ForwardIt1 last1, ForwardIt2 first2) {
    NEKO_TODO();
}

// ---------------------------------------------------------------------------
// Minimum and maximum
//
// Returning const& rather than by value is what makes `const int& m =
// neko::min(a, b)` bind to `a` itself. It is also the classic dangling trap:
// neko::min(x, y + 1) returns a reference that may outlive the temporary.
//
// Both must return the *first* argument when the two compare equal. That is
// what makes a sort built on them stable, and it is why the body is written
// against `<` in one direction only rather than the intuitive one.
// ---------------------------------------------------------------------------

template <typename T>
const T& min(const T& a, const T& b) {
    NEKO_TODO();
}

template <typename T>
const T& max(const T& a, const T& b) {
    NEKO_TODO();
}

// ---------------------------------------------------------------------------
// Heap operations
//
// A heap here is not a memory region -- it is an ordering on a random-access
// range. [first, last) is a max-heap when no element compares less than its
// two children at 2i+1 and 2i+2. That index arithmetic is the whole trick: a
// complete binary tree stored in a flat array with no pointers at all.
//
// The invariant is weaker than sorted, which is exactly why it is useful:
// restoring it after one insertion or removal costs O(log n) instead of
// O(n log n), and building it from scratch costs O(n), not O(n log n).
//
// neko::priority_queue is these four functions and nothing else.
//
// Each comes in two forms: the default uses `<` on the elements, the other
// takes a comparator. Two parameters versus three, so no ambiguity here --
// unlike equal(), below.
// ---------------------------------------------------------------------------

// Precondition: [first, last - 1) is already a heap and the new element sits
// at last - 1. Restores the heap over the whole range by moving that one
// element up. This is the sift-up direction.
template <typename RandomAccessIt>
void push_heap(RandomAccessIt first, RandomAccessIt last) {
    NEKO_TODO();
}
template <typename RandomAccessIt, typename Compare>
void push_heap(RandomAccessIt first, RandomAccessIt last, Compare comp) {
    NEKO_TODO();
}

// Does not remove anything -- it moves the largest element to last - 1 and
// restores the heap over [first, last - 1). The caller pops it afterwards.
// That split is why priority_queue::pop() is two statements.
template <typename RandomAccessIt>
void pop_heap(RandomAccessIt first, RandomAccessIt last) {
    NEKO_TODO();
}
template <typename RandomAccessIt, typename Compare>
void pop_heap(RandomAccessIt first, RandomAccessIt last, Compare comp) {
    NEKO_TODO();
}

// Heapifies an arbitrary range. Doing this bottom-up is O(n); repeatedly
// calling push_heap from the front is O(n log n). The difference is worth
// deriving on paper once -- most of the nodes are near the leaves and sift
// down almost no distance.
template <typename RandomAccessIt>
void make_heap(RandomAccessIt first, RandomAccessIt last) {
    NEKO_TODO();
}
template <typename RandomAccessIt, typename Compare>
void make_heap(RandomAccessIt first, RandomAccessIt last, Compare comp) {
    NEKO_TODO();
}

// Turns a heap into a sorted range, ascending. Precondition: the range is
// already a heap -- this is the second half of heapsort, and it is a loop over
// pop_heap with nothing else in it.
template <typename RandomAccessIt>
void sort_heap(RandomAccessIt first, RandomAccessIt last) {
    NEKO_TODO();
}
template <typename RandomAccessIt, typename Compare>
void sort_heap(RandomAccessIt first, RandomAccessIt last, Compare comp) {
    NEKO_TODO();
}

// ============================================================================
// YOUR TURN
//
// Add these as you need them; tests/test_algorithm.cpp has a section for each.
//
//   the predicate overloads
//       equal(first1, last1, first2, pred) and
//       lexicographical_compare(first1, last1, first2, last2, comp), plus
//       copy_if / find_if / fill's cousin generate.
//       Note the trap before you write them: equal-with-predicate takes four
//       arguments and so does equal-with-two-ranges. Overload resolution
//       cannot tell them apart on arity, and the standard disambiguates with
//       constraints on whether the fourth parameter is an iterator. You have
//       the detection idiom in type_traits.hpp already.
//
//   find / find_if / count / count_if
//   all_of / any_of / none_of      (all three are one loop and a negation)
//   transform                      (the unary and binary forms)
//   remove / remove_if             remove-erase: they do not erase anything,
//                                  they partition and hand you the new end
//   reverse / rotate
//   min_element / max_element      the range forms, returning iterators
//
// Then go back to array.hpp and replace the loop bodies in operator== and
// operator< with neko::equal and neko::lexicographical_compare.
// ============================================================================

}  // namespace neko
