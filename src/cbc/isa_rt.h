#pragma once

#include "decoder.h"
#include "isa.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/math.h"
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
    X(NEWARR_G, B2rr, "newarr.g IR1, IR2, $0ir")                                                                       \
    X(INITCLOSURE, B1, "init.closure")                                                                                 \
    X(INITCLOSURE_SRET, B1, "init.closure.sret")                                                                       \
    X(SPAWN, B9i64, "spawn $0U64")                                                                                     \
    X(SPAWN_FUTURE, B1, "spawn.future")                                                                                \
    /* Integral Load/Store instructions start */                                                                       \
    X(LOAD_ADDR, B4xri16, "ld.addr.$0ldk $1ir $2U16L")                                                                 \
    X(STORE_ADDR, B4xri16, "st.addr.$0stk $1ir $2U16L")                                                                \
    X(LOAD_OBJ, B4xi12rr, "ld.$0ldk $2ir [$3ir $1U12]")                                                                \
    X(STORE_OBJ, B4xi12rr, "st.$0stk $2ir [$3ir $1U12]")                                                               \
    X(LOAD_ARR, B3xrrr, "ld.arr.$0ldk $1ir $2ir $3ir")                                                                 \
    X(STORE_ARR, B3xrrr, "st.arr.$0stk $1ir $2ir $3ir]")                                                               \
    X(LOAD_REC, B4xi12rr, "ld.rec.$0ldk $2ir [$3ir $1U12]")                                                            \
    X(STORE_REC, B4xi12rr, "st.rec.$0stk $2ir [$3ir $1U12]")                                                           \
    X(LOAD_FRAME, B4xi12rr, "ld.frame.$0ldk $2ir [$3ir $1I12]")                                                        \
    X(STORE_FRAME, B4xi12rr, "st.frame.$0stk $2ir [$3ir $1I12]")                                                       \
    X(LOAD_LONG_DERIVED, B7xrrri32, "ld.derived.$0ldk $1ir [($2ir $3ir) $4U32]")                                       \
    X(STORE_LONG_DERIVED, B7xrrri32, "st.derived.$0stk $1ir [($2ir $3ir) $4U32]")                                      \
    X(LOAD_LONG_REC, B7xrrri32, "ld.rec.$0ldk $1ir [$3ir $4U32]")                                                      \
    X(STORE_LONG_REC, B7xrrri32, "st.rec.$0stk $1ir [$3ir $4U32]")                                                     \
    X(LOAD_LONG_FRAME, B7xrrri32, "ld.frame.$0ldk $1ir [$4I32]")                                                       \
    X(STORE_LONG_FRAME, B7xrrri32, "st.frame.$0stk $1ir [$4I32]")                                                      \
    X(LOAD_GENERIC, B3rrrr, "ld.g $0ir $3ir [($1ir $2ir)]")                                                            \
    X(STORE_GENERIC, StoreGenericOp, "st.g0 $0ir $1ir $2ir $3U32")                                                     \
    X(STORE_GENERIC_1, StoreGenericOp, "st.g1 $0ir $1ir $2ir $3U32")                                                   \
    X(STORE_GENERIC_4, StoreGenericOp, "st.g4 $0ir $1ir $2ir $3U32")                                                   \
    X(LEA_GENERIC, B7xrrri32, "lea.g $1ir $3ir [$2ir ord=$4U32]")                                                      \
    X(INIT_NONE_GENERIC_OPT0, InitOption, "init.none.g0 $1ir")                                                         \
    X(INIT_SOME_GENERIC_OPT0, InitOption, "init.some.g0 $0ir $1ir")                                                    \
    X(INIT_SOME_GENERIC_OPT1, InitOption, "init.some.g1 $0ir $1ir")                                                    \
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
    X(CHECKED_CONVERT, B3xxrr, "checked.convert $0ct $1ct $2ir $3ir")                                                  \
    X(CALL_CLOSURE, B1, "call.closure")                                                                                \
    X(CALL_CLOSURE_SRET, B1, "call.closure.sret")                                                                      \
    X(CALL_CLOSURE_GENERIC, B1, "call.closure.g")                                                                      \
    X(DIRECT_CALL_2I, B3xi12, "call.2i $1I12L")                                                                        \
    X(DIRECT_CALL_2C, B3xi12, "call.2c $1I12L")                                                                        \
    X(DIRECT_CALL_2N, B4xi12i8, "call.2n $1I12L ($0I4, $2U8)")                                                         \
    X(CALL_CFUNC, B3xri8, "call.cfunc $1ir ($0I4, $2U8)")                                                              \
    X(VIRTUAL_CALL, VirtualCall, "vcall $0U16 $1U16")                                                                  \
    X(INTERFACE_CALL, InterfaceCall, "icall.$2U8 $0U16 $1U64 outer=$3U16 this=$4U16")                                  \
    X(INTERFACE_CALL_GENERIC, InterfaceCallGeneric, "icall.g.$1U8 $0U16 outer=$2U16 this=$3U16")                       \
    X(GC_POINT, B1, "gcpoint")                                                                                         \
    X(BFXS, BFX, "bfxs $0ir $1ir $2U8 $3U8")                                                                           \
    X(BFXZ, BFX, "bfxz $0ir $1ir $2U8 $3U8")                                                                           \
    X(STRING_INIT, B13i64i32, "string.init $0U64 $1U32")                                                               \
    X(NULLCHECK, B2xr, "nullcheck $1ir")                                                                               \
    X(DIVCHECK, B2xr, "divcheck $1ir")                                                                                 \
    X(LOAD_GENERIC_TI, B9i64, "load.generic.ti $0U64")                                                                 \
    X(IOF, IOF, "iof $0ir $1ir $2U64")                                                                                 \
    X(NEWBOX, B2xr, "newbox $0U8")                                                                                     \
    X(NEWBOX2, B9i64, "newbox2 $0U64")                                                                                 \
    X(UNBOX_T, B6xri32, "unbox.t $1ir $2U32")                                                                          \
    X(OFFSET, B6xri32, "offset $1ir $2U32 }")                                                                          \
    X(READ_STRUCT_FIELD, StructFieldOp, "read.struct.field $0ir $1ir $2U8 $4U64")                                      \
    X(WRITE_STRUCT_FIELD, StructFieldOp, "write.struct.field $0ir $1ir $2U8 $4U64")                                    \
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
    X(COPY_LOCAL, CopyLocal, "copy.local $0ir $1ir $2U64")                                                             \
    X(COPY_TO_LOCAL, CopyDerived, "copy.to.local $1ir $2ir $3ir $4U64")                                                \
    X(COPY_FROM_LOCAL, CopyDerived, "copy.from.local $0ir $1ir $3ir $4U64")                                            \
    X(COPY_LOCAL_GENERIC, CopyLocalGeneric, "copy.local.g $0ir $1ir $2ir")                                             \
    X(COPY_TO_LOCAL_GENERIC, CopyLocalGeneric, "copy.to.local.g $0ir $1ir $2ir $3ir")                                  \
    X(COPY_FROM_LOCAL_GENERIC, CopyLocalGeneric, "copy.from.local.g $0ir $1ir $2ir $3ir")                              \
    X(COPY_TYPESLOT, CopyTypeSlot, "copy.typed $0I32 $1I32 $2U64")                                                     \
    X(COPY_TO_TYPESLOT, CopyToTypeSlot, "copy.to.typed $0I32 $1ir $2ir $3U64")                                         \
    X(COPY_FROM_TYPESLOT, CopyFromTypeSlot, "copy.from.typed $0ir $1ir $2I32 $3U64")                                   \
    X(COPY_TYPESLOT_GENERIC, CopyTypeSlotGeneric, "copy.typed.g $0I32 $1I32 $2ir")                                     \
    X(COPY_TO_TYPESLOT_GENERIC, CopyToTypeSlotGeneric, "copy.to.typed.g $0I32 $1ir $2ir $3ir")                         \
    X(COPY_FROM_TYPESLOT_GENERIC, CopyFromTypeSlotGeneric, "copy.from.typed.g $0ir $1ir $2I32 $3ir")                   \
    X(INDEX, Index, "index $0ir [$1ir $2ir] $3U64")                                                                    \
    X(INDEX_GENERIC, IndexGeneric, "index.g $0ir [$1ir $2ir] $3ir")                                                    \
    X(CONST_INDEX_GENERIC, ConstIndexGeneric, "index.g $0ir [$1ir $2I64] $3ir")                                        \
    X(SBIN8, B3xrrr, "$0sbin.8 $1ir $2ir $3ir")                                                                        \
    X(SBIN16, B3xrrr, "$0sbin.16 $1ir $2ir $3ir")                                                                      \
    X(SBIN32, B3xrrr, "$0sbin.32 $1ir $2ir $3ir")                                                                      \
    X(SBIN64, B3xrrr, "$0sbin.64 $1ir $2ir $3ir")                                                                      \
    X(SBINIMM8, B4xi12rr, "$0sbin.8 $2ir $3ir $1I12")                                                                  \
    X(SBINIMM16, B4xi12rr, "$0sbin.16 $2ir $3ir $1I12")                                                                \
    X(SBINIMM32, B4xi12rr, "$0sbin.32 $2ir $3ir $1I12")                                                                \
    X(SBINIMM64, B4xi12rr, "$0sbin.64 $2ir $3ir $1I12")                                                                \
    X(SBINIMM8W, BinarySaturating, "$0sbin.8 $2ir $3ir $1I64")                                                         \
    X(SBINIMM16W, BinarySaturating, "$0sbin.16 $2ir $3ir $1I64")                                                       \
    X(SBINIMM32W, BinarySaturating, "$0sbin.32 $2ir $3ir $1I64")                                                       \
    X(SBINIMM64W, BinarySaturating, "$0sbin.64 $2ir $3ir $1I64")                                                       \
    X(STK_PARAM, StackParam, "st.stack.param $0ir $1U16")                                                              \
    X(STK_PARAM_F, StackParamF, "st.stack.param.f $0fr $1U16")                                                         \
    X(STK_PARAM_S16, StackParamS16, "st.stack.param.s.16 $0I64 $1U16")                                                 \
    X(STK_PARAM_S32, StackParamS32, "st.stack.param.s.32 $0I64 $1U16")                                                 \
    X(LEA_STACK_PARAM, LeaStackParam, "lea.stack.param $0ir $1U16")                                                    \
    X(STORE_FRAME_IMM_1, StoreFrameImm, "st.frame.imm.1 $0stk $1U8 $2U32")                                             \
    X(STORE_FRAME_IMM_2, StoreFrameImm, "st.frame.imm.2 $0stk $1U16 $2U32")                                            \
    X(STORE_FRAME_IMM_4, StoreFrameImm, "st.frame.imm.4 $0stk $1U32 $2U32")                                            \
    X(STORE_FRAME_IMM_8, StoreFrameImm, "st.frame.imm.8 $0stk $1U64 $2U32")

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

struct B4xi12i8 {
    static constexpr int SIZE = 4;

    Opcode opc;
    Format::XImm12 xi12;
    Format::Imm8 imm8;

    static B4xi12i8 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        auto imm8 = Format::Imm8::Decode(reader);
        return B4xi12i8 { opc, xi12, imm8 };
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

// 2 bytes: opcode + one byte packing `option` (low nibble) and
// `underlyingTi` (high nibble). The discriminant-byte flag is baked into
// the opcode itself (OPT0/OPT1) rather than encoded as a register.
struct InitOption {
    Opcode opc;
    IReg src;
    IReg underlyingTi;

    static InitOption Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        uint8_t b = reader.Read8();
        return InitOption { opc, IReg::From(b & 0xf), IReg::From(b >> 4) };
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

struct BinarySaturating {
    Opcode opc;
    Format::Saturating op;
    Format::RR rr;
    Format::Imm64 imm;

    static BinarySaturating Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto op  = Format::Imm8::Decode(reader).Saturating();
        auto rr  = Format::RR::Decode(reader);
        auto imm = Format::Imm64::Decode(reader);
        return BinarySaturating { opc, op, rr, imm };
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
    uint8_t offset;
    RTSupport::TypeInfo ti;

    static StructFieldOp Decode(Decoder::ByteReader& reader)
    {
        auto opc    = Opcode::Decode(reader);
        auto rr     = Format::RR::Decode(reader);
        auto offset = reader.Read8();
        auto ti     = reader.Read<RTSupport::TypeInfo>();
        return StructFieldOp { opc, rr, offset, ti };
    }
};

struct StoreGenericOp {
    Opcode opc;
    IReg src;
    IReg base;
    IReg derived;
    IReg ti;
    uint32_t offset;

    static StoreGenericOp Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        switch (opc) {
            case Opcode::STORE_GENERIC:   return Decode0(reader);
            case Opcode::STORE_GENERIC_1: return Decode1(reader);
            case Opcode::STORE_GENERIC_4: return Decode4(reader);
            default:                      FATAL("unexpected opcode: %u", opc);
        }
    }

    static StoreGenericOp Decode0(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        ASSERT(opc == Opcode::STORE_GENERIC);
        auto rr1 = Format::RR::Decode(reader);
        auto rr2 = Format::RR::Decode(reader);
        return StoreGenericOp { opc, rr1.x.IR(), rr1.y.IR(), rr2.x.IR(), rr2.y.IR(), 0 };
    }

    static StoreGenericOp Decode1(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        ASSERT(opc == Opcode::STORE_GENERIC_1);
        auto rr1    = Format::RR::Decode(reader);
        auto rr2    = Format::RR::Decode(reader);
        auto offset = reader.Read8();
        return StoreGenericOp { opc, rr1.x.IR(), rr1.y.IR(), rr2.x.IR(), rr2.y.IR(), offset };
    }

    static StoreGenericOp Decode4(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        ASSERT(opc == Opcode::STORE_GENERIC_4);
        auto rr1    = Format::RR::Decode(reader);
        auto rr2    = Format::RR::Decode(reader);
        auto offset = reader.Read32();
        return StoreGenericOp { opc, rr1.x.IR(), rr1.y.IR(), rr2.x.IR(), rr2.y.IR(), offset };
    }
};

struct StoreFrameImm {
    Opcode opc;
    Format::StoreAccessKind stk;
    uint64_t imm;
    int32_t offs;

    static StoreFrameImm Decode(Decoder::ByteReader& reader)
    {
        auto opc = reader.PeekOpcode();
        switch (opc) {
            case Opcode::STORE_FRAME_IMM_1: return Decode1(reader);
            case Opcode::STORE_FRAME_IMM_2: return Decode2(reader);
            case Opcode::STORE_FRAME_IMM_4: return Decode4(reader);
            case Opcode::STORE_FRAME_IMM_8: return Decode8(reader);
            default:                        FATAL("unexpected opcode: %u", opc);
        }
    }

    static StoreFrameImm Decode1(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        ASSERT(opc == Opcode::STORE_FRAME_IMM_1);
        auto stk  = Format::StoreAccessKind::From(reader.Read8());
        auto imm  = reader.Read8();
        auto offs = static_cast<int32_t>(reader.Read32());
        ASSERT(MathUtils::IsNBitsSigned(imm, 8));
        return StoreFrameImm { opc, stk, imm, offs };
    }

    static StoreFrameImm Decode2(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        ASSERT(opc == Opcode::STORE_FRAME_IMM_2);
        auto stk  = Format::StoreAccessKind::From(reader.Read8());
        auto imm  = reader.Read16();
        auto offs = static_cast<int32_t>(reader.Read32());
        ASSERT(MathUtils::IsNBitsSigned(imm, 16));
        return StoreFrameImm { opc, stk, imm, offs };
    }

    static StoreFrameImm Decode4(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        ASSERT(opc == Opcode::STORE_FRAME_IMM_4);
        auto stk  = Format::StoreAccessKind::From(reader.Read8());
        auto imm  = reader.Read32();
        auto offs = static_cast<int32_t>(reader.Read32());
        ASSERT(MathUtils::IsNBitsSigned(imm, 32));
        return StoreFrameImm { opc, stk, imm, offs };
    }

    static StoreFrameImm Decode8(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        ASSERT(opc == Opcode::STORE_FRAME_IMM_8);
        auto stk  = Format::StoreAccessKind::From(reader.Read8());
        auto imm  = reader.Read64();
        auto offs = static_cast<int32_t>(reader.Read32());
        ASSERT(MathUtils::IsNBitsSigned(imm, 64));
        return StoreFrameImm { opc, stk, imm, offs };
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

struct CopyLocal {
    Opcode opc;
    Format::RR records;
    RTSupport::TypeInfo ti;

    static CopyLocal Decode(Decoder::ByteReader& reader)
    {
        auto opc     = Opcode::Decode(reader);
        auto records = Format::RR::Decode(reader);
        auto ti      = reader.Read<RTSupport::TypeInfo>();
        return CopyLocal { opc, records, ti };
    }
};

struct CopyLocalGeneric {
    Opcode opc;
    Format::RR first;
    Format::RR second;

    static CopyLocalGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc    = Opcode::Decode(reader);
        auto first  = Format::RR::Decode(reader);
        auto second = Format::RR::Decode(reader);
        return CopyLocalGeneric { opc, first, second };
    }
};

struct CopyTypeSlot {
    Opcode opc;
    Format::Imm32 dstOffset;
    Format::Imm32 srcOffset;
    RTSupport::TypeInfo ti;

    static CopyTypeSlot Decode(Decoder::ByteReader& reader)
    {
        auto opc       = Opcode::Decode(reader);
        auto dstOffset = Format::Imm32::Decode(reader);
        auto srcOffset = Format::Imm32::Decode(reader);
        auto ti        = reader.Read<RTSupport::TypeInfo>();
        return CopyTypeSlot { opc, dstOffset, srcOffset, ti };
    }
};

struct CopyTypeSlotGeneric {
    Opcode opc;
    Format::Imm32 dstOffset;
    Format::Imm32 srcOffset;
    Format::RR ti;

    static CopyTypeSlotGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc       = Opcode::Decode(reader);
        auto dstOffset = Format::Imm32::Decode(reader);
        auto srcOffset = Format::Imm32::Decode(reader);
        auto ti        = Format::RR::Decode(reader);
        return CopyTypeSlotGeneric { opc, dstOffset, srcOffset, ti };
    }
};

struct CopyToTypeSlot {
    Opcode opc;
    Format::Imm32 dstOffset;
    Format::RR src;
    RTSupport::TypeInfo ti;

    static CopyToTypeSlot Decode(Decoder::ByteReader& reader)
    {
        auto opc       = Opcode::Decode(reader);
        auto dstOffset = Format::Imm32::Decode(reader);
        auto src       = Format::RR::Decode(reader);
        auto ti        = reader.Read<RTSupport::TypeInfo>();
        return CopyToTypeSlot { opc, dstOffset, src, ti };
    }
};

struct CopyToTypeSlotGeneric {
    Opcode opc;
    Format::Imm32 dstOffset;
    Format::RR src;
    Format::RR ti;

    static CopyToTypeSlotGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc       = Opcode::Decode(reader);
        auto dstOffset = Format::Imm32::Decode(reader);
        auto src       = Format::RR::Decode(reader);
        auto ti        = Format::RR::Decode(reader);
        return CopyToTypeSlotGeneric { opc, dstOffset, src, ti };
    }
};

struct CopyFromTypeSlot {
    Opcode opc;
    Format::RR dst;
    Format::Imm32 srcOffset;
    RTSupport::TypeInfo ti;

    static CopyFromTypeSlot Decode(Decoder::ByteReader& reader)
    {
        auto opc       = Opcode::Decode(reader);
        auto dst       = Format::RR::Decode(reader);
        auto srcOffset = Format::Imm32::Decode(reader);
        auto ti        = reader.Read<RTSupport::TypeInfo>();
        return CopyFromTypeSlot { opc, dst, srcOffset, ti };
    }
};

struct CopyFromTypeSlotGeneric {
    Opcode opc;
    Format::RR dst;
    Format::Imm32 srcOffset;
    Format::RR ti;

    static CopyFromTypeSlotGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc       = Opcode::Decode(reader);
        auto dst       = Format::RR::Decode(reader);
        auto srcOffset = Format::Imm32::Decode(reader);
        auto ti        = Format::RR::Decode(reader);
        return CopyFromTypeSlotGeneric { opc, dst, srcOffset, ti };
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

struct ConstIndexGeneric {
    Opcode opc;
    Format::RR rr;
    Format::RR ti;
    Format::Imm64 idx;

    static ConstIndexGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        auto ti  = Format::RR::Decode(reader);
        auto idx = Format::Imm64::Decode(reader);
        return ConstIndexGeneric { opc, rr, ti, idx };
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
    uint16_t outerTiArg;
    uint16_t thisTiArg; // -1 means instance dispatch; otherwise a register or outgoing stack slot.

    static InterfaceCall Decode(Decoder::ByteReader& reader)
    {
        auto opc        = Opcode::Decode(reader);
        auto vnum       = reader.Read16();
        auto ti         = reader.Read64();
        auto sret       = reader.Read8();
        auto outerTiArg = reader.Read16();
        auto thisTiArg  = reader.Read16();
        return InterfaceCall { opc, vnum, ti, sret, outerTiArg, thisTiArg };
    }
};

struct InterfaceCallGeneric {
    Opcode opc;
    uint16_t vnum;
    uint8_t sret; // TODO: add two instruction for sret/non-sret versions
    uint16_t outerTiArg;
    uint16_t thisTiArg; // -1 means instance dispatch.

    static InterfaceCallGeneric Decode(Decoder::ByteReader& reader)
    {
        auto opc        = Opcode::Decode(reader);
        auto vnum       = reader.Read16();
        auto sret       = reader.Read8();
        auto outerTiArg = reader.Read16();
        auto thisTiArg  = reader.Read16();
        return InterfaceCallGeneric { opc, vnum, sret, outerTiArg, thisTiArg };
    }
};

struct StackParam {
    Opcode opc;
    IReg src;
    uint16_t dst;

    inline static StackParam Decode(Decoder::ByteReader& reader)
    {
        auto opc    = Opcode(reader.Read8());
        auto b      = reader.Read8();
        auto src    = IReg::From(b >> 4);
        auto offset = reader.Read16();
        return StackParam { opc, src, offset };
    }
};

struct StackParamF {
    Opcode opc;
    FReg src;
    uint16_t dst;

    inline static StackParamF Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode(reader.Read8());
        auto b   = reader.Read8();
        auto src = FReg::From(b >> 4);
        auto dst = reader.Read16();
        return StackParamF { opc, src, dst };
    }
};

struct StackParamS16 {
    Opcode opc;
    int16_t src;
    uint16_t dst;

    inline static StackParamS16 Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode(reader.Read8());
        auto src = static_cast<int16_t>(reader.Read16());
        auto dst = reader.Read16();
        return StackParamS16 { opc, src, dst };
    }
};

struct StackParamS32 {
    Opcode opc;
    int32_t src;
    uint16_t dst;

    inline static StackParamS32 Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode(reader.Read8());
        auto src = static_cast<int32_t>(reader.Read32());
        auto dst = reader.Read16();
        return StackParamS32 { opc, src, dst };
    }
};

struct LeaStackParam {
    Opcode opc;
    IReg dst;
    uint16_t offset;

    inline static LeaStackParam Decode(Decoder::ByteReader& reader)
    {
        auto opc    = Opcode(reader.Read8());
        auto b      = reader.Read8();
        auto dst    = IReg::From(b >> 4);
        auto offset = reader.Read16();
        return LeaStackParam { opc, dst, offset };
    }
};

} // namespace RT
} // namespace Cbc
