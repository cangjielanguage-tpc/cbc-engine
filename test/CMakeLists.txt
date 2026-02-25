include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

set(TEST_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

add_executable(cbc_test "${TEST_SOURCES}")

if(HAVE_ASM)
  target_compile_definitions(cbc_test PRIVATE HAVE_ASM)
endif()

target_link_libraries(cbc_test PRIVATE cbc_lib GTest::gtest_main)
target_compile_definitions(cbc_test PRIVATE TEST_RESOURCE_DIR="${TEST_RESOURCE_DIR}")

gtest_discover_tests(cbc_test)
