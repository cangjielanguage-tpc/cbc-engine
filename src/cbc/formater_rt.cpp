#include <cmath>
#include <iomanip>
#include <ostream>

#include "cbc/isa.h"
#include "formater_rt.h"
#include "utils/math.h"

namespace Cbc {
namespace RT {

constexpr static char const* instruction_format_strings[] = {
#define GET_FORMAT(opc, dfmt, sfmt) sfmt,
    CBC_RT_OPCODES(GET_FORMAT)
#undef GET_FORMAT
};

// Characters that could end an argument format descriptor.
constexpr static std::string_view delimiters(" .]");

struct Operand {
    uint64_t const value;

    uint8_t U8() { return static_cast<uint8_t>(value); }

    uint16_t U16() { return static_cast<uint16_t>(value); }

    uint32_t U32() { return static_cast<uint32_t>(value); }

    uint64_t U64() { return static_cast<uint32_t>(value); }

    Format::LoadAccessKind Ldk() { return Format::LoadAccessKind::From(U8()); }

    Format::StoreAccessKind Stk() { return Format::StoreAccessKind::From(U8()); }

    Format::Common Bin() { return Format::Common::From(U8()); }

    Format::FloatOperations Fop() { return Format::FloatOperations::From(U8()); }

    Format::CC CC() { return Format::CC::From(U8()); }

    Format::ConvertType Ct() { return Format::ConvertType::From(U8()); }

    IReg IR() { return IReg::From(U32() & 0xf); }

    FReg FR() { return FReg::From(U32() & 0xf); }

    int64_t I4() { return static_cast<int64_t>(MathUtils::SignExtend(value, 4)); }

    int64_t I12() { return static_cast<int64_t>(MathUtils::SignExtend(value, 12)); }

    uint64_t U12() { return static_cast<uint64_t>(MathUtils::ZeroExtend(value, 12)); }

    int64_t I32() { return static_cast<int64_t>(MathUtils::SignExtend(value, 32)); }

    float F32()
    {
        float fvalue;
        std::memcpy(&fvalue, &value, sizeof(float));
        return fvalue;
    }

    double F64()
    {
        double fvalue;
        std::memcpy(&fvalue, &value, sizeof(double));
        return fvalue;
    }
};

class Formatter {
public:
    Formatter(
        Interpretation::LiteralTable* table,
        std::ostream& stream,
        std::string_view formatString,
        Operand* operands,
        size_t operandCount
    )
        : table(table),
          stream(stream),
          operands(operands),
          operandCount(operandCount),
          formatString(formatString)
    {}

    void Format()
    {
        auto fmtSize     = formatString.size();
        size_t cursor    = 0;
        size_t oldCursor = 0;
        while (cursor < fmtSize) {
            if (formatString[cursor] != '$') {
                cursor++;
                continue;
            }
            stream << formatString.substr(oldCursor, cursor - oldCursor); // dump buffered
            cursor = cursor + 1;                                          // skip '$'
            FormatArg(cursor, fmtSize);
            oldCursor = cursor; // clear buffer
        }
        stream << formatString.substr(oldCursor, cursor - oldCursor);
        stream << std::endl;
    }

private:
    void Write(IReg ir)
    {
        if (ir == IReg::IRZ) {
            stream << "IRZ";
        } else {
            stream << "IR" << ir.Raw();
        }
    }

    void Write(FReg fr) { stream << "FR" << fr.Raw(); }

    void Write(uint64_t v) { stream << v; }

    void Write(int64_t v) { stream << v; }

    void Write(float v) { stream << v; }

    void Write(double v) { stream << v; }

    void Write(Format::LoadAccessKind v) { stream << v.ToStr(); }

    void Write(Format::StoreAccessKind v) { stream << v.ToStr(); }

    void Write(Format::CC v) { stream << v.ToStr(); }

    void Write(Format::Common v) { stream << v.ToStr(); }

    void Write(Format::FloatOperations v) { stream << v.ToStr(); }

    void Write(Format::ConvertType v) { stream << v.ToStr(); }

    void FormatArg(size_t& cursor, size_t fmtSize)
    {
        size_t start     = cursor;
        size_t newCursor = cursor;
        while (newCursor < fmtSize && delimiters.find(formatString[newCursor]) == std::string_view::npos) {
            newCursor++;
        }

        int argIdx = GetArgIdx(formatString, start);

        size_t typeDescEnd    = newCursor;
        size_t typeDescStart  = start + 1;
        std::string_view type = formatString.substr(typeDescStart, typeDescEnd - typeDescStart);
        if (auto sepId = type.find(':'); sepId != std::string_view::npos) {
            FormatArgWithSeparator(type, argIdx, sepId);
        } else {
            FormatArgWithoutSeparator(type, argIdx);
        }
        cursor = newCursor;
    }

    void FormatArgWithSeparator(std::string_view type, int argIdx, size_t sepId)
    {
        // Separator found.
        // This operand is needed for proper register kind formatting, which depends on ldk/stk)
        ASSERT(sepId + 3 < type.size());
        auto rightArgIdx = GetArgIdx(type, sepId + 2);
        ASSERT(argIdx < operandCount);
        ASSERT(rightArgIdx < operandCount);
        auto leftType     = type.substr(0, sepId);
        auto rightType    = type.substr(sepId + 3, std::string_view::npos);
        auto leftOperand  = operands[argIdx];
        auto rightOperand = operands[rightArgIdx];

        if (leftType == "r") {
            bool isFloat =
                leftType == "ldk" && leftOperand.Ldk().IsFloat() || rightType == "stk" && rightOperand.Stk().IsFloat();
            if (isFloat) {
                Write(leftOperand.FR());
            } else {
                Write(leftOperand.IR());
            }
        } else {
            FATAL("unexpected format type");
        }
    }

    void FormatArgWithoutSeparator(std::string_view type, int argIdx)
    {
        auto operand = operands[argIdx];
        if (type == "ir") {
            Write(operand.IR());
        } else if (type == "fr") {
            Write(operand.FR());
        } else if (type == "F32") {
            Write(operand.F32());
        } else if (type == "F64") {
            Write(operand.F64());
        } else if (type == "I4") {
            Write(operand.I4());
        } else if (type == "I32") {
            Write(operand.I32());
        } else if (type == "U8") {
            Write(static_cast<uint64_t>(operand.U8()));
        } else if (type == "U16") {
            Write(static_cast<uint64_t>(operand.U16()));
        } else if (type == "U32") {
            Write(static_cast<uint64_t>(operand.U32()));
        } else if (type == "U64") {
            Write(operand.U64());
        } else if (type == "I12") {
            Write(operand.I12());
        } else if (type == "U12") {
            Write(operand.U12());
        } else if (type == "ldk") {
            Write(operand.Ldk());
        } else if (type == "stk") {
            Write(operand.Stk());
        } else if (type == "cc") {
            Write(operand.CC());
        } else if (type == "bin") {
            Write(operand.Bin());
        } else if (type == "fop") {
            Write(operand.Fop());
        } else if (type == "I12L") {
            Write(table->at(operand.U12()).i64);
        } else if (type == "I12L") {
            Write(table->at(operand.U12()).i64);
        } else if (type == "U12L") {
            Write(table->at(operand.U12()).u64);
        } else if (type == "I16L") {
            Write(table->at(operand.U16()).i64);
        } else if (type == "U16L") {
            Write(table->at(operand.U16()).u64);
        } else if (type == "ct") {
            Write(operand.Ct());
        } else {
            FATAL("unexpected format type: %.*s", static_cast<int>(type.length()), type.data());
        }
    }

    static int GetArgIdx(std::string_view str, size_t idx)
    {
        /// There are no format strings in instruction encoding that have more than 9 operands.
        int argIdx = str[idx] - '0';
        ASSERT(0 <= argIdx);
        ASSERT(argIdx < 10);
        return argIdx;
    }

private:
    Interpretation::LiteralTable* table;
    std::ostream& stream;
    std::string_view formatString;
    Operand* operands;
    size_t operandCount;
};

static constexpr std::string_view format_strings[] = {
#define CBC_RT_OPCODE_FMT_STR(opcode, fmt, sfmt) std::string_view(sfmt),
    CBC_RT_OPCODES(CBC_RT_OPCODE_FMT_STR)
};

static constexpr std::string_view memspace_format_strings[] = {
#define CBC_RT_MEMOPCODE_FMT_STR(opcode, fmt, sfmt, isTail) std::string_view(sfmt),
    CBC_RT_MEMOPCODES(CBC_RT_MEMOPCODE_FMT_STR)
};

template <size_t N> static size_t Length(Operand (&)[N]) { return N; }

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B1 args)
{
    Formatter formatter(table, stream, format_strings[args.opc], nullptr, 0);
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B2rr args)
{
    Operand operands[] = { args.rr.x, args.rr.y };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B2xr args)
{
    Operand operands[] = { args.xr.imm, args.xr.r };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B6xri32 args)
{
    Operand operands[] = { args.xr.imm, args.xr.r, args.imm32.imm };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B10xri64 args)
{
    Operand operands[] = { args.xr.imm, args.xr.r, args.imm64.imm };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B4xi12rr args)
{
    Operand operands[] = { args.xi12.imm4, args.xi12.imm12, args.rr.x, args.rr.y };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B5xi12ri12 args)
{
    Operand operands[] = { args.xi12.imm4, args.xi12.imm12, args.ri12.r, args.ri12.imm12 };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B5i16i16 args)
{
    Operand operands[] = { args.imm1.imm, args.imm2.imm };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B5i32 args)
{
    Operand operands[] = { args.imm32.imm };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B3xrrr args)
{
    Operand operands[] = { args.xr.imm, args.xr.r, args.rr.x, args.rr.y };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B3xxrr args)
{
    Operand operands[] = { args.xx.imm1, args.xx.imm2, args.rr.x, args.rr.y };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B4xi12xr args)
{
    Operand operands[] = { args.xi12.imm4, args.xi12.imm12, args.xr.imm, args.xr.r };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B3xi12 args)
{
    Operand operands[] = { args.xi12.imm4, args.xi12.imm12 };
    Formatter formatter(table, stream, format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, M1 args)
{
    Formatter formatter(table, stream, memspace_format_strings[args.opc], nullptr, 0);
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, M2rr args)
{
    Operand operands[] = { args.rr.x, args.rr.y };
    Formatter formatter(table, stream, memspace_format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, M2xr args)
{
    Operand operands[] = { args.xr.imm, args.xr.r };
    Formatter formatter(table, stream, memspace_format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, M2i8 args)
{
    Operand operands[] = { args.imm8 };
    Formatter formatter(table, stream, memspace_format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, M3i16 args)
{
    Operand operands[] = { args.imm16 };
    Formatter formatter(table, stream, memspace_format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, M5i32 args)
{
    Operand operands[] = { args.imm32 };
    Formatter formatter(table, stream, memspace_format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void Log(Interpretation::LiteralTable* table, std::ostream& stream, M9i64 args)
{
    Operand operands[] = { args.imm64 };
    Formatter formatter(table, stream, memspace_format_strings[args.opc], operands, Length(operands));
    formatter.Format();
}

void LogBaseSpaceInstruction(
    uint32_t opc, Interpretation::LiteralTable* table, std::ostream& stream, Decoder::ByteReader& reader
)
{
#define FMT_LOGGER(opcode, fmt, sfmt)                                                                                  \
    case Opcode::opcode: Log(table, stream, fmt::Decode(reader)); break;
    switch (opc) {
        CBC_RT_OPCODES(FMT_LOGGER)
    }
#undef FMT_LOGGER
}

bool LogMemSpaceInstruction(
    uint32_t opc, Interpretation::LiteralTable* table, std::ostream& stream, Decoder::ByteReader& reader
)
{
    stream << "  ";
#define FMT_LOGGER(opcode, fmt, sfmt, isTail)                                                                          \
    case MemOpcode::opcode: Log(table, stream, fmt::Decode(reader)); return isTail;

    switch (opc) {
        CBC_RT_MEMOPCODES(FMT_LOGGER)
    }
#undef FMT_LOGGER
    return true;
}

void Log(Interpretation::Code code, std::ostream& stream)
{
    using namespace RT;
    auto bytecode = code.bytecode;
    auto end      = bytecode + code.bytecodeSize;
    auto table    = code.literals;

    auto log10size = static_cast<int>(1.0 + std::log10(code.bytecodeSize));
    log10size      = std::max(log10size, 1);

    bool inMemspace = false;

    Decoder::ByteReader reader(bytecode, bytecode, end);
    while (!reader.EndOfMem(end)) {
        auto opc      = reader.PeekOpcode();
        auto position = reader.Cursor() - bytecode;
        stream << std::setfill('0') << std::setw(log10size) << position << ": " << std::setfill(' ');

        if (inMemspace) {
            bool isTail = LogMemSpaceInstruction(opc, table, stream, reader);

            if (isTail) {
                inMemspace = false;
            }
        } else {
            LogBaseSpaceInstruction(opc, table, stream, reader);

            if (opc == Opcode::MEMSPACE) {
                inMemspace = true;
            }
        }
    }
#undef FMT_LOGGER
}

} // namespace RT
} // namespace Cbc
