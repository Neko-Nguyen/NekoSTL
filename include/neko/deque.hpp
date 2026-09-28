// ============================================================================
// neko/deque.hpp
//
// Tests: tests/test_deque.cpp
//
// The container that breaks the assumptions vector let you keep.
//
// vector is one contiguous block, so `T*` is a perfectly good iterator and
// every reallocation invalidates everything. deque is a *map* -- an array of
// pointers -- each entry owning one fixed-size block of elements:
//
//     map_:  [ . ][ . ][ . ][ . ]
//              |    |    |    |
//              v    v    v    v
//            block block block block          each holds block_size() elements
//            .....  ####  ####  ##...         # = a live element
//
// Three consequences, and they are the reason this container exists:
//
//   1. push_front is O(1). There is always room at the low end, or a new
//      block is added there; nothing else moves.
//
//   2. push_front/push_back invalidate iterators but NOT references. Growth
//      reallocates the *map*, never the blocks, so an existing element never
//      changes address. No other sequence container offers this, and it is
//      why std::queue adapts deque rather than vector.
//
//   3. iterator can no longer be T*. A pointer cannot know it has run off the
//      end of its block and must consult the map for the next one. So this
//      module is where you write a real iterator class -- and where the
//      library starts needing iterator_traits, because algorithm.hpp can no
//      longer assume `T*`.
//
// Storage is uninitialised: blocks are raw memory and elements are placement-
// newed into them, exactly as in vector.hpp. Reuse that discipline.
//
// The stubs are not marked noexcept even where the finished function will be
// (the move constructor, swap, size, begin/end). NEKO_TODO() throws, and a
// throw out of a noexcept function is std::terminate, not a caught TODO -- the
// test runner would die instead of reporting. Put noexcept back on as each one
// stops throwing.
// ============================================================================
#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>

#include "neko/config.hpp"
#include "neko/type_traits.hpp"
#include "neko/utility.hpp"

namespace neko {

// How many T fit in one block. The 512-byte target is what libstdc++ uses: big
// enough that crossing a block boundary is rare, small enough that a deque of
// something large does not allocate 512 bytes to hold one element. Always at
// least 1, or a deque of a huge T could not store anything.
template <typename T>
constexpr std::size_t deque_block_size() {
    return sizeof(T) < 512 ? 512 / sizeof(T) : 1;
}

// ---------------------------------------------------------------------------
// The iterator
//
// Four pointers, and each one earns its place:
//
//   cur    the element this iterator denotes
//   first  start of cur's block          )  the two bounds that tell ++ and --
//   last   one past the end of cur's block)  when to hop to another block
//   node   the map entry pointing at that block -- the only way to find the
//          neighbouring block, since blocks are not adjacent in memory
//
// Ref and Ptr are template parameters so that one class body produces both
// iterator (T&, T*) and const_iterator (const T&, const T*). Writing the
// const version out twice is the alternative, and it rots.
// ---------------------------------------------------------------------------
template <typename T, typename Ref, typename Ptr>
struct deque_iterator {
    using value_type = T;
    using reference = Ref;
    using pointer = Ptr;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using self = deque_iterator;

    // Not yet used -- iterator_traits does not exist in this library. Add the
    // tag types alongside it and name random_access_iterator_tag here; that is
    // what lets algorithm.hpp pick the O(1) path for distance() and advance().
    // using iterator_category = random_access_iterator_tag;

    T* cur = nullptr;
    T* first = nullptr;
    T* last = nullptr;
    T** node = nullptr;

    deque_iterator() = default;

    deque_iterator(const self<T, T&, T*>& other)
        : cur(other.cur),
          first(other.first),
          last(other.last),
          node(other.node) {}

    void set_node(T** n) {
        node = n;
        first = *n;
        last = first + deque_block_size<T>();
    }

    Ref operator*() const {
        return *cur;
    }
    Ptr operator->() const {
        return cur;
    }

    self& operator++() {
        if (++cur == last) {
            set_node(node + 1);
            cur = first;
        }
        return *this;
    }
    self operator++(int) {
        self temp = *this;
        if (++cur == last) {
            set_node(node + 1);
            cur = first;
        }
        return temp;
    }
    self& operator--() {
        if (cur-- == first) {
            set_node(node - 1);
            cur = last - 1;
        }
        return *this;
    }
    self operator--(int) {
        self temp = *this;
        if (cur-- == first) {
            set_node(node - 1);
            cur = last - 1;
        }
        return temp;
    }

    self& operator+=(difference_type n) {
        const difference_type bsize = deque_block_size<T>();
        const difference_type offset = n + (cur - first);
        
        if (offset >= 0 && offset < bsize) {
            cur = first + offset;
        } else {
            const difference_type node_offset = offset >= 0
                    ? offset / bsize
                    : (offset + 1) / bsize - 1;
            set_node(node + node_offset);
            cur = first + (offset - node_offset * bsize);
        }
        return *this;
    }
    self& operator-=(difference_type n) {
        *this += -n;
        return *this;
    }
    self operator+(difference_type n) const {
        self temp = *this;
        temp += n;
        return temp;
    }
    self operator-(difference_type n) const {
        return *this + (-n);
    }
    Ref operator[](difference_type n) const {
        return *(*this + n).cur;
    }

    difference_type operator-(const self& other) const {
        if (node == other.node) {
            return cur - other.cur;
        }

        const difference_type bsize = deque_block_size<T>();
        const difference_type node_offset = node - other.node;
        if (node_offset < 0) {
            return - (other - *this);
        }

        const difference_type first_part = last - cur;
        const difference_type mid_part = (node_offset - 1) * bsize;
        const difference_type last_part = other.cur - other.first;
        return first_part + mid_part + last_part;
    }

    bool operator==(const self& other) const { return cur == other.cur; }
    bool operator!=(const self& other) const { return cur != other.cur; }
    bool operator<(const self& other) const {
        return node == other.node ? cur < other.cur : node < other.node;
    }
};

// ---------------------------------------------------------------------------
// The container
// ---------------------------------------------------------------------------
template <typename T>
class deque {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = deque_iterator<T, T&, T*>;
    using const_iterator = deque_iterator<T, const T&, const T*>;

    // --- construction / destruction ----------------------------------------
    // Note that an empty deque is not empty of storage: start_ and finish_ must
    // both denote a real position, so one block is allocated up front. That is
    // the price of push_front and push_back both being O(1) with no branch for
    // "no blocks yet".
    deque() { NEKO_TODO(); }
    explicit deque(size_type /*count*/) { NEKO_TODO(); }
    deque(size_type /*count*/, const T& /*value*/) { NEKO_TODO(); }

    deque(const deque& /*other*/) { NEKO_TODO(); }
    deque(deque&& /*other*/) { NEKO_TODO(); }
    deque& operator=(const deque& /*other*/) { NEKO_TODO(); }
    deque& operator=(deque&& /*other*/) { NEKO_TODO(); }
    ~deque() {}  // destroy every element, then free every block, then the map

    // --- element access ----------------------------------------------------
    // Both are O(1). operator[] is iterator arithmetic, not a single add --
    // this is where the cost of the block layout actually shows up.
    reference operator[](size_type /*i*/) { NEKO_TODO(); }
    const_reference operator[](size_type /*i*/) const { NEKO_TODO(); }
    reference at(size_type /*i*/) { NEKO_TODO(); }
    const_reference at(size_type /*i*/) const { NEKO_TODO(); }

    reference front() { NEKO_TODO(); }
    const_reference front() const { NEKO_TODO(); }
    reference back() { NEKO_TODO(); }
    const_reference back() const { NEKO_TODO(); }

    // There is no data(). A deque is not contiguous, so the question has no
    // answer -- the absence is part of the interface.

    // --- iterators ---------------------------------------------------------
    iterator begin() { NEKO_TODO(); }
    const_iterator begin() const { NEKO_TODO(); }
    const_iterator cbegin() const { NEKO_TODO(); }
    iterator end() { NEKO_TODO(); }
    const_iterator end() const { NEKO_TODO(); }
    const_iterator cend() const { NEKO_TODO(); }

    // --- capacity ----------------------------------------------------------
    size_type size() const { NEKO_TODO(); }
    bool empty() const { NEKO_TODO(); }

    // No capacity() and no reserve(). deque does not over-allocate at the
    // element level, so there is nothing to report or reserve.

    // --- modifiers ---------------------------------------------------------
    // The pair that justifies the whole container. Each is O(1) amortised and
    // leaves every existing element at the same address.
    void push_back(const T& /*value*/) { NEKO_TODO(); }
    void push_back(T&& /*value*/) { NEKO_TODO(); }
    void push_front(const T& /*value*/) { NEKO_TODO(); }
    void push_front(T&& /*value*/) { NEKO_TODO(); }

    template <typename... Args>
    reference emplace_back(Args&&... /*args*/) {
        NEKO_TODO();
    }
    template <typename... Args>
    reference emplace_front(Args&&... /*args*/) {
        NEKO_TODO();
    }

    void pop_back() { NEKO_TODO(); }
    void pop_front() { NEKO_TODO(); }

    void clear() { NEKO_TODO(); }

    // O(1) and noexcept: three scalars change hands and not one element moves.
    // Contrast array::swap, which is O(N) because an array owns its storage
    // directly.
    void swap(deque& /*other*/) { NEKO_TODO(); }

private:
    // map_ is itself a dynamically sized array of block pointers. When it
    // fills, it is reallocated and the existing pointers are re-centred in the
    // new one -- centring is what keeps both ends cheap.
    T** map_ = nullptr;
    size_type map_size_ = 0;
    iterator start_;   // first element
    iterator finish_;  // one past the last element

    // Suggested helpers. Writing these first makes the public members short:
    //
    T* allocate_block() {

    }
    //   void deallocate_block(T*)
    //   void initialise_map(size_type)  map + enough blocks for n elements
    //   void reserve_map_at_back(n)     grow/re-centre the map if needed
    //   void reserve_map_at_front(n)
    //   void reallocate_map(nodes_to_add, at_front)
};

// ============================================================================
// YOUR TURN
//
// The order below is the one the tests are grouped in -- each step is testable
// before the next exists.
//
//   1. deque_block_size and the iterator's set_node / operator* / ++ / --.
//      Construct iterators by hand in a test first; you do not need the
//      container to check that ++ hops blocks correctly.
//   2. initialise_map, the default constructor, begin/end, size, empty.
//   3. push_back / pop_back, then push_front / pop_front. Keep the map fixed
//      size at first and only handle growth within the blocks you have.
//   4. reallocate_map, so push_back can run off the end of the map.
//   5. operator[] / at / front / back, which is iterator arithmetic once
//      operator+= is right.
//   6. copy, move, swap, clear, the destructor.
//
// Then, once the module is real:
//
//   insert / erase        -- deque's trick is to shift towards whichever end
//                            is nearer, so at most half the elements move
//   resize
//   the comparison operators, via neko::lexicographical_compare
//   iterator_traits + the iterator category tags, then reverse_iterator --
//                            deque is the container that forces the issue
//   stack and queue       -- both adapt deque, and both are about forty lines
//                            once this works
// ============================================================================

}  // namespace neko
