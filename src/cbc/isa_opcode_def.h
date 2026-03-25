#define NONE
#define COMMA_(x, y) x, y
#define COMMA2_(x, y, z) x, y,
#define LAST(x) COMMA_(, x)
#define LAST2(x, y) COMMA_(, x, y)
#define FIRST(x) COMMA_(x, )
#define FIRST2(x, y) COMMA2_(x, y, )
#define GET_OPC(x, y...) x,
#define GET_OPS(x, y, ...) y,

#define GEN_ENUM(NAME, DEF)                                                                                            \
    enum class NAME : ::Cbc::Opcode_t {                                                                                \
        DEF ___LAST                                                                                                    \
    };
#define GEN_JUMP_TABLE(ENTRIES)                                                                                        \
    inline void ::Cbc::Parser::InterpretOne(uint32_t opcode)                                                           \
    {                                                                                                                  \
        switch (opcode) {                                                                                              \
            ENTRIES                                                                                                    \
            default: ASSERTION(false, "Unexpected opcode"); break;                                                     \
        }                                                                                                              \
    }

#define GEN_DECODER_HEADER(OPC)                                                                                        \
    template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::OPC>(::Decoder::ByteReader & codeReader)                \
    {
#define GEN_B2_READER(RR_TYPE) auto [d, r] = ::Cbc::Read##RR_TYPE(codeReader);
#define GEN_B3_READER(RR_TYPE) auto [x, d, l, r] = ::Cbc::Read##RR_TYPE(codeReader);
#define GEN_READ_IMM64(NEEDED)                                                                                  \
    uint64_t r_or_imm = static_cast<uint64_t>(r);                                                                      \
    if constexpr (NEEDED) {                                                                                            \
        if (immPrefix == ImmPrefix::ImmPrefix32) { \
            auto [x] = ::Cbc::ReadImm32(codeReader); \
            r_or_imm = ::std::move(x);\
        } else { /* ImmPrefix64 */ \
            auto [x] = ::Cbc::ReadImm64(codeReader); \
            r_or_imm = ::std::move(x);\
        } \
    }

#define GEN_B2_MANUAL(OPC, RR_TYPE, IMPL_FUNC_NAME, FIRST_ARGS, LAST_ARGS)                                             \
    GEN_DECODER_HEADER(OPC)                                                                                            \
    GEN_B2_READER(RR_TYPE)                                                                                             \
    IMPL_FUNC_NAME(FIRST_ARGS d, r LAST_ARGS);                                                                         \
    }
#define GEN_B2_COMMON(OPC, OPS, RR_TYPE, BASE, WIDTH)                                                                  \
    GEN_DECODER_HEADER(OPC)                                                                                            \
    GEN_B2_READER(RR_TYPE)                                                                                             \
    DoCommonOp(::Cbc::CommonOpc::OPS, ::Cbc::GetCommonType(WIDTH, SIGN), d, d, r);                                     \
    }
#define GEN_B3_COMMON(OPC, RR_TYPE, WIDTH, WITH_IMM)                                                                   \
    GEN_DECODER_HEADER(OPC)                                                                                            \
    GEN_B3_READER(RR_TYPE)                                                                                             \
    GEN_READ_IMM64(WITH_IMM)                                                                      \
    DoCommonOp(::Cbc::CommonOpc(x), ::Cbc::GetCommonType(WIDTH, SIGN), d, l, r_or_imm);                                \
    }
#define GEN_B3_CHECKED(OPC, OPS, RR_TYPE, WITH_IMM)                                                                    \
    GEN_DECODER_HEADER(OPC)                                                                                            \
    GEN_B3_READER(RR_TYPE)                                                                                             \
    GEN_READ_IMM64(WITH_IMM)                              \
    DoCheckedOp(::Cbc::CheckedOpc::OPS, ::Cbc::GetCheckedType(x), d, l, r_or_imm);                                     \
    }
#define GEN_B3_FLOAT(OPC, RR_TYPE, WITH_IMM)                                                                           \
    GEN_DECODER_HEADER(OPC)                                                                                            \
    GEN_B3_READER(RR_TYPE)                                                                                             \
    GEN_READ_IMM64(WITH_IMM)                                                            \
    DoBinaryFloatOp(::Cbc::FloatOpc(x), ::Cbc::GetFloatType(x), d, l, r_or_imm);                                       \
    }
#define GEN_RET(OPC, RR_TYPE, WIDTH)                                                                                   \
    GEN_DECODER_HEADER(OPC)                                                                                            \
    GEN_B2_READER(RR_TYPE)                                                                                             \
    DoReturn(WIDTH, r);                                                                                                \
    }
#define GEN_IMM_PREFIX(OPC, RR_TYPE)                                                                                   \
    GEN_DECODER_HEADER(OPC)                                                                                            \
    immPrefix = ReadOp(codeReader); \
    }
#define EMPTY_IMPL(OPC, _)                                                                                             \
    GEN_DECODER_HEADER(OPC)                                                                                            \
    }

// OPCODES DEFINITION

// GENERAL OPCODES NAMES

#define GEN_OPCODE_DECODER_COMMON_OPS(X, RR_TYPE, WIDTH_N, IMM)                                                        \
    X(Add##WIDTH_N##IMM, Add, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                             \
    X(Sub##WIDTH_N##IMM, Sub, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                             \
    X(Mul##WIDTH_N##IMM, Mul, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                             \
    X(And##WIDTH_N##IMM, And, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                             \
    X(Or##WIDTH_N##IMM, Or, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                               \
    X(Xor##WIDTH_N##IMM, Xor, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                             \
    X(DivSigned##WIDTH_N##IMM, DivSigned, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                 \
    X(RemSigned##WIDTH_N##IMM, RemSigned, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                 \
    X(DivUnsigned##WIDTH_N##IMM, DivUnsigned, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                             \
    X(RemUnsigned##WIDTH_N##IMM, RemUnsigned, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                             \
    X(Lsr##WIDTH_N##IMM, Lsr, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                             \
    X(Asr##WIDTH_N##IMM, Asr, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)                                             \
    X(Lsl##WIDTH_N##IMM, Lsl, RR_TYPE, common##WIDTH_N##_base, W##WIDTH_N)
#define GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X, RR_TYPE, WIDTH_N, WITH_IMM, K)                                        \
    X(IntegerCommon##WIDTH_N##K, RR_TYPE, W##WIDTH_N, WITH_IMM)
#define GEN_OPCODE_DECODER_CHECKED_OPS(X, RR_TYPE, WITH_IMM, IMM)                                                      \
    X(CheckedAdd##IMM, Add, RR_TYPE, WITH_IMM)                                                                         \
    X(CheckedSub##IMM, Sub, RR_TYPE, WITH_IMM)                                                                         \
    X(CheckedMul##IMM, Mul, RR_TYPE, WITH_IMM)                                                                         \
    X(CheckedDiv##IMM, Div, RR_TYPE, WITH_IMM)
#define GEN_OPCODE_DECODER_FLOAT_OPS(X)                                                                                \
    X(Add)                                                                                                             \
    X(Sub)                                                                                                             \
    X(Mul)                                                                                                             \
    X(Div)                                                                                                             \
    X(Mov)                                                                                                             \
    X(Neg)                                                                                                             \
    X(Abs)                                                                                                             \
    X(Sqrt)
#define GEN_OPCODE_DECODER_FLOAT_FLOAT_CONVERSIONS_OPS(X, RR_TYPE)                                                     \
    X(FloatToFloat32, RR_TYPE)                                                                                         \
    X(Float32ToFloat, RR_TYPE)
#define GEN_OPCODE_DECODER_IMM_PREFIX_OPS(X, RR_TYPE)                                                                  \
    X(ImmPrefix32, RR_TYPE)                                                                                            \
    X(ImmPrefix64, RR_TYPE)                                                                                            \

// OPCODES SEMANTIC

#define GEN_OPCODE_DECODER_MOV_EXTEND(X)                                                                               \
    X(Mov32, RR, DoMov, , LAST(false))                                                                                 \
    X(Mov64, RR, DoMov, , LAST(false))                                                                                 \
    X(Mov32i, RI, DoMovImm, FIRST(W32), )                                                                              \
    X(Mov64i, RI, DoMovImm, FIRST(W64), )                                                                              \
    X(MovVst, RR, DoMovVST, , )                                                                                        \
    X(MovRef, RR, DoMov, , LAST(true))                                                                                 \
    X(ExtendSigned, RI, DoExtend, FIRST2(SIGN, d), )                                                                   \
    X(ExtendUnsigned, RI, DoExtend, FIRST2(USIGN, d), )
#define GEN_OPCODE_DECODER_COMMON(X)                                                                                   \
    GEN_OPCODE_DECODER_COMMON_OPS(X, RR, 32, )                                                                         \
    GEN_OPCODE_DECODER_COMMON_OPS(X, RR, 64, )                                                                         \
    GEN_OPCODE_DECODER_COMMON_OPS(X, RI, 32, Imm)                                                                      \
    GEN_OPCODE_DECODER_COMMON_OPS(X, RI, 64, Imm)
#define GEN_OPCODE_DECODER_NEG(X)                                                                                      \
    X(Neg32, RR, DoINeg, FIRST(::Cbc::GetCommonType(W32, SIGN)), )                                                     \
    X(Neg64, RR, DoINeg, FIRST(::Cbc::GetCommonType(W64, SIGN)), )                                                     \
    X(Neg32Imm, RI, DoINeg, FIRST(::Cbc::GetCommonType(W32, SIGN)), )                                                  \
    X(Neg64Imm, RI, DoINeg, FIRST(::Cbc::GetCommonType(W64, SIGN)), )
#define GEN_OPCODE_DECODER_INTEGER_COMMON(X)                                                                           \
    GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X, XRRR, 32, false, )                                                        \
    GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X, XRRR, 64, false, )                                                        \
    GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X, XRRI, 32, true, K16)                                                      \
    GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X, XRRI, 64, true, K16)
#define GEN_OPCODE_DECODER_CHECKED(X)                                                                                  \
    GEN_OPCODE_DECODER_CHECKED_OPS(X, XRRR, false, )                                                                   \
    GEN_OPCODE_DECODER_CHECKED_OPS(X, XRRI, true, Imm)
#define GEN_OPCODE_DECODER_BFX(X) X(Bfx, )
#define GEN_OPCODE_DECODER_FLOAT_COMMON(X)                                                                             \
    X(FloatCommon, XFFF, false)                                                                                        \
    X(FloatCommonImm, XFFI, true)
#define GEN_OPCODE_DECODER_FLOAT_MISC(X) X(FloatMisc, )
#define GEN_OPCODE_DECODER_SETIF(X)                                                                                    \
    X(SetIf32, )                                                                                                       \
    X(SetIf64, )                                                                                                       \
    X(SetIf32Float, )                                                                                                  \
    X(SetIf64Float, )
#define GEN_OPCODE_DECODER_RET(X)                                                                                      \
    X(Ret32, ZR, W32)                                                                                                  \
    X(Ret64, ZR, W64)                                                                                                  \
    X(Ret32F, ZR, W32)                                                                                                 \
    X(Ret64F, ZR, W64)
#define GEN_OPCODE_DECODER_IMM_PREFIX(X)                                                                               \
    GEN_OPCODE_DECODER_IMM_PREFIX_OPS(X, Op)                                                                \

// END OF OPCODES DEFINITION

#define DO_WITH_COMMON_OPCODES(A) GEN_OPCODE_DECODER_COMMON_OPS(A, , , )

#define DO_WITH_CHECKED_OPCODES(A) GEN_OPCODE_DECODER_CHECKED_OPS(A, , , )

#define DO_WITH_FLOAT_OPCODES(A) GEN_OPCODE_DECODER_FLOAT_OPS(A)

#define DO_WITH_IMM_PREFIX_OPCODES(A) GEN_OPCODE_DECODER_IMM_PREFIX_OPS(A,)

#define DO_WITH_ALL_OPCODES(A)                                                                                         \
    GEN_OPCODE_DECODER_MOV_EXTEND(A)                                                                                   \
    GEN_OPCODE_DECODER_COMMON(A)                                                                                       \
    GEN_OPCODE_DECODER_NEG(A)                                                                                          \
    GEN_OPCODE_DECODER_INTEGER_COMMON(A)                                                                               \
    GEN_OPCODE_DECODER_CHECKED(A)                                                                                      \
    GEN_OPCODE_DECODER_BFX(A)                                                                                          \
    GEN_OPCODE_DECODER_FLOAT_COMMON(A)                                                                                 \
    GEN_OPCODE_DECODER_FLOAT_MISC(A)                                                                                   \
    GEN_OPCODE_DECODER_SETIF(A)                                                                                        \
    GEN_OPCODE_DECODER_RET(A) \
    GEN_OPCODE_DECODER_IMM_PREFIX(A)

#define GEN_JUMP_TABLE_ENTRY(OPC, x...)                                                                                \
    case ::Cbc::Opc(::Cbc::InputOpcode::OPC): {                                                                        \
        ::Cbc::Parser::Decode<::Cbc::InputOpcode::OPC>(codeReader);                                                    \
        break;                                                                                                         \
    }
