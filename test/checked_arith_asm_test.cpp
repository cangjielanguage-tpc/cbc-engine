#include "cbc_test_harness.h"
#include "checked_arith.h"

#include <iterator>
#include <ostream>
#include <string>
#include <vector>

enum class CheckedAsmForm {
    Rr,
    ImmShort,
    ImmWide
};

struct CheckedAsmParam {
    size_t opIndex;
    Cbc::Format::Width width;
    CheckedAsmForm form;
};

static const char* CheckedAsmFormName(CheckedAsmForm form, bool gtestName)
{
    switch (form) {
        case CheckedAsmForm::Rr:       return "rr";
        case CheckedAsmForm::ImmShort: return gtestName ? "imm_short" : "imm-short";
        case CheckedAsmForm::ImmWide:  return gtestName ? "imm_wide" : "imm-wide";
    }
    return "";
}

static std::ostream& operator<<(std::ostream& os, const CheckedAsmParam& p)
{
    return os << CheckedAsmFormName(p.form, false) << "/" << kCheckedOps[p.opIndex].base << "."
              << CheckedWidthBits(p.width);
}

static std::string CheckedAsmParamName(const testing::TestParamInfo<CheckedAsmParam>& info)
{
    const CheckedAsmParam& p = info.param;
    return std::string(CheckedAsmFormName(p.form, true)) + "_" + kCheckedOps[p.opIndex].base + "_" +
           std::to_string(CheckedWidthBits(p.width));
}

static std::vector<CheckedAsmParam> CheckedAsmParams()
{
    std::vector<CheckedAsmParam> params;
    for (size_t i = 0; i < std::size(kCheckedOps); ++i) {
        for (Cbc::Format::Width width : kCheckedWidths) {
            if (kCheckedOps[i].rr) {
                params.push_back({ i, width, CheckedAsmForm::Rr });
            }
            if (kCheckedOps[i].imm) {
                params.push_back({ i, width, CheckedAsmForm::ImmShort });
                if (width != Cbc::Format::Width::W8) {
                    params.push_back({ i, width, CheckedAsmForm::ImmWide });
                }
            }
        }
    }
    return params;
}

class CbcCheckedArithAsm : public ::testing::TestWithParam<CheckedAsmParam> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcCheckedArithAsm, pipeline)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    const CheckedAsmParam& param = GetParam();
    const CheckedOpInfo& info    = kCheckedOps[param.opIndex];
    const int bits               = CheckedWidthBits(param.width);
    const std::string method     = "test_" + std::to_string(bits);

    std::string path;
    uint64_t bakedRhs = 0;
    if (param.form == CheckedAsmForm::Rr) {
        path = "./generated-asm/checked-arith/rr/" + std::string(info.base) + ".asm";
    } else if (param.form == CheckedAsmForm::ImmShort) {
        path     = "./generated-asm/checked-arith/imm-short/" + std::string(info.base) + "i.asm";
        bakedRhs = 0x1;
    } else {
        path     = "./generated-asm/checked-arith/imm-wide/" + std::string(info.base) + "i.asm";
        bakedRhs = 0x1000;
    }

    Interpretation::Code code  = OpenAndRewrite(method, path)->code;
    const uint64_t lhsValues[] = { 1, 5, CheckedSMin(bits), CheckedSMax(bits), CheckedUMax(bits) };
    if (param.form == CheckedAsmForm::Rr) {
        const uint64_t rhsValues[] = { 1, CheckedSMax(bits) };
        for (uint64_t rhs : rhsValues) {
            for (uint64_t lhs : lhsValues) {
                ExpectCheckedAsmRr(info.op, bits, lhs, rhs, code);
            }
        }
    } else {
        for (uint64_t lhs : lhsValues) {
            ExpectCheckedAsmImm(info.op, bits, lhs, bakedRhs, code);
        }
    }
}

static const auto kCheckedAsmParams = CheckedAsmParams();

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcCheckedArithAsm, ::testing::ValuesIn(kCheckedAsmParams), CheckedAsmParamName);
