// ============================================================================
// A runnable walk through raw storage and object lifetime.
//
//     ./t demo
//
// Not a test -- an explainer you can edit and re-run. Every constructor and
// destructor announces itself, so you can watch exactly when objects come into
// existence and when they stop existing, independently of when their memory is
// acquired and released.
//
// That independence is the whole subject. `new T` and `delete p` fuse the two,
// which is why vector cannot use them: vector owns storage for capacity()
// elements but objects for only size() of them.
// ============================================================================
#include <cstddef>
#include <cstdio>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

// A type with a real owned resource. The std::string matters: a type made only
// of ints hides every mistake in this file, because writing to an int that does
// not exist happens to do the right thing. Reaching into a std::string that
// does not exist does not.
struct Loud {
    int id;
    std::string payload;

    explicit Loud(int i) : id(i), payload("payload-" + std::to_string(i)) {
        std::printf("      + ctor      Loud(%d)   @ %p\n", id, self());
    }
    Loud(const Loud& o) : id(o.id), payload(o.payload) {
        std::printf("      + copy ctor Loud(%d)   @ %p\n", id, self());
    }
    Loud(Loud&& o) noexcept : id(o.id), payload(std::move(o.payload)) {
        std::printf("      + move ctor Loud(%d)   @ %p\n", id, self());
    }
    Loud& operator=(const Loud& o) {
        std::printf("      = copy assign Loud(%d) <- Loud(%d) @ %p\n", id, o.id,
                    self());
        id = o.id;
        payload = o.payload;
        return *this;
    }
    Loud& operator=(Loud&& o) noexcept {
        std::printf("      = move assign Loud(%d) <- Loud(%d) @ %p\n", id, o.id,
                    self());
        id = o.id;
        payload = std::move(o.payload);
        return *this;
    }
    ~Loud() { std::printf("      - dtor      Loud(%d)   @ %p\n", id, self()); }

    const void* self() const { return static_cast<const void*>(this); }
};

void heading(const char* text) { std::printf("\n\033[1m%s\033[0m\n", text); }

void note(const char* text) { std::printf("    %s\n", text); }

// --- helpers, exactly the ones suggested in vector.hpp ----------------------

Loud* allocate(std::size_t n) {
    // ::operator new is the *raw* one -- it returns bytes. The leading `::`
    // skips any class-specific operator new. It does not construct anything,
    // and it does not require Loud to be default-constructible.
    return static_cast<Loud*>(::operator new(n * sizeof(Loud)));
}

void deallocate(Loud* p) { ::operator delete(p); }

// ============================================================================
// 1. Storage is not objects
// ============================================================================
void storage_is_not_objects() {
    heading("1. Storage is not objects");

    Loud* buf = allocate(4);
    std::printf("    allocated %zu bytes at %p\n", 4 * sizeof(Loud),
                static_cast<const void*>(buf));
    note("Nothing was printed by a constructor, because no Loud exists yet.");
    note("There are 4 Loud-shaped *holes* here, and zero Loud objects.");
    note("buf[0] must not be read, written, or assigned to. Not yet.");

    deallocate(buf);
    note("Freed. No destructor ran either -- there was nothing to destroy.");
}

// ============================================================================
// 2. Placement new begins a lifetime; ~T() ends it
// ============================================================================
void placement_new_and_explicit_destructor() {
    heading("2. Placement new begins a lifetime; an explicit ~T() ends it");

    Loud* buf = allocate(1);

    // Placement new: "construct an object at an address I already own".
    // The static_cast<void*> selects the placement form of operator new.
    Loud* a = ::new (static_cast<void*>(buf)) Loud(1);
    std::printf("    a->id == %d, a->payload == \"%s\"\n", a->id,
                a->payload.c_str());

    // Explicit destructor call: the exact inverse of placement new. It ends
    // the object's lifetime and does NOT release the storage.
    a->~Loud();
    note("The object is gone. The 32-ish bytes are still mine.");

    // So the same storage can host a different object. Same address, different
    // lifetime -- these are two unrelated objects that happen to share a slot.
    Loud* b = ::new (static_cast<void*>(buf)) Loud(2);
    b->~Loud();

    deallocate(buf);
    note("Storage released. Constructions: 2. Destructions: 2. Always equal.");
}

// ============================================================================
// 3. The vector layout: objects in [begin_, end_), holes in [end_, cap_)
// ============================================================================
void the_two_regions() {
    heading("3. Two regions: constructed [begin_, end_), raw [end_, cap_)");

    constexpr std::size_t cap = 4;
    Loud* begin = allocate(cap);
    Loud* end = begin;

    // push_back: construct at end_, THEN advance. If the constructor throws,
    // end_ has not moved, so the vector never claims an object it does not
    // have. Advancing first would be a bug that only shows up under exceptions.
    for (int i = 0; i < 2; ++i) {
        ::new (static_cast<void*>(end)) Loud(i);
        ++end;
    }

    std::printf("    size == %td, capacity == %zu\n", end - begin, cap);
    note("Slots 0 and 1 hold objects. Slots 2 and 3 are raw memory.");
    note("at(2) must throw out_of_range even though the memory exists:");
    note("the bound is size(), never capacity().");

    // The destructor's job: destroy [begin_, end_) -- and only that. Running
    // destructors over the whole capacity would destroy two objects that were
    // never constructed.
    while (end != begin) (--end)->~Loud();
    deallocate(begin);
    note("Two destructors ran, not four.");
}

// ============================================================================
// 4. Relocation: reserve() constructs into the new buffer, it does not assign
// ============================================================================
void relocation() {
    heading("4. Relocation: reserve() constructs into the new buffer");

    Loud* old_begin = allocate(2);
    for (int i = 0; i < 2; ++i)
        ::new (static_cast<void*>(old_begin + i)) Loud(i);

    note("--- now reserve(4) ---");
    Loud* new_begin = allocate(4);

    // The new buffer is raw, so each element must be *constructed* there.
    // std::move here is what makes this a move construction rather than a copy
    // -- and it is safe to move only because Loud's move ctor is noexcept.
    // If it could throw, a throw halfway through would leave the old elements
    // gutted and the new ones incomplete, with no way back: that is why
    // vector::reserve must copy for a type like ThrowingMove.
    for (std::size_t i = 0; i < 2; ++i)
        ::new (static_cast<void*>(new_begin + i)) Loud(std::move(old_begin[i]));

    // The moved-from originals still exist. They must still be destroyed:
    // moving out of an object does not end its lifetime.
    for (std::size_t i = 0; i < 2; ++i) old_begin[i].~Loud();
    deallocate(old_begin);
    note("Old buffer released. Any iterator into it now dangles -- which is");
    note("exactly why reserve() invalidates iterators.");

    for (std::size_t i = 0; i < 2; ++i) new_begin[i].~Loud();
    deallocate(new_begin);
}

// ============================================================================
// 5. erase(): assignment over live objects, then destroy the vacated tail
// ============================================================================
void erase_is_assignment_then_destroy() {
    heading("5. erase(): shift by assignment, then destroy the tail");

    constexpr std::size_t n = 4;
    Loud* begin = allocate(n);
    for (int i = 0; i < 4; ++i) ::new (static_cast<void*>(begin + i)) Loud(i);
    Loud* end = begin + n;

    note("--- erase(begin) : remove Loud(0) ---");

    // Here is the subtlety that catches everyone. Slots 1..3 hold *live
    // objects*, so shifting them down is move-ASSIGNMENT, not construction --
    // there is already an object in slot 0 to assign over.
    for (Loud* p = begin; p + 1 != end; ++p) *p = std::move(*(p + 1));

    // After shifting, the last slot still holds a live (moved-from) object.
    // Nothing has been destroyed yet: assignment never ends a lifetime. So the
    // vacated tail slot must be destroyed explicitly, and end_ pulled back.
    --end;
    end->~Loud();

    std::printf("    size is now %td: ", end - begin);
    for (Loud* p = begin; p != end; ++p) std::printf("Loud(%d) ", p->id);
    std::printf("\n");
    note("Construct/destroy for raw slots; assign for live ones. Mixing them "
         "up");
    note("is the classic insert/erase bug -- and it leaks or double-frees.");

    while (end != begin) (--end)->~Loud();
    deallocate(begin);
}

// ============================================================================
// 6. When a constructor throws halfway through
// ============================================================================
struct ThrowsOnThird {
    static inline int made = 0;
    std::string payload = "resource";
    ThrowsOnThird() {
        if (++made == 3) throw std::runtime_error("boom");
    }
};

void cleanup_when_a_constructor_throws() {
    heading("6. When a constructor throws halfway through");

    ThrowsOnThird::made = 0;
    void* raw = ::operator new(5 * sizeof(ThrowsOnThird));
    ThrowsOnThird* buf = static_cast<ThrowsOnThird*>(raw);

    std::size_t built = 0;
    try {
        for (; built < 5; ++built)
            ::new (static_cast<void*>(buf + built)) ThrowsOnThird();
    } catch (const std::runtime_error&) {
        std::printf("    threw while building element %zu\n", built);

        // `built` counts only the elements that finished. The one that threw
        // never existed -- a constructor that exits by throwing leaves no
        // object behind, and its already-built members are unwound for you.
        // Everything before it, though, is yours to clean up.
        while (built > 0) buf[--built].~ThrowsOnThird();
        ::operator delete(raw);

        note("Destroyed the 2 completed elements and freed the buffer.");
        note("Skip this and it leaks silently: no CHECK fails, but ASan "
             "reports");
        note("it at exit. That is why the test suite runs under ASan.");
        return;
    }
    ::operator delete(raw);
}

}  // namespace

// ============================================================================
// 7. THE BUG. Flip to `#if 1`, run it, and read what ASan says.
// ============================================================================
#if 0
namespace {
void assigning_into_raw_storage() {
    heading("7. Assigning into raw storage");

    Loud* buf = allocate(1);

    // buf[0] is a hole, not an object. This calls Loud::operator=, which calls
    // std::string::operator= on a string that was never constructed. That
    // reads the left-hand side's internal pointer and capacity -- whatever
    // bytes happen to be in the slot -- and then copies or frees through them.
    // Here it dies inside memcpy:
    //
    //   ERROR: AddressSanitizer: SEGV on unknown address
    //     #4 std::__cxx11::basic_string<...>::operator=(basic_string&&)
    //     #5 operator=  lifetime_demo.cpp
    //
    // The exact symptom is luck -- a crash, a double free, or silent
    // corruption. Swap std::string for an int and it "works", which is the
    // trap: your vector will pass every int test and destroy every real type.
    //
    // The fix is not a different assignment. There is no object to assign to.
    // You must CONSTRUCT:  ::new (static_cast<void*>(buf)) Loud(1);
    buf[0] = Loud(1);

    buf[0].~Loud();
    deallocate(buf);
}
}  // namespace
#endif

int main() {
    storage_is_not_objects();
    placement_new_and_explicit_destructor();
    the_two_regions();
    relocation();
    erase_is_assignment_then_destroy();
    cleanup_when_a_constructor_throws();
#if 0
    assigning_into_raw_storage();
#endif
    std::printf("\n");
    return 0;
}
