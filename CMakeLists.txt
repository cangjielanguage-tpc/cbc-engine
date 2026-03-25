cmake_minimum_required(VERSION 3.16)
project(cbc-engine CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)


set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# FIXME: make source search platform-aware
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.S")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*entrypoint\\.cpp$")

add_library(cbc_lib OBJECT ${LIB_SOURCES})

set_target_properties(cbc_lib PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON)

target_include_directories(cbc_lib PUBLIC "src")
target_include_directories(cbc_lib PUBLIC "include")

if(UNIT_TEST_MODE)
  target_compile_definitions(cbc_lib PRIVATE UNIT_TEST_MODE)
endif()

add_library(cbcengine SHARED src/engine/entrypoint.cpp)
target_link_libraries(cbcengine PRIVATE cbc_lib)

if(NOT APPLE)
  target_link_options(cbcengine PRIVATE
    "-Wl,-z,noexecstack"
    "-Wl,-z,relro"
    "-Wl,-z,now")
endif()

set_target_properties(cbcengine PROPERTIES POSITION_INDEPENDENT_CODE ON)
