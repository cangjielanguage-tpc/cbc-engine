#define NONE
#define COMMA_(x,y) x,y
#define COMMA2_(x,y,z) x,y,
#define LAST(x) COMMA_(,x)
#define LAST2(x,y) COMMA_(,x,y)
#define FIRST(x) COMMA_(x,)
#define FIRST2(x,y) COMMA2_(x,y,)
#define GET_OPC(x,y...) x,
#define GET_OPS(x,y,...) y,

#define GEN_ENUM(NAME,DEF) \
    enum class NAME : opcode_t { \
    DEF \
    ___LAST \
    };
#define GEN_JUMP_TABLE(ENTRIES) \
    void ::Cbc::Parser::InterpretOne(uint32_t opcode) \
    { \
        switch(opcode) { \
        ENTRIES \
            default: ASSERTION(false, "Unexpected opcode"); ::std::printf("%d", opcode); break; \
        } \
    }

#define GEN_DECODER_HEADER(OPC) \
    template<> \
    void Parser::decode<opcode::OPC, void>(Decoder::ByteReader& codeReader) \
    {
#define GEN_B2_READER(RR_TYPE) \
        auto [d, r] = read_##RR_TYPE(codeReader);
#define GEN_B3_READER(RR_TYPE) \
        auto [x, d, l, r] = read_##RR_TYPE(codeReader);
#define GEN_READ_IMM16_INTEGER(NEEDED,T4,WIDTH,SIGN) \
    uint64_t imm = static_cast<uint64_t>(r); \
    if constexpr (NEEDED) { \
        auto [imm16] = read_imm16(codeReader); \
        imm = DecodeB3ImmInteger(WIDTH,SIGN,r,imm16); \
    }
#define GEN_READ_IMM16_FLOAT(NEEDED,T4,WIDTH) \
    uint64_t imm = static_cast<uint64_t>(r); \
    if constexpr (NEEDED) { \
        auto [imm16] = read_imm16(codeReader); \
        imm = DecodeB3ImmFloat(WIDTH,r,imm16); \
    }

#define GEN_B2_MANUAL(OPC,RR_TYPE,IMPL_FUNC_NAME,FIRST_ARGS,LAST_ARGS) \
    GEN_DECODER_HEADER(OPC) \
        GEN_B2_READER(RR_TYPE) \
        IMPL_FUNC_NAME(FIRST_ARGS d, r LAST_ARGS); \
    }
#define GEN_B2_COMMON(OPC,OPS,RR_TYPE,BASE,WIDTH) \
    GEN_DECODER_HEADER(OPC) \
        GEN_B2_READER(RR_TYPE) \
        DoCommonOp(common_opc::OPS, common_type(WIDTH, SIGN), d, d, r); \
    }
#define GEN_B3_COMMON(OPC,RR_TYPE,WIDTH,WITH_IMM) \
    GEN_DECODER_HEADER(OPC) \
        GEN_B3_READER(RR_TYPE) \
        GEN_READ_IMM16_INTEGER(WITH_IMM,r,WIDTH,SIGN) \
        DoCommonOp(common_opc(x), common_type(WIDTH, SIGN), d, l, imm); \
    }
#define GEN_B3_CHECKED(OPC,OPS,RR_TYPE,WITH_IMM) \
    GEN_DECODER_HEADER(OPC) \
        GEN_B3_READER(RR_TYPE) \
        GEN_READ_IMM16_INTEGER(WITH_IMM,r,checked_width(x),checked_sign(x)) \
        DoCheckedOp(checked_opc::OPS, checked_type(x), d, l, imm); \
    }
#define GEN_RET(OPC,RR_TYPE,WIDTH) \
    GEN_DECODER_HEADER(OPC) \
        GEN_B2_READER(RR_TYPE) \
        DoReturn(WIDTH, r); \
    }
#define EMPTY_IMPL(OPC,_) \
    GEN_DECODER_HEADER(OPC) \
    }

// OPCODES DEFINITION

// GENERAL OPCODES NAMES

#define GEN_OPCODE_DECODER_COMMON_OPS(X,RR_TYPE,WIDTH_N,IMM) \
    X(Add##WIDTH_N##IMM,Add,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(Sub##WIDTH_N##IMM,Sub,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(Mul##WIDTH_N##IMM,Mul,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(And##WIDTH_N##IMM,And,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(Or##WIDTH_N##IMM,Or,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(Xor##WIDTH_N##IMM,Xor,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(DivSigned##WIDTH_N##IMM,DivSigned,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(RemSigned##WIDTH_N##IMM,RemSigned,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(DivUnsigned##WIDTH_N##IMM,DivUnsigned,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(RemUnsigned##WIDTH_N##IMM,RemUnsigned,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(Lsr##WIDTH_N##IMM,Lsr,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(Asr##WIDTH_N##IMM,Asr,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N) \
    X(Lsl##WIDTH_N##IMM,Lsl,RR_TYPE,common##WIDTH_N##_base,W##WIDTH_N)
#define GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X,RR_TYPE,WIDTH_N,WITH_IMM,K) \
    X(IntegerCommon##WIDTH_N##K,RR_TYPE,W##WIDTH_N,WITH_IMM)
#define GEN_OPCODE_DECODER_CHECKED_OPS(X,RR_TYPE,WITH_IMM,IMM) \
    X(CheckedAdd##IMM,Add,RR_TYPE,WITH_IMM) \
    X(CheckedSub##IMM,Sub,RR_TYPE,WITH_IMM) \
    X(CheckedMul##IMM,Mul,RR_TYPE,WITH_IMM) \
    X(CheckedDiv##IMM,Div,RR_TYPE,WITH_IMM)

// OPCODES SEMANTIC

#define GEN_OPCODE_DECODER_MOV_EXTEND(X) \
    X(Mov32,rr,DoMov,,LAST(false)) \
    X(Mov64,rr,DoMov,,LAST(false)) \
    X(Mov32i,ri,DoMovImm,FIRST(W32),) \
    X(Mov64i,ri,DoMovImm,FIRST(W64),) \
    X(MovVst,rr,DoMovVST,,) \
    X(MovRef,rr,DoMov,,LAST(true)) \
    X(ExtendSigned,ri,DoExtend,FIRST2(SIGN, d),) \
    X(ExtendUnsigned,ri,DoExtend,FIRST2(USIGN, d),)
#define GEN_OPCODE_DECODER_COMMON(X) \
    GEN_OPCODE_DECODER_COMMON_OPS(X,ri,32,) \
    GEN_OPCODE_DECODER_COMMON_OPS(X,ri,64,) \
    GEN_OPCODE_DECODER_COMMON_OPS(X,ri,32,Imm) \
    GEN_OPCODE_DECODER_COMMON_OPS(X,ri,64,Imm)
#define GEN_OPCODE_DECODER_NEG(X) \
    X(Neg32,rr,DoINeg,FIRST(common_type(W32,SIGN)),) \
    X(Neg64,rr,DoINeg,FIRST(common_type(W64,SIGN)),) \
    X(Neg32Imm,ri,DoINeg,FIRST(common_type(W32,SIGN)),) \
    X(Neg64Imm,ri,DoINeg,FIRST(common_type(W64,SIGN)),)
#define GEN_OPCODE_DECODER_INTEGER_COMMON(X) \
    GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X,xrrr,32,false,) \
    GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X,xrrr,64,false,) \
    GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X,xrri,32,true,K16) \
    GEN_OPCODE_DECODER_INTEGER_COMMON_OPS(X,xrri,64,true,K16)
#define GEN_OPCODE_DECODER_CHECKED(X) \
    GEN_OPCODE_DECODER_CHECKED_OPS(X,xrrr,false,) \
    GEN_OPCODE_DECODER_CHECKED_OPS(X,xrri,true,Imm)
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
#define GEN_OPCODE_RET(X) \
    X(Ret32,zr,W32) \
    X(Ret64,zr,W64) \
    X(Ret32F,zr,W32) \
    X(Ret64F,zr,W64)

// END OF OPCODES DEFINITION

#define DO_WITH_COMMON_OPCODES(A) \
    GEN_OPCODE_DECODER_COMMON_OPS(A,,,)

#define DO_WITH_CHECKED_OPCODES(A) \
    GEN_OPCODE_DECODER_CHECKED_OPS(A,,,)

#define DO_WITH_ALL_OPCODES(A) \
    GEN_OPCODE_DECODER_MOV_EXTEND(A) \
    GEN_OPCODE_DECODER_COMMON(A) \
    GEN_OPCODE_DECODER_NEG(A) \
    GEN_OPCODE_DECODER_INTEGER_COMMON(A) \
    GEN_OPCODE_DECODER_CHECKED(A) \
    GEN_OPCODE_BFX(A) \
    GEN_OPCODE_FLOAT(A) \
    GEN_OPCODE_SETIF(A) \
    GEN_OPCODE_RET(A)

#define GEN_JUMP_TABLE_ENTRY(OPC,x...) \
    case static_cast<opcode_t>(opcode::OPC): { decode<opcode::OPC, void>(codeReader); break; }
