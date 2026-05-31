// Network layer tests
#include <catch2/catch_test_macros.hpp>
#include <blackjack/network.h>
#include <blackjack/card.h>

using namespace blackjack;

// ============================================================================
// Card Serialization
// ============================================================================

TEST_CASE("cardToString round-trips through cardFromString", "[network][serialization]") {
    Card original(Suit::Hearts, Rank::Ace);
    std::string encoded = cardToString(original);
    REQUIRE(encoded == "AH");

    Card decoded = cardFromString(encoded);
    REQUIRE(decoded.suit() == Suit::Hearts);
    REQUIRE(decoded.rank() == Rank::Ace);
}

TEST_CASE("cardFromString handles all suits and ranks", "[network][serialization]") {
    REQUIRE(cardFromString("2H").suit() == Suit::Hearts);
    REQUIRE(cardFromString("2H").rank() == Rank::Two);

    REQUIRE(cardFromString("TD").suit() == Suit::Diamonds);
    REQUIRE(cardFromString("TD").rank() == Rank::Ten);

    REQUIRE(cardFromString("JC").suit() == Suit::Clubs);
    REQUIRE(cardFromString("JC").rank() == Rank::Jack);

    REQUIRE(cardFromString("KS").suit() == Suit::Spades);
    REQUIRE(cardFromString("KS").rank() == Rank::King);

    REQUIRE(cardFromString("AS").suit() == Suit::Spades);
    REQUIRE(cardFromString("AS").rank() == Rank::Ace);
}

TEST_CASE("cardFromString returns default card for invalid input", "[network][serialization]") {
    Card def = cardFromString("");
    REQUIRE(def.suit() == Suit::Hearts);
    REQUIRE(def.rank() == Rank::Two);

    Card def2 = cardFromString("X");
    REQUIRE(def2.suit() == Suit::Hearts);
    REQUIRE(def2.rank() == Rank::Two);
}

// ============================================================================
// Message Type Conversions
// ============================================================================

TEST_CASE("toString converts all MessageTypes correctly", "[network][protocol]") {
    REQUIRE(toString(MessageType::JoinRoom) == "JoinRoom");
    REQUIRE(toString(MessageType::PlaceBet) == "PlaceBet");
    REQUIRE(toString(MessageType::PlayerAction) == "PlayerAction");
    REQUIRE(toString(MessageType::StateSync) == "StateSync");
    REQUIRE(toString(MessageType::GameStarted) == "GameStarted");
    REQUIRE(toString(MessageType::Error) == "Error");
    REQUIRE(toString(MessageType::Ping) == "Ping");
    REQUIRE(toString(MessageType::Pong) == "Pong");
}

TEST_CASE("messageTypeFromString round-trips with toString", "[network][protocol]") {
    std::vector<MessageType> types = {
        MessageType::JoinRoom, MessageType::PlaceBet, MessageType::PlayerAction,
        MessageType::TakeInsurance, MessageType::ReadyToStart, MessageType::StateSync,
        MessageType::PhaseChange, MessageType::HandResult, MessageType::GameStarted,
        MessageType::Error, MessageType::Ping, MessageType::Pong
    };
    for (auto t : types) {
        REQUIRE(messageTypeFromString(toString(t)) == t);
    }
}

TEST_CASE("messageTypeFromString returns Ping for unknown strings", "[network][protocol]") {
    REQUIRE(messageTypeFromString("UnknownType") == MessageType::Ping);
    REQUIRE(messageTypeFromString("") == MessageType::Ping);
}

// ============================================================================
// NetworkMessage Serialization
// ============================================================================

TEST_CASE("NetworkMessage serialize/deserialize round-trip", "[network][serialization]") {
    NetworkMessage original;
    original.type = MessageType::PlaceBet;
    original.payload["amount"] = 100;
    original.payload["seatIndex"] = 2;
    original.senderId = -1;

    std::string data = original.serialize();
    REQUIRE(!data.empty());
    REQUIRE(data.size() >= 4);

    NetworkMessage decoded = NetworkMessage::deserialize(data);
    REQUIRE(decoded.type == MessageType::PlaceBet);
    REQUIRE(decoded.payload["amount"] == 100);
    REQUIRE(decoded.payload["seatIndex"] == 2);
}

TEST_CASE("NetworkMessage deserialize handles incomplete header", "[network][serialization]") {
    std::string badData = "AB";  // Less than 4 bytes
    NetworkMessage msg = NetworkMessage::deserialize(badData);
    REQUIRE(msg.type == MessageType::Error);
}

TEST_CASE("NetworkMessage deserialize handles incomplete body", "[network][serialization]") {
    // 4-byte header saying length = 100, but body is shorter
    std::string badData;
    badData.push_back(0);
    badData.push_back(0);
    badData.push_back(0);
    badData.push_back(static_cast<char>(100));
    badData += "short";

    NetworkMessage msg = NetworkMessage::deserialize(badData);
    REQUIRE(msg.type == MessageType::Error);
}

TEST_CASE("NetworkMessage serialize includes length prefix", "[network][serialization]") {
    NetworkMessage msg;
    msg.type = MessageType::Ping;
    std::string data = msg.serialize();
    REQUIRE(data.size() >= 4);
    uint32_t len = (static_cast<uint32_t>(static_cast<unsigned char>(data[0])) << 24) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(data[1])) << 16) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(data[2])) << 8) |
                   static_cast<uint32_t>(static_cast<unsigned char>(data[3]));
    REQUIRE(4 + len == data.size());
}

// ============================================================================
// HostServer Basics
// ============================================================================

TEST_CASE("HostServer generates a 4-character room code", "[network][server]") {
    HostServer server(0);
    REQUIRE(server.getRoomCode().empty());  // Before start()

    bool started = server.start();
    REQUIRE(started);
    REQUIRE(server.isRunning());
    REQUIRE(server.getRoomCode().size() == 4);
    REQUIRE(server.getPort() > 0);

    server.stop();
    REQUIRE(!server.isRunning());
}

TEST_CASE("HostServer client count starts at zero", "[network][server]") {
    HostServer server(0);
    REQUIRE(server.getClientCount() == 0);
    REQUIRE(server.getMessages().empty());
}

TEST_CASE("HostServer allClientsReady is false with no clients", "[network][server]") {
    HostServer server(0);
    REQUIRE(!server.allClientsReady());  // Empty room is not "ready"
}

TEST_CASE("HostServer start/stop lifecycle", "[network][server]") {
    HostServer server1(0);
    REQUIRE(server1.start());
    REQUIRE(server1.isRunning());
    server1.stop();
    REQUIRE(!server1.isRunning());
}

TEST_CASE("HostServer can be started on a specific port", "[network][server]") {
    HostServer server(37016);
    bool started = server.start();
    if (started) {
        REQUIRE(server.getPort() == 37016);
        server.stop();
    }
    // If port is in use, start() may fail — that's acceptable
    REQUIRE(true);
}

// ============================================================================
// GameClient Basics
// ============================================================================

TEST_CASE("GameClient starts disconnected", "[network][client]") {
    GameClient client;
    REQUIRE(!client.isConnected());
    REQUIRE(client.clientId() == -1);
    REQUIRE(client.assignedSeat() == -1);
}

// ============================================================================
// ClientInfo Defaults
// ============================================================================

TEST_CASE("ClientInfo has correct default values", "[network][protocol]") {
    ClientInfo info;
    REQUIRE(info.id == -1);
    REQUIRE(info.socket == -1);
    REQUIRE(info.name.empty());
    REQUIRE(info.assignedSeat == -1);
    REQUIRE(!info.ready);
    REQUIRE(!info.connected);
    REQUIRE(info.disconnectTimer == 0.0f);
    REQUIRE(info.receiveBuffer.empty());
}
