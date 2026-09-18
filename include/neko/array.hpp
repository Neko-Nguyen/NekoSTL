// ============================================================================
// neko/array.hpp  --  WORKED REFERENCE MODULE
//
// The simplest real container: a fixed-size aggregate. No allocation, no
// growth, no exception-safety puzzles -- so it is the cleanest place to see
// the container *interface* conventions that every other container repeats:
// the nested typedefs, the const/non-const member pairs, iterators as a
// [begin, end) half-open range, and checked vs unchecked element access.
//
// Deliberately an aggregate (public member, no user-declared constructors),
// exactly like std::array, so that `array<int,3> a{1,2,3}` works via aggregate
// initialisation rather than an initializer_list constructor.
// ============================================================================
#pragma once

#include <cstddef>
#include <stdexcept>

#include "neko/type_traits.hpp"
#include "neko/utility.hpp"

namespace neko {

template <typename T, std::size_t N>
struct array {
    // --- member types -------------------------------------------------------
    // Every standard container publishes these. Generic algorithms ask the
    // container for them instead of assuming raw pointers.
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;  // a raw pointer is a valid random-access iterator
    using const_iterator = const T*;

    // Public, and named with a trailing underscore only by convention -- it is
    // part of the aggregate, so aggregate init writes straight into it.
    T elems_[N];

    // --- element access -----------------------------------------------------
    constexpr reference operator[](size_type i) { return elems_[i]; }
    constexpr const_reference operator[](size_type i) const {
        return elems_[i];
    }

    constexpr reference at(size_type i) {
        if (i >= N) throw std::out_of_range("neko::array::at");
        return elems_[i];
    }
    constexpr const_reference at(size_type i) const {
        if (i >= N) throw std::out_of_range("neko::array::at");
        return elems_[i];
    }

    constexpr reference front() { return elems_[0]; }
    constexpr const_reference front() const { return elems_[0]; }
    constexpr reference back() { return elems_[N - 1]; }
    constexpr const_reference back() const { return elems_[N - 1]; }

    constexpr pointer data() noexcept { return elems_; }
    constexpr const_pointer data() const noexcept { return elems_; }

    // --- iterators ----------------------------------------------------------
    constexpr iterator begin() noexcept { return elems_; }
    constexpr const_iterator begin() const noexcept { return elems_; }
    constexpr const_iterator cbegin() const noexcept { return elems_; }
    constexpr iterator end() noexcept { return elems_ + N; }
    constexpr const_iterator end() const noexcept { return elems_ + N; }
    constexpr const_iterator cend() const noexcept { return elems_ + N; }

    // --- capacity -----------------------------------------------------------
    constexpr size_type size() const noexcept { return N; }
    constexpr size_type max_size() const noexcept { return N; }
    [[nodiscard]] constexpr bool empty() const noexcept { return N == 0; }

    // --- operations ---------------------------------------------------------
    constexpr void fill(const T& value) {
        for (size_type i = 0; i < N; ++i) elems_[i] = value;
    }

    constexpr void swap(array& other) {
        for (size_type i = 0; i < N; ++i)
            neko::swap(elems_[i], other.elems_[i]);
    }
};

// Lexicographical comparison, written out by hand here; once you have written
// the <algorithm> versions, replace these bodies with calls to neko::equal and
// neko::lexicographical_compare.
template <typename T, std::size_t N>
constexpr bool operator==(const array<T, N>& a, const array<T, N>& b) {
    for (std::size_t i = 0; i < N; ++i)
        if (!(a[i] == b[i])) return false;
    return true;
}

template <typename T, std::size_t N>
constexpr bool operator!=(const array<T, N>& a, const array<T, N>& b) {
    return !(a == b);
}

template <typename T, std::size_t N>
constexpr bool operator<(const array<T, N>& a, const array<T, N>& b) {
    for (std::size_t i = 0; i < N; ++i) {
        if (a[i] < b[i]) return true;
        if (b[i] < a[i]) return false;
    }
    return false;
}

}  // namespace neko
