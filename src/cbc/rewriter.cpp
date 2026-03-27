#include "cbc/rewriter.h"
#include "engine/symlevel/references.h"

namespace Cbc {

void Rewriter::DoExtend(Sign sign, IReg dst, IReg src, uint64_t imm) { ASSERTION(false, "Not implmeneted"); }

void Rewriter::DoBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm)
{
    ASSERTION(false, "Not implmeneted");
}

void Rewriter::DoMov(IReg dst, IReg src, bool isReference)
{
    if (isReference) {
        e.MovRef(dst, src);
    } else {
        e.Mov(dst, src);
    }
}

void Rewriter::DoMovVST(IReg dst, IReg src) { ASSERTION(false, "Not implmeneted"); }

void Rewriter::DoMovImm(Width width, IReg dst, uint64_t imm) { e.MovImm(width, dst, imm); }

void Rewriter::DoINeg(CbcTypeKind tkind, IReg dst, IReg src) { ASSERTION(false, "Not implmeneted"); }

void Rewriter::DoINeg(CbcTypeKind tkind, IReg dst, uint64_t imm) { ASSERTION(false, "Not implmeneted"); }

void Rewriter::DoCommonOp(InputCommonOpc op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2)
{
    e.Binary(op, Width::FromCbcTypeKind(tkind), dst, src1, src2);
}

void Rewriter::DoCommonOp(InputCommonOpc op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2)
{
    e.BinaryImm(op, Width::FromCbcTypeKind(tkind), dst, src1, src2);
}

void Rewriter::DoCheckedOp(InputCheckedOpc op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2)
{
    ASSERTION(false, "Not implmeneted");
}

void Rewriter::DoCheckedOp(InputCheckedOpc op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2)
{
    ASSERTION(false, "Not implmeneted");
}

void Rewriter::DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2)
{
    auto width = Width::FromCbcTypeKind(tkind);
    switch (static_cast<Opcode_t>(op)) {
        case static_cast<Opcode_t>(InputFloatOpc::Add): e.Add(width, dst, src1, src2); break;
        case static_cast<Opcode_t>(InputFloatOpc::Sub): e.Sub(width, dst, src1, src2); break;
        case static_cast<Opcode_t>(InputFloatOpc::Mul): e.Mul(width, dst, src1, src2); break;
        case static_cast<Opcode_t>(InputFloatOpc::Div): e.Div(width, dst, src1, src2); break;

        default: ASSERTION(false, "Unexpected op"); break;
    }
}

void Rewriter::DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2)
{
    ASSERTION(false, "Not implmeneted");
}

void Rewriter::DoReturn(Width width, IReg dst)
{
    // TODO: mov
    e.Ret();
}

void Rewriter::DoReturn(Width width, FReg dst)
{
    // TODO: mov
    e.Ret();
}

void Rewriter::DoBranchIf(InputCcOpc op, Width width, IReg l, IReg r, uint8_t* target)
{
    e.Bcc(CC::Value(op), width, l, r, InstructionLabel(target));
}

void Rewriter::DoBranchIf(InputCcOpc op, Width width, FReg l, FReg r, uint8_t* target)
{
    ASSERTION(false, "Not implemented");
}

void Rewriter::DoBranchIfImm(InputCcOpc op, Width width, IReg l, uint64_t r, uint8_t* target)
{
    e.BccImm(CC::Value(op), width, l, r, InstructionLabel(target));
}

void Rewriter::DoCallDirect(IReg d, uint16_t methodIndex)
{
    Symlevel::Index<Symlevel::MethodReference> index {
        .region = 0, // TODO: use region
        .index  = methodIndex,
    };

    auto* method = resolver->Resolve(index);
    auto* fuh    = method->FUH().value();

    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(fuh));
    e.DirectCall(d, sym);
}

void Rewriter::BeforeInterpretOne(uint8_t* position) { e.Bind(InstructionLabel(position)); }

Emitter::Label Rewriter::InstructionLabel(uint8_t* position)
{
    if (auto existing = instructionLabel.find(position); existing != instructionLabel.end()) {
        return existing->second;
    } else {
        auto label = e.NewLabel();
        instructionLabel.insert({ position, label });
        return label;
    }
}

} // namespace Cbc
