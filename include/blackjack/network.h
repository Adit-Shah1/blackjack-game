#pragma once

#include <blackjack/round.h>
#include <blackjack/ai.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace blackjack {

// ---------------------------------------------------------------------------
// Message Types
// ---------------------------------------------------------------------------
enum class MessageType {
    // Client -> Host
    JoinRoom,
    PlaceBet,
    PlayerAction,
    TakeInsurance,
    ReadyToStart,
    ChatMessage,
    Reconnect,
    Disconnect,

    // Host -> Client
    LobbyUpdate,
    SeatAssignment,
    StateSync,
    PhaseChange,
    CardDealt,
    LegalActions,
    HandResult,
    RoundResults,
    PlayerJoined,
    PlayerLeft,
    PlayerDisconnected,
    GameStarted,
    Error,
    Ping,
    Pong
};

std::string toString(MessageType type);
MessageType messageTypeFromString(const std::string& str);

// Card serialization helpers (also used by GUI state sync)
std::string cardToString(const Card& card);
Card cardFromString(const std::string& str);

// ---------------------------------------------------------------------------
// Network Message
// ---------------------------------------------------------------------------
struct NetworkMessage {
    MessageType type;
    nlohmann::json payload;
    int senderId = -1;  // Set by host when receiving from a client

    std::string serialize() const;
    static NetworkMessage deserialize(const std::string& data);
};

// ---------------------------------------------------------------------------
// Client Info (host-side tracking)
// ---------------------------------------------------------------------------
struct ClientInfo {
    int id = -1;
    int socket = -1;
    std::string name;
    int assignedSeat = -1;
    bool ready = false;
    bool connected = false;
    float disconnectTimer = 0.0f;
    std::string receiveBuffer;
    std::string sendBuffer;  // Outbound data pending flush
};

// ---------------------------------------------------------------------------
// Host Server
// ---------------------------------------------------------------------------
class HostServer {
public:
    explicit HostServer(int port = 0);
    ~HostServer();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    std::string getRoomCode() const { return m_roomCode; }
    int getPort() const { return m_port; }
    int getClientCount() const;

    void broadcast(const NetworkMessage& msg);
    void sendTo(int clientId, const NetworkMessage& msg);
    void sendToSeat(int seatIndex, const NetworkMessage& msg);

    // Process one update tick. Returns received messages.
    void update(float deltaTime);

    // Access connected clients
    const std::unordered_map<int, ClientInfo>& clients() const { return m_clients; }
    std::unordered_map<int, ClientInfo>& clients() { return m_clients; }

    // Check if all assigned clients are ready
    bool allClientsReady() const;

    // Kick a client
    void kickClient(int clientId);
    std::vector<NetworkMessage> getMessages();

private:
    int m_listenSocket = -1;
    int m_port = 0;
    bool m_running = false;
    std::string m_roomCode;
    int m_nextClientId = 1;

    std::unordered_map<int, ClientInfo> m_clients;
    std::vector<NetworkMessage> m_receivedMessages;

    bool generateRoomCode();
    bool setNonBlocking(int fd);
    void acceptNewConnection();
    void readClientMessages();
    void processClientBuffer(ClientInfo& client);
    void removeDisconnectedClients();
    void closeSocket(int fd);
};

// ---------------------------------------------------------------------------
// Game Client
// ---------------------------------------------------------------------------
class GameClient {
public:
    GameClient();
    ~GameClient();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool isConnected() const { return m_connected; }

    bool send(const NetworkMessage& msg);
    std::vector<NetworkMessage> receive();  // Non-blocking, returns all available messages

    int clientId() const { return m_clientId; }
    int assignedSeat() const { return m_assignedSeat; }
    void setAssignedSeat(int seat) { m_assignedSeat = seat; }

    // Reconnect with previous client ID
    bool reconnect(const std::string& host, int port, int previousClientId);

private:
    int m_socket = -1;
    bool m_connected = false;
    int m_clientId = -1;
    int m_assignedSeat = -1;
    std::string m_receiveBuffer;

    bool setNonBlocking(int fd);
    void processBuffer(std::vector<NetworkMessage>& outMessages);
};

// ---------------------------------------------------------------------------
// Network Game Session (Authoritative Host)
// ---------------------------------------------------------------------------
class NetworkGameSession {
public:
    NetworkGameSession();
    ~NetworkGameSession();

    // Host lifecycle
    bool startHosting(const RuleSet& rules);
    void stopHosting();
    bool isHosting() const { return m_hosting; }

    // Process one tick on the host
    void updateHost(float deltaTime);

    // Access the round state (host authority)
    RoundState& round() { return *m_round; }
    const RoundState& round() const { return *m_round; }

    HostServer& server() { return *m_server; }

    // Map client IDs to seat indices
    int getSeatForClient(int clientId) const;
    int getClientForSeat(int seatIndex) const;

private:
    std::unique_ptr<HostServer> m_server;
    std::unique_ptr<RoundState> m_round;
    RuleSet m_rules;
    bool m_hosting = false;
    bool m_gameStarted = false;
    bool m_bettingPhaseComplete = false;

    std::unordered_map<int, int> m_clientToSeat;  // clientId -> seatIndex
    std::unordered_map<int, int> m_seatToClient;  // seatIndex -> clientId

    // Pending actions from clients that arrived between ticks
    struct PendingAction {
        int clientId;
        NetworkMessage message;
    };
    std::vector<PendingAction> m_pendingActions;

    void processClientMessages();
    void handleJoinRoom(int clientId, const nlohmann::json& payload);
    void handlePlaceBet(int clientId, const nlohmann::json& payload);
    void handlePlayerAction(int clientId, const nlohmann::json& payload);
    void handleTakeInsurance(int clientId, const nlohmann::json& payload);
    void handleReadyToStart(int clientId);
    void handleReconnect(int clientId, const nlohmann::json& payload);
    void handleDisconnect(int clientId);

    void broadcastLobbyUpdate();
    void broadcastStateSync(int targetClientId = -1);
    void broadcastPhaseChange();
    void broadcastCardDealt(int seatIndex, int handIndex, const Card& card);
    void broadcastHandResult(int seatIndex, int handIndex);
    void broadcastRoundResults();

    void startGame();
    void checkAutoAdvance(float deltaTime);
    void processAIAndDealer(float deltaTime);

    float m_autoAdvanceTimer = 0.0f;
    float m_aiTurnTimer = 0.0f;
    float m_aiTurnDelay = 0.6f;

    // AI controllers for any empty seats
    std::vector<std::unique_ptr<AIController>> m_aiControllers;
    void setupAIForEmptySeats();
    void executeAIAction(int seatIndex, int handIndex);
};

// ---------------------------------------------------------------------------
// Network Client Session (Client-side mirror)
// ---------------------------------------------------------------------------
class NetworkClientSession {
public:
    NetworkClientSession();
    ~NetworkClientSession();

    bool connect(const std::string& host, int port, const std::string& playerName);
    void disconnect();
    bool isConnected() const;

    // Process one tick on the client
    void update(float deltaTime);

    // Send actions to host
    bool sendBet(int amount);
    bool sendAction(PlayerAction action);
    bool sendInsurance(bool take);
    bool sendReady();
    bool sendChat(const std::string& text);

    // Inject an already-connected GameClient (e.g. from NetworkJoinScreen)
    void setGameClient(std::unique_ptr<GameClient> client);

    // Access mirrored state
    RoundState& round() { return *m_round; }
    const RoundState& round() const { return *m_round; }

    int mySeatIndex() const { return m_mySeat; }
    bool isMyTurn() const;
    bool isInLobby() const { return m_inLobby; }
    bool isGameStarted() const { return m_gameStarted; }

    // Lobby info
    struct LobbyPlayer {
        std::string name;
        int seatIndex;
        bool ready;
    };
    const std::vector<LobbyPlayer>& lobbyPlayers() const { return m_lobbyPlayers; }

    // Event callbacks (set by GUI)
    std::function<void()> onStateSync;
    std::function<void()> onGameStarted;
    std::function<void(const std::string&)> onError;
    std::function<void()> onDisconnected;
    std::function<void()> onLobbyUpdate;

private:
    std::unique_ptr<GameClient> m_client;
    std::unique_ptr<RoundState> m_round;
    std::string m_playerName;
    int m_mySeat = -1;
    bool m_inLobby = false;
    bool m_gameStarted = false;
    std::vector<LobbyPlayer> m_lobbyPlayers;
    bool m_connected = false;

    void processMessages();
    void handleStateSync(const nlohmann::json& payload);
    void handlePhaseChange(const nlohmann::json& payload);
    void handleCardDealt(const nlohmann::json& payload);
    void handleHandResult(const nlohmann::json& payload);
    void handleRoundResults(const nlohmann::json& payload);
    void handleLobbyUpdate(const nlohmann::json& payload);
    void handleSeatAssignment(const nlohmann::json& payload);
    void handleGameStarted(const nlohmann::json& payload);
    void handlePlayerJoined(const nlohmann::json& payload);
    void handlePlayerLeft(const nlohmann::json& payload);
    void handleError(const nlohmann::json& payload);
};

}  // namespace blackjack
