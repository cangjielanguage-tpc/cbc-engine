#include "cbc/rewriter.h"

namespace Cbc {

void Rewriter::DoExtend(Sign sign, IReg dst, IReg src, uint64_t imm) {
    ASSERTION(false, "Not implmeneted");
}
void Rewriter::DoBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm) {
    ASSERTION(false, "Not implmeneted");
}

void Rewriter::DoMov(Width width, IReg dst, IReg src, bool isReference) {
    if (isReference) {
        e.MovRef(dst, src);
    } else {
        e.Mov(dst, src);
    }
}
void Rewriter::DoMovVST(IReg dst, IReg src) {
    ASSERTION(false, "Not implmeneted");
}
void Rewriter::DoMovImm(Width width, IReg dst, uint64_t imm) {
    e.MovImm(width, dst, imm);
}

void Rewriter::DoINeg(CbcTypeKind tkind, IReg dst, IReg src) {
    ASSERTION(false, "Not implmeneted");
}

void Rewriter::DoCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2) {
    e.Binary(op, Width::FromCbcTypeKind(tkind), dst, src1, src2);
}
void Rewriter::DoCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) {
    e.BinaryImm(op, Width::FromCbcTypeKind(tkind), dst, src1, src2);
}

void Rewriter::DoCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2) {
    ASSERTION(false, "Not implmeneted");
}
void Rewriter::DoCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) {
    ASSERTION(false, "Not implmeneted");
}

void Rewriter::DoBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2) {
    auto width = Width::FromCbcTypeKind(tkind);
    switch (op) {
        case Common::ADD:  e.Add(width, dst, src1, src2); break;
        case Common::SUB:  e.Sub(width, dst, src1, src2); break;
        case Common::MUL:  e.Mul(width, dst, src1, src2); break;
        case Common::SDIV: e.Div(width, dst, src1, src2); break;
        
        default: ASSERTION(false, "Unexpected op"); break;
    }
}
void Rewriter::DoBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2) {
    ASSERTION(false, "Not implmeneted");
}

void Rewriter::DoReturn(Width width, IReg dst) {
    // TODO: mov
    e.Ret();
}
void Rewriter::DoReturn(Width width, FReg dst) {
    // TODO: mov
    e.Ret();
}

} // namespace Cbc