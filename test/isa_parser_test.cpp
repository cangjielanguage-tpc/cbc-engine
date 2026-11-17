#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "cbc/isa_disasm.h"
#include "utils/ostream.h"

namespace {

std::string Disassemble(std::initializer_list<uint8_t> bytes)
{
    uint8_t buffer[16];
    size_t size = 0;
    for (auto byte : bytes) {
        buffer[size++] = byte;
    }
    Stream::StringBuffer stream;
    Cbc::RawDisasm(stream, Decoder::FatByteReader(buffer, buffer, buffer + size));
    return stream.ToString();
}

uint8_t RegSymGroupByte(Cbc::RegSymGroup group, Cbc::IReg dst) { return (group.Raw() << 4) | dst.Raw(); }

uint8_t SizeGByte(Cbc::IReg dst) { return (dst.Raw() << 4) | 0; }

} // namespace

// Encoding pins for the size/align opcodes: RegSymGroup cases carry
// `dst nibble + uleb type symbol id` (same shape as load.type.info), the
// TypeSize/TypeAlign opcode slots carry `dst nibble + zero pad nibble + uleb idx`.
TEST(IsaParserTest, Size)
{
    auto bytes = {
        static_cast<uint8_t>(Cbc::Opcode::RegSymGroup),
        RegSymGroupByte(Cbc::RegSymGroup::Size, Cbc::IReg::IR12),
        static_cast<uint8_t>(122),
    };
    EXPECT_EQ(Disassemble(bytes), "0: size IR12, @122\n");
}

TEST(IsaParserTest, Align)
{
    auto bytes = {
        static_cast<uint8_t>(Cbc::Opcode::RegSymGroup),
        RegSymGroupByte(Cbc::RegSymGroup::Align, Cbc::IReg::IR3),
        static_cast<uint8_t>(5),
    };
    EXPECT_EQ(Disassemble(bytes), "0: align IR3, @5\n");
}

TEST(IsaParserTest, SizeGeneric)
{
    auto bytes = {
        static_cast<uint8_t>(Cbc::Opcode::TypeSize),
        SizeGByte(Cbc::IReg::IR12),
        static_cast<uint8_t>(5),
    };
    EXPECT_EQ(Disassemble(bytes), "0: size.g IR12, 5\n");
}

TEST(IsaParserTest, AlignGeneric)
{
    auto bytes = {
        static_cast<uint8_t>(Cbc::Opcode::TypeAlign),
        SizeGByte(Cbc::IReg::IR3),
        static_cast<uint8_t>(0xAC),
        static_cast<uint8_t>(0x02), // uleb 300
    };
    EXPECT_EQ(Disassemble(bytes), "0: align.g IR3, 300\n");
}
