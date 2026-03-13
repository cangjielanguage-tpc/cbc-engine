#define NONE
#define COMMA_(x,y) x,y
#define COMMA2_(x,y,z) x,y,
#define LAST(x) COMMA_(,x)
#define LAST2(x,y) COMMA_(,x,y)
#define FIRST(x) COMMA_(x,)
#define FIRST2(x,y) COMMA2_(x,y,)

#define GEN_DECODER_HEADER(OPC) \
    template<> \
    void Parser::decode<opcode::OPC, void>(Decoder::ByteReader& codeReader) \
    {
#define GEN_B2_READER(RR_TYPE) \
        auto [d, r] = read_##RR_TYPE(codeReader);
#define GEN_B3_READER(RR_TYPE) \
        auto [x, d, l, r] = read_##RR_TYPE(codeReader);

#define GEN_B2_MANUAL(OPC, RR_TYPE, IMPL_FUNC_NAME, FIRST_ARGS, LAST_ARGS) \
    GEN_DECODER_HEADER(OPC) \
        GEN_B2_READER(RR_TYPE) \
        IMPL_FUNC_NAME(FIRST_ARGS d, r LAST_ARGS); \
    }
#define GEN_B2_COMMON(OPC, RR_TYPE, BASE, WIDTH) \
    GEN_DECODER_HEADER(OPC) \
        GEN_B2_READER(RR_TYPE) \
        DoCommonOp(common(opcode::OPC, BASE), common(WIDTH, SIGN), d, d, r); \
    }
#define GEN_B3_COMMON(OPC, RR_TYPE, WIDTH) \
        GEN_B3_READER(RR_TYPE) \
        DoCommonOp(Common::Value(x), common(WIDTH, SIGN), d, l, r); \
    }

#define GEN_OPCODE_DECODER_MOV_EXTEND(X) \
    X(Mov32,rr,DoMov,NONE,LAST(false)) \
    X(Mov64,rr,DoMov,NONE,LAST(false)) \
    X(Mov32i,ri,DoMovImm,FIRST(W32),NONE) \
    X(Mov64i,ri,DoMovImm,FIRST(W64),NONE) \
    X(MovVst,rr,DoMovVST,NONE,NONE) \
    X(MovRef,rr,DoMov,NONE,LAST(true)) \
    X(ExtendSigned,ri,DoExtend,FIRST2(SIGN, d),NONE) \
    X(ExtendUnsigned,ri,DoExtend,FIRST2(USIGN, d),NONE)
#define GEN_OPCODE_DECODER_COMMON(X) \
    X(Add32,rr,common32_base,W32) \
    X(Sub32,rr,common32_base,W32) \
    X(Mul32,rr,common32_base,W32) \
    X(And32,rr,common32_base,W32) \
    X(Or32,rr,common32_base,W32) \
    X(Xor32,rr,common32_base,W32) \
    X(Div32Signed,rr,common32_base,W32) \
    X(Rem32Signed,rr,common32_base,W32) \
    X(Div32Unsigned,rr,common32_base,W32) \
    X(Rem32Unsigned,rr,common32_base,W32) \
    X(Lsr32,rr,common32_base,W32) \
    X(Asr32,rr,common32_base,W32) \
    X(Lsl32,rr,common32_base,W32) \
    X(Add64,rr,common64_base,W64) \
    X(Sub64,rr,common64_base,W64) \
    X(Mul64,rr,common64_base,W64) \
    X(And64,rr,common64_base,W64) \
    X(Or64,rr,common64_base,W64) \
    X(Xor64,rr,common64_base,W64) \
    X(Div64Signed,rr,common64_base,W64) \
    X(Rem64Signed,rr,common64_base,W64) \
    X(Div64Unsigned,rr,common64_base,W64) \
    X(Rem64Unsigned,rr,common64_base,W64) \
    X(Lsr64,rr,common64_base,W64) \
    X(Asr64,rr,common64_base,W64) \
    X(Lsl64,rr,common64_base,W64) \
    X(Add32Imm,ri,common32imm_base,W32) \
    X(Sub32Imm,ri,common32imm_base,W32) \
    X(Mul32Imm,ri,common32imm_base,W32) \
    X(And32Imm,ri,common32imm_base,W32) \
    X(Or32Imm,ri,common32imm_base,W32) \
    X(Xor32Imm,ri,common32imm_base,W32) \
    X(Div32SignedImm,ri,common32imm_base,W32) \
    X(Rem32SignedImm,ri,common32imm_base,W32) \
    X(Div32UnsignedImm,ri,common32imm_base,W32) \
    X(Rem32UnsignedImm,ri,common32imm_base,W32) \
    X(Lsr32Imm,ri,common32imm_base,W32) \
    X(Asr32Imm,ri,common32imm_base,W32) \
    X(Lsl32Imm,ri,common32imm_base,W32) \
    X(Add64Imm,ri,common64imm_base,W64) \
    X(Sub64Imm,ri,common64imm_base,W64) \
    X(Mul64Imm,ri,common64imm_base,W64) \
    X(And64Imm,ri,common64imm_base,W64) \
    X(Or64Imm,ri,common64imm_base,W64) \
    X(Xor64Imm,ri,common64imm_base,W64) \
    X(Div64SignedImm,ri,common64imm_base,W64) \
    X(Rem64SignedImm,ri,common64imm_base,W64) \
    X(Div64UnsignedImm,ri,common64imm_base,W64) \
    X(Rem64UnsignedImm,ri,common64imm_base,W64) \
    X(Lsr64Imm,ri,common64imm_base,W64) \
    X(Asr64Imm,ri,common64imm_base,W64) \
    X(Lsl64Imm,ri,common64imm_base,W64)
#define GEN_OPCODE_DECODER_NEG(X) \
    X(Neg32,) \
    X(Neg64,) \
    X(Neg32Imm,) \
    X(Neg64Imm,)
#define GEN_OPCODE_DECODER_INTEGER_COMMON(X) \
    X(IntegerCommon32,) \
    X(IntegerCommon64,) \
    X(IntegerCommon32K0,) \
    X(IntegerCommon64K0,) \
    X(IntegerCommon32K8,) \
    X(IntegerCommon64K8,) \
    X(IntegerCommon32K16,) \
    X(IntegerCommon64K16,)
#define GEN_OPCODE_DECODER_CHECKED(X) \
    X(CheckedAdd,) \
    X(CheckedSub,) \
    X(CheckedMul,) \
    X(CheckedDiv,) \
    X(CheckedAddImm,) \
    X(CheckedSubImm,) \
    X(CheckedMulImm,) \
    X(CheckedDivImm,)
#define GEN_OPCODE_BFX(X) \
    X(Bfx,)
#define GEN_OPCODE_FLOAT(X) \
    X(FloatCommon,) \
    X(FloatMisc,)
#define GEN_OPCODE_SETIF(X) \
    X(SetIf32,) \
    X(SetIf64,) \
    X(SetIf32Float,) \
    X(SetIf64Float,)

#define GEN_ENUM(DEF) \
    enum class opcode : opcode_t { \
    DEF \
    ___LAST \
    };
#define GET_OPC(x,y...) x,
#define DO_WITH_OPCODES(A, B, C, D, E, F, G, H) \
    GEN_OPCODE_DECODER_MOV_EXTEND(A) \
    GEN_OPCODE_DECODER_COMMON(B) \
    GEN_OPCODE_DECODER_NEG(C) \
    GEN_OPCODE_DECODER_INTEGER_COMMON(D) \
    GEN_OPCODE_DECODER_CHECKED(E) \
    GEN_OPCODE_BFX(F) \
    GEN_OPCODE_FLOAT(G) \
    GEN_OPCODE_SETIF(H)
