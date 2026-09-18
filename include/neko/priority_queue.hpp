// ============================================================================
// neko/priority_queue.hpp
//
// Tests: tests/test_priority_queue.cpp
//
// A container *adaptor*, not a container. It owns some other container and
// restricts what you may do to it -- there is no begin(), no end(), no
// operator[], and top() hands back a const reference even on a non-const
// queue. Every one of those absences is deliberate: the class exists to keep
// the heap invariant true, and any of them would let you break it from
// outside.
//
// The implementation is thin to the point of feeling like a cheat. Every
// member below is one or two calls into algorithm.hpp's heap functions. That
// is the lesson: the data structure lives in the algorithms, and the adaptor
// only decides which to call. Write the heap functions first and this file
// takes twenty minutes.
//
// The surprise worth getting wrong once: the default Compare is less, and
// less produces a MAX-heap -- top() is the largest element. For a min-queue
// you pass greater<T>, which reads backwards until you notice that Compare
// answers "does a come before b in the ordering", and the heap keeps the
// element that comes *last* at the top.
// ============================================================================
#pragma once

#include <cstddef>

#include "neko/algorithm.hpp"
#include "neko/config.hpp"
#include "neko/functional.hpp"
#include "neko/utility.hpp"
#include "neko/vector.hpp"

namespace neko {

template <typename T, typename Container = vector<T>,
          typename Compare = less<typename Container::value_type>>
class priority_queue {
public:
    using container_type = Container;
    using value_compare = Compare;
    using value_type = typename Container::value_type;
    using size_type = typename Container::size_type;
    using reference = typename Container::reference;
    using const_reference = typename Container::const_reference;

    priority_queue() = default;

    explicit priority_queue(const Compare& compare) : comp(compare) {}

    // Takes a container that is not yet a heap and heapifies it. One
    // make_heap at O(n) rather than n push_heap calls at O(n log n) -- this
    // constructor is the reason make_heap exists as a separate function.
    priority_queue(const Compare& compare, Container cont) { NEKO_TODO(); }

    template <typename InputIt>
    priority_queue(InputIt first, InputIt last) {
        NEKO_TODO();
    }

    // No begin/end. A priority_queue is not iterable, and adding it would be
    // the one change that makes the invariant unenforceable.

    // const_reference on purpose, with no non-const overload: mutating the top
    // element in place would silently corrupt the heap.
    const_reference top() const { NEKO_TODO(); }

    bool empty() const { return c.empty(); }
    size_type size() const { return c.size(); }

    // Append, then sift the new element up into place.
    void push(const value_type& value) { NEKO_TODO(); }
    void push(value_type&& value) { NEKO_TODO(); }

    template <typename... Args>
    void emplace(Args&&... args) {
        NEKO_TODO();
    }

    // Two statements, and the split is the whole reason pop_heap does not pop:
    // pop_heap moves the top element to the back and repairs the heap over
    // what remains, then the container actually discards it.
    void pop() { NEKO_TODO(); }

    void swap(priority_queue& other) { NEKO_TODO(); }

protected:
    // protected, not private, and named c and comp -- both are what the
    // standard specifies, because a derived adaptor is expected to reach them.
    // This is the one place in the library where the standard hands a derived
    // class the representation on purpose.
    Container c;
    Compare comp;
};

// ============================================================================
// YOUR TURN
//
// Order: algorithm.hpp's push_heap, pop_heap and make_heap first -- none of
// this class can be tested before they work. Then top/push/pop, then the
// constructors, then emplace and swap.
//
// Afterwards:
//
//   stack<T, Container = deque<T>>    the trivial adaptor; write it to see how
//                                     little an adaptor really is
//   queue<T, Container = deque<T>>    and note why its default is deque and
//                                     not vector -- pop_front on a vector is
//                                     O(n)
//   sort_heap + is_heap / is_heap_until
//   a value-initialised Compare is assumed throughout; once you add
//   is_empty/EBO to type_traits.hpp, comp can occupy no space at all
// ============================================================================

}  // namespace neko
