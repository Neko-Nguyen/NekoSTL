// ============================================================================
// neko/functional.hpp
//
// Tests: tests/test_priority_queue.cpp (exercised through the adaptor)
//
// Function objects: types whose instances are callable. The reason the STL
// takes a Compare *type* rather than a function pointer is that a stateless
// struct with an inline operator() costs nothing -- the call is inlined away
// and the object occupies no space in the container that holds it. A function
// pointer can be neither.
// ============================================================================
#pragma once

#include "neko/config.hpp"
#include "neko/utility.hpp"

namespace neko {

// The default comparator for every ordered thing in the library. Note it is
// the *only* comparison any of them may use: priority_queue, sort and map all
// express "equivalent" as !comp(a, b) && !comp(b, a) rather than asking T for
// an operator==.
template <typename T>
struct less {
    constexpr bool operator()(const T& a, const T& b) const { return a < b; }
};

template <typename T>
struct greater {
    constexpr bool operator()(const T& a, const T& b) const { return b < a; }
};

// ============================================================================
// YOUR TURN
//
//   less_equal / greater_equal / equal_to / not_equal_to
//   plus / minus / multiplies / negate       (for transform)
//   logical_and / logical_or / logical_not
//
//   the void specialisations -- less<void>, with a templated operator() that
//   deduces both sides. This is what lets neko::less<>{} compare an int to a
//   long without converting either. Do it after you have decltype-based
//   return type deduction straight.
//
//   reference_wrapper / ref / cref           (needs invoke_result)
//   function                                 (type erasure -- a project in
//                                             its own right, leave it last)
// ============================================================================

}  // namespace neko
