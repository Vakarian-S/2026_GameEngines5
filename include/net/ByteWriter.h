#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <utility>

namespace NET
{
    /** Writes numbers into a byte list, high byte first, one field after another */
    class ByteWriter
    {
    public:
        /** 
         * Appends one byte
         * @param value unsigned int byte to write
         */
        void WriteU8(std::uint8_t value);

        /** Appends two bytes, high byte first
         * @param value unsigned int bytes to write
         */
        void WriteU16(std::uint16_t value);

        /** Appends four bytes, high byte first
         * @param value unsigned int bytes to write
         */
        void WriteU32(std::uint32_t value);

        /** @return The bytes written so far. Valid until the next write */
        [[nodiscard]] std::span<const std::byte> GetBytes() const { return bytes; }
        /** @return The bytes written, moved out. The writer is empty afterwards */
        [[nodiscard]] std::vector<std::byte> TakeBytes() { return std::move(bytes); }

    private:
        /** Every byte written, in order */
        std::vector<std::byte> bytes;
    };
}
