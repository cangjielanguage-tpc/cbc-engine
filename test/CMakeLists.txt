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

target_link_libraries(cbc_test PRIVATE cbc_lib GTest::gtest_main)
target_compile_definitions(cbc_test PRIVATE TEST_RESOURCE_DIR="${TEST_RESOURCE_DIR}")

gtest_discover_tests(cbc_test)
