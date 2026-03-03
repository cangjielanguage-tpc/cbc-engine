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

option(ENABLE_LTO "Enable Link-Time Optimization" ON)
if(ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*entrypoint\\.cpp$")

add_library(cbc_lib STATIC ${LIB_SOURCES})

target_compile_options(cbc_lib PRIVATE
    ${PROJECT_WARNINGS}
    $<$<C_COMPILER_ID:GNU,Clang>:-fvisibility=hidden>
)

target_include_directories(cbc_lib PUBLIC "src")

add_executable(Engine src/engine/entrypoint.cpp)
target_link_libraries(Engine PRIVATE cbc_lib)

# 6. Testing Subdirectory
enable_testing() # Enable CTest discovery
add_subdirectory(test)
