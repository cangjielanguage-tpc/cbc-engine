#if defined(__APPLE__) && __has_include(<TargetConditionals.h>)
    #include <TargetConditionals.h>
#endif

#if defined(TARGET_OS_IOS) && TARGET_OS_IOS && (defined(__aarch64__) || defined(_M_ARM64))
    #include "arch_os/aarch64_ios/platform_asm_export.h"
#elif defined(__x86_64__) || defined(_M_X64)
    #include "arch_os/x86_64_linux/platform_asm_export.h"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #include "arch_os/aarch64_linux/platform_asm_export.h"
#endif

#define TRAMPOLINE_COUNT 1024

#define TLS_STACK_BORDER_OFFSET 40
#define TLS_FIBER_DATA_OFFSET 16
#define FIBER_DATA_ECTYPE_OFFSET 16

#define ECTYPE_IACC_NUM 14
#define ECTYPE_REG_SIZE 8
#define ECTYPE_IREGS_COUNT 15
#define ECTYPE_IREGS_OFFSET 0
#define ECTYPE_FREGS_OFFSET 120
#define ECTYPE_IACC_OFFSET (ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE * ECTYPE_IACC_NUM)

#define ECTYPE_FUNC_COUNTER_OFFSET 248

#define FUNCTION_HANDLE_C2CALL_OFFSET 8
#define FUNCTION_HANDLE_BYTECODE_OFFSET 16

#define EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET 0
#define EXEC_BYTECODE_INFO_BYTECODE_OFFSET 8
#define EXEC_BYTECODE_INFO_LITERALS_OFFSET 16
#define EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET 24
#define EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET 32
#define EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET 40
#define TYPEINFO_INSTANCESIZE_OFFSET 12

#define TYPEINFO_DATA_MT_OFFSET 96

#ifndef ADDITIONAL_STACK_SPACE // can be set externally
    // TODO: adjust values appropriately (it is best if the ADDITIONAL_STACK_SPACE is 0 for release mode)
    // FIXME: usage of libc stdio for logging require more than 20K of stack
    #ifndef NDEBUG
        #define ADDITIONAL_STACK_SPACE (64 * 1024)
    #else
        #define ADDITIONAL_STACK_SPACE (64 * 1024) // TODO: return to 8 Kb
    #endif // NDEBUG
#endif     // ADDITIONAL_STACK_SPACE

#define STACK_OVERFLOW_REGDUMP_SIZE (((ECTYPE_IREGS_COUNT * ECTYPE_REG_SIZE) + 15) & ~15)

#define EXC_HANDLER_NOT_FOUND 0
#define EXC_HANDLER_FOUND 1
#define EXC_SOE_THROWN 2
