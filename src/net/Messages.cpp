#include "net/Messages.h"

#include "net/ByteReader.h"
#include "net/ByteWriter.h"

namespace
{
    /**
     * Creates a byte writer initialized with the message type byte.
     * @param type the message type to write as the first byte
     * @return a byte writer containing the encoded message type
     */
    NET::ByteWriter Begin(const NET::MessageType type)
    {
        NET::ByteWriter writer;
        writer.WriteU8(static_cast<std::uint8_t>(type));
        return writer;
    }
    
    /**
     * Reads and validates the next byte as the expected message type.
     * @param reader the byte reader to read the message type from
     * @param type the expected message type to compare against
     * @return true if the next byte matches the expected type and the reader remains valid, false otherwise
     */
    bool ReadType(NET::ByteReader& reader, const NET::MessageType type)
    {
        return reader.ReadU8() == static_cast<std::uint8_t>(type) && reader.IsValid();
    }


    /**
     * Finishes decoding a message by verifying that the reader remained valid and consumed all bytes.
     * @tparam T MessageType of the message to read
     * @param reader the byte reader used to decode the message
     * @param message the decoded message to return when decoding completed successfully
     * @return the decoded message, or no value if the reader is invalid or has unread bytes remaining
     */
    template <typename T>
    std::optional<T> Finish(const NET::ByteReader& reader, const T& message)
    {
        if (!reader.IsValid() || !reader.AtEnd()) return std::nullopt;
        return message;
    }
}

std::optional<NET::MessageType> NET::PeekMessageType(const std::span<const std::byte> bytes)
{
    if (bytes.empty()) return std::nullopt;
    const auto value = std::to_integer<std::uint8_t>(bytes[0]);
    if (value < static_cast<std::uint8_t>(MessageType::Hello) || value > static_cast<std::uint8_t>(
        MessageType::PlayerLeft))
        return std::nullopt;
    return static_cast<MessageType>(value);
}

std::vector<std::byte> NET::Write(const HelloMessage& message)
{
    ByteWriter writer = Begin(MessageType::Hello);
    writer.WriteU16(message.protocolVersion);
    return writer.TakeBytes();
}

std::vector<std::byte> NET::Write(const WelcomeMessage& message)
{
    ByteWriter writer = Begin(MessageType::Welcome);
    writer.WriteU32(message.clientId);
    writer.WriteU32(message.playerNetworkId);
    return writer.TakeBytes();
}

std::vector<std::byte> NET::Write(const PlayerJoinedMessage& message)
{
    ByteWriter writer = Begin(MessageType::PlayerJoined);
    writer.WriteU32(message.clientId);
    return writer.TakeBytes();
}

std::vector<std::byte> NET::Write(const PlayerLeftMessage& message)
{
    ByteWriter writer = Begin(MessageType::PlayerLeft);
    writer.WriteU32(message.clientId);
    return writer.TakeBytes();
}

std::optional<NET::HelloMessage> NET::ReadHello(const std::span<const std::byte> bytes)
{
    ByteReader reader(bytes);
    if (!ReadType(reader, MessageType::Hello)) return std::nullopt;
    const HelloMessage message{.protocolVersion = reader.ReadU16()};
    return Finish(reader, message);
}

std::optional<NET::WelcomeMessage> NET::ReadWelcome(const std::span<const std::byte> bytes)
{
    ByteReader reader(bytes);
    if (!ReadType(reader, MessageType::Welcome)) return std::nullopt;
    /* A braced list runs its parts left to right, so clientId is read first */
    const WelcomeMessage message{.clientId = reader.ReadU32(), .playerNetworkId = reader.ReadU32()};
    return Finish(reader, message);
}

std::optional<NET::PlayerJoinedMessage> NET::ReadPlayerJoined(const std::span<const std::byte> bytes)
{
    ByteReader reader(bytes);
    if (!ReadType(reader, MessageType::PlayerJoined)) return std::nullopt;
    const PlayerJoinedMessage message{.clientId = reader.ReadU32()};
    return Finish(reader, message);
}

std::optional<NET::PlayerLeftMessage> NET::ReadPlayerLeft(const std::span<const std::byte> bytes)
{
    ByteReader reader(bytes);
    if (!ReadType(reader, MessageType::PlayerLeft)) return std::nullopt;
    const PlayerLeftMessage message{.clientId = reader.ReadU32()};
    return Finish(reader, message);
}
