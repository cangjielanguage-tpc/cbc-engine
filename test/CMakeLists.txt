include(FetchContent)

set(GOOGLETEST_FETCHCONTENT_ARGS
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  list(APPEND GOOGLETEST_FETCHCONTENT_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

FetchContent_Declare(googletest ${GOOGLETEST_FETCHCONTENT_ARGS})

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

set(TEST_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

add_executable(cbc_test "${TEST_SOURCES}")

target_link_libraries(cbc_test PRIVATE cbc_lib GTest::gtest_main ${CMAKE_DL_LIBS})
target_compile_definitions(cbc_test PRIVATE TEST_RESOURCE_DIR="${TEST_RESOURCE_DIR}")

# gtest_discover_tests generates add_test() calls that ignore the target
# ENVIRONMENT property, so pass via PROPERTIES argument
set(CBC_TEST_ENV_PROPERTIES "")
if(ENABLE_SANITIZERS)
  # detect_invalid_pointer_pairs is disabled (0) since it happens in stdlib overlap-check idioms
  # (e.g. in basic_string::_M_disjunct)
  # Set it to 1 or 2 explicitly for a deeper pointer-pair investigation.
  list(APPEND CBC_TEST_ENV_PROPERTIES
    "ENVIRONMENT" "ASAN_OPTIONS=halt_on_error=1:detect_stack_use_after_return=1:strict_string_checks=1:detect_invalid_pointer_pairs=0:check_initialization_order=1:detect_leaks=1"
    "ENVIRONMENT" "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
    "ENVIRONMENT" "LSAN_OPTIONS=report_objects=1:fast_unwind_on_malloc=0")
endif()

gtest_discover_tests(cbc_test NO_PRETTY_VALUES PROPERTIES "${CBC_TEST_ENV_PROPERTIES}")
