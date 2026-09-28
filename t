#!/usr/bin/env bash
# Build and run the neko tests.
#
#   ./t                     configure if needed, build everything, run everything
#   ./t vector              build + run only test_vector
#   ./t vector reserve      ...and only the tests whose name contains "reserve"
#   ./t --release           build without sanitizers, -O2 (for timing work)
#   ./t --clean             delete the build directory and start over
#   ./t fmt                 clang-format every header and test in place
#   ./t fmt --check         report unformatted files without touching them
#
# Exit status is the test status, so this is usable in a watch loop:
#   while inotifywait -qqre modify include tests; do ./t vector; done
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR=build
CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Debug -DNEKO_SANITIZE=ON)

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)
            rm -rf "$BUILD_DIR"
            echo "removed $BUILD_DIR/"
            shift
            ;;
        --release)
            BUILD_DIR=build-release
            CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEKO_SANITIZE=OFF)
            shift
            ;;
        --*)
            echo "unknown option: $1" >&2
            exit 2
            ;;
        *)
            break
            ;;
    esac
done

MODULE="${1:-}"
FILTER="${2:-}"

# `./t fmt` -- formatting is not a build target, so handle it before cmake.
if [[ "$MODULE" == "fmt" ]]; then
    CF="${CLANG_FORMAT:-clang-format}"
    if ! command -v "$CF" > /dev/null; then
        echo "$CF not found (try: sudo apt install clang-format)" >&2
        exit 127
    fi
    mapfile -t files < <(git ls-files '*.hpp' '*.cpp' 2>/dev/null)
    if [[ ${#files[@]} -eq 0 ]]; then
        # Not a git checkout -- fall back to walking the source dirs.
        mapfile -t files < <(find include tests -name '*.hpp' -o -name '*.cpp')
    fi
    if [[ "$FILTER" == "--check" ]]; then
        "$CF" --dry-run --Werror "${files[@]}"
        exit $?
    fi
    "$CF" -i "${files[@]}" || exit 1
    echo "formatted ${#files[@]} files"
    exit 0
fi

# The set of test targets is a glob, so it changes under us every time a branch
# switch swaps one module's test file for another's. CONFIGURE_DEPENDS re-runs
# that glob, but only as a prerequisite of a target that is already in the
# generated build system -- and `./t deque` asks make for test_deque by name,
# which a stale build system cannot even parse. So look for the drift out here,
# where nothing has to exist first.
test_sources() { printf '%s\n' tests/test_*.cpp | sort; }

STAMP="$BUILD_DIR/.neko-test-sources"
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]] || ! test_sources | cmp -s - "$STAMP"; then
    cmake -S . -B "$BUILD_DIR" "${CMAKE_ARGS[@]}" > /dev/null || exit 1
    # Re-configuring drops the departed targets but leaves their executables
    # sitting in the build tree for the run loop to find. Those were built
    # against headers this branch does not have; passing green is the worst
    # thing they could do.
    for bin in "$BUILD_DIR"/tests/test_*; do
        [[ -f "$bin" ]] || continue
        [[ -f "tests/$(basename "$bin").cpp" ]] || rm -f "$bin"
    done
    test_sources > "$STAMP"
fi

# Report leaks *and* keep going to the end of the run, so one bad test does not
# hide the rest. abort_on_error=0 keeps UBSan diagnostics non-fatal too.
export ASAN_OPTIONS="detect_leaks=1:abort_on_error=0:${ASAN_OPTIONS:-}"
export UBSAN_OPTIONS="print_stacktrace=1:${UBSAN_OPTIONS:-}"

if [[ -n "$MODULE" ]]; then
    # `./t demo` runs the object-lifetime explainer; everything else is a test.
    if [[ "$MODULE" == "demo" ]]; then
        target="lifetime_demo"
    else
        target="test_$MODULE"
    fi
    cmake --build "$BUILD_DIR" --target "$target" -j || exit 1
    echo
    "$BUILD_DIR/tests/$target" $FILTER
    exit $?
fi

cmake --build "$BUILD_DIR" -j || exit 1

# Driven by the sources, not by whatever executables happen to be lying in the
# build tree, so a leftover from another branch can never join the run.
status=0
for src in tests/test_*.cpp; do
    name="$(basename "$src" .cpp)"
    [[ "$name" == "test_main" ]] && continue
    bin="$BUILD_DIR/tests/$name"
    [[ -x "$bin" && -f "$bin" ]] || continue
    echo
    echo "=== $name ==="
    "$bin" || status=1
done
exit $status
