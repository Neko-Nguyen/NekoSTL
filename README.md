# neko

Reimplementing the C++ standard library from scratch, one piece at a time.
Header-only, C++20, no dependencies.

## Build and test

```sh
./t                     # build everything, run everything
./t utility             # just test_utility
./t utility swap        # just the tests whose name contains "swap"
./t --release           # no sanitizers, -O2, for when you care about timings
./t --clean             # blow away the build directory

./t demo                # runnable walk through raw storage and object lifetime
```

`./t` is a wrapper around CMake; `cmake -S . -B build && cmake --build build &&
ctest --test-dir build` works too. Debug + AddressSanitizer + UndefinedBehavior-
Sanitizer are on by default, which is what you want while writing containers:
several tests are really leak tests and only fail through ASan.

`compile_commands.json` is generated in `build/` for clangd. Symlink it to the
root if your editor wants it there:

```sh
ln -sf build/compile_commands.json .
```

## How the tests work

Unimplemented functions have `NEKO_TODO()` as their body, which throws. The
runner catches that and reports the test as **TODO** instead of **FAIL**, so an
unfinished module reads as a work queue rather than a wall of red:

```
  PASS  move_has_the_right_return_type
  TODO  move_selects_the_move_constructor        (not implemented: move)
  FAIL  swap_exchanges_two_values
```

Only `FAIL` sets a non-zero exit status. Delete the `NEKO_TODO()` line as you
write each body.

Note the split in every module: tests that only check *types* (`STATIC_CHECK`)
pass from the start, because the signatures are already written for you — they
are the specification. Tests that check *behaviour* are the ones you turn green.

The test framework is `tests/test_framework.hpp` (~150 lines): `NEKO_TEST`,
`CHECK`, `REQUIRE`, `CHECK_EQ`, `REQUIRE_EQ`, `CHECK_THROWS_AS`, `STATIC_CHECK`.
`tests/tracker.hpp` has `Tracker`, an instrumented value type that counts
constructions, copies, moves and destructions — that is how the tests tell a
move from a copy, and how they catch leaked or doubly-destroyed elements.

## Layout

```
include/neko/
  config.hpp        NEKO_TODO and the not_implemented exception
  type_traits.hpp   worked reference module, plus a "YOUR TURN" list
  utility.hpp       move / forward / swap / exchange        <- start here
  array.hpp         worked reference module
  vector.hpp        skeleton only
tests/
  test_*.cpp        one executable per module
```

## Order of work

1. **`utility.hpp`** — `move`, `forward`, `swap`, `exchange`. Four lines each and
   the foundation for everything else. `array::swap` and every container's move
   constructor depend on them.
2. **`vector.hpp`** — the suggested order is in the header comment, and the tests
   in `test_vector.cpp` are grouped to match it. Roughly: capacity → `reserve` +
   `push_back` → access → copy → move → `emplace_back` → `resize` → `insert`/
   `erase` → `swap`.
3. **`type_traits.hpp`** — add traits from the "YOUR TURN" list as you need them.
   `test_type_traits.cpp` has a test for each, inside an `#if 0` block. These are
   compile-time tests, so an unimplemented trait is a *build* error rather than a
   TODO: open one block at a time.

The traits in step 3 are not optional extras — `vector::reserve` needs
`is_nothrow_move_constructible` to decide whether it may move the old elements
or must copy them.

## Next, once vector works

`<algorithm>` basics (`copy`, `move`, `fill`, `equal`, `lexicographical_compare`)
and use them to replace the hand-written loops in `array.hpp`; then iterator
traits and reverse iterators; then `unique_ptr`; then thread an `Allocator`
through `vector`. Doing the allocator *second* is deliberate — it is much
clearer what an allocator is for once you have written the raw-storage version
by hand.
