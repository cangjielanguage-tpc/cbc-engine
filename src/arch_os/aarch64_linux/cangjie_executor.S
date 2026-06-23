// --- SECTION TEXT ---
.section .text

// expects:
// x0, x1, x2 - arguments
// x3  - cangjie native function pointer
// x4  - thread local data
.align 2
.global engine_execute_cangjie_cfunc
.type   engine_execute_cangjie_cfunc, @function
engine_execute_cangjie_cfunc:
    stp x28, x30, [sp, #-16]!
    mov x28, x4
    mov x8,  #0 // change if there is need to call CJ functions that return structs
    blr x3
    ldp x28, x30, [sp], #16
    ret
