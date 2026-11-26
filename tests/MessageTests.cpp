/*
 * Tests for ByteWriter, ByteReader and the step 3 messages, built as their own program.
 *
 * These files include no GNS header, so this target builds on Win32 as well as x64, and the
 * byte layout is checked on both.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <vector>

#include "net/ByteReader.h"
#include "net/ByteWriter.h"
#include "net/Messages.h"

namespace
{
    int checks = 0;
    int failures = 0;

    constexpr const char* Pass = "\x1b[1;32m";
    constexpr const char* Fail = "\x1b[1;91m";
    constexpr const char* Reset = "\x1b[0m";

    /** Prints one named expectation, and remembers whether it held. */
    void Check(const char* what, const bool held)
    {
        ++checks;
        if (!held) ++failures;
        std::cout << (held ? "  pass  " : "  FAIL  ") << what << "\n";
    }

    /** @return the given numbers as bytes, so an expected message reads as a list of hex values */
    std::vector<std::byte> Bytes(const std::initializer_list<int> values)
    {
        std::vector<std::byte> bytes;
        for (const int value : values) bytes.push_back(static_cast<std::byte>(value));
        return bytes;
    }
}

int main()
{
    std::cout << "MessageTests\n\n";

    std::cout << "TEST CASE: the writer puts the high byte first, and the reader reads it back\n";
    {
        NET::ByteWriter writer;
        writer.WriteU8(0xAB);
        writer.WriteU16(0x1234);
        writer.WriteU32(0xDEADBEEF);
        const std::vector<std::byte> written(writer.GetBytes().begin(), writer.GetBytes().end());
        Check("the bytes are AB 12 34 DE AD BE EF", written == Bytes({0xAB, 0x12, 0x34, 0xDE, 0xAD, 0xBE, 0xEF}));

        const std::vector<std::byte> taken = writer.TakeBytes();
        Check("TakeBytes hands back all seven bytes", taken.size() == 7);

        NET::ByteReader reader(taken);
        const std::uint8_t u8 = reader.ReadU8();
        const std::uint16_t u16 = reader.ReadU16();
        const std::uint32_t u32 = reader.ReadU32();
        Check("the reader returns the same three numbers", u8 == 0xAB && u16 == 0x1234 && u32 == 0xDEADBEEF);
        Check("the reader is valid and at the end", reader.IsValid() && reader.AtEnd());
    }

    std::cout << "\nTEST CASE: zero and the largest values round-trip\n";
    {
        NET::ByteWriter writer;
        writer.WriteU16(std::numeric_limits<std::uint16_t>::max());
        writer.WriteU32(std::numeric_limits<std::uint32_t>::max());
        writer.WriteU32(0);
        NET::ByteReader reader(writer.GetBytes());
        const std::uint16_t max16 = reader.ReadU16();
        const std::uint32_t max32 = reader.ReadU32();
        const std::uint32_t zero = reader.ReadU32();
        Check("FFFF, FFFFFFFF and 0 come back", max16 == 0xFFFF && max32 == 0xFFFFFFFFu && zero == 0);
        Check("the reader is valid and at the end", reader.IsValid() && reader.AtEnd());
    }

    std::cout << "\nTEST CASE: a read past the end returns 0, and the failure sticks\n";
    {
        const std::vector<std::byte> one = Bytes({7});
        NET::ByteReader reader(one);
        reader.ReadU8();
        Check("a second U8 from one byte returns 0", reader.ReadU8() == 0);
        Check("the reader is now invalid", !reader.IsValid());

        const std::vector<std::byte> three = Bytes({1, 2, 3});
        NET::ByteReader shortReader(three);
        Check("a U32 from three bytes returns 0, not half a number", shortReader.ReadU32() == 0);
        Check("a later U8 fails too, though a byte is left", shortReader.ReadU8() == 0 && !shortReader.IsValid());
    }

    std::cout << "\nTEST CASE: each message has the exact bytes the plan sets\n";
    {
        Check("Hello { 1 } is 01 00 01", NET::Write(NET::HelloMessage{NET::kProtocolVersion}) == Bytes({1, 0, 1}));
        Check("Welcome { 0x01020304, 5 } is 02 01 02 03 04 00 00 00 05",
              NET::Write(NET::WelcomeMessage{0x01020304, 5}) == Bytes({2, 1, 2, 3, 4, 0, 0, 0, 5}));
        Check("PlayerJoined { 7 } is 03 00 00 00 07", NET::Write(NET::PlayerJoinedMessage{7}) == Bytes({3, 0, 0, 0, 7}));
        Check("PlayerLeft { FFFFFFFF } is 04 FF FF FF FF",
              NET::Write(NET::PlayerLeftMessage{0xFFFFFFFFu}) == Bytes({4, 0xFF, 0xFF, 0xFF, 0xFF}));
    }

    std::cout << "\nTEST CASE: each message round-trips\n";
    {
        const auto hello = NET::ReadHello(NET::Write(NET::HelloMessage{0xBEEF}));
        Check("Hello", hello && hello->protocolVersion == 0xBEEF);
        const auto welcome = NET::ReadWelcome(NET::Write(NET::WelcomeMessage{123456789, 987654321}));
        Check("Welcome, with its two fields in order",
              welcome && welcome->clientId == 123456789 && welcome->playerNetworkId == 987654321);
        const auto joined = NET::ReadPlayerJoined(NET::Write(NET::PlayerJoinedMessage{42}));
        Check("PlayerJoined", joined && joined->clientId == 42);
        const auto left = NET::ReadPlayerLeft(NET::Write(NET::PlayerLeftMessage{43}));
        Check("PlayerLeft", left && left->clientId == 43);
    }

    std::cout << "\nTEST CASE: a bad message reads as no value\n";
    {
        const std::vector<std::byte> welcome = NET::Write(NET::WelcomeMessage{1, 2});
        bool everyCutFails = true;
        for (std::size_t size = 0; size < welcome.size(); ++size)
        {
            const std::vector<std::byte> cut(welcome.begin(), welcome.begin() + static_cast<std::ptrdiff_t>(size));
            if (NET::ReadWelcome(cut)) everyCutFails = false;
        }
        Check("every cut-short Welcome, 0 to 8 bytes", everyCutFails);

        std::vector<std::byte> extra = welcome;
        extra.push_back(std::byte{0});
        Check("a Welcome with one extra byte", !NET::ReadWelcome(extra));
        Check("Welcome bytes read as a Hello", !NET::ReadHello(welcome));
        Check("PlayerLeft bytes read as a PlayerJoined", !NET::ReadPlayerJoined(NET::Write(NET::PlayerLeftMessage{9})));
        Check("an empty message read as a Hello", !NET::ReadHello({}));
    }

    std::cout << "\nTEST CASE: PeekMessageType knows types 1 to 4 and nothing else\n";
    {
        Check("an empty message has no type", !NET::PeekMessageType({}));
        Check("type 0 is refused, so a zeroed message is never valid", !NET::PeekMessageType(Bytes({0})));
        Check("type 5 is refused", !NET::PeekMessageType(Bytes({5})));
        Check("type 255 is refused", !NET::PeekMessageType(Bytes({255})));
        bool allKnown = true;
        for (int type = 1; type <= 4; ++type)
        {
            const auto peeked = NET::PeekMessageType(Bytes({type}));
            allKnown = allKnown && peeked && static_cast<int>(*peeked) == type;
        }
        Check("types 1 to 4 are known", allKnown);
        const auto peeked = NET::PeekMessageType(NET::Write(NET::WelcomeMessage{1, 2}));
        Check("a written Welcome peeks as Welcome", peeked && *peeked == NET::MessageType::Welcome);
    }

    std::cout << "\n" << (failures == 0 ? Pass : Fail) << checks - failures << " of " << checks
        << " checks passed" << Reset << "\n";
    return failures == 0 ? 0 : 1;
}
