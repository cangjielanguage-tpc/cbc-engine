cmake_minimum_required(VERSION 3.16)
project(launcher C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED CANGJIE_HOME)
  message(FATAL_ERROR "CANGJIE_HOME must be defined (path to the cangjie toolchain)")
endif()

if(NOT DEFINED CBCENGINE_LIB)
  message(FATAL_ERROR "CBCENGINE_LIB must be defined (path to the libcbcengine shared library)")
endif()

if(NOT DEFINED CJ_ARCH_DIR)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CJ_OS "darwin")
  else()
    set(CJ_OS "linux")
  endif()

  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(CJ_ARCH "x86_64")
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(CJ_ARCH "aarch64")
  else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  set(CJ_ARCH_DIR "${CJ_OS}_${CJ_ARCH}_cjnative")
endif()

set(CJ_RUNTIME_LIB "${CANGJIE_HOME}/runtime/lib/${CJ_ARCH_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}cangjie-runtime${CMAKE_SHARED_LIBRARY_SUFFIX}")

if(NOT EXISTS "${CJ_RUNTIME_LIB}")
  message(FATAL_ERROR "cangjie runtime lib not found at: ${CJ_RUNTIME_LIB}")
endif()

if(NOT EXISTS "${CBCENGINE_LIB}")
  message(FATAL_ERROR "cbcengine lib not found at: ${CBCENGINE_LIB}")
endif()

add_executable(launcher launcher.c)

target_include_directories(launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

target_link_libraries(launcher PRIVATE
  "${CBCENGINE_LIB}"
  "${CJ_RUNTIME_LIB}"
  m
  c
)

target_compile_definitions(launcher PRIVATE "INTERPRETER_LIB=\"${CMAKE_SHARED_LIBRARY_PREFIX}cbcengine${CMAKE_SHARED_LIBRARY_SUFFIX}\"")

get_filename_component(CBCENGINE_LIB_DIR "${CBCENGINE_LIB}" DIRECTORY)
set_target_properties(launcher PROPERTIES BUILD_RPATH "${CBCENGINE_LIB_DIR}")

if(NOT APPLE)
  target_link_options(launcher PRIVATE
    "-Wl,-z,noexecstack"
    "-rdynamic"
  )
endif()
