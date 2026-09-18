# One executable per test file, so a compile error in a module you are
# mid-way through does not block the tests for every other module.
function(neko_add_test name)
  add_executable(${name} ${name}.cpp test_main.cpp)
  target_link_libraries(${name} PRIVATE neko neko_warnings neko_sanitizers)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

# Discovered rather than listed by hand. Each module lives on its own branch
# and brings exactly one test_<module>.cpp with it; naming them here would make
# this file the one place every branch edits, and so the one place every merge
# conflicts. CONFIGURE_DEPENDS re-runs the glob when the set of files changes.
file(GLOB neko_test_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
foreach(src ${neko_test_sources})
  get_filename_component(name ${src} NAME_WE)
  if(NOT name STREQUAL "test_main")
    neko_add_test(${name})
  endif()
endforeach()

# A runnable explainer rather than a test: it is not registered with ctest and
# `./t` does not run it. Build and run it with `./t demo`. It arrives with
# vector, so it is absent on the branches below that.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lifetime_demo.cpp")
  add_executable(lifetime_demo lifetime_demo.cpp)
  target_link_libraries(lifetime_demo PRIVATE neko neko_warnings neko_sanitizers)
  target_include_directories(lifetime_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
