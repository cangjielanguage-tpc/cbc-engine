cmake_minimum_required(VERSION 3.16)
project(cbc-engine CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)


set(CMAKE_CXX_STANDARD 17)
include(CTest)

set(DEBUG_OPTIONS "-DCBC_ENGINE_IMMEDIATE_ASSERTION -DCBC_ENGINE_PRETTY_FUNC_NAME")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG "-g -gz ${DEBUG_OPTIONS}")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# cbc_lib is core part that is shared between unit tests and resulting cbcengine shared lib
# runtime specific sources are needed only for cbcengine
file(GLOB_RECURSE COMMON_LIB_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER COMMON_LIB_SOURCES EXCLUDE REGEX "src/runtimesupport/.*")
list(FILTER COMMON_LIB_SOURCES EXCLUDE REGEX "src/arch_os/.*")

add_subdirectory(src/arch_os/)

set(LIB_SOURCES ${COMMON_LIB_SOURCES})

add_library(cbc_lib OBJECT ${LIB_SOURCES})

set_target_properties(cbc_lib PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON)

target_include_directories(cbc_lib PUBLIC "src")

if(UNIT_TEST_MODE)
  target_compile_definitions(cbc_lib PRIVATE UNIT_TEST_MODE)
endif()

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "src/runtimesupport/*.cpp")

# NOTE: platform-specific sources are supported only for cbcengine shared lib
add_library(cbcengine SHARED ${PLATFORM_SOURCES} ${ENGINE_SOURCES})
target_include_directories(cbcengine PUBLIC "include")
target_include_directories(cbcengine PUBLIC "third_party/mrt/include")
target_include_directories(cbc_lib PRIVATE "${PLATFORM_DIR}")
target_link_libraries(cbcengine PRIVATE cbc_lib)

target_link_options(cbcengine PRIVATE
  "-Wl,-z,noexecstack"
  "-Wl,-z,relro"
  "-Wl,-z,now")

set_target_properties(cbcengine PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(tools/dis)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
