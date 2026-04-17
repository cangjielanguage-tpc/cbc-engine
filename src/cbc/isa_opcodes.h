#define ISA_OPCODES(X)                                                                                                 \
    X(Bcc32Eq, (BranchSpecialized<Width::W32, CC::EQ>[w]))                                                             \
    X(Bcc32Ne, (BranchSpecialized<Width::W32, CC::NE>[w]))                                                             \
    X(Bcc32Lt, (BranchSpecialized<Width::W32, CC::LT>[w]))                                                             \
    X(Bcc32Ge, (BranchSpecialized<Width::W32, CC::GE>[w]))                                                             \
    X(Bcc32Ult, (BranchSpecialized<Width::W32, CC::ULT>[w]))                                                           \
    X(Bcc32Uge, (BranchSpecialized<Width::W32, CC::UGE>[w]))                                                           \
    X(Bcc64Eq, (BranchSpecialized<Width::W64, CC::EQ>[w]))                                                             \
    X(Bcc64Ne, (BranchSpecialized<Width::W64, CC::NE>[w]))                                                             \
    X(Bcc64Lt, (BranchSpecialized<Width::W64, CC::LT>[w]))                                                             \
    X(Bcc64Ge, (BranchSpecialized<Width::W64, CC::GE>[w]))                                                             \
    X(Bcc64Ult, (BranchSpecialized<Width::W64, CC::ULT>[w]))                                                           \
    X(Bcc64Uge, (BranchSpecialized<Width::W64, CC::UGE>[w]))                                                           \
    X(BccReq, (BranchSpecialized<Width::W64, CC::REQ>[w]))                                                             \
    X(BccRne, (BranchSpecialized<Width::W64, CC::RNE>[w]))                                                             \
    X(Bcc32, BranchGeneric<Width::W32>[w])                                                                             \
    X(Bcc64, BranchGeneric<Width::W64>[w])                                                                             \
    X(BccImm32, BranchImm<Width::W32>[w])                                                                              \
    X(BccImm64, BranchImm<Width::W64>[w])                                                                              \
    X(Jump, Jump[w])                                                                                                   \
    X(WidePrefix, Unreachable)                                                                                         \
    X(Mov32, Mov<Width::W32>)                                                                                          \
    X(Mov64, Mov<Width::W64>)                                                                                          \
    X(Mov32i, MovImm<Width::W32>)                                                                                      \
    X(Mov64i, MovImm<Width::W64>)                                                                                      \
    X(MovRef, MovRef)                                                                                                  \
    X(FMov32, FMov<Width::W32>)                                                                                        \
    X(FMov64, FMov<Width::W64>)                                                                                        \
    X(FMov32i, FMovImm<Width::W32>)                                                                                    \
    X(FMov64i, FMovImm<Width::W64>)                                                                                    \
    X(Mov32_FloatToInt, FloatToInt<Width::W32>)                                                                        \
    X(Mov64_FloatToInt, FloatToInt<Width::W64>)                                                                        \
    X(Mov32_IntToFloat, IntToFloat<Width::W32>)                                                                        \
    X(Mov64_IntToFloat, IntToFloat<Width::W64>)                                                                        \
    X(Add32, (BinarySpecialized<Common::ADD, Width::W32>))                                                             \
    X(Sub32, (BinarySpecialized<Common::SUB, Width::W32>))                                                             \
    X(Mul32, (BinarySpecialized<Common::MUL, Width::W32>))                                                             \
    X(And32, (BinarySpecialized<Common::AND, Width::W32>))                                                             \
    X(Or32, (BinarySpecialized<Common::OR, Width::W32>))                                                               \
    X(Xor32, (BinarySpecialized<Common::XOR, Width::W32>))                                                             \
    X(UDiv32, (BinarySpecialized<Common::UDIV, Width::W32>))                                                           \
    X(URem32, (BinarySpecialized<Common::UREM, Width::W32>))                                                           \
    X(LSR32, (BinarySpecialized<Common::LSR, Width::W32>))                                                             \
    X(ASR32, (BinarySpecialized<Common::ASR, Width::W32>))                                                             \
    X(LSL32, (BinarySpecialized<Common::LSL, Width::W32>))                                                             \
    X(Add64, (BinarySpecialized<Common::ADD, Width::W64>))                                                             \
    X(Sub64, (BinarySpecialized<Common::SUB, Width::W64>))                                                             \
    X(Mul64, (BinarySpecialized<Common::MUL, Width::W64>))                                                             \
    X(And64, (BinarySpecialized<Common::AND, Width::W64>))                                                             \
    X(Or64, (BinarySpecialized<Common::OR, Width::W64>))                                                               \
    X(Xor64, (BinarySpecialized<Common::XOR, Width::W64>))                                                             \
    X(UDiv64, (BinarySpecialized<Common::UDIV, Width::W64>))                                                           \
    X(URem64, (BinarySpecialized<Common::UREM, Width::W64>))                                                           \
    X(LSR64, (BinarySpecialized<Common::LSR, Width::W64>))                                                             \
    X(ASR64, (BinarySpecialized<Common::ASR, Width::W64>))                                                             \
    X(LSL64, (BinarySpecialized<Common::LSL, Width::W64>))                                                             \
    X(Binary32, BinaryGeneric<Width::W32>)                                                                             \
    X(Binary64, BinaryGeneric<Width::W64>)                                                                             \
    X(BinaryImm32, BinaryImm<Width::W32>)                                                                              \
    X(BinaryImm64, BinaryImm<Width::W64>)                                                                              \
    X(Convert, Convert)                                                                                                \
    X(NewArr, NewArr)                                                                                                  \
    X(GcPoint, GcPoint)                                                                                                \
    X(PrepareRecord, PrepareRecord)                                                                                    \
    X(ZeroRefs, ZeroRefs)                                                                                              \
    X(Scc32, Scc<Width::W32>)                                                                                          \
    X(Scc64, Scc<Width::W64>)                                                                                          \
    X(SccImm32, SccImm<Width::W32>)                                                                                    \
    X(SccImm64, SccImm<Width::W32>)                                                                                    \
    X(InstanceOf, InstanceOf)                                                                                          \
    X(LoadTypeInfoObj, LoadTypeInfoObj)                                                                                \
    X(RegSymGroup, RegSymGroup)                                                                                        \
    X(RegGroup, RegGroup)                                                                                              \
    X(InitObj, InitObj)                                                                                                \
    X(InitString, InitString)                                                                                          \
    X(ArrayLength, ArrayLength)                                                                                        \
    X(ArrayIndexCheck, ArrayIndexCheck)                                                                                \
    X(FBinary32, FloatBinary<Width::W32>)                                                                              \
    X(FBinary64, FloatBinary<Width::W64>)                                                                              \
    X(LoadStatic, LoadStatic)                                                                                          \
    X(StoreStatic, StoreStatic)                                                                                        \
    X(LoadObj, LoadObj)                                                                                                \
    X(StoreObj, StoreObj)                                                                                              \
    X(LoadRec, LoadRec)                                                                                                \
    X(StoreRec, StoreRec)


#define ISA_REG_SYM_GROUP_OPCODES(X)                                                                                   \
    X(LoadTypeInfoSig)                                                                                                 \
    X(LoadTypeInfoFtc)                                                                                                 \
    X(NewObj)                                                                                                          \
    X(CallDirect)                                                                                                      \
    X(CallVirt)                                                                                                        \
    X(CallInterf)

#define ISA_REG_GROUP_OPCODES(X)                                                                                       \
    X(Ret32)                                                                                                           \
    X(Ret64)                                                                                                           \
    X(FRet32)                                                                                                          \
    X(FRet64)                                                                                                          \
    X(DivCheck)                                                                                                        \
    X(Catch)                                                                                                           \
    X(Throw)
