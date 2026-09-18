// An instrumented value type. Containers are easy to get *looking* right and
// still wrong underneath -- a copy where a move was promised, a missing
// destructor call, an element left constructed in raw storage. Tracker counts
// every operation so tests can assert on them.
//
//   Tracker::reset();
//   { neko::vector<Tracker> v; v.push_back(Tracker{1}); }
//   CHECK_EQ(Tracker::alive, 0);      // no leaks
//   CHECK_EQ(Tracker::moves, 1);      // and it moved rather than copied
#pragma once

namespace neko_test {

struct Tracker {
    static inline int ctors = 0;
    static inline int copies = 0;
    static inline int moves = 0;
    static inline int dtors = 0;
    static inline int alive = 0;

    static void reset() { ctors = copies = moves = dtors = alive = 0; }

    int value = 0;
    bool moved_from = false;

    Tracker() { ++ctors, ++alive; }
    explicit Tracker(int v) : value(v) { ++ctors, ++alive; }

    Tracker(const Tracker& o) : value(o.value) { ++copies, ++alive; }
    Tracker(Tracker&& o) noexcept : value(o.value) {
        o.moved_from = true;
        ++moves, ++alive;
    }

    Tracker& operator=(const Tracker& o) {
        value = o.value;
        ++copies;
        return *this;
    }
    Tracker& operator=(Tracker&& o) noexcept {
        value = o.value;
        o.moved_from = true;
        ++moves;
        return *this;
    }

    ~Tracker() { ++dtors, --alive; }

    friend bool operator==(const Tracker& a, const Tracker& b) {
        return a.value == b.value;
    }
};

// Same idea, but its move constructor may throw -- use it to check that
// vector::reserve falls back to copying for types like this.
struct ThrowingMove {
    int value = 0;
    explicit ThrowingMove(int v = 0) : value(v) {}
    ThrowingMove(const ThrowingMove&) = default;
    ThrowingMove(ThrowingMove&& o) : value(o.value) {}  // note: NOT noexcept
    ThrowingMove& operator=(const ThrowingMove&) = default;
    ThrowingMove& operator=(ThrowingMove&&) { return *this; }
};

}  // namespace neko_test
