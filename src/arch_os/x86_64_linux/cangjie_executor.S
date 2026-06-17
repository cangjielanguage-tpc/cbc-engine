.intel_syntax noprefix

// --- SECTION TEXT ---
.section .text

// expects:
// rdi, rsi, rdx - arguments
// rcx - cangjie native function pointer
// r8  - thread local data
.align 2
.global engine_execute_cangjie_cfunc
.type   engine_execute_cangjie_cfunc, @function
engine_execute_cangjie_cfunc:
    push    r15
    mov     r15, r8
    call    rcx
    pop     r15
    ret