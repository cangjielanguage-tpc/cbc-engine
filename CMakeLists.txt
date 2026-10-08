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

set(ADDITIONAL_COMPILER_WARNINGS
  -Werror
  -Warray-parameter
  -Wbool-operation
  -Wchar-subscripts
  -Wcomment
  -Wdelete-non-virtual-dtor
  -Wfor-loop-analysis
  -Wformat
  -Wframe-address
  -Wimplicit
  -Winfinite-recursion
  -Wint-in-bool-context
  -Wmisleading-indentation
  -Wmismatched-tags
  -Wmove
  -Wmultichar
  -Woverloaded-virtual
  -Wparentheses
  -Wrange-loop-construct
  -Wreorder
  -Wreturn-type
  -Wself-assign
  -Wself-move
  -Wsizeof-array-argument
  -Wsizeof-array-decay
  -Wstring-plus-int
  -Wswitch
  -Wswitch-bool
  -Wtautological-compare
  -Wtrigraphs
  -Wunknown-pragmas
  -Wunneeded-internal-declaration
  -Wunused-argument
  -Wunused-but-set-variable
  -Wunused-const-variable
  -Wunused-function
  -Wunused-label
  -Wunused-lambda-capture
  -Wunused-private-field
  -Wunused-value
  -Wuser-defined-warnings
  -Wvolatile-register-var
  -Wbind-to-temporary-copy
  -Wbit-int-extension
  -Wbitwise-conditional-parentheses
  -Wbitwise-instead-of-logical
  -Wbitwise-op-parentheses
  -Wc++11-extensions
  -Wc++11-long-long
  -Wc++14-attribute-extensions
  -Wc++14-binary-literal
  -Wc++17-attribute-extensions
  -Wc++17-extensions
  -Wc++20-attribute-extensions
  -Wc11-extensions
  -Wcast-of-sel-type
  -Wcomplex-component-init
  -Wcuda-compat
  -Wdangling-else
  -Wdeclaration-after-statement
  -Wdelete-abstract-non-virtual-dtor
  -Wdelete-non-abstract-non-virtual-dtor
  -Wdelimited-escape-sequence-extension
  -Wdeprecated-copy
  -Wdeprecated-copy-with-user-provided-copy
  -Wdollar-in-identifier-extension
  -Wdtor-name
  -Wduplicate-decl-specifier
  -Wembedded-directive
  -Wempty-init-stmt
  -Wempty-translation-unit
  -Wexpansion-to-defined
  -Wextern-c-compat
  -Wformat-extra-args
  -Wformat-insufficient-args
  -Wformat-invalid-specifier
  -Wformat-pedantic
  -Wformat-security
  -Wformat-y2k
  -Wformat-zero-length
  -Wfuse-ld-path
  -Wfuture-attribute-extensions
  -Wgcc-compat
  -Wgnu-anonymous-struct
  -Wgnu-auto-type
  -Wgnu-case-range
  -Wgnu-complex-integer
  -Wgnu-compound-literal-initializer
  -Wgnu-conditional-omitted-operand
  -Wgnu-designator
  -Wgnu-empty-struct
  -Wgnu-flexible-array-initializer
  -Wgnu-folding-constant
  -Wgnu-imaginary-constant
  -Wgnu-include-next
  -Wgnu-line-marker
  -Wgnu-null-pointer-arithmetic
  -Wgnu-pointer-arith
  -Wgnu-redeclared-enum
  -Wgnu-statement-expression
  -Wgnu-statement-expression-from-macro-expansion
  -Wgnu-union-cast
  -Wgnu-zero-line-directive
  -Wgnu-zero-variadic-macro-arguments
  -Wignored-qualifiers
  -Wignored-reference-qualifiers
  -Wimplicit-function-declaration
  -Wimplicit-int
  -Wimport-preprocessor-directive-pedantic
  -Winitializer-overrides
  -Winvalid-utf8
  -Wkeyword-macro
  -Wlanguage-extension-token
  -Wlogical-not-parentheses
  -Wlogical-op-parentheses
  -Wlong-long
  -Wmain
  -Wmain-return-type
  -Wmicrosoft-anon-tag
  -Wmicrosoft-charize
  -Wmicrosoft-comment-paste
  -Wmicrosoft-cpp-macro
  -Wmicrosoft-end-of-file
  -Wmicrosoft-enum-value
  -Wmicrosoft-exception-spec
  -Wmicrosoft-fixed-enum
  -Wmicrosoft-flexible-array
  -Wmicrosoft-redeclare-static
  -Wmissing-method-return-type
  -Wnonnull
  -Wnull-pointer-arithmetic
  -Wnull-pointer-subtraction
  -Wnullability-extension
  -Wobjc-designated-initializers
  -Wobjc-flexible-array
  -Wobjc-missing-super-calls
  -Woverlength-strings
  -Woverloaded-shift-op-parentheses
  -Wparentheses-equality
  -Wpessimizing-move
  -Wpointer-arith
  -Wpotentially-evaluated-expression
  -Wprivate-extern
  -Wredundant-move
  -Wreorder-ctor
  -Wreorder-init-list
  -Wretained-language-linkage
  -Wreturn-std-move
  -Wreturn-type-c-linkage
  -Wself-assign-field
  -Wself-assign-overloaded
  -Wsemicolon-before-method-body
  -Wshift-op-parentheses
  -Wstatic-in-inline
  -Wstatic-self-init
  -Wstrict-prototypes
  -Wstring-concatenation
  -Wtautological-bitwise-compare
  -Wtautological-constant-compare
  -Wtautological-constant-out-of-range-compare
  -Wtautological-objc-bool-compare
  -Wtautological-overlap-compare
  -Wtautological-pointer-compare
  -Wtautological-undefined-compare
  -Wtentative-definition-incomplete-type
  -Wundefined-internal-type
  -Wunevaluated-expression
  -Wuninitialized-const-reference
  -Wunused-but-set-parameter
  -Wunused-comparison
  -Wunused-property-ivar
  -Wunused-result
  -Wvariadic-macros
  -Wvla-extension
)

set(DEBUG_OPTIONS "-DCBC_ENGINE_IMMEDIATE_ASSERTION -DCBC_ENGINE_PRETTY_FUNC_NAME -DINT_SYMS")

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

target_compile_options(cbc_lib PRIVATE
  -fno-omit-frame-pointer
  -fno-stack-protector
  -fno-exceptions
  -fno-asynchronous-unwind-tables
  -fno-unwind-tables
)

if(COMPILER_WARNINGS)
  target_compile_options(cbc_lib PRIVATE ${ADDITIONAL_COMPILER_WARNINGS})
endif()

if(NOT APPLE)
  target_compile_options(cbc_lib PRIVATE
    -fno-plt
  )
endif()

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
target_link_libraries(cbcengine PRIVATE
  cbc_lib
  ${CMAKE_DL_LIBS}
)

if(NOT APPLE)
  target_link_options(cbcengine PRIVATE
    "-Wl,-z,noexecstack"
    "-Wl,-z,relro"
    "-Wl,-z,now"
  )
endif()

if(INT_SYMS)
  target_compile_options(cbc_lib PRIVATE "-DINT_SYMS")
endif()

set_target_properties(cbcengine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set_target_properties(cbcengine PROPERTIES OUTPUT_NAME "cbcengine")
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  add_subdirectory(tools/dis)
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
