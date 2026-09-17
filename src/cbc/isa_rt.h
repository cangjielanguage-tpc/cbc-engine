#pragma once

#include "decoder.h"
#include "isa.h"
#include "runtimesupport/runtime.h"
#include <cstdint>

// X parameters: opcode, encoding format, string format
#define CBC_RT_OPCODES(X)                                                                                              \
    X(HALT, B1, "halt")                                                                                                \
    X(RET, B1, "ret")                                                                                                  \
    X(NOP, B1, "nop")                                                                                                  \
    X(MOV, B2rr, "mov $0ir $1ir")                                                                                      \
    X(MOVI, B2xr, "movi $1ir $0I4")                                                                                    \
    X(FMOV, B2rr, "fmov $0fr $1fr")                                                                                    \
    X(MOVI2F, B2rr, "i2f $0fr $1ir")                                                                                   \
    X(MOVF2I, B2rr, "f2i $0ir $1fr")                                                                                   \
    X(FMOVI32, B6xri32, "fmovi.32 $0fr $2F32")                                                                         \
    X(FMOVI64, B10xri64, "fmovi.64 $0fr $2F64")                                                                        \
    X(BCC32I, B4xi12rr, "bcc.32 $0cc $2ir $3ir $1I12")                                                                 \
    X(BCC64I, B4xi12rr, "bcc.64 $0cc $2ir $3ir $1I12")                                                                 \
    X(BCC32L, B4xi12rr, "bcc.32 $0cc $2ir $3ir $1I12L")                                                                \
    X(BCC64L, B4xi12rr, "bcc.64 $0cc $2ir $3ir $1I12L")                                                                \
    X(BCCI32I, B5xi12ri12, "bcc.32 $0cc $2ir $3I12 $1I12")                                                             \
    X(BCCI64I, B5xi12ri12, "bcc.64 $0cc $2ir $3I12 $1I12")                                                             \
    X(BCCI32L, B5xi12ri12, "bcc.32 $0cc $2ir $3I12 $1I12L")                                                            \
    X(BCCI64L, B5xi12ri12, "bcc.64 $0cc $2ir $3I12 $1I12L")                                                            \
    X(BCCL32I, B5xi12ri12, "bcc.32 $0cc $2ir $3I12L $1I12")                                                            \
    X(BCCL64I, B5xi12ri12, "bcc.64 $0cc $2ir $3I12L $1I12")                                                            \
    X(BCCL32L, B5xi12ri12, "bcc.32 $0cc $2ir $3I12L $1I12L")                                                           \
    X(BCCL64L, B5xi12ri12, "bcc.64 $0cc $2ir $3I12L $1I12L")                                                           \
    X(JMP32, B5i32, "jmp $0I32")                                                                                       \
    X(BRANCH_IS_REF, B3xi12, "branch.is.ref $0ir $1I12")                                                               \
    X(BIN32, B3xrrr, "$0bin.32 $1ir $2ir $3ir")                                                                        \
    X(BIN64, B3xrrr, "$0bin.64 $1ir $2ir $3ir")                                                                        \
    X(BINI32I, B4xi12rr, "$0bin.32 $2ir $3ir $1I12")                                                                   \
    X(BINI64I, B4xi12rr, "$0bin.64 $2ir $3ir $1I12")                                                                   \
    X(BINI32L, B4xi12rr, "$0bin.32 $2ir $3ir $1I12L")                                                                  \
    X(BINI64L, B4xi12rr, "$0bin.64 $2ir $3ir $1I12L")                                                                  \
    X(FBIN32, B3xrrr, "$0fop.32 $1fr $2fr $3fr")                                                                       \
    X(FBIN64, B3xrrr, "$0fop.64 $1fr $2fr $3fr")                                                                       \
    X(FUN32, B3xrrr, "$0fop.32 $1fr $3fr")                                                                             \
    X(FUN64, B3xrrr, "$0fop.64 $1fr $3fr")                                                                             \
    X(FMATHUN32, B3xrrr, "$0fmatop.32 $1fr $3fr")                                                                      \
    X(FMATHUN64, B3xrrr, "$0fmatop.64 $1fr $3fr")                                                                      \
    X(NEWOBJ, B9i64, "newobj IR1, $0U64")                                                                              \
    X(NEWOBJ_PINNED, B9i64, "newobj.pinned IR1, $0U64")                                                                \
    X(NEWOBJ_G, B2rr, "newobj.g $0ir")                                                                                 \
    X(NEWOBJ_PINNED_G, B2rr, "newobj.pinned.g $0ir")                                                                   \
    X(NEWOBJ_ACC_G, B2rr, "newobj.acc.g $0ir")                                                                         \
    X(NEWARR, B9i64, "newarr IR1, IR2, $0U64")                                                                         \
    X(INITCLOSURE, B1, "init.closure")                                                                                 \
    X(INITCLOSURE_SRET, B1, "init.closure.sret")                                                                       \
    X(SPAWN, B9i64, "spawn $0U64")                                                                                     \
    X(SPAWN_FUTURE, B1, "spawn.future")                                                                                \
    /* Integral Load/Store instructions start */                                                                       \
    X(LOAD_ADDR, B4xri16, "ld.addr.$0ldk $1ir $2U16")                                                                  \
    X(STORE_ADDR, B4xri16, "st.addr.$0ldk $1ir $2U16")                                                                 \
    X(LOAD_OBJ, B4xi12rr, "ld.$0ldk $2ir [$3ir $1U12]")                                                                \
    X(STORE_OBJ, B4xi12rr, "st.$0stk $2ir [$3ir $1U12]")                                                               \
    X(LOAD_ARR, B3xrrr, "ld.arr.$0ldk $1ir $2ir $3ir")                                                                 \
    X(STORE_ARR, B3xrrr, "st.arr.$0stk $1ir $2ir $3ir]")                                                               \
    X(LOAD_REC, B4xi12rr, "ld.rec.$0ldk $2ir [$3ir $1U12]")                                                            \
    X(STORE_REC, B4xi12rr, "st.rec.$0stk $2ir [$3ir $1U12]")                                                           \
    X(LOAD_FRAME, B4xi12rr, "ld.frame.$0ldk $2ir [$3ir $1U12]")                                                        \
    X(STORE_FRAME, B4xi12rr, "st.frame.$0stk $2ir [$3ir $1U12]")                                                       \
    X(LOAD_LONG_DERIVED, B7xrrri32, "ld.derived.$0ldk $1ir [($2ir $3ir) $4U32]")                                       \
    X(STORE_LONG_DERIVED, B7xrrri32, "st.derived.$0stk $1ir [($2ir $3ir) $4U32]")                                      \
    X(LOAD_LONG_REC, B7xrrri32, "ld.rec.$0ldk $1ir [$3ir $4U32]")                                                      \
    X(STORE_LONG_REC, B7xrrri32, "st.rec.$0stk $1ir [$3ir $4U32]")                                                     \
    X(LOAD_LONG_FRAME, B7xrrri32, "ld.frame.$0ldk $1ir [$4U32]")                                                       \
    X(STORE_LONG_FRAME, B7xrrri32, "st.frame.$0stk $1ir [$4U32]")                                                      \
    X(LOAD_GENERIC, B3rrrr, "ld.g $0ir $3ir [($1ir $2ir)]")                                                            \
    X(STORE_GENERIC, B3rrrr, "st.g $0ir $3ir [($1ir $2ir)]")                                                           \
    X(LEA_GENERIC, B7xrrri32, "lea.g $1ir $3ir [$2ir ord=$4U32]")                                                      \
    /* Integral Load/Store instructions end*/                                                                          \
    /* Float Load/Store instructions end*/                                                                             \
    X(LOAD_OBJ_F, B4xi12rr, "ld.$0ldk $2fr [$3ir $1U12]")                                                              \
    X(STORE_OBJ_F, B4xi12rr, "st.$0stk $2fr [$3ir $1U12]")                                                             \
    X(LOAD_ARR_F, B3xrrr, "ld.arr.$0ldk $1fr $2ir $3ir]")                                                              \
    X(STORE_ARR_F, B3xrrr, "st.arr.$0stk $1fr $2ir $3ir]")                                                             \
    X(LOAD_REC_F, B4xi12rr, "ld.rec.$0ldk $2fr [$3ir $1U12]")                                                          \
    X(STORE_REC_F, B4xi12rr, "st.rec.$0stk $2fr [$3ir $1U12]")                                                         \
    X(LOAD_FRAME_F, B4xi12rr, "ld.frame.$0ldk $2fr [$3ir $1U12]")                                                      \
    X(STORE_FRAME_F, B4xi12rr, "st.frame.$0stk $2fr [$3ir $1U12]")                                                     \
    X(LOAD_LONG_DERIVED_F, B7xrrri32, "ld.derived.$0ldk $1ir [($2ir $3ir) $4U32]")                                     \
    X(STORE_LONG_DERIVED_F, B7xrrri32, "st.derived.$0stk $1fr [($2ir $3ir) $4U32]")                                    \
    X(LOAD_LONG_REC_F, B7xrrri32, "ld.rec.$0ldk $1fr [$3ir $4U32]")                                                    \
    X(STORE_LONG_REC_F, B7xrrri32, "st.rec.$0stk $1fr [$3ir $4U32]")                                                   \
    X(LOAD_LONG_FRAME_F, B7xrrri32, "ld.frame.$0ldk $1fr [$4U32]")                                                     \
    X(STORE_LONG_FRAME_F, B7xrrri32, "st.frame.$0stk $1fr [$4U32]")                                                    \
    /* Float Load/Store instructions end*/                                                                             \
    X(PREP_TYPED, B13i64i32, "prep.typed $0U64 $1U32")                                                                 \
    X(SCC32, B3xrrr, "scc.32 $0cc $1ir $2ir $3ir")                                                                     \
    X(SCC64, B3xrrr, "scc.64 $0cc $1ir $2ir $3ir")                                                                     \
    X(FSCC32, B3xrrr, "fscc.32 $0cc $1ir $2fr $3fr")                                                                   \
    X(FSCC64, B3xrrr, "fscc.64 $0cc $1ir $2fr $3fr")                                                                   \
    X(SCCI32I, B4xi12rr, "scci.32 $0cc $2ir $3ir $1I12")                                                               \
    X(SCCI64I, B4xi12rr, "scci.64 $0cc $2ir $3ir $1I12")                                                               \
    X(SCCI32L, B4xi12rr, "scci.32 $0cc $2ir $3ir $1I12L")                                                              \
    X(SCCI64L, B4xi12rr, "scci.64 $0cc $2ir $3ir $1I12L")                                                              \
    X(TYPE_ARG, B4xi12rr, "type.arg $2ir $3ir $1I12")                                                                  \
    X(CONVERT, B3xxrr, "convert $0ct $1ct $2ir $3ir") /* FIXME: ir/fr */                                               \
    X(CALL_CLOSURE, B1, "call.closure")                                                                                \
    X(CALL_CLOSURE_SRET, B1, "call.closure.sret")                                                                      \
    X(CALL_CLOSURE_GENERIC, B1, "call.closure.g")                                                                      \
    X(DIRECT_CALL_2I, B3xi12, "call.2i $1I12L")                                                                        \
    X(DIRECT_CALL_2C, B3xi12, "call.2c $1I12L")                                                                        \
    X(VIRTUAL_CALL, VirtualCall, "vcall $0U16 $1U16")                                                                  \
    X(INTERFACE_CALL, InterfaceCall, "icall $0U16 $1U64")                                                              \
    X(INTERFACE_CALL_GENERIC, InterfaceCallGeneric, "icall.g.$2U8 $0U16 $1U16")                                        \
    X(MEMSPACE, B1, "memspace {")                                                                                      \
    X(GC_POINT, B1, "gcpoint")                                                                                         \
    X(BFXS, BFX, "bfxs $0ir $1ir $2U8 $3U8")                                                                           \
    X(BFXZ, BFX, "bfxz $0ir $1ir $2U8 $3U8")                                                                           \
    X(STRING_INIT, B13i64i32, "string.init $0U64 $1U32")                                                               \
    X(NULLCHECK, B2xr, "nullcheck $1ir")                                                                               \
    X(DIVCHECK, B2xr, "divcheck $1ir")                                                                                 \
    X(LOAD_TI, B9i64, "load.ti $0U64")                                                                                 \
    X(LOAD_GENERIC_TI, B9i64, "load.generic.ti $0U64")                                                                 \
    X(IOF, IOF, "iof $0ir $1ir $2U64")                                                                                 \
    X(NEWBOX, B2xr, "newbox $0U8")                                                                                     \
    X(NEWBOX2, B9i64, "newbox2 $0U64")                                                                                 \
    X(OFFSET, B6xri32, "offset $1ir $2U32 }")                                                                          \
    X(READ_STRUCT_FIELD, StructFieldOp, "read.struct.field $0ir $1ir $2ir $4U64")                                      \
    X(WRITE_STRUCT_FIELD, StructFieldOp, "write.struct.field $0ir $1ir $2ir $4U64")                                    \
    X(THROW, B2xr, "throw $1ir")                                                                                       \
    X(CATCH, B2xr, "catch $1ir")                                                                                       \
    X(LOG, B9i64, "log $0U64")                                                                                         \
    X(ASSIGN_GENERIC, B3xrrr, "assign.g $1ir $2ir $3ir")                                                               \
    X(IOF_GENERIC, B3xrrr, "iof.g $1ir $2ir $3ir")                                                                     \
    X(ATOMIC_FETCH_ADD_8, AtomicOp, "atomic.fetch.add.8 $1ir [$2ir $3U12]")                                            \
    X(ATOMIC_FETCH_ADD_16, AtomicOp, "atomic.fetch.add.16 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_ADD_32, AtomicOp, "atomic.fetch.add.32 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_ADD_64, AtomicOp, "atomic.fetch.add.64 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_SUB_8, AtomicOp, "atomic.fetch.sub.8 $1ir [$2ir $3U12]")                                            \
    X(ATOMIC_FETCH_SUB_16, AtomicOp, "atomic.fetch.sub.16 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_SUB_32, AtomicOp, "atomic.fetch.sub.32 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_SUB_64, AtomicOp, "atomic.fetch.sub.64 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_AND_8, AtomicOp, "atomic.fetch.and.8 $1ir [$2ir $3U12]")                                            \
    X(ATOMIC_FETCH_AND_16, AtomicOp, "atomic.fetch.and.16 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_AND_32, AtomicOp, "atomic.fetch.and.32 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_AND_64, AtomicOp, "atomic.fetch.and.64 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_OR_8, AtomicOp, "atomic.fetch.or.8 $1ir [$2ir $3U12]")                                              \
    X(ATOMIC_FETCH_OR_16, AtomicOp, "atomic.fetch.or.16 $1ir [$2ir $3U12]")                                            \
    X(ATOMIC_FETCH_OR_32, AtomicOp, "atomic.fetch.or.32 $1ir [$2ir $3U12]")                                            \
    X(ATOMIC_FETCH_OR_64, AtomicOp, "atomic.fetch.or.64 $1ir [$2ir $3U12]")                                            \
    X(ATOMIC_FETCH_XOR_8, AtomicOp, "atomic.fetch.xor.8 $1ir [$2ir $3U12]")                                            \
    X(ATOMIC_FETCH_XOR_16, AtomicOp, "atomic.fetch.xor.16 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_XOR_32, AtomicOp, "atomic.fetch.xor.32 $1ir [$2ir $3U12]")                                          \
    X(ATOMIC_FETCH_XOR_64, AtomicOp, "atomic.fetch.xor.64 $1ir [$2ir $3U12]")                                          \
    X(CAS_8, AtomicOp, "cas.8 $1ir [$2ir $3U12] $4ir")                                                                 \
    X(CAS_16, AtomicOp, "cas.16 $1ir [$2ir $3U12] $4ir")                                                               \
    X(CAS_32, AtomicOp, "cas.32 $1ir [$2ir $3U12] $4ir")                                                               \
    X(CAS_64, AtomicOp, "cas.64 $1ir [$2ir $3U12] $4ir")                                                               \
    X(CAS_REF, AtomicOp, "cas.ref $1ir [$2ir $3U12] $4ir")                                                             \
    X(ATOMIC_SWAP_8, AtomicOp, "atomic.swap.8 $1ir [$2ir $3U12]")                                                      \
    X(ATOMIC_SWAP_16, AtomicOp, "atomic.swap.16 $1ir [$2ir $3U12]")                                                    \
    X(ATOMIC_SWAP_32, AtomicOp, "atomic.swap.32 $1ir [$2ir $3U12]")                                                    \
    X(ATOMIC_SWAP_64, AtomicOp, "atomic.swap.64 $1ir [$2ir $3U12]")                                                    \
    X(ATOMIC_SWAP_REF, AtomicOp, "atomic.swap.ref $1ir [$2ir $3U12]")                                                  \
    X(ATOMIC_LOAD, B4xi12rr, "atomic.ld.$0ldk $2ir [$3ir $1U12]")                                                      \
    X(ATOMIC_STORE, B4xi12rr, "atomic.st.$0stk $2ir [$3ir $1U12]")                                                     \
    X(CBIN8, B3xrrr, "$0cbin.8 $1ir $2ir $3ir")                                                                        \
    X(CBIN16, B3xrrr, "$0cbin.16 $1ir $2ir $3ir")                                                                      \
    X(CBIN32, B3xrrr, "$0cbin.32 $1ir $2ir $3ir")                                                                      \
    X(CBIN64, B3xrrr, "$0cbin.64 $1ir $2ir $3ir")                                                                      \
    X(CBINI8I, B4xi12rr, "$0cbin.8 $2ir $3ir $1I12")                                                                   \
    X(CBINI16I, B4xi12rr, "$0cbin.16 $2ir $3ir $1I12")                                                                 \
    X(CBINI32I, B4xi12rr, "$0cbin.32 $2ir $3ir $1I12")                                                                 \
    X(CBINI64I, B4xi12rr, "$0cbin.64 $2ir $3ir $1I12")                                                                 \
    X(CBINI8W, BinaryChecked, "$0cbin.8 $2ir $3ir $1I64")                                                              \
    X(CBINI16W, BinaryChecked, "$0cbin.16 $2ir $3ir $1I64")                                                            \
    X(CBINI32W, BinaryChecked, "$0cbin.32 $2ir $3ir $1I64")                                                            \
    X(CBINI64W, BinaryChecked, "$0cbin.64 $2ir $3ir $1I64")                                                            \
    X(COPY_DERIVED, CopyDerived, "copy.derived $0ir $1ir $2ir $3ir $4U64")                                             \
    X(COPY_DERIVED_GENERIC, CopyDerivedGeneric, "copy.derived.g $0ir $1ir $2ir $3ir $4ir")                             \
    X(INDEX, Index, "index $0ir [$1ir $2ir] $3U64")                                                                    \
    X(INDEX_GENERIC, IndexGeneric, "index.g $0ir [$1ir $2ir] $3ir")                                                    \
    X(SBIN8, B3xrrr, "$0sbin.8 $1ir $2ir $3ir")                                                                        \
    X(SBIN16, B3xrrr, "$0sbin.16 $1ir $2ir $3ir")                                                                      \
    X(SBIN32, B3xrrr, "$0sbin.32 $1ir $2ir $3ir")                                                                      \
    X(SBIN64, B3xrrr, "$0sbin.64 $1ir $2ir $3ir")

// X parameters: opcode, encoding format, string format, is tail
#define CBC_RT_MEMOPCODES(X)                                                                                           \
    X(MEM_HALT, M1, "halt", true)                                                                                      \
    X(OFFS16, M3i16, "offs.16 $0U16", false)                                                                           \
    X(OFFS32, M5i32, "offs.32 $0U32", false)                                                                           \
    X(OFFS64, M9i64, "offs.64 $0U64", false)                                                                           \
    X(OFFS_REG, M2xr, "offs.r $1ir", false)                                                                            \
    X(GENERIC_FIELD, M6xri32, "generic.field $1ir $2U32 }", false)                                                     \
    X(OFFS_REG_IDX64, M10xri64, "offs.r.idx.64 [$1ir * $2U64] }", false)                                               \
    X(R_READ_STRUCT, MStructFieldOp, "r.read.struct $0ir $1ir $2U64 }", true)                                          \
    X(R_WRITE_STRUCT, MStructFieldOp, "r.write.struct $0ir $1ir $2U64 }", true)                                        \
    X(RLD_U8, M2rr, "rld.u8 $0ir $1ir }", true)                                                                        \
    X(RLD_U16, M2rr, "rld.u16 $0ir $1ir }", true)                                                                      \
    X(RLD_32, M2rr, "rld.32 $0ir $1ir }", true)                                                                        \
    X(RLD_S8, M2rr, "rld.s8 $0ir $1ir }", true)                                                                        \
    X(RLD_S16, M2rr, "rld.s16 $0ir $1ir }", true)                                                                      \
    X(RLD_F32, M2rr, "rld.f32 $0fr $1ir }", true)                                                                      \
    X(RLD_F64, M2rr, "rld.f64 $0fr $1ir }", true)                                                                      \
    X(RLD_64, M2rr, "rld.64 $0ir $1ir }", true)                                                                        \
    X(RLD_S32TO64, M2rr, "rld.s32to64 $0ir $1ir }", true)                                                              \
    X(RLD_REF, M2rr, "rld.ref $0ir $1ir }", true)                                                                      \
    X(RLD_LEA, M2rr, "rld.lea $0ir $1ir }", true)                                                                      \
    X(RST_8, M2rr, "rst.8 $0ir $1ir }", true)                                                                          \
    X(RST_16, M2rr, "rst.16 $0ir $1ir }", true)                                                                        \
    X(RST_32, M2rr, "rst.32 $0ir $1ir }", true)                                                                        \
    X(RST_64, M2rr, "rst.64 $0ir $1ir }", true)                                                                        \
    X(RST_REF, M2rr, "rst.ref $0ir $1ir }", true)                                                                      \
    X(RST_F32, M2rr, "rst.f32 $0fr $1ir }", true)                                                                      \
    X(RST_F64, M2rr, "rst.f64 $0fr $1ir }", true)                                                                      \
    X(RSTI_8_8, M3xri8, "rsti.8.8 $1ir $2U8 }", true)                                                                  \
    X(RSTI_16_8, M3xri8, "rsti.16.8 $1ir $2U8 }", true)                                                                \
    X(RSTI_16_16, M4xri16, "rsti.16.16 $1ir $2U16 }", true)                                                            \
    X(RSTI_32_8, M3xri8, "rsti.32.8 $1ir $2U8 }", true)                                                                \
    X(RSTI_32_16, M4xri16, "rsti.32.16 $1ir $2U16 }", true)                                                            \
    X(RSTI_32_32, M6xri32, "rsti.32.32 $1ir $2U32 }", true)                                                            \
    X(RSTI_64_8, M3xri8, "rsti.64.8 $1ir $2U8 }", true)                                                                \
    X(RSTI_64_16, M4xri16, "rsti.64.16 $1ir $2U16 }", true)                                                            \
    X(RSTI_64_32, M6xri32, "rsti.64.32 $1ir $2U32 }", true)                                                            \
    X(RSTI_64_64, M10xri64, "rsti.64.64 $1ir $2U64 }", true)                                                           \
    X(DLD_U8, M3xrrr, "dld.u8 $1ir $2ir $3ir }", true)                                                                 \
    X(DLD_U16, M3xrrr, "dld.u16 $1ir $2ir $3ir }", true)                                                               \
    X(DLD_32, M3xrrr, "dld.u32 $1ir $2ir $3ir }", true)                                                                \
    X(DLD_S8, M3xrrr, "dld.s8 $1ir $2ir $3ir }", true)                                                                 \
    X(DLD_S16, M3xrrr, "dld.s16 $1ir $2ir $3ir }", true)                                                               \
    X(DLD_F32, M3xrrr, "dld.f32 $1fr $2ir $3ir }", true)                                                               \
    X(DLD_F64, M3xrrr, "dld.f64 $1fr $2ir $3ir }", true)                                                               \
    X(DLD_64, M3xrrr, "dld.64 $1ir $2ir $3ir }", true)                                                                 \
    X(DLD_S32TO64, M3xrrr, "dld.s32to64 $1ir $2ir $3ir }", true)                                                       \
    X(DLD_REF, M3xrrr, "dld.ref $1ir $2ir $3ir }", true)                                                               \
    X(DLD_LEA, M3xrrr, "dld.lea $1ir $2ir $3ir }", true)                                                               \
    X(DLD_GENERIC, M3rrrr, "dld.g $0ir $1ir $2ir $3ir }", true)                                                        \
    X(DST_8, M3xrrr, "dst.8 $1ir $2ir $3ir }", true)                                                                   \
    X(DST_16, M3xrrr, "dst.16 $1ir $2ir $3ir }", true)                                                                 \
    X(DST_32, M3xrrr, "dst.32 $1ir $2ir $3ir }", true)                                                                 \
    X(DST_64, M3xrrr, "dst.64 $1ir $2ir $3ir }", true)                                                                 \
    X(DST_REF, M3xrrr, "dst.ref $1ir $2ir $3ir }", true)                                                               \
    X(DST_F32, M3xrrr, "dst.f32 $1fr $2ir $3ir }", true)                                                               \
    X(DST_F64, M3xrrr, "dst.f64 $1fr $2ir $3ir }", true)                                                               \
    X(DST_GENERIC, M3rrrr, "dst.g $1ir $2ir $3ir }", true)                                                             \
    X(DSTI_8_8, M3rri8, "dsti.8.8 $0ir $1ir $2U8 }", true)                                                             \
    X(DSTI_16_8, M3rri8, "dsti.16.8 $0ir $1ir $2U8 }", true)                                                           \
    X(DSTI_16_16, M4rri16, "dsti.16.16 $0ir $1ir $2U16 }", true)                                                       \
    X(DSTI_32_8, M3rri8, "dsti.32.8 $0ir $1ir $2U8 }", true)                                                           \
    X(DSTI_32_16, M4rri16, "dsti.32.16 $0ir $1ir $2U16 }", true)                                                       \
    X(DSTI_32_32, M6rri32, "dsti.32.32 $0ir $1ir $2U32 }", true)                                                       \
    X(DSTI_64_8, M3rri8, "dsti.64.8 $0ir $1ir $2U8 }", true)                                                           \
    X(DSTI_64_16, M4rri16, "dsti.64.16 $0ir $1ir $2U16 }", true)                                                       \
    X(DSTI_64_32, M6rri32, "dsti.64.32 $0ir $1ir $2U32 }", true)                                                       \
    X(DSTI_64_64, M10rri64, "dsti.64.64 $0ir $1ir $2U64 }", true)                                                      \
    X(SLD_U8, M2rr, "sld.u8 $0ir $1ir }", true)                                                                        \
    X(SLD_U16, M2rr, "sld.u16 $0ir $1ir }", true)                                                                      \
    X(SLD_32, M2rr, "sld.u32 $0ir $1ir }", true)                                                                       \
    X(SLD_S8, M2rr, "sld.s8 $0ir $1ir }", true)                                                                        \
    X(SLD_S16, M2rr, "sld.s16 $0ir $1ir }", true)                                                                      \
    X(SLD_F32, M2rr, "sld.f32 $0fr $1ir }", true)                                                                      \
    X(SLD_F64, M2rr, "sld.f64 $0fr $1ir }", true)                                                                      \
    X(SLD_64, M2rr, "sld.64 $0ir $1ir }", true)                                                                        \
    X(SLD_S32TO64, M2rr, "sld.s32to64 $0ir $1ir }", true)                                                              \
    X(SLD_REF, M2rr, "sld.ref $0ir $1ir }", true)                                                                      \
    X(SLD_LEA, M2rr, "sld.lea $0ir $1ir }", true)                                                                      \
    X(SST_8, M2rr, "sst.8 $0ir $1ir }", true)                                                                          \
    X(SST_16, M2rr, "sst.16 $0ir $1ir }", true)                                                                        \
    X(SST_32, M2rr, "sst.32 $0ir $1ir }", true)                                                                        \
    X(SST_64, M2rr, "sst.64 $0ir $1ir }", true)                                                                        \
    X(SST_REF, M2rr, "sst.ref $0ir $1ir }", true)                                                                      \
    X(SST_F32, M2rr, "sst.f32 $0fr $1ir }", true)                                                                      \
    X(SST_F64, M2rr, "sst.f64 $0fr $1ir }", true)                                                                      \
    X(SSTI_8_8, M3xri8, "ssti.8.8 $1ir $2U8 }", true)                                                                  \
    X(SSTI_16_8, M3xri8, "ssti.16.8 $1ir $2U8 }", true)                                                                \
    X(SSTI_16_16, M4xri16, "ssti.16.16 $1ir $2U16 }", true)                                                            \
    X(SSTI_32_8, M3xri8, "ssti.32.8 $1ir $2U8 }", true)                                                                \
    X(SSTI_32_16, M4xri16, "ssti.32.16 $1ir $2U16 }", true)                                                            \
    X(SSTI_32_32, M6xri32, "ssti.32.32 $1ir $2U32 }", true)                                                            \
    X(SSTI_64_8, M3xri8, "ssti.64.8 $1ir $2U8 }", true)                                                                \
    X(SSTI_64_16, M4xri16, "ssti.64.16 $1ir $2U16 }", true)                                                            \
    X(SSTI_64_32, M6xri32, "ssti.64.32 $1ir $2U32 }", true)                                                            \
    X(SSTI_64_64, M10xri64, "ssti.64.64 $1ir $2U64 }", true)                                                           \
    X(FLD_U8, M2rr, "fld.u8 $0ir $1ir }", true)                                                                        \
    X(FLD_U16, M2rr, "fld.u16 $0ir $1ir }", true)                                                                      \
    X(FLD_32, M2rr, "fld.u32 $0ir $1ir }", true)                                                                       \
    X(FLD_S8, M2rr, "fld.s8 $0ir $1ir }", true)                                                                        \
    X(FLD_S16, M2rr, "fld.s16 $0ir $1ir }", true)                                                                      \
    X(FLD_F32, M2rr, "fld.f32 $0fr $1ir }", true)                                                                      \
    X(FLD_F64, M2rr, "fld.f64 $0fr $1ir }", true)                                                                      \
    X(FLD_64, M2rr, "fld.64 $0ir $1ir }", true)                                                                        \
    X(FLD_S32TO64, M2rr, "fld.s32to64 $0ir $1ir }", true)                                                              \
    X(FLD_REF, M2rr, "fld.ref $0ir $1ir }", true)                                                                      \
    X(FLD_LEA, M2rr, "fld.lea $0ir $1ir }", true)                                                                      \
    X(FST_8, M2rr, "fst.8 $0ir $1ir }", true)                                                                          \
    X(FST_16, M2rr, "fst.16 $0ir $1ir }", true)                                                                        \
    X(FST_32, M2rr, "fst.32 $0ir $1ir }", true)                                                                        \
    X(FST_64, M2rr, "fst.64 $0ir $1ir }", true)                                                                        \
    X(FST_REF, M2rr, "fst.ref $0ir $1ir }", true)                                                                      \
    X(FST_F32, M2rr, "fst.f32 $0fr $1ir }", true)                                                                      \
    X(FST_F64, M2rr, "fst.f64 $0fr $1ir }", true)                                                                      \
    X(FSTI_8_8, M2i8, "fsti.8.8 $0U8 }", true)                                                                         \
    X(FSTI_16_8, M2i8, "fsti.16.8 $0U8 }", true)                                                                       \
    X(FSTI_16_16, M3i16, "fsti.16.16 $0U16 }", true)                                                                   \
    X(FSTI_32_8, M2i8, "fsti.32.8 $0U8 }", true)                                                                       \
    X(FSTI_32_16, M3i16, "fsti.32.16 $0U16 }", true)                                                                   \
    X(FSTI_32_32, M5i32, "fsti.32.32 $0U32 }", true)                                                                   \
    X(FSTI_64_8, M2i8, "fsti.64.8 $0U8 }", true)                                                                       \
    X(FSTI_64_16, M3i16, "fsti.64.16 $0U16 }", true)                                                                   \
    X(FSTI_64_32, M5i32, "fsti.64.32 $0U32 }", true)                                                                   \
    X(FSTI_64_64, M9i64, "fsti.64.64 $0U64 }", true)                                                                   \
    X(COPY_REC_FROM_OBJ, MStructFieldOp, "reg.copy.from.obj $0ir $1ir $2U64 }", true)                                  \
    X(COPY_REC_FROM_REC, MStructFieldOp, "reg.copy.from.rec $0ir $1ir $2U64 }", true)                                  \
    X(COPY_REC_TO_OBJ, MStructFieldOp, "reg.copy.to.obj $0ir $1ir $2U64 }", true)                                      \
    X(COPY_REC_TO_REC, MStructFieldOp, "reg.copy.to.rec $0ir $1ir $2U64 }", true)

namespace Cbc {
namespace RT {

constexpr int LIT_TABLE_SIZE = 4096;

class Opcode {
public:
#define DEFINE_OPCODE(opc, dfmt, sfmt) opc,

    enum Value : uint8_t {
        CBC_RT_OPCODES(DEFINE_OPCODE) OPCODE_NUM
    };

#undef DEFINE_OPCODE

    static_assert(OPCODE_NUM <= 256);

    constexpr Opcode(const Value value) : _value(value) {}

    constexpr Opcode(const uint8_t raw) : _value(static_cast<Value>(raw)) {}

    constexpr Opcode() : _value(HALT) {}

    constexpr operator Value() const { return _value; }

    inline static Opcode Decode(Decoder::ByteReader& reader)
    {
        uint8_t b = reader.Read8();
        return Opcode(b);
    }

private:
    Value _value;
};

/// ISA12 is encoding a number of different mem.head operations
/// which are describing the kind of a base.
/// This is not convenient for interpretation,
/// since the actual value is only needed at the tail of memspace
/// in the operation itself (and sometimes it is not needed at all).
///
/// So, the ISA12 would require from memspace interpreter to store
/// the state of a head all the way to the tail.
///
/// Instead, we will encode memspace command as one `MEMSPACE` opcode,
/// which will enter the memspace, accumulate the offset and
/// use it to perform the actual operation.
///
/// Additionally, we would expect that the whole MEMSPACE instruction
/// will not throw any exception and must execute without errors from start to finish.
class MemOpcode {
public:
#define DEFINE_OPCODE(opc, dfmt, sfmt, isTail) opc,

    enum Value : uint8_t {
        CBC_RT_MEMOPCODES(DEFINE_OPCODE) OPCODE_NUM
    };

#undef DEFINE_OPCODE

    static constexpr auto RLD_START_OPCODE  = RLD_U8;
    static constexpr auto RLD_END_OPCODE    = RLD_LEA;
    static constexpr auto DLD_START_OPCODE  = DLD_U8;
    static constexpr auto DLD_END_OPCODE    = DLD_LEA;
    static constexpr auto SLD_START_OPCODE  = SLD_U8;
    static constexpr auto SLD_END_OPCODE    = SLD_LEA;
    static constexpr auto FLD_START_OPCODE  = FLD_U8;
    static constexpr auto FLD_END_OPCODE    = FLD_LEA;
    static constexpr auto RST_START_OPCODE  = RST_8;
    static constexpr auto RST_END_OPCODE    = RST_F64;
    static constexpr auto DST_START_OPCODE  = DST_8;
    static constexpr auto DST_END_OPCODE    = DST_F64;
    static constexpr auto SST_START_OPCODE  = SST_8;
    static constexpr auto SST_END_OPCODE    = SST_F64;
    static constexpr auto FST_START_OPCODE  = FST_8;
    static constexpr auto FST_END_OPCODE    = FST_F64;
    static constexpr auto RSTI_START_OPCODE = RSTI_8_8;
    static constexpr auto RSTI_END_OPCODE   = RSTI_64_64;
    static constexpr auto DSTI_START_OPCODE = DSTI_8_8;
    static constexpr auto DSTI_END_OPCODE   = DSTI_64_64;
    static constexpr auto SSTI_START_OPCODE = SSTI_8_8;
    static constexpr auto SSTI_END_OPCODE   = SSTI_64_64;
    static constexpr auto FSTI_START_OPCODE = FSTI_8_8;
    static constexpr auto FSTI_END_OPCODE   = FSTI_64_64;

    static_assert(OPCODE_NUM <= 256);

    constexpr MemOpcode(const Value value) : _value(value) {}

    constexpr MemOpcode(const uint32_t raw) : _value(static_cast<Value>(raw)) {}

    constexpr MemOpcode() : _value(MEM_HALT) {}

    constexpr operator Value() const { return _value; }

    inline static MemOpcode Decode(Decoder::ByteReader& reader)
    {
        uint8_t b = reader.Read8();
        return MemOpcode(b);
    }

private:
    Value _value;
};

class ImmKind {
public:
    enum Value : uint32_t {
        VALUE   = 0b00,
        LITERAL = 0b01,
    };

    constexpr ImmKind(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

private:
    Value _value;
};

struct B1 {
    Opcode opc;

    static B1 Decode(Decoder::ByteReader& reader) { return B1 { Opcode::Decode(reader) }; }
};

struct B2rr {
    Opcode opc;
    Format::RR rr;

    inline static B2rr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return B2rr { opc, rr };
    }
};

struct B2xr {
    Opcode opc;
    Format::XR xr;

    inline static B2xr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto xr  = Format::XR::Decode(reader);
        return B2xr { opc, xr };
    }
};

struct B3xri8 {
    static constexpr int SIZE = 3;

    Opcode opc;
    Format::XR xr;
    Format::Imm8 imm8;

    static B3xri8 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xr   = Format::XR::Decode(reader);
        auto imm8 = Format::Imm8::Decode(reader);
        return B3xri8 { opc, xr, imm8 };
    }
};

struct B4xri16 {
    Opcode opc;
    Format::XR xr;
    Format::Imm16 imm;

    static B4xri16 Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto xr  = Format::XR::Decode(reader);
        auto imm = Format::Imm16::Decode(reader);
        return B4xri16 { opc, xr, imm };
    }
};

struct B3xrrr {
    Opcode opc;
    Format::XR xr;
    Format::RR rr;

    static B3xrrr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto xr  = Format::XR::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return B3xrrr { opc, xr, rr };
    }
};

struct B3xxrr {
    Opcode opc;
    Format::XX xx;
    Format::RR rr;

    static B3xxrr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto xx  = Format::XX::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return B3xxrr { opc, xx, rr };
    }
};

struct B3xi12 {
    static constexpr int SIZE = 3;

    Opcode opc;
    Format::XImm12 xi12;

    static B3xi12 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        return B3xi12 { opc, xi12 };
    }
};

struct B3rrrr {
    Opcode opc;
    Format::RR rr1;
    Format::RR rr2;

    static B3rrrr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto rr1 = Format::RR::Decode(reader);
        auto rr2 = Format::RR::Decode(reader);
        return B3rrrr { opc, rr1, rr2 };
    }
};

struct B4xi12rr {
    static constexpr int SIZE = 4;

    Opcode opc;
    Format::XImm12 xi12;
    Format::RR rr;

    static B4xi12rr Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        auto rr   = Format::RR::Decode(reader);
        return B4xi12rr { opc, xi12, rr };
    }
};

struct B4xi12xr {
    Opcode opc;
    Format::XImm12 xi12;
    Format::XR xr;

    static B4xi12xr Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        auto xr   = Format::XR::Decode(reader);
        return B4xi12xr { opc, xi12, xr };
    }
};

struct BinaryChecked {
    Opcode opc;
    Format::Checked op;
    Format::RR rr;
    Format::Imm64 imm;

    static BinaryChecked Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto op  = Format::Imm8::Decode(reader).Checked();
        auto rr  = Format::RR::Decode(reader);
        auto imm = Format::Imm64::Decode(reader);
        return BinaryChecked { opc, op, rr, imm };
    }
};

struct BFX {
    static constexpr int SIZE = 4;

    Opcode opc;
    Format::RR rr;
    uint8_t offs;
    uint8_t size;

    static BFX Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto rr   = Format::RR::Decode(reader);
        auto offs = reader.Read8();
        auto size = reader.Read8();
        return BFX { opc, rr, offs, size };
    }
};

struct IOF {
    Opcode opc;
    Format::RR rr;
    uint64_t imm64;

    static IOF Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto rr    = Format::RR::Decode(reader);
        auto imm64 = reader.Read64();
        return IOF { opc, rr, imm64 };
    }
};

struct StructFieldOp {
    Opcode opc;
    Format::RR rr;
    Format::RR field;
    RTSupport::TypeInfo ti;

    static StructFieldOp Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto rr    = Format::RR::Decode(reader);
        auto field = Format::RR::Decode(reader);
        auto ti    = reader.Read<RTSupport::TypeInfo>();
        return StructFieldOp { opc, rr, field, ti };
    }
};

struct CopyFieldOp {
    Opcode opc;
    Format::RR rr;
    Format::Imm32 offset;
    Format::Imm32 size;

    static CopyFieldOp Decode(Decoder::ByteReader& reader)
    {
        auto opc    = Opcode::Decode(reader);
        auto rr     = Format::RR::Decode(reader);
        auto offset = Format::Imm32::Decode(reader);
        auto size   = Format::Imm32::Decode(reader);
        return CopyFieldOp { opc, rr, offset, size };
    }
};

struct CopyDerived {
    Opcode opc;
    Format::RR dst;
    Format::RR src;
    RTSupport::TypeInfo ti;

    static CopyDerived Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto dst = Format::RR::Decode(reader);
        auto src = Format::RR::Decode(reader);
        auto ti  = reader.Read<RTSupport::TypeInfo>();
        return CopyDerived { opc, dst, src, ti };
    }
};

struct CopyDerivedGeneric {
    Opcode opc;
    Format::RR dst;
    Format::RR src;
    Format::RR ti;

    static CopyDerivedGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto dst = Format::RR::Decode(reader);
        auto src = Format::RR::Decode(reader);
        auto ti  = Format::RR::Decode(reader);
        return CopyDerivedGeneric { opc, dst, src, ti };
    }
};

struct Index {
    Opcode opc;
    Format::RR rr;
    Format::XR idx;
    RTSupport::TypeInfo ti;

    static Index Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        auto idx = Format::XR::Decode(reader);
        auto ti  = reader.Read<RTSupport::TypeInfo>();
        return Index { opc, rr, idx, ti };
    }
};

struct IndexGeneric {
    Opcode opc;
    Format::RR rr;
    Format::RR idx;

    static IndexGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        auto idx = Format::RR::Decode(reader);
        return IndexGeneric { opc, rr, idx };
    }
};

struct MStructFieldOp {
    MemOpcode opc;
    Format::RR rr;
    RTSupport::TypeInfo ti;

    static MStructFieldOp Decode(Decoder::ByteReader& reader)
    {
        auto opc = MemOpcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        auto ti  = reader.Read<RTSupport::TypeInfo>();
        return MStructFieldOp { opc, rr, ti };
    }
};

struct B5xi12ri12 {
    static constexpr int SIZE = 5;

    Opcode opc;
    Format::XImm12 xi12;
    Format::RImm12 ri12;

    static B5xi12ri12 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        auto ri12 = Format::RImm12::Decode(reader);
        return B5xi12ri12 { opc, xi12, ri12 };
    }
};

struct VirtualCall {
    static constexpr int SIZE = 5;

    Opcode opc;
    uint16_t vnum;
    uint16_t edef;
    uint8_t sret;

    static VirtualCall Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto vnum = reader.Read16();
        auto edef = reader.Read16();
        auto sret = reader.Read8();
        return VirtualCall { opc, vnum, edef, sret };
    }
};

struct AtomicOp {
    static constexpr int SIZE = 5;

    Opcode opc;
    Format::RR rr1;
    Format::RR rr2;
    uint16_t offset;

    static AtomicOp Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto rr1   = Format::RR::Decode(reader);
        auto rr2   = Format::RR::Decode(reader);
        auto field = reader.Read16();
        return AtomicOp { opc, rr1, rr2, field };
    }
};

struct B5i32 {
    static constexpr int SIZE = 5;

    Opcode opc;
    Format::Imm32 imm32;

    static B5i32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto imm32 = Format::Imm32::Decode(reader);
        return B5i32 { opc, imm32 };
    }
};

struct B6xri32 {
    static constexpr int SIZE = 6;

    Opcode opc;
    Format::XR xr;
    Format::Imm32 imm32;

    static B6xri32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto xr    = Format::XR::Decode(reader);
        auto imm32 = Format::Imm32::Decode(reader);
        return B6xri32 { opc, xr, imm32 };
    }
};

struct Offset {
    static constexpr int SIZE = 6;

    Opcode opc;
    Format::RR rr;
    uint32_t idx;

    static Offset Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        auto imm = reader.Read32();
        return Offset { opc, rr, imm };
    }
};

struct B9i64 {
    Opcode opc;
    Format::Imm64 imm64;

    static B9i64 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto imm64 = Format::Imm64::Decode(reader);
        return B9i64 { opc, imm64 };
    }
};

struct B7xrrri32 {
    Opcode opc;
    Format::XR xr;
    Format::RR rr;
    Format::Imm32 imm32;

    static B7xrrri32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto xr    = Format::XR::Decode(reader);
        auto rr    = Format::RR::Decode(reader);
        auto imm32 = Format::Imm32::Decode(reader);
        return B7xrrri32 { opc, xr, rr, imm32 };
    }
};

struct B13i64i32 {
    Opcode opc;
    Format::Imm64 imm64;
    Format::Imm32 imm32;

    static B13i64i32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto imm64 = Format::Imm64::Decode(reader);
        auto imm32 = Format::Imm32::Decode(reader);
        return B13i64i32 { opc, imm64, imm32 };
    }
};

struct B10xri64 {
    static constexpr int SIZE = 9;

    Opcode opc;
    Format::XR xr;
    Format::Imm64 imm64;

    static B10xri64 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto xr    = Format::XR::Decode(reader);
        auto imm64 = Format::Imm64::Decode(reader);
        return B10xri64 { opc, xr, imm64 };
    }
};

struct InterfaceCall {
    Opcode opc;
    uint16_t vnum;
    uint64_t ti;
    uint8_t sret;

    static InterfaceCall Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto vnum = reader.Read16();
        auto ti   = reader.Read64();
        auto sret = reader.Read8();
        return InterfaceCall { opc, vnum, ti, sret };
    }
};

struct InterfaceCallGeneric {
    Opcode opc;
    uint16_t vnum;
    uint16_t argn;
    uint8_t sret; // TODO: add two instruction for sret/non-sret versions

    static InterfaceCallGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto vnum = reader.Read16();
        auto argn = reader.Read16();
        auto sret = reader.Read8();
        return InterfaceCallGeneric { opc, vnum, argn, sret };
    }
};

struct M1 {
    MemOpcode opc;

    static M1 Decode(Decoder::ByteReader& reader) { return M1 { MemOpcode::Decode(reader) }; }
};

struct M2i8 {
    MemOpcode opc;
    uint8_t imm8;

    inline static M2i8 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = MemOpcode::Decode(reader);
        auto imm8 = reader.Read8();
        return M2i8 { opc, imm8 };
    }
};

struct M3i16 {
    MemOpcode opc;
    uint16_t imm16;

    inline static M3i16 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto imm16 = reader.Read16();
        return M3i16 { opc, imm16 };
    }
};

struct M5i32 {
    MemOpcode opc;
    uint32_t imm32;

    inline static M5i32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto imm32 = reader.Read32();
        return M5i32 { opc, imm32 };
    }
};

struct M9i64 {
    MemOpcode opc;
    uint64_t imm64;

    inline static M9i64 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto imm64 = reader.Read64();
        return M9i64 { opc, imm64 };
    }
};

struct M3xri8 {
    MemOpcode opc;
    Format::XR xr;
    Format::Imm8 imm8;

    inline static M3xri8 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = MemOpcode::Decode(reader);
        auto xr   = Format::XR::Decode(reader);
        auto imm8 = Format::Imm8::Decode(reader);
        return M3xri8 { opc, xr, imm8 };
    }
};

struct M4xri16 {
    MemOpcode opc;
    Format::XR xr;
    Format::Imm16 imm16;

    inline static M4xri16 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto xr    = Format::XR::Decode(reader);
        auto imm16 = Format::Imm16::Decode(reader);
        return M4xri16 { opc, xr, imm16 };
    }
};

struct M6xri32 {
    MemOpcode opc;
    Format::XR xr;
    Format::Imm32 imm32;

    inline static M6xri32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto xr    = Format::XR::Decode(reader);
        auto imm32 = Format::Imm32::Decode(reader);
        return M6xri32 { opc, xr, imm32 };
    }
};

struct M10xri64 {
    MemOpcode opc;
    Format::XR xr;
    Format::Imm64 imm64;

    inline static M10xri64 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto xr    = Format::XR::Decode(reader);
        auto imm64 = Format::Imm64::Decode(reader);
        return M10xri64 { opc, xr, imm64 };
    }
};

struct M2rr {
    MemOpcode opc;
    Format::RR rr;

    inline static M2rr Decode(Decoder::ByteReader& reader)
    {
        auto opc = MemOpcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return M2rr { opc, rr };
    }
};

struct M2xr {
    MemOpcode opc;
    Format::XR xr;

    inline static M2xr Decode(Decoder::ByteReader& reader)
    {
        auto opc = MemOpcode::Decode(reader);
        auto xr  = Format::XR::Decode(reader);
        return M2xr { opc, xr };
    }
};

struct M3xrrr {
    MemOpcode opc;
    Format::XR xr;
    Format::RR rr;

    inline static M3xrrr Decode(Decoder::ByteReader& reader)
    {
        auto opc = MemOpcode::Decode(reader);
        auto xr  = Format::XR::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return M3xrrr { opc, xr, rr };
    }
};

struct M3rrrr {
    MemOpcode opc;
    Format::RR rr1;
    Format::RR rr2;

    inline static M3rrrr Decode(Decoder::ByteReader& reader)
    {
        auto opc = MemOpcode::Decode(reader);
        auto rr1 = Format::RR::Decode(reader);
        auto rr2 = Format::RR::Decode(reader);
        return M3rrrr { opc, rr1, rr2 };
    }
};

struct M3rri8 {
    MemOpcode opc;
    Format::RR rr;
    Format::Imm8 imm8;

    inline static M3rri8 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = MemOpcode::Decode(reader);
        auto rr   = Format::RR::Decode(reader);
        auto imm8 = Format::Imm8::Decode(reader);
        return M3rri8 { opc, rr, imm8 };
    }
};

struct M4rri16 {
    MemOpcode opc;
    Format::RR rr;
    Format::Imm16 imm16;

    inline static M4rri16 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto rr    = Format::RR::Decode(reader);
        auto imm16 = Format::Imm16::Decode(reader);
        return M4rri16 { opc, rr, imm16 };
    }
};

struct M6rri32 {
    MemOpcode opc;
    Format::RR rr;
    Format::Imm32 imm32;

    inline static M6rri32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto rr    = Format::RR::Decode(reader);
        auto imm32 = Format::Imm32::Decode(reader);
        return M6rri32 { opc, rr, imm32 };
    }
};

struct M10rri64 {
    MemOpcode opc;
    Format::RR rr;
    Format::Imm64 imm64;

    inline static M10rri64 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto rr    = Format::RR::Decode(reader);
        auto imm64 = Format::Imm64::Decode(reader);
        return M10rri64 { opc, rr, imm64 };
    }
};

} // namespace RT
} // namespace Cbc
