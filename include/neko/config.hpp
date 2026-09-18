#pragma once

#include <stdexcept>
#include <string>  // std::logic_error takes a std::string; do not rely on
                   // <stdexcept> pulling it in transitively

namespace neko {

// Thrown by NEKO_TODO(). The test runner catches it and reports the test as
// "not implemented" rather than a failure, so an unfinished module shows up as
// a work item instead of a red regression.
struct not_implemented : std::logic_error {
    explicit not_implemented(const char* where)
        : std::logic_error(std::string("not implemented: ") + where) {}
};

}  // namespace neko

// Use as the entire body of a function you have not written yet:
//     size_type size() const { NEKO_TODO(); }
// It works in value-returning functions because `throw` ends the function.
// Do not mark such a stub noexcept -- add noexcept once it is real.
#define NEKO_TODO() throw ::neko::not_implemented(__func__)
