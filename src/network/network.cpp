// Network layer — Host-based authoritative multiplayer with room codes
#include <blackjack/network.h>
#include <blackjack/ai.h>

#include <iostream>
#include <random>
#include <algorithm>
#include <cstring>

// Cross-platform socket headers
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCKET closesocket
#define SOCKET_ERR WSAGetLastError()
#define WOULD_BLOCK WSAEWOULDBLOCK
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define CLOSE_SOCKET close
#define SOCKET_ERR errno
#define WOULD_BLOCK EAGAIN
#endif

namespace blackjack {

// ============================================================================
// Helpers
// ============================================================================

static std::string generateRandomRoomCode() {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 35);
    std::string code;
    code.reserve(4);
    for (int i = 0; i < 4; ++i) {
        code += chars[dist(rng)];
    }
    return code;
}

static bool setSocketNonBlocking(int fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
#endif
}

static int getSocketError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static bool isWouldBlock(int err) {
#ifdef _WIN32
    return err == WSAEWOULDBLOCK || err == WSAEINTR;
#else
    return err == EAGAIN || err == EWOULDBLOCK || err == EINTR;
#endif
}

std::string cardToString(const Card& card) {
    char buf[3];
    buf[0] = card.rankChar();
    switch (card.suit()) {
        case Suit::Hearts:   buf[1] = 'H'; break;
        case Suit::Diamonds: buf[1] = 'D'; break;
        case Suit::Clubs:    buf[1] = 'C'; break;
        case Suit::Spades:   buf[1] = 'S'; break;
    }
    buf[2] = '\0';
    return std::string(buf);
}

Card cardFromString(const std::string& str) {
    if (str.size() < 2) return Card(Suit::Hearts, Rank::Two);
    Rank rank;
    switch (str[0]) {
        case '2': rank = Rank::Two; break;
        case '3': rank = Rank::Three; break;
        case '4': rank = Rank::Four; break;
        case '5': rank = Rank::Five; break;
        case '6': rank = Rank::Six; break;
        case '7': rank = Rank::Seven; break;
        case '8': rank = Rank::Eight; break;
        case '9': rank = Rank::Nine; break;
        case 'T': rank = Rank::Ten; break;
        case 'J': rank = Rank::Jack; break;
        case 'Q': rank = Rank::Queen; break;
        case 'K': rank = Rank::King; break;
        case 'A': rank = Rank::Ace; break;
        default:  rank = Rank::Two; break;
    }
    Suit suit;
    switch (str[1]) {
        case 'H': suit = Suit::Hearts; break;
        case 'D': suit = Suit::Diamonds; break;
        case 'C': suit = Suit::Clubs; break;
        case 'S': suit = Suit::Spades; break;
        default:  suit = Suit::Hearts; break;
    }
    return Card(suit, rank);
}

static constexpr uint32_t MAX_MESSAGE_LENGTH = 1 * 1024 * 1024;  // 1 MB

// ============================================================================
// Message Type Conversions
// ============================================================================

std::string toString(MessageType type) {
    switch (type) {
        case MessageType::JoinRoom: return "JoinRoom";
        case MessageType::PlaceBet: return "PlaceBet";
        case MessageType::PlayerAction: return "PlayerAction";
        case MessageType::TakeInsurance: return "TakeInsurance";
        case MessageType::ReadyToStart: return "ReadyToStart";
        case MessageType::ChatMessage: return "ChatMessage";
        case MessageType::Reconnect: return "Reconnect";
        case MessageType::Disconnect: return "Disconnect";
        case MessageType::LobbyUpdate: return "LobbyUpdate";
        case MessageType::SeatAssignment: return "SeatAssignment";
        case MessageType::StateSync: return "StateSync";
        case MessageType::PhaseChange: return "PhaseChange";
        case MessageType::CardDealt: return "CardDealt";
        case MessageType::LegalActions: return "LegalActions";
        case MessageType::HandResult: return "HandResult";
        case MessageType::RoundResults: return "RoundResults";
        case MessageType::PlayerJoined: return "PlayerJoined";
        case MessageType::PlayerLeft: return "PlayerLeft";
        case MessageType::PlayerDisconnected: return "PlayerDisconnected";
        case MessageType::GameStarted: return "GameStarted";
        case MessageType::Error: return "Error";
        case MessageType::Ping: return "Ping";
        case MessageType::Pong: return "Pong";
    }
    return "Unknown";
}

MessageType messageTypeFromString(const std::string& str) {
    if (str == "JoinRoom") return MessageType::JoinRoom;
    if (str == "PlaceBet") return MessageType::PlaceBet;
    if (str == "PlayerAction") return MessageType::PlayerAction;
    if (str == "TakeInsurance") return MessageType::TakeInsurance;
    if (str == "ReadyToStart") return MessageType::ReadyToStart;
    if (str == "ChatMessage") return MessageType::ChatMessage;
    if (str == "Reconnect") return MessageType::Reconnect;
    if (str == "Disconnect") return MessageType::Disconnect;
    if (str == "LobbyUpdate") return MessageType::LobbyUpdate;
    if (str == "SeatAssignment") return MessageType::SeatAssignment;
    if (str == "StateSync") return MessageType::StateSync;
    if (str == "PhaseChange") return MessageType::PhaseChange;
    if (str == "CardDealt") return MessageType::CardDealt;
    if (str == "LegalActions") return MessageType::LegalActions;
    if (str == "HandResult") return MessageType::HandResult;
    if (str == "RoundResults") return MessageType::RoundResults;
    if (str == "PlayerJoined") return MessageType::PlayerJoined;
    if (str == "PlayerLeft") return MessageType::PlayerLeft;
    if (str == "PlayerDisconnected") return MessageType::PlayerDisconnected;
    if (str == "GameStarted") return MessageType::GameStarted;
    if (str == "Error") return MessageType::Error;
    if (str == "Ping") return MessageType::Ping;
    if (str == "Pong") return MessageType::Pong;
    return MessageType::Ping;
}

// ============================================================================
// Network Message Serialization
// ============================================================================

std::string NetworkMessage::serialize() const {
    nlohmann::json j;
    j["type"] = toString(type);
    j["payload"] = payload;
    std::string body = j.dump();
    uint32_t len = static_cast<uint32_t>(body.size());
    std::string result;
    result.reserve(4 + body.size());
    result.push_back(static_cast<char>((len >> 24) & 0xFF));
    result.push_back(static_cast<char>((len >> 16) & 0xFF));
    result.push_back(static_cast<char>((len >> 8) & 0xFF));
    result.push_back(static_cast<char>(len & 0xFF));
    result += body;
    return result;
}

NetworkMessage NetworkMessage::deserialize(const std::string& data) {
    if (data.size() < 4) {
        return {MessageType::Error, {{"message", "Incomplete header"}}};
    }
    uint32_t len = (static_cast<uint32_t>(static_cast<unsigned char>(data[0])) << 24) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(data[1])) << 16) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(data[2])) << 8) |
                   static_cast<uint32_t>(static_cast<unsigned char>(data[3]));
    if (data.size() < 4 + len) {
        return {MessageType::Error, {{"message", "Incomplete body"}}};
    }
    std::string body = data.substr(4, len);
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded()) {
        return {MessageType::Error, {{"message", "Invalid JSON"}}};
    }
    NetworkMessage msg;
    msg.type = messageTypeFromString(j.value("type", "Ping"));
    msg.payload = j.value("payload", nlohmann::json::object());
    return msg;
}

// ============================================================================
// Host Server
// ============================================================================

HostServer::HostServer(int port) : m_port(port) {}

HostServer::~HostServer() {
    stop();
}

bool HostServer::start() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif

    m_listenSocket = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (m_listenSocket < 0) {
        std::cerr << "socket() failed" << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        std::cerr << "setsockopt() failed" << std::endl;
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = -1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(m_port));

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed" << std::endl;
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = -1;
        return false;
    }

    if (listen(m_listenSocket, 8) < 0) {
        std::cerr << "listen() failed" << std::endl;
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = -1;
        return false;
    }

    if (!setSocketNonBlocking(m_listenSocket)) {
        std::cerr << "Failed to set non-blocking" << std::endl;
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = -1;
        return false;
    }

    // Get assigned port if auto-assigned
    if (m_port == 0) {
        sockaddr_in sin{};
        socklen_t sinLen = sizeof(sin);
        if (getsockname(m_listenSocket, reinterpret_cast<sockaddr*>(&sin), &sinLen) == 0) {
            m_port = ntohs(sin.sin_port);
        }
    }

    m_roomCode = generateRandomRoomCode();
    m_running = true;
    m_nextClientId = 1;
    return true;
}

void HostServer::stop() {
    m_running = false;
    for (auto& [id, client] : m_clients) {
        (void)id;
        if (client.socket >= 0) {
            CLOSE_SOCKET(client.socket);
        }
    }
    m_clients.clear();
    if (m_listenSocket >= 0) {
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = -1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

int HostServer::getClientCount() const {
    int count = 0;
    for (const auto& [id, client] : m_clients) {
        (void)id;
        if (client.connected) ++count;
    }
    return count;
}

static bool sendAll(int socket, const std::string& data, std::string& pendingBuffer) {
    // First try to flush any pending data
    size_t flushOffset = 0;
    if (!pendingBuffer.empty()) {
        while (flushOffset < pendingBuffer.size()) {
            ssize_t sent = ::send(socket, pendingBuffer.c_str() + flushOffset,
                                  pendingBuffer.size() - flushOffset, 0);
            if (sent < 0) {
                int err = getSocketError();
                if (isWouldBlock(err)) {
                    // Still blocked — keep entire pending buffer + new data
                    pendingBuffer = pendingBuffer.substr(flushOffset) + data;
                    return true;
                }
                return false;  // Fatal error
            }
            flushOffset += static_cast<size_t>(sent);
        }
        pendingBuffer.clear();
    }

    // Now send the new data
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        ssize_t sent = ::send(socket, data.c_str() + totalSent, data.size() - totalSent, 0);
        if (sent < 0) {
            int err = getSocketError();
            if (isWouldBlock(err)) {
                pendingBuffer = data.substr(totalSent);
                return true;  // Partial send queued for retry
            }
            return false;  // Fatal error
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

void HostServer::broadcast(const NetworkMessage& msg) {
    std::string data = msg.serialize();
    for (auto& [id, client] : m_clients) {
        (void)id;
        if (client.connected && client.socket >= 0) {
            if (!sendAll(client.socket, data, client.sendBuffer)) {
                client.connected = false;
            }
        }
    }
}

void HostServer::sendTo(int clientId, const NetworkMessage& msg) {
    auto it = m_clients.find(clientId);
    if (it == m_clients.end() || !it->second.connected || it->second.socket < 0) {
        return;
    }
    std::string data = msg.serialize();
    if (!sendAll(it->second.socket, data, it->second.sendBuffer)) {
        it->second.connected = false;
    }
}

void HostServer::sendToSeat(int seatIndex, const NetworkMessage& msg) {
    for (auto& [id, client] : m_clients) {
        (void)id;
        if (client.connected && client.assignedSeat == seatIndex) {
            sendTo(client.id, msg);
        }
    }
}

void HostServer::update(float deltaTime) {
    if (!m_running) return;

    // Flush pending outbound data first
    for (auto& [id, client] : m_clients) {
        (void)id;
        if (client.connected && client.socket >= 0 && !client.sendBuffer.empty()) {
            NetworkMessage dummy;
            // Reuse sendAll with empty new data to flush pending buffer
            if (!sendAll(client.socket, "", client.sendBuffer)) {
                client.connected = false;
            }
        }
    }

    acceptNewConnection();
    readClientMessages();
    removeDisconnectedClients();

    // Update disconnect timers
    for (auto& [id, client] : m_clients) {
        (void)id;
        if (!client.connected && client.assignedSeat >= 0) {
            client.disconnectTimer += deltaTime;
        }
    }
}

bool HostServer::allClientsReady() const {
    for (const auto& [id, client] : m_clients) {
        (void)id;
        if (client.connected && client.assignedSeat >= 0 && !client.ready) {
            return false;
        }
    }
    return !m_clients.empty();
}

void HostServer::kickClient(int clientId) {
    auto it = m_clients.find(clientId);
    if (it != m_clients.end()) {
        if (it->second.socket >= 0) {
            CLOSE_SOCKET(it->second.socket);
        }
        m_clients.erase(it);
    }
}

void HostServer::acceptNewConnection() {
    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientSocket = static_cast<int>(accept(m_listenSocket,
        reinterpret_cast<sockaddr*>(&clientAddr), &addrLen));

    if (clientSocket < 0) {
        int err = getSocketError();
        if (!isWouldBlock(err)) {
            std::cerr << "accept() failed: " << err << std::endl;
        }
        return;
    }

    if (!setSocketNonBlocking(clientSocket)) {
        CLOSE_SOCKET(clientSocket);
        return;
    }

    ClientInfo info;
    info.id = m_nextClientId++;
    info.socket = clientSocket;
    info.connected = true;
    m_clients[info.id] = info;
}

void HostServer::readClientMessages() {
    char buffer[4096];
    static constexpr size_t MAX_BUFFER_SIZE = 2 * MAX_MESSAGE_LENGTH;
    for (auto& [id, client] : m_clients) {
        (void)id;
        if (!client.connected || client.socket < 0) continue;

        while (true) {
            ssize_t received = recv(client.socket, buffer, sizeof(buffer), 0);
            if (received > 0) {
                client.receiveBuffer.append(buffer, static_cast<size_t>(received));
                if (client.receiveBuffer.size() > MAX_BUFFER_SIZE) {
                    // Excessive buffer growth — malicious or buggy client
                    client.connected = false;
                    client.receiveBuffer.clear();
                    break;
                }
            } else if (received == 0) {
                client.connected = false;
                break;
            } else {
                int err = getSocketError();
                if (isWouldBlock(err)) {
                    break;
                } else {
                    client.connected = false;
                    break;
                }
            }
        }

        processClientBuffer(client);
    }
}

void HostServer::processClientBuffer(ClientInfo& client) {
    while (client.receiveBuffer.size() >= 4) {
        uint32_t len = (static_cast<uint32_t>(static_cast<unsigned char>(client.receiveBuffer[0])) << 24) |
                       (static_cast<uint32_t>(static_cast<unsigned char>(client.receiveBuffer[1])) << 16) |
                       (static_cast<uint32_t>(static_cast<unsigned char>(client.receiveBuffer[2])) << 8) |
                       static_cast<uint32_t>(static_cast<unsigned char>(client.receiveBuffer[3]));
        if (client.receiveBuffer.size() < 4 + len) break;

        if (len > MAX_MESSAGE_LENGTH) {
            // Malicious length — disconnect client
            client.connected = false;
            client.receiveBuffer.clear();
            break;
        }
        std::string msgData = client.receiveBuffer.substr(4, len);
        client.receiveBuffer.erase(0, 4 + len);

        auto msg = NetworkMessage::deserialize(msgData);
        if (msg.type != MessageType::Error) {
            msg.senderId = client.id;
            m_receivedMessages.push_back(std::move(msg));
        }
    }
}

void HostServer::removeDisconnectedClients() {
    for (auto it = m_clients.begin(); it != m_clients.end();) {
        if (!it->second.connected) {
            if (it->second.socket >= 0) {
                CLOSE_SOCKET(it->second.socket);
            }
            it = m_clients.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<NetworkMessage> HostServer::getMessages() {
    std::vector<NetworkMessage> result = std::move(m_receivedMessages);
    m_receivedMessages.clear();
    return result;
}

// ============================================================================
// Game Client
// ============================================================================

GameClient::GameClient() = default;

GameClient::~GameClient() {
    disconnect();
}

bool GameClient::connect(const std::string& host, int port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif

    m_socket = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (m_socket < 0) return false;

    // Set non-blocking BEFORE connect for proper async behavior
    if (!setSocketNonBlocking(m_socket)) {
        CLOSE_SOCKET(m_socket);
        m_socket = -1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        CLOSE_SOCKET(m_socket);
        m_socket = -1;
        return false;
    }

    int result = ::connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result < 0) {
        int err = getSocketError();
#ifdef _WIN32
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
#else
        if (err != EINPROGRESS) {
#endif
            CLOSE_SOCKET(m_socket);
            m_socket = -1;
            return false;
        }
    }

    m_connected = true;
    return true;
}

void GameClient::disconnect() {
    m_connected = false;
    if (m_socket >= 0) {
        CLOSE_SOCKET(m_socket);
        m_socket = -1;
    }
    m_receiveBuffer.clear();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool GameClient::send(const NetworkMessage& msg) {
    if (!m_connected || m_socket < 0) return false;
    std::string data = msg.serialize();
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        ssize_t sent = ::send(m_socket, data.c_str() + totalSent, data.size() - totalSent, 0);
        if (sent < 0) {
            int err = getSocketError();
            if (isWouldBlock(err)) {
                return true;  // Partial send, OS buffer full; will retry next tick
            }
            m_connected = false;
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

std::vector<NetworkMessage> GameClient::receive() {
    std::vector<NetworkMessage> messages;
    if (!m_connected || m_socket < 0) return messages;

    char buffer[4096];
    while (true) {
        ssize_t received = recv(m_socket, buffer, sizeof(buffer), 0);
        if (received > 0) {
            m_receiveBuffer.append(buffer, static_cast<size_t>(received));
        } else if (received == 0) {
            m_connected = false;
            break;
        } else {
            int err = getSocketError();
            if (isWouldBlock(err)) {
                break;
            } else {
                m_connected = false;
                break;
            }
        }
    }

    processBuffer(messages);
    return messages;
}

void GameClient::processBuffer(std::vector<NetworkMessage>& outMessages) {
    while (m_receiveBuffer.size() >= 4) {
        uint32_t len = (static_cast<uint32_t>(static_cast<unsigned char>(m_receiveBuffer[0])) << 24) |
                       (static_cast<uint32_t>(static_cast<unsigned char>(m_receiveBuffer[1])) << 16) |
                       (static_cast<uint32_t>(static_cast<unsigned char>(m_receiveBuffer[2])) << 8) |
                       static_cast<uint32_t>(static_cast<unsigned char>(m_receiveBuffer[3]));
        if (m_receiveBuffer.size() < 4 + len) break;

        if (len > MAX_MESSAGE_LENGTH) {
            // Malicious length — clear buffer and disconnect
            m_connected = false;
            m_receiveBuffer.clear();
            break;
        }
        std::string msgData = m_receiveBuffer.substr(4, len);
        m_receiveBuffer.erase(0, 4 + len);

        auto msg = NetworkMessage::deserialize(msgData);
        if (msg.type != MessageType::Error) {
            outMessages.push_back(msg);
        }
    }
}

bool GameClient::reconnect(const std::string& host, int port, int previousClientId) {
    if (!connect(host, port)) return false;
    NetworkMessage msg;
    msg.type = MessageType::Reconnect;
    msg.payload["clientId"] = previousClientId;
    return send(msg);
}

// ============================================================================
// Network Game Session (Authoritative Host)
// ============================================================================

NetworkGameSession::NetworkGameSession() = default;

NetworkGameSession::~NetworkGameSession() {
    stopHosting();
}

bool NetworkGameSession::startHosting(const RuleSet& rules) {
    if (m_hosting) return false;

    m_server = std::make_unique<HostServer>(0);
    if (!m_server->start()) {
        m_server.reset();
        return false;
    }

    m_rules = rules;
    m_round = std::make_unique<RoundState>(m_rules);
    m_round->startRound();

    // Host occupies seat 0
    m_round->seats()[0].name = "Host";

    m_hosting = true;
    m_gameStarted = false;
    m_bettingPhaseComplete = false;
    m_clientToSeat.clear();
    m_seatToClient.clear();
    m_pendingActions.clear();
    m_aiControllers.clear();
    m_autoAdvanceTimer = 0.0f;
    m_aiTurnTimer = 0.0f;

    return true;
}

void NetworkGameSession::stopHosting() {
    m_hosting = false;
    m_gameStarted = false;
    if (m_server) {
        m_server->stop();
        m_server.reset();
    }
    m_round.reset();
    m_aiControllers.clear();
}

int NetworkGameSession::getSeatForClient(int clientId) const {
    auto it = m_clientToSeat.find(clientId);
    if (it != m_clientToSeat.end()) return it->second;
    return -1;
}

int NetworkGameSession::getClientForSeat(int seatIndex) const {
    auto it = m_seatToClient.find(seatIndex);
    if (it != m_seatToClient.end()) return it->second;
    return -1;
}

void NetworkGameSession::updateHost(float deltaTime) {
    if (!m_hosting || !m_server) return;

    m_server->update(deltaTime);
    processClientMessages();

    if (!m_gameStarted) {
        broadcastLobbyUpdate();
        return;
    }

    // Game is running — capture phase before processing
    RoundPhase phaseBefore = m_round->phase();
    checkAutoAdvance(deltaTime);
    processAIAndDealer(deltaTime);

    // Handle phase transitions
    if (m_round->phase() != phaseBefore) {
        broadcastPhaseChange();
        broadcastStateSync();
    }

    // Update disconnect timers and replace with AI if needed
    for (auto& [id, client] : m_server->clients()) {
        (void)id;
        if (!client.connected && client.assignedSeat >= 0 && client.disconnectTimer > 60.0f) {
            // Remove client from seat mapping
            m_seatToClient.erase(client.assignedSeat);
            m_clientToSeat.erase(client.id);
            client.assignedSeat = -1;

            // Setup AI for the abandoned seat
            setupAIForEmptySeats();
        }
    }
}

void NetworkGameSession::processClientMessages() {
    // Read all pending messages from the host server's queue
    auto messages = m_server->getMessages();
    for (auto& msg : messages) {
        int clientId = msg.senderId;
        if (clientId < 0) continue;

        switch (msg.type) {
            case MessageType::JoinRoom:
                handleJoinRoom(clientId, msg.payload);
                break;
            case MessageType::PlaceBet:
                handlePlaceBet(clientId, msg.payload);
                break;
            case MessageType::PlayerAction:
                handlePlayerAction(clientId, msg.payload);
                break;
            case MessageType::TakeInsurance:
                handleTakeInsurance(clientId, msg.payload);
                break;
            case MessageType::ReadyToStart:
                handleReadyToStart(clientId);
                break;
            case MessageType::Reconnect:
                handleReconnect(clientId, msg.payload);
                break;
            case MessageType::Disconnect:
                handleDisconnect(clientId);
                break;
            default:
                break;
        }
    }
}

void NetworkGameSession::handleJoinRoom(int clientId, const nlohmann::json& payload) {
    std::string name = payload.value("playerName", "Player");

    // Find an available seat (skip seat 0, which is the host)
    int assignedSeat = -1;
    for (int i = 1; i < 4; ++i) {
        if (m_seatToClient.find(i) == m_seatToClient.end()) {
            assignedSeat = i;
            break;
        }
    }

    if (assignedSeat < 0) {
        NetworkMessage errorMsg;
        errorMsg.type = MessageType::Error;
        errorMsg.payload["message"] = "Room is full";
        m_server->sendTo(clientId, errorMsg);
        return;
    }

    // Ensure seat exists in RoundState
    while (static_cast<int>(m_round->seats().size()) <= assignedSeat) {
        m_round->addSeat("", m_rules.startingBankroll);
    }
    m_round->seats()[assignedSeat].name = name;
    m_round->seats()[assignedSeat].bankroll = m_rules.startingBankroll;

    auto& client = m_server->clients()[clientId];
    client.name = name;
    client.assignedSeat = assignedSeat;
    client.ready = false;

    m_clientToSeat[clientId] = assignedSeat;
    m_seatToClient[assignedSeat] = clientId;

    // Send seat assignment
    NetworkMessage assignMsg;
    assignMsg.type = MessageType::SeatAssignment;
    assignMsg.payload["seatIndex"] = assignedSeat;
    assignMsg.payload["clientId"] = clientId;
    m_server->sendTo(clientId, assignMsg);

    // Send current lobby state
    broadcastLobbyUpdate();

    // If game already started, send state sync
    if (m_gameStarted) {
        broadcastStateSync(clientId);
    }
}

void NetworkGameSession::handlePlaceBet(int clientId, const nlohmann::json& payload) {
    int seat = getSeatForClient(clientId);
    if (seat < 0) return;

    int amount = payload.value("amount", 0);
    if (amount < m_rules.minBet || amount > m_rules.maxBet) return;

    m_round->placeBet(seat, amount);

    // Broadcast state sync to all clients with full round state
    broadcastStateSync();

    if (m_round->allSeatsHaveBets()) {
        m_bettingPhaseComplete = true;
        m_round->advancePhase();
        broadcastPhaseChange();
    } else {
        broadcastLobbyUpdate();
    }
}

void NetworkGameSession::handlePlayerAction(int clientId, const nlohmann::json& payload) {
    int seat = getSeatForClient(clientId);
    if (seat < 0) return;

    if (m_round->phase() != RoundPhase::PlayerTurns) return;
    if (m_round->currentSeatIndex() != seat) return;

    std::string actionStr = payload.value("action", "");
    int handIdx = payload.value("handIndex", 0);

    PlayerAction action = PlayerAction::Stand;
    if (actionStr == "Hit") action = PlayerAction::Hit;
    else if (actionStr == "Stand") action = PlayerAction::Stand;
    else if (actionStr == "DoubleDown") action = PlayerAction::DoubleDown;
    else if (actionStr == "Split") action = PlayerAction::Split;
    else if (actionStr == "Surrender") action = PlayerAction::Surrender;

    auto legal = m_round->getLegalActions(seat, handIdx);
    bool valid = false;
    switch (action) {
        case PlayerAction::Hit: valid = legal.canHit; break;
        case PlayerAction::Stand: valid = legal.canStand; break;
        case PlayerAction::DoubleDown: valid = legal.canDouble; break;
        case PlayerAction::Split: valid = legal.canSplit; break;
        case PlayerAction::Surrender: valid = legal.canSurrender; break;
        default: break;
    }

    if (!valid) {
        NetworkMessage errorMsg;
        errorMsg.type = MessageType::Error;
        errorMsg.payload["message"] = "Invalid action";
        m_server->sendTo(clientId, errorMsg);
        return;
    }

    switch (action) {
        case PlayerAction::Hit: m_round->hit(seat, handIdx); break;
        case PlayerAction::Stand: m_round->stand(seat, handIdx); break;
        case PlayerAction::DoubleDown: m_round->doubleDown(seat, handIdx); break;
        case PlayerAction::Split: m_round->split(seat, handIdx); break;
        case PlayerAction::Surrender: m_round->surrender(seat, handIdx); break;
        default: break;
    }

    m_round->nextHand();
    m_round->advancePhase();

    broadcastPhaseChange();
    broadcastStateSync();
}

void NetworkGameSession::handleTakeInsurance(int clientId, const nlohmann::json& payload) {
    int seat = getSeatForClient(clientId);
    if (seat < 0) return;

    bool take = payload.value("take", false);
    if (!take) return;

    if (m_round->seats()[seat].hands.empty()) return;
    int maxInsurance = m_round->seats()[seat].hands[0].bet.mainBet / 2;
    m_round->takeInsurance(seat, maxInsurance);

    broadcastStateSync();
}

void NetworkGameSession::handleReadyToStart(int clientId) {
    auto it = m_server->clients().find(clientId);
    if (it != m_server->clients().end()) {
        it->second.ready = true;
    }
    broadcastLobbyUpdate();
}

void NetworkGameSession::handleReconnect(int clientId, const nlohmann::json& payload) {
    int prevId = payload.value("clientId", -1);
    if (prevId < 0) return;

    auto prevSeatIt = m_clientToSeat.find(prevId);
    if (prevSeatIt == m_clientToSeat.end()) {
        // Treat as new join
        handleJoinRoom(clientId, payload);
        return;
    }

    int seat = prevSeatIt->second;
    m_seatToClient[seat] = clientId;
    m_clientToSeat[clientId] = seat;
    m_clientToSeat.erase(prevId);

    auto& client = m_server->clients()[clientId];
    client.assignedSeat = seat;
    client.ready = true;

    // Send state sync
    NetworkMessage assignMsg;
    assignMsg.type = MessageType::SeatAssignment;
    assignMsg.payload["seatIndex"] = seat;
    assignMsg.payload["clientId"] = clientId;
    m_server->sendTo(clientId, assignMsg);

    broadcastStateSync(clientId);
    broadcastLobbyUpdate();
}

void NetworkGameSession::handleDisconnect(int clientId) {
    auto seatIt = m_clientToSeat.find(clientId);
    if (seatIt != m_clientToSeat.end()) {
        int seat = seatIt->second;
        m_seatToClient.erase(seat);
        m_clientToSeat.erase(clientId);

        // Mark client as disconnected but keep seat reserved for 60 seconds
        auto& client = m_server->clients()[clientId];
        client.connected = false;
        client.disconnectTimer = 0.0f;

        NetworkMessage msg;
        msg.type = MessageType::PlayerDisconnected;
        msg.payload["seatIndex"] = seat;
        msg.payload["timeoutSeconds"] = 60;
        m_server->broadcast(msg);
    }
}

void NetworkGameSession::broadcastLobbyUpdate() {
    NetworkMessage msg;
    msg.type = MessageType::LobbyUpdate;
    nlohmann::json players = nlohmann::json::array();

    // Host
    nlohmann::json hostPlayer;
    hostPlayer["name"] = m_round->seats()[0].name;
    hostPlayer["seatIndex"] = 0;
    hostPlayer["ready"] = true;
    players.push_back(hostPlayer);

    for (const auto& [id, client] : m_server->clients()) {
        (void)id;
        if (client.assignedSeat >= 0) {
            nlohmann::json player;
            player["name"] = client.name;
            player["seatIndex"] = client.assignedSeat;
            player["ready"] = client.ready;
            players.push_back(player);
        }
    }

    msg.payload["players"] = players;
    msg.payload["roomCode"] = m_server->getRoomCode();
    msg.payload["canStart"] = m_server->allClientsReady();
    m_server->broadcast(msg);
}

void NetworkGameSession::broadcastStateSync(int targetClientId) {
    NetworkMessage msg;
    msg.type = MessageType::StateSync;

    nlohmann::json payload;
    payload["phase"] = toString(m_round->phase());
    payload["currentSeat"] = m_round->currentSeatIndex();
    payload["currentHand"] = m_round->currentHandIndex();

    // Dealer
    nlohmann::json dealer;
    dealer["holeVisible"] = m_round->dealer().holeCardVisible;
    nlohmann::json dealerCards = nlohmann::json::array();
    for (const auto& card : m_round->dealer().hand.cards()) {
        dealerCards.push_back(cardToString(card));
    }
    if (!m_round->dealer().holeCardVisible && m_round->dealer().hand.cardCount() == 2) {
        // Remove hole card from sync if not visible
        dealerCards = nlohmann::json::array();
        if (!m_round->dealer().hand.cards().empty()) {
            dealerCards.push_back(cardToString(m_round->dealer().hand.cards()[0]));
        }
    }
    dealer["cards"] = dealerCards;
    payload["dealer"] = dealer;

    // Seats
    nlohmann::json seats = nlohmann::json::array();
    for (const auto& seat : m_round->seats()) {
        nlohmann::json seatJson;
        seatJson["name"] = seat.name;
        seatJson["bankroll"] = seat.bankroll;
        nlohmann::json hands = nlohmann::json::array();
        for (const auto& hand : seat.hands) {
            nlohmann::json handJson;
            nlohmann::json cards = nlohmann::json::array();
            for (const auto& card : hand.hand.cards()) {
                cards.push_back(cardToString(card));
            }
            handJson["cards"] = cards;
            handJson["mainBet"] = hand.bet.mainBet;
            handJson["insuranceBet"] = hand.bet.insuranceBet;
            handJson["doubled"] = hand.doubled;
            handJson["surrendered"] = hand.surrendered;
            handJson["isSplit"] = hand.isSplit;
            handJson["finished"] = hand.finished;
            handJson["outcome"] = toString(hand.outcome);
            hands.push_back(handJson);
        }
        seatJson["hands"] = hands;
        seats.push_back(seatJson);
    }
    payload["seats"] = seats;

    msg.payload = payload;
    if (targetClientId >= 0) {
        m_server->sendTo(targetClientId, msg);
    } else {
        m_server->broadcast(msg);
    }
}

void NetworkGameSession::broadcastPhaseChange() {
    NetworkMessage msg;
    msg.type = MessageType::PhaseChange;
    msg.payload["phase"] = toString(m_round->phase());
    msg.payload["currentSeat"] = m_round->currentSeatIndex();
    msg.payload["currentHand"] = m_round->currentHandIndex();
    m_server->broadcast(msg);
}

void NetworkGameSession::broadcastCardDealt(int seatIndex, int handIndex, const Card& card) {
    NetworkMessage msg;
    msg.type = MessageType::CardDealt;
    msg.payload["seatIndex"] = seatIndex;
    msg.payload["handIndex"] = handIndex;
    msg.payload["card"] = cardToString(card);
    m_server->broadcast(msg);
}

void NetworkGameSession::broadcastHandResult(int seatIndex, int handIndex) {
    if (seatIndex < 0 || seatIndex >= static_cast<int>(m_round->seats().size())) return;
    if (handIndex < 0 || handIndex >= static_cast<int>(m_round->seats()[seatIndex].hands.size())) return;

    const auto& hand = m_round->seats()[seatIndex].hands[handIndex];
    NetworkMessage msg;
    msg.type = MessageType::HandResult;
    msg.payload["seatIndex"] = seatIndex;
    msg.payload["handIndex"] = handIndex;
    msg.payload["outcome"] = toString(hand.outcome);
    msg.payload["payout"] = m_round->calculatePayout(seatIndex, handIndex);
    m_server->broadcast(msg);
}

void NetworkGameSession::broadcastRoundResults() {
    NetworkMessage msg;
    msg.type = MessageType::RoundResults;
    nlohmann::json results = nlohmann::json::array();
    for (size_t s = 0; s < m_round->seats().size(); ++s) {
        for (size_t h = 0; h < m_round->seats()[s].hands.size(); ++h) {
            nlohmann::json result;
            result["seatIndex"] = s;
            result["handIndex"] = h;
            result["outcome"] = toString(m_round->seats()[s].hands[h].outcome);
            result["payout"] = m_round->calculatePayout(static_cast<int>(s), static_cast<int>(h));
            results.push_back(result);
        }
    }
    msg.payload["results"] = results;
    m_server->broadcast(msg);
}

void NetworkGameSession::startGame() {
    if (m_gameStarted) return;

    // Add AI for any empty seats (1-3)
    setupAIForEmptySeats();

    m_gameStarted = true;
    m_round->startRound();

    NetworkMessage msg;
    msg.type = MessageType::GameStarted;
    m_server->broadcast(msg);

    broadcastStateSync();
}

void NetworkGameSession::checkAutoAdvance(float deltaTime) {
    RoundPhase phase = m_round->phase();
    if (phase == RoundPhase::DealerTurn ||
        phase == RoundPhase::EvaluateHands ||
        phase == RoundPhase::Payout) {
        m_autoAdvanceTimer += deltaTime;
        if (m_autoAdvanceTimer >= 1.5f) {
            m_autoAdvanceTimer = 0.0f;
            m_round->advancePhase();
            broadcastPhaseChange();
            if (m_round->phase() == RoundPhase::RoundComplete) {
                broadcastRoundResults();
            } else if (m_round->phase() == RoundPhase::Payout) {
                broadcastStateSync();
            }
        }
    }
}

void NetworkGameSession::processAIAndDealer(float deltaTime) {
    if (!m_gameStarted) return;

    // AI insurance resolution during InsuranceOffer
    if (m_round->phase() == RoundPhase::InsuranceOffer) {
        for (size_t i = 1; i < m_round->seats().size(); ++i) {
            // Skip human-controlled seats
            if (m_seatToClient.find(static_cast<int>(i)) != m_seatToClient.end()) continue;

            if (i - 1 < m_aiControllers.size() && m_aiControllers[i - 1]) {
                bool takeIns = m_aiControllers[i - 1]->chooseInsurance(*m_round, static_cast<int>(i));
                if (takeIns && !m_round->seats()[i].hands.empty()) {
                    int maxInsurance = m_round->seats()[i].hands[0].bet.mainBet / 2;
                    m_round->takeInsurance(static_cast<int>(i), maxInsurance);
                }
            }
        }
        m_round->advancePhase();
        broadcastPhaseChange();
        broadcastStateSync();
        return;
    }

    // AI turns during PlayerTurns
    if (m_round->phase() == RoundPhase::PlayerTurns) {
        int currentSeat = m_round->currentSeatIndex();
        if (currentSeat > 0 && currentSeat < static_cast<int>(m_round->seats().size())) {
            // Check if this seat is AI-controlled
            if (m_seatToClient.find(currentSeat) == m_seatToClient.end()) {
                m_aiTurnTimer += deltaTime;
                if (m_aiTurnTimer >= m_aiTurnDelay) {
                    m_aiTurnTimer = 0.0f;
                    int handIdx = m_round->currentHandIndex();
                    if (handIdx >= 0) {
                        executeAIAction(currentSeat, handIdx);
                        broadcastPhaseChange();
                        broadcastStateSync();
                    }
                }
            }
        }
    }
}

void NetworkGameSession::setupAIForEmptySeats() {
    // Ensure we have AI controllers for seats 1-3 that don't have clients
    for (int i = 1; i < 4; ++i) {
        if (m_seatToClient.find(i) != m_seatToClient.end()) continue;

        while (static_cast<int>(m_aiControllers.size()) < i) {
            std::unique_ptr<IAIStrategy> strategy = std::make_unique<BasicStrategy>();
            m_aiControllers.push_back(std::make_unique<AIController>(std::move(strategy)));
        }

        while (static_cast<int>(m_round->seats().size()) <= i) {
            m_round->addSeat("AI " + std::to_string(i), m_rules.startingBankroll);
        }

        if (m_round->seats()[i].name.empty()) {
            m_round->seats()[i].name = "AI " + std::to_string(i);
        }
    }
}

void NetworkGameSession::executeAIAction(int seatIndex, int handIndex) {
    if (seatIndex <= 0 || seatIndex >= static_cast<int>(m_round->seats().size())) return;
    if (handIndex < 0 || handIndex >= static_cast<int>(m_round->seats()[seatIndex].hands.size())) return;

    int aiIdx = seatIndex - 1;
    if (aiIdx < 0 || aiIdx >= static_cast<int>(m_aiControllers.size()) || !m_aiControllers[aiIdx]) {
        m_round->stand(seatIndex, handIndex);
        m_round->nextHand();
        m_round->advancePhase();
        return;
    }

    PlayerAction action = m_aiControllers[aiIdx]->chooseAction(*m_round, seatIndex, handIndex);
    switch (action) {
        case PlayerAction::Hit: m_round->hit(seatIndex, handIndex); break;
        case PlayerAction::Stand: m_round->stand(seatIndex, handIndex); break;
        case PlayerAction::DoubleDown: m_round->doubleDown(seatIndex, handIndex); break;
        case PlayerAction::Split: m_round->split(seatIndex, handIndex); break;
        case PlayerAction::Surrender: m_round->surrender(seatIndex, handIndex); break;
        default: m_round->stand(seatIndex, handIndex); break;
    }

    m_round->nextHand();
    m_round->advancePhase();
}

// ============================================================================
// Network Client Session
// ============================================================================

NetworkClientSession::NetworkClientSession() = default;

NetworkClientSession::~NetworkClientSession() {
    disconnect();
}

bool NetworkClientSession::connect(const std::string& host, int port, const std::string& playerName) {
    m_playerName = playerName;
    m_client = std::make_unique<GameClient>();
    if (!m_client->connect(host, port)) {
        m_client.reset();
        return false;
    }

    // Send join room message
    NetworkMessage msg;
    msg.type = MessageType::JoinRoom;
    msg.payload["playerName"] = playerName;
    m_client->send(msg);

    m_inLobby = true;
    m_gameStarted = false;
    m_mySeat = -1;
    m_round = std::make_unique<RoundState>(RuleSet{});
    return true;
}

void NetworkClientSession::disconnect() {
    if (m_client && m_client->isConnected()) {
        NetworkMessage msg;
        msg.type = MessageType::Disconnect;
        m_client->send(msg);
    }
    m_client.reset();
    m_inLobby = false;
    m_gameStarted = false;
    m_mySeat = -1;
}

bool NetworkClientSession::isConnected() const {
    return m_client && m_client->isConnected();
}

void NetworkClientSession::update(float /*deltaTime*/) {
    if (!m_client || !m_client->isConnected()) {
        if (m_connected && onDisconnected) {
            onDisconnected();
        }
        m_connected = false;
        return;
    }
    m_connected = true;
    processMessages();
}

bool NetworkClientSession::sendBet(int amount) {
    if (!m_client || !m_client->isConnected()) return false;
    NetworkMessage msg;
    msg.type = MessageType::PlaceBet;
    msg.payload["amount"] = amount;
    return m_client->send(msg);
}

bool NetworkClientSession::sendAction(PlayerAction action) {
    if (!m_client || !m_client->isConnected()) return false;
    NetworkMessage msg;
    msg.type = MessageType::PlayerAction;
    msg.payload["action"] = toString(action);
    msg.payload["handIndex"] = m_round ? m_round->currentHandIndex() : 0;
    return m_client->send(msg);
}

bool NetworkClientSession::sendInsurance(bool take) {
    if (!m_client || !m_client->isConnected()) return false;
    NetworkMessage msg;
    msg.type = MessageType::TakeInsurance;
    msg.payload["take"] = take;
    return m_client->send(msg);
}

bool NetworkClientSession::sendReady() {
    if (!m_client || !m_client->isConnected()) return false;
    NetworkMessage msg;
    msg.type = MessageType::ReadyToStart;
    return m_client->send(msg);
}

bool NetworkClientSession::sendChat(const std::string& text) {
    if (!m_client || !m_client->isConnected()) return false;
    NetworkMessage msg;
    msg.type = MessageType::ChatMessage;
    msg.payload["text"] = text;
    return m_client->send(msg);
}

bool NetworkClientSession::isMyTurn() const {
    if (!m_round || m_mySeat < 0) return false;
    return m_round->phase() == RoundPhase::PlayerTurns &&
           m_round->currentSeatIndex() == m_mySeat;
}

void NetworkClientSession::processMessages() {
    if (!m_client) return;
    auto messages = m_client->receive();
    for (const auto& msg : messages) {
        switch (msg.type) {
            case MessageType::LobbyUpdate:
                handleLobbyUpdate(msg.payload);
                break;
            case MessageType::SeatAssignment:
                handleSeatAssignment(msg.payload);
                break;
            case MessageType::StateSync:
                handleStateSync(msg.payload);
                break;
            case MessageType::PhaseChange:
                handlePhaseChange(msg.payload);
                break;
            case MessageType::CardDealt:
                handleCardDealt(msg.payload);
                break;
            case MessageType::HandResult:
                handleHandResult(msg.payload);
                break;
            case MessageType::RoundResults:
                handleRoundResults(msg.payload);
                break;
            case MessageType::PlayerJoined:
                handlePlayerJoined(msg.payload);
                break;
            case MessageType::PlayerLeft:
                handlePlayerLeft(msg.payload);
                break;
            case MessageType::GameStarted:
                handleGameStarted(msg.payload);
                break;
            case MessageType::Error:
                handleError(msg.payload);
                break;
            case MessageType::Ping:
                {
                    NetworkMessage pong;
                    pong.type = MessageType::Pong;
                    m_client->send(pong);
                }
                break;
            default:
                break;
        }
    }
}

void NetworkClientSession::handleLobbyUpdate(const nlohmann::json& payload) {
    m_lobbyPlayers.clear();
    auto players = payload.value("players", nlohmann::json::array());
    for (const auto& p : players) {
        LobbyPlayer lp;
        lp.name = p.value("name", "");
        lp.seatIndex = p.value("seatIndex", -1);
        lp.ready = p.value("ready", false);
        m_lobbyPlayers.push_back(lp);
    }
    if (onLobbyUpdate) onLobbyUpdate();
}

void NetworkClientSession::handleSeatAssignment(const nlohmann::json& payload) {
    m_mySeat = payload.value("seatIndex", -1);
    int clientId = payload.value("clientId", -1);
    (void)clientId;
    if (m_client) m_client->setAssignedSeat(m_mySeat);
}

void NetworkClientSession::handleStateSync(const nlohmann::json& payload) {
    if (!m_round) return;

    // Update phase
    std::string phaseStr = payload.value("phase", "WaitingForBets");
    m_round->phase() = roundPhaseFromString(phaseStr);
    m_round->currentSeatIndex() = payload.value("currentSeat", -1);
    m_round->currentHandIndex() = payload.value("currentHand", -1);

    // Update dealer
    auto dealerJson = payload.value("dealer", nlohmann::json::object());
    bool holeVisible = dealerJson.value("holeVisible", false);
    m_round->dealer().holeCardVisible = holeVisible;
    m_round->dealer().hand.clear();
    auto dealerCards = dealerJson.value("cards", nlohmann::json::array());
    for (const auto& c : dealerCards) {
        m_round->dealer().hand.addCard(cardFromString(c.get<std::string>()));
    }

    // Update seats
    auto seatsJson = payload.value("seats", nlohmann::json::array());
    for (size_t s = 0; s < seatsJson.size(); ++s) {
        const auto& seatJson = seatsJson[s];
        if (s >= m_round->seats().size()) {
            m_round->addSeat("", 0);
        }
        auto& seat = m_round->seats()[s];
        seat.name = seatJson.value("name", "");
        seat.bankroll = seatJson.value("bankroll", 0);
        seat.hands.clear();
        auto handsJson = seatJson.value("hands", nlohmann::json::array());
        for (const auto& handJson : handsJson) {
            PlayerHandState handState;
            auto cards = handJson.value("cards", nlohmann::json::array());
            for (const auto& c : cards) {
                handState.hand.addCard(cardFromString(c.get<std::string>()));
            }
            handState.bet.mainBet = handJson.value("mainBet", 0);
            handState.bet.insuranceBet = handJson.value("insuranceBet", 0);
            handState.doubled = handJson.value("doubled", false);
            handState.surrendered = handJson.value("surrendered", false);
            handState.isSplit = handJson.value("isSplit", false);
            handState.finished = handJson.value("finished", false);
            handState.outcome = handOutcomeFromString(handJson.value("outcome", "Pending"));
            seat.hands.push_back(handState);
        }
    }

    if (onStateSync) onStateSync();
}

void NetworkClientSession::handlePhaseChange(const nlohmann::json& payload) {
    if (!m_round) return;
    std::string phaseStr = payload.value("phase", "WaitingForBets");
    m_round->phase() = roundPhaseFromString(phaseStr);
    m_round->currentSeatIndex() = payload.value("currentSeat", -1);
    m_round->currentHandIndex() = payload.value("currentHand", -1);
    if (onStateSync) onStateSync();
}

void NetworkClientSession::handleCardDealt(const nlohmann::json& /*payload*/) {
    // Animation trigger - handled by GUI
}

void NetworkClientSession::handleHandResult(const nlohmann::json& payload) {
    int seat = payload.value("seatIndex", -1);
    int hand = payload.value("handIndex", -1);
    if (seat < 0 || seat >= static_cast<int>(m_round->seats().size())) return;
    if (hand < 0 || hand >= static_cast<int>(m_round->seats()[seat].hands.size())) return;
    m_round->seats()[seat].hands[hand].outcome =
        handOutcomeFromString(payload.value("outcome", "Pending"));
}

void NetworkClientSession::handleRoundResults(const nlohmann::json& payload) {
    auto results = payload.value("results", nlohmann::json::array());
    for (const auto& r : results) {
        int seat = r.value("seatIndex", -1);
        int hand = r.value("handIndex", -1);
        if (seat < 0 || seat >= static_cast<int>(m_round->seats().size())) continue;
        if (hand < 0 || hand >= static_cast<int>(m_round->seats()[seat].hands.size())) continue;
        m_round->seats()[seat].hands[hand].outcome =
            handOutcomeFromString(r.value("outcome", "Pending"));
        m_round->seats()[seat].bankroll += r.value("payout", 0);
    }
}

void NetworkClientSession::handlePlayerJoined(const nlohmann::json& payload) {
    std::string name = payload.value("name", "");
    int seat = payload.value("seatIndex", -1);
    if (seat >= 0) {
        // Update local state if needed
    }
}

void NetworkClientSession::handlePlayerLeft(const nlohmann::json& payload) {
    int seat = payload.value("seatIndex", -1);
    if (seat >= 0 && seat < static_cast<int>(m_round->seats().size())) {
        m_round->seats()[seat].name += " (Disconnected)";
    }
}

void NetworkClientSession::handleGameStarted(const nlohmann::json& /*payload*/) {
    m_inLobby = false;
    m_gameStarted = true;
    if (onGameStarted) onGameStarted();
}

void NetworkClientSession::setGameClient(std::unique_ptr<GameClient> client) {
    m_client = std::move(client);
    m_connected = m_client && m_client->isConnected();
}

void NetworkClientSession::handleError(const nlohmann::json& payload) {
    std::string message = payload.value("message", "Unknown error");
    if (onError) onError(message);
}

}  // namespace blackjack
