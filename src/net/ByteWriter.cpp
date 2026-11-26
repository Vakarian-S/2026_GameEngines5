#include "net/ByteWriter.h"

void NET::ByteWriter::WriteU8(const std::uint8_t value)
{
    bytes.push_back(static_cast<std::byte>(value));
}

void NET::ByteWriter::WriteU16(const std::uint16_t value)
{
    WriteU8(static_cast<std::uint8_t>(value >> 8));
    WriteU8(static_cast<std::uint8_t>(value));
}

void NET::ByteWriter::WriteU32(const std::uint32_t value)
{
    WriteU16(static_cast<std::uint16_t>(value >> 16));
    WriteU16(static_cast<std::uint16_t>(value));
}