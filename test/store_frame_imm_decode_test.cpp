#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "cbc/isa_rt.h"

template <size_t N> static Cbc::RT::StoreFrameImm DecodeBytes(std::array<uint8_t, N>& bytes)
{
    Decoder::ByteReader reader(bytes.data(), bytes.data(), bytes.data() + bytes.size());
    return Cbc::RT::StoreFrameImm::Decode(reader);
}

TEST(StoreFrameImmDecode, Positive1)
{
    std::array<uint8_t, 7> bytes = {
        static_cast<uint8_t>(Cbc::RT::Opcode::STORE_FRAME_IMM_1),
        static_cast<uint8_t>(Cbc::Format::StoreAccessKind::ST_64),
        0x7F,
        0xFC,
        0xFF,
        0xFF,
        0xFF,
    };
    auto res = DecodeBytes(bytes);
    EXPECT_EQ(res.opc, Cbc::RT::Opcode::STORE_FRAME_IMM_1);
    EXPECT_EQ(res.stk, Cbc::Format::StoreAccessKind::ST_64);
    EXPECT_EQ(res.imm, 0x7Full);
    EXPECT_EQ(res.offs, -4);
}

TEST(StoreFrameImmDecode, Positive2)
{
    std::array<uint8_t, 8> bytes = {
        static_cast<uint8_t>(Cbc::RT::Opcode::STORE_FRAME_IMM_2),
        static_cast<uint8_t>(Cbc::Format::StoreAccessKind::ST_64),
        0x34,
        0x12,
        0xFC,
        0xFF,
        0xFF,
        0xFF,
    };
    auto res = DecodeBytes(bytes);
    EXPECT_EQ(res.imm, 0x1234ull);
}

TEST(StoreFrameImmDecode, Positive4)
{
    std::array<uint8_t, 10> bytes = {
        static_cast<uint8_t>(Cbc::RT::Opcode::STORE_FRAME_IMM_4),
        static_cast<uint8_t>(Cbc::Format::StoreAccessKind::ST_64),
        0x78,
        0x56,
        0x34,
        0x12,
        0xFC,
        0xFF,
        0xFF,
        0xFF,
    };
    auto res = DecodeBytes(bytes);
    EXPECT_EQ(res.imm, 0x12345678ull);
}

TEST(StoreFrameImmDecode, Positive8)
{
    std::array<uint8_t, 14> bytes = {
        static_cast<uint8_t>(Cbc::RT::Opcode::STORE_FRAME_IMM_8),
        static_cast<uint8_t>(Cbc::Format::StoreAccessKind::ST_64),
        0x88,
        0x77,
        0x66,
        0x55,
        0x44,
        0x33,
        0x22,
        0x11,
        0xFC,
        0xFF,
        0xFF,
        0xFF,
    };
    auto res = DecodeBytes(bytes);
    EXPECT_EQ(res.imm, 0x1122334455667788ull);
}

TEST(StoreFrameImmDecode, Negative8)
{
    std::array<uint8_t, 14> bytes = {
        static_cast<uint8_t>(Cbc::RT::Opcode::STORE_FRAME_IMM_8),
        static_cast<uint8_t>(Cbc::Format::StoreAccessKind::ST_64),
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFC,
        0xFF,
        0xFF,
        0xFF,
    };
    auto res = DecodeBytes(bytes);
    EXPECT_EQ(res.imm, 0xFFFFFFFFFFFFFFFFull);
}

TEST(StoreFrameImmDecode, Negative1)
{
    std::array<uint8_t, 7> bytes = {
        static_cast<uint8_t>(Cbc::RT::Opcode::STORE_FRAME_IMM_1),
        static_cast<uint8_t>(Cbc::Format::StoreAccessKind::ST_64),
        0xFF,
        0xFC,
        0xFF,
        0xFF,
        0xFF,
    };
    auto res = DecodeBytes(bytes);
    EXPECT_EQ(res.imm, 0xFFFFFFFFFFFFFFFFull);
}

TEST(StoreFrameImmDecode, Negative2)
{
    std::array<uint8_t, 8> bytes = {
        static_cast<uint8_t>(Cbc::RT::Opcode::STORE_FRAME_IMM_2),
        static_cast<uint8_t>(Cbc::Format::StoreAccessKind::ST_64),
        0xFF,
        0xFF,
        0xFC,
        0xFF,
        0xFF,
        0xFF,
    };
    auto res = DecodeBytes(bytes);
    EXPECT_EQ(res.imm, 0xFFFFFFFFFFFFFFFFull);
}

TEST(StoreFrameImmDecode, Negative4)
{
    std::array<uint8_t, 10> bytes = {
        static_cast<uint8_t>(Cbc::RT::Opcode::STORE_FRAME_IMM_4),
        static_cast<uint8_t>(Cbc::Format::StoreAccessKind::ST_64),
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFC,
        0xFF,
        0xFF,
        0xFF,
    };
    auto res = DecodeBytes(bytes);
    EXPECT_EQ(res.imm, 0xFFFFFFFFFFFFFFFFull);
}
