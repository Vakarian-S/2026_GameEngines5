#include "net/ByteReader.h"

std::uint8_t NET::ByteReader::ReadU8()
{
    if (!HasRoom(1)) return 0;
    return std::to_integer<std::uint8_t>(bytes[offset++]);
}

std::uint16_t NET::ByteReader::ReadU16()
{
    if (!HasRoom(2)) return 0;
    const auto high = std::to_integer<std::uint16_t>(bytes[offset]);
    const auto low = std::to_integer<std::uint16_t>(bytes[offset + 1]);
    offset += 2;
    return static_cast<std::uint16_t>((high << 8) | low);
}

std::uint32_t NET::ByteReader::ReadU32()
{
    if (!HasRoom(4)) return 0;
    const std::uint32_t high = ReadU16();
    const std::uint32_t low = ReadU16();
    return (high << 16) | low;
}

bool NET::ByteReader::HasRoom(const std::size_t count)
{
    /* Written as a subtraction, so a huge count cannot wrap around past the end */
    if (!valid || bytes.size() - offset < count)
    {
        valid = false;
        return false;
    }
    return true;
}
