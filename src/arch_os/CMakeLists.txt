unset(DETECTED_OS)
unset(DETECTED_ARCH)

string(TOLOWER "${CMAKE_SYSTEM_NAME}" DETECTED_OS)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" DETECTED_ARCH)

if(DETECTED_ARCH MATCHES "^(amd64|x64)$")
    set(DETECTED_ARCH "x86_64")
elseif(DETECTED_ARCH MATCHES "^(arm64|arm64-v8a)$")
    set(DETECTED_ARCH "aarch64")
endif()

if(DETECTED_OS STREQUAL "android" AND DETECTED_ARCH STREQUAL "aarch64")
    # Android arm64 uses the same AArch64 ELF ABI trampolines as Linux here.
    set(DETECTED_OS "linux")
endif()

set(ARCH_OS_DIR "${DETECTED_ARCH}_${DETECTED_OS}")
set(ARCH_OS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_OS_DIR}")

if(NOT EXISTS "${ARCH_OS_PATH}")
    message(FATAL_ERROR "Missing directory: ${ARCH_OS_PATH}")
endif()

message(STATUS "Selected platform-specific directory: ${ARCH_OS_PATH}")

file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS "${ARCH_OS_PATH}/*.cpp" "${ARCH_OS_PATH}/*.S")

set(PLATFORM_SOURCES ${PLATFORM_SOURCES} PARENT_SCOPE)
set(PLATFORM_DIR     ${ARCH_OS_PATH}     PARENT_SCOPE)
