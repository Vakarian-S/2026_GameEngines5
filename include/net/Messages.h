#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace NET
{
    /** The first byte of every message. 0 is never used, so an empty or zeroed message is never valid */
    enum class MessageType : std::uint8_t
    {
        Hello = 1,
        Welcome = 2,
        PlayerJoined = 3,
        PlayerLeft = 4,
    };

    /** Raised whenever a message's layout changes. The server turns away a client whose Hello differs */
    inline constexpr std::uint16_t kProtocolVersion = 1;

    /** Client to server, first thing after Connected */
    struct HelloMessage
    {
        /** The client's kProtocolVersion */
        std::uint16_t protocolVersion = 0;
    };

    /** Server to one client, the answer to its Hello */
    struct WelcomeMessage
    {
        /** This client's number, never reused within a server run */
        std::uint32_t clientId = 0;
        /** The NetworkId of the player the server spawned for this client */
        std::uint32_t playerNetworkId = 0;
    };

    /** Server to every other client when a client joins */
    struct PlayerJoinedMessage
    {
        /** The client that joined */
        std::uint32_t clientId = 0;
    };

    /** Server to every other client when a client leaves */
    struct PlayerLeftMessage
    {
        /** The client that left */
        std::uint32_t clientId = 0;
    };

    /** @return the type in the first byte, or no value when there is none or it is unknown */
    std::optional<MessageType> PeekMessageType(std::span<const std::byte> bytes);

    /** Serializes a Hello message, including the message type and protocol version.
     * @param message the Hello message to serialize
     * @return the encoded message bytes
     */
    std::vector<std::byte> Write(const HelloMessage& message);
    /** Serializes a Welcome message, including the message type, client id, and player network id.
     * @param message the Welcome message to serialize
     * @return the encoded message bytes
     */
    std::vector<std::byte> Write(const WelcomeMessage& message);
    /** Serializes a PlayerJoined message, including the message type and the client id of the client that joined.
     * @param message the PlayerJoined message to serialize
     * @return the encoded message bytes
     */
    std::vector<std::byte> Write(const PlayerJoinedMessage& message);
    /** Serializes a PlayerLeft message, including the message type and the client id of the client that left.
     * @param message the PlayerLeft message to serialize
     * @return the encoded message bytes
     */
    std::vector<std::byte> Write(const PlayerLeftMessage& message);

    /**
     * Reads a Hello message from a byte sequence.
     * @param bytes the bytes to parse
     * @return the decoded HelloMessage, or no value if the bytes do not contain a valid complete Hello message
     */
    std::optional<HelloMessage> ReadHello(std::span<const std::byte> bytes);
    /**
     * Reads a Welcome message from its serialized byte representation.
     * @param bytes the bytes to parse
     * @return the decoded Welcome message, or no value when the bytes do not contain a complete and valid Welcome message
     */
    std::optional<WelcomeMessage> ReadWelcome(std::span<const std::byte> bytes);
    /**
     * Reads a PlayerJoined message from its serialized byte representation.
     * @param bytes the bytes to parse
     * @return the decoded PlayerJoinedMessage, or no value when the bytes do not contain a complete and valid PlayerJoined message
     */
    std::optional<PlayerJoinedMessage> ReadPlayerJoined(std::span<const std::byte> bytes);
    /**
     * Reads a PlayerLeft message from its serialized byte representation.
     * @param bytes the bytes to parse
     * @return the decoded PlayerLeftMessage, or no value when the bytes do not contain a complete and valid PlayerLeft message
     */
    std::optional<PlayerLeftMessage> ReadPlayerLeft(std::span<const std::byte> bytes);
}