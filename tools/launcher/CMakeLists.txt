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
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(CJ_ARCH "x86_64")
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(CJ_ARCH "aarch64")
  else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CJ_ARCH_DIR "darwin_${CJ_ARCH}_cjnative")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(CJ_ARCH_DIR "linux_android_${CJ_ARCH}_cjnative")
  else()
    set(CJ_ARCH_DIR "linux_${CJ_ARCH}_cjnative")
  endif()
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

# Final-layout rpaths baked at link time (BUILD_WITH_INSTALL_RPATH). The
# packed toolchain places the launcher in runtime/lib/<arch>/bin with the
# engine and cangjie runtime libs one level up, and the stdx extension dylibs
# four levels up (<toolchain-root>/stdx/dynamic/stdx). The same relative
# layout holds in the build tree for the engine lib (output/<arch>/launcher).

if(NOT APPLE)
  target_link_options(launcher PRIVATE
    "-Wl,-z,noexecstack"
    "-rdynamic"
  )
endif()

set(LAUNCHER_RPATH
  "@loader_path/.."
  "@loader_path/../../../../stdx/dynamic/stdx"
)
if(NOT APPLE)
  string(REPLACE "@loader_path" "$ORIGIN" LAUNCHER_RPATH "${LAUNCHER_RPATH}")
  # $ORIGIN in an executable's own DT_RUNPATH requires the DF_1_ORIGIN flag.
  target_link_options(launcher PRIVATE "-Wl,-z,origin")
endif()
set_target_properties(launcher PROPERTIES
  BUILD_WITH_INSTALL_RPATH ON
  INSTALL_RPATH "${LAUNCHER_RPATH}"
)

add_library(cbc-preload SHARED
  preload.c
  engine-common.c
)

set_target_properties(cbc-preload PROPERTIES
  C_STANDARD 11
)

if(NOT APPLE)
  target_link_options(cbc-preload PRIVATE
    "-Wl,-z,noexecstack"
  )
  target_link_libraries(cbc-preload PRIVATE
    dl
  )
else()
  # The __DATA,__interpose replacement calls the real CJ_MRT_CjRuntimeInit,
  # resolved through normal linkage against the cangjie runtime.
  target_link_libraries(cbc-preload PRIVATE "${CJ_RUNTIME_LIB}")
  # Packed next to libcbcengine and the cangjie runtime; its own @rpath deps
  # also resolve through the launcher's rpaths.
  set_target_properties(cbc-preload PROPERTIES
    BUILD_WITH_INSTALL_RPATH ON
    INSTALL_RPATH "@loader_path"
  )
endif()
