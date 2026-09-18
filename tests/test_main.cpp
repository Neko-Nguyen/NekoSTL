#include "test_framework.hpp"

#include <cstring>

namespace neko_test {

int run_all(int argc, char** argv) {
    const char* filter = (argc > 1) ? argv[1] : nullptr;

    int passed = 0, failed = 0, todo = 0, skipped = 0;

    for (const TestCase& tc : registry()) {
        if (filter && std::strstr(tc.name, filter) == nullptr) {
            ++skipped;
            continue;
        }

        current_failures() = 0;
        const char* status = nullptr;

        try {
            tc.fn();
        } catch (const Abort&) {
            // REQUIRE already reported the failure.
        } catch (const neko::not_implemented& e) {
            status = "TODO";
            std::printf("  \033[33mTODO\033[0m  %-40s (%s)\n", tc.name,
                        e.what());
            ++todo;
        } catch (const std::exception& e) {
            fail(tc.file, tc.line,
                 std::string("unexpected exception: ") + e.what());
        } catch (...) {
            fail(tc.file, tc.line, "unexpected non-std exception");
        }

        if (status) continue;
        if (current_failures() == 0) {
            std::printf("  \033[32mPASS\033[0m  %s\n", tc.name);
            ++passed;
        } else {
            std::printf("  \033[31mFAIL\033[0m  %s\n", tc.name);
            ++failed;
        }
    }

    std::printf("\n%d passed, %d failed, %d not implemented", passed, failed,
                todo);
    if (skipped) std::printf(", %d filtered out", skipped);
    std::printf("  (%d assertions)\n", total_checks());

    // TODOs do not fail the build -- they are your work queue, not regressions.
    return failed == 0 ? 0 : 1;
}

}  // namespace neko_test

int main(int argc, char** argv) { return neko_test::run_all(argc, argv); }
