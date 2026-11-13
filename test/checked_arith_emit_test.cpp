#include "checked_arith.h"
#include "emit_test_common.h"

#include <iterator>
#include <ostream>
#include <string>
#include <vector>

namespace Cbc {
namespace Emitter {

using namespace Cbc::Format;

struct CheckedEmitParam {
    size_t opIndex;
    Width width;
};

static std::ostream& operator<<(std::ostream& os, const CheckedEmitParam& p)
{
    return os << kCheckedOps[p.opIndex].base << "." << CheckedWidthBits(p.width);
}

static std::string CheckedEmitParamName(const testing::TestParamInfo<CheckedEmitParam>& info)
{
    return std::string(kCheckedOps[info.param.opIndex].base) + "_" + std::to_string(CheckedWidthBits(info.param.width));
}

static std::vector<CheckedEmitParam> CheckedEmitRrParams()
{
    std::vector<CheckedEmitParam> params;
    for (size_t i = 0; i < std::size(kCheckedOps); ++i) {
        if (!kCheckedOps[i].rr) {
            continue;
        }
        for (Width width : kCheckedWidths) {
            params.push_back({ i, width });
        }
    }
    return params;
}

static std::vector<CheckedEmitParam> CheckedEmitImmParams()
{
    std::vector<CheckedEmitParam> params;
    for (size_t i = 0; i < std::size(kCheckedOps); ++i) {
        if (!kCheckedOps[i].imm) {
            continue;
        }
        for (Width width : kCheckedWidths) {
            params.push_back({ i, width });
        }
    }
    return params;
}

class CbcCheckedArithEmitRr : public EmitTest, public ::testing::WithParamInterface<CheckedEmitParam> {};

class CbcCheckedArithEmitImm : public EmitTest, public ::testing::WithParamInterface<CheckedEmitParam> {};

TEST_P(CbcCheckedArithEmitRr, differential)
{
    const CheckedEmitParam& param = GetParam();
    const CheckedOpInfo& info     = kCheckedOps[param.opIndex];
    const int bits                = CheckedWidthBits(param.width);

    Emitter e;
    if (info.kind == CheckedKind::Checked) {
        e.Binary(info.checked(), param.width, IReg::IR1, IReg::IR2, IReg::IR3);
    } else {
        e.SatBinary(info.saturating(), param.width, IReg::IR1, IReg::IR2, IReg::IR3);
    }
    e.Ret();
    auto code = e.Build(heap);

    const std::vector<uint64_t> values = CheckedOperandValues(bits);
    for (uint64_t rhs : values) {
        if (rhs == 0 && info.noZeroRhs) {
            continue;
        }
        for (uint64_t lhs : values) {
            ExpectCheckedRr(info.op, bits, lhs, rhs, code);
        }
    }
}

TEST_P(CbcCheckedArithEmitImm, differential)
{
    const CheckedEmitParam& param         = GetParam();
    const CheckedOpInfo& info             = kCheckedOps[param.opIndex];
    const int bits                        = CheckedWidthBits(param.width);
    const std::vector<uint64_t> lhsValues = CheckedOperandValues(bits);

    for (uint64_t rhs : CheckedGridValues(bits)) {
        if (rhs == 0 && info.noZeroRhs) {
            continue;
        }
        heap.Reset();
        Emitter e;
        if (info.kind == CheckedKind::Checked) {
            e.BinaryImm(info.checked(), param.width, IReg::IR1, IReg::IR2, rhs);
        } else {
            e.SatBinaryImm(info.saturating(), param.width, IReg::IR1, IReg::IR2, rhs);
        }
        e.Ret();
        auto code = e.Build(heap);
        for (uint64_t lhs : lhsValues) {
            ExpectCheckedImm(info.op, bits, lhs, rhs, code);
        }
    }
}

static const auto kCheckedEmitRrParams  = CheckedEmitRrParams();
static const auto kCheckedEmitImmParams = CheckedEmitImmParams();

INSTANTIATE_TEST_SUITE_P(
    CbcTest, CbcCheckedArithEmitRr, ::testing::ValuesIn(kCheckedEmitRrParams), CheckedEmitParamName
);

INSTANTIATE_TEST_SUITE_P(
    CbcTest, CbcCheckedArithEmitImm, ::testing::ValuesIn(kCheckedEmitImmParams), CheckedEmitParamName
);

} // namespace Emitter
} // namespace Cbc
