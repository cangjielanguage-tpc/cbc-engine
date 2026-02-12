#pragma once

#include "api/method.h"
#include "cbc/emitter/emitter.h"
#include "cbc/parser.h"

namespace Cbc {

using namespace Format;

class Rewriter : public Parser {

public:
    Rewriter(API::Method* _method, Decoder::ByteReader& _stream, Emitter::Emitter& _e) : Parser(_method, _stream), e(_e) {}

protected:
    void doExtend(Sign sign, IReg dst, IReg src, uint64_t imm) override {
        ASSERTION(false, "Not implmeneted");
    }
    void doBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm) override {
        ASSERTION(false, "Not implmeneted");
    }

    void doMov(Width width, IReg dst, IReg src, bool is_reference) override {
        if (is_reference) {
            e.MovRef(dst, src);
        } else {
            e.Mov(dst, src);
        }
    }
    void doMovVST(IReg dst, IReg src) override {
        ASSERTION(false, "Not implmeneted");
    }
    void doMovImm(Width width, IReg dst, uint64_t imm) override {
        e.MovImm(width, dst, imm);
    }

    void doINeg(CbcTypeKind tkind, IReg dst, IReg src) override {
        ASSERTION(false, "Not implmeneted");
    }

    void doCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2) override {
        e.Binary(op, Width::FromCbcTypeKind(tkind), dst, src1, src2);
    }
    void doCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) override {
        e.BinaryImm(op, Width::FromCbcTypeKind(tkind), dst, src1, src2);
    }

    void doCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2) override {
        ASSERTION(false, "Not implmeneted");
    }
    void doCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) override {
        ASSERTION(false, "Not implmeneted");
    }

    void doBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2) override {
        auto width = Width::FromCbcTypeKind(tkind);
        switch (op) {
            case Common::ADD:  e.Add(width, dst, src1, src2); break;
            case Common::SUB:  e.Sub(width, dst, src1, src2); break;
            case Common::MUL:  e.Mul(width, dst, src1, src2); break;
            case Common::SDIV: e.Div(width, dst, src1, src2); break;
            
            default: ASSERTION(false, "Unexpected op"); break;
        }
    }
    void doBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2) override {
        ASSERTION(false, "Not implmeneted");
    }

    void doReturn(Width width, IReg dst) override {
        // TODO: mov
        e.Ret();
    }
    void doReturn(Width width, FReg dst) override {
        // TODO: mov
        e.Ret();
    }

private:
    Emitter::Emitter& e;
};
    
} // namespace Cbc