#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace NET
{
    /**
     * Reads numbers back out of bytes, high byte first. A read past the end returns 0 and marks the reader invalid,
     * and every later read fails too, so a message checks IsValid once after all its reads
     */
    class ByteReader
    {
    public:
        /** @param bytes_ the bytes to read. They must outlive the reader */
        explicit ByteReader(std::span<const std::byte> bytes_) : bytes(bytes_)
        {
        }
        
        /** @return the next byte, or 0 when none is left */
        std::uint8_t ReadU8();
        /** @return the next two bytes as a number, or 0 when fewer are left */
        std::uint16_t ReadU16();
        /** @return the next four bytes as a number, or 0 when fewer are left */
        std::uint32_t ReadU32();

        /** @return false once any read ran past the end */
        [[nodiscard]] bool IsValid() const { return valid; }
        /** @return true when every byte has been read */
        [[nodiscard]] bool AtEnd() const { return offset == bytes.size(); }

    private:
        /** Checks that count more bytes are left. Marks the reader invalid when they are not
         * @param count amount of bytes to check
         */
        bool HasRoom(std::size_t count);

        /** The bytes being read */
        std::span<const std::byte> bytes;
        /** Where the next read starts */
        std::size_t offset = 0;
        /** False once a read ran past the end */
        bool valid = true;
    };
}
