cmake_minimum_required(VERSION 3.22)
project(cbc-engine CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)


set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_FLAGS_DEBUG "-g -Og")
set(CMAKE_C_FLAGS_RELEASE "-O2")

list(APPEND PROJECT_WARNINGS
    -Werror
    -Wconversion
    -Wsign-conversion
    -Wfloat-conversion
    -Wundef
    -Wwrite-strings
    -Wformat=2
)

option(ENABLE_LTO "Enable Link-Time Optimization" ON)
if(ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

file(GLOB_RECURSE MAIN_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(Engine ${MAIN_SOURCES})

target_compile_options(Engine PRIVATE
    ${PROJECT_WARNINGS}
    $<$<C_COMPILER_ID:GNU,Clang>:-fvisibility=hidden>
)

target_include_directories(Engine PRIVATE "src")

# 6. Testing Subdirectory
# Option to enable building of tests
# option(BUILD_TESTING "Build the tests for the project" ON)
# if(BUILD_TESTING)
#     enable_testing() # Enable CTest discovery
#     add_subdirectory(test)
# endif()
