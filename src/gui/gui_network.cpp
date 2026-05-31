#include <blackjack/gui.h>
#include <blackjack/network.h>

#include "gui_helpers.h"

#include <SDL.h>

namespace blackjack {

// NetworkCreateScreen
// ============================================================================

NetworkCreateScreen::NetworkCreateScreen(Application* app)
    : Screen(AppState::NetworkCreate), m_app(app) {
    setupButtons();
}

NetworkCreateScreen::~NetworkCreateScreen() = default;

void NetworkCreateScreen::setupButtons() {
    m_buttons.clear();
    const int bw = 280;
    const int bh = 50;
    const int bx = (1280 - bw) / 2;
    int by = 500;

    m_buttons.push_back(std::make_unique<Button>(
        bx, by, bw, bh, "Start Game",
        [this]() {
            if (!m_hostServer || m_hostServer->getClientCount() == 0) return;
            m_gameStarted = true;
            m_app->networkConfig().mode = NetworkConfig::Mode::Host;
            m_app->networkConfig().roomCode = m_roomCode;
            m_app->networkConfig().port = m_hostServer->getPort();
            m_app->screenManager().transitionTo(AppState::InRound);
        },
        m_app->font()));
    m_buttons.back()->theme = &m_app->theme();
    m_buttons.back()->enabled = false;

    by += bh + 16;
    m_buttons.push_back(std::make_unique<Button>(
        bx, by, bw, bh, "Back",
        [this]() { m_app->screenManager().transitionTo(AppState::MainMenu); },
        m_app->font()));
    m_buttons.back()->theme = &m_app->theme();
}

void NetworkCreateScreen::onEnter() {
    Screen::onEnter();
    m_gameStarted = false;
    m_roomCode.clear();
    m_connectedPlayers.clear();
    m_pollTimer = 0.0f;

    m_hostServer = std::make_unique<HostServer>(0);
    if (m_hostServer->start()) {
        m_roomCode = m_hostServer->getRoomCode();
    } else {
        m_roomCode = "ERROR";
        m_hostServer.reset();
    }
    setupButtons();
}

void NetworkCreateScreen::onExit() {
    if (!m_gameStarted && m_hostServer) {
        m_hostServer->stop();
        m_hostServer.reset();
    }
}

void NetworkCreateScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    if (handleButtonNavigation(event, m_buttons, m_focusedButtonIndex)) return;
    if (routeButtons(event, m_buttons)) return;
}

void NetworkCreateScreen::update(float deltaTime) {
    if (!m_hostServer || !m_hostServer->isRunning()) return;

    m_hostServer->update(deltaTime);

    // Process join requests and assign seats
    auto messages = m_hostServer->getMessages();
    for (auto& msg : messages) {
        if (msg.type == MessageType::JoinRoom) {
            std::string name = msg.payload.value("playerName", "Player");
            int assignedSeat = -1;
            for (int i = 1; i < 4; ++i) {
                bool taken = false;
                for (const auto& [id, client] : m_hostServer->clients()) {
                    if (client.assignedSeat == i) {
                        taken = true;
                        break;
                    }
                }
                if (!taken) {
                    assignedSeat = i;
                    break;
                }
            }
            if (assignedSeat >= 0) {
                auto& client = m_hostServer->clients()[msg.senderId];
                client.name = name;
                client.assignedSeat = assignedSeat;

                NetworkMessage assignMsg;
                assignMsg.type = MessageType::SeatAssignment;
                assignMsg.payload["seatIndex"] = assignedSeat;
                m_hostServer->sendTo(msg.senderId, assignMsg);
            } else {
                NetworkMessage errorMsg;
                errorMsg.type = MessageType::Error;
                errorMsg.payload["message"] = "Room is full";
                m_hostServer->sendTo(msg.senderId, errorMsg);
            }
        }
    }

    m_pollTimer += deltaTime;
    if (m_pollTimer >= 0.5f) {
        m_pollTimer = 0.0f;
        refreshPlayerList();
    }
}

void NetworkCreateScreen::refreshPlayerList() {
    m_connectedPlayers.clear();
    for (const auto& [id, client] : m_hostServer->clients()) {
        (void)id;
        if (client.connected && !client.name.empty()) {
            m_connectedPlayers.push_back(client.name);
        }
    }
    // Enable Start Game button if at least one client connected
    for (auto& btn : m_buttons) {
        if (btn->label == "Start Game") {
            btn->enabled = !m_connectedPlayers.empty();
        }
    }
}

void NetworkCreateScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "Create Room");

    if (!m_roomCode.empty() && m_roomCode != "ERROR") {
        drawTextCentered(renderer, m_app->font(), "Room Code:", 640, 200, 200, 200, 200);
        drawTextCentered(renderer, m_app->font(), m_roomCode, 640, 250, 255, 215, 0);

        if (m_hostServer) {
            std::string portStr = "Port: " + std::to_string(m_hostServer->getPort());
            drawTextCentered(renderer, m_app->font(), portStr, 640, 290, 180, 180, 180);
        }

        drawTextCentered(renderer, m_app->font(), "Connected Players:", 640, 340, 200, 200, 200);
        int y = 380;
        if (m_connectedPlayers.empty()) {
            drawTextCentered(renderer, m_app->font(), "Waiting for players...", 640, y, 150, 150, 150);
        } else {
            for (const auto& name : m_connectedPlayers) {
                drawTextCentered(renderer, m_app->font(), name, 640, y, 255, 255, 255);
                y += 35;
            }
        }
    } else if (m_roomCode == "ERROR") {
        drawTextCentered(renderer, m_app->font(), "Failed to start server", 640, 250, 212, 0, 0);
    }

    renderButtons(renderer, m_buttons);
    drawEscHint(renderer, m_app->font());
}

// ============================================================================
// NetworkJoinScreen
// ============================================================================

NetworkJoinScreen::NetworkJoinScreen(Application* app)
    : Screen(AppState::NetworkJoin), m_app(app) {
    setupButtons();
}

NetworkJoinScreen::~NetworkJoinScreen() = default;

void NetworkJoinScreen::setupButtons() {
    m_buttons.clear();
    const int bw = 280;
    const int bh = 50;
    const int bx = (1280 - bw) / 2;
    int by = 520;

    m_buttons.push_back(std::make_unique<Button>(
        bx, by, bw, bh, "Connect",
        [this]() { tryConnect(); },
        m_app->font()));
    m_buttons.back()->theme = &m_app->theme();

    by += bh + 16;
    m_buttons.push_back(std::make_unique<Button>(
        bx, by, bw, bh, "Back",
        [this]() { m_app->screenManager().transitionTo(AppState::MainMenu); },
        m_app->font()));
    m_buttons.back()->theme = &m_app->theme();
}

void NetworkJoinScreen::onEnter() {
    Screen::onEnter();
    m_state = JoinState::EnterCode;
    m_roomCodeInput.clear();
    m_statusMessage = "Enter host IP and room code";
    m_connectTimer = 0.0f;
    m_gameClient.reset();
    SDL_StartTextInput();
    setupButtons();
}

void NetworkJoinScreen::onExit() {
    SDL_StopTextInput();
    if (m_gameClient && m_gameClient->isConnected() && !m_gameStarted) {
        m_gameClient->disconnect();
    }
}

void NetworkJoinScreen::tryConnect() {
    if (m_roomCodeInput.empty()) {
        m_statusMessage = "Please enter a room code or host IP";
        m_state = JoinState::Error;
        return;
    }

    m_state = JoinState::Connecting;
    m_statusMessage = "Connecting...";
    m_connectTimer = 0.0f;

    // Parse input: either "IP:PORT", "IP", or just treated as IP with default port
    std::string host = m_roomCodeInput;
    int port = 37015; // default port

    size_t colonPos = host.find(':');
    if (colonPos != std::string::npos) {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    }

    m_gameClient = std::make_unique<GameClient>();
    if (!m_gameClient->connect(host, port)) {
        m_statusMessage = "Connection failed";
        m_state = JoinState::Error;
        m_gameClient.reset();
        return;
    }

    // Send join room message
    NetworkMessage msg;
    msg.type = MessageType::JoinRoom;
    msg.payload["playerName"] = m_app->networkConfig().playerName.empty()
        ? "Player" : m_app->networkConfig().playerName;
    m_gameClient->send(msg);

    m_state = JoinState::Waiting;
    m_statusMessage = "Waiting for host...";
}

void NetworkJoinScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) {
        SDL_StopTextInput();
        return;
    }

    if (m_state == JoinState::EnterCode || m_state == JoinState::Error) {
        if (event.type == SDL_TEXTINPUT) {
            if (m_roomCodeInput.size() < 32) {
                m_roomCodeInput += event.text.text;  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            }
            return;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_BACKSPACE && !m_roomCodeInput.empty()) {
                m_roomCodeInput.pop_back();
                return;
            }
            if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                tryConnect();
                return;
            }
        }
        return;
    }

    if (handleButtonNavigation(event, m_buttons, m_focusedButtonIndex)) return;
    if (routeButtons(event, m_buttons)) return;
}

void NetworkJoinScreen::update(float deltaTime) {
    if (!m_gameClient || !m_gameClient->isConnected()) {
        if (m_state == JoinState::Waiting || m_state == JoinState::Ready) {
            m_statusMessage = "Disconnected from host";
            m_state = JoinState::Error;
        }
        return;
    }

    if (m_state == JoinState::Connecting) {
        m_connectTimer += deltaTime;
        if (m_connectTimer >= 5.0f) {
            m_statusMessage = "Connection timed out";
            m_state = JoinState::Error;
            m_gameClient->disconnect();
            m_gameClient.reset();
        }
        return;
    }

    if (m_state == JoinState::Waiting || m_state == JoinState::Ready) {
        auto messages = m_gameClient->receive();
        for (const auto& msg : messages) {
            switch (msg.type) {
                case MessageType::SeatAssignment: {
                    int seat = msg.payload.value("seatIndex", -1);
                    if (seat >= 0) {
                        m_app->networkConfig().mode = NetworkConfig::Mode::Client;
                        m_app->networkConfig().port = 0;
                        m_gameClient->setAssignedSeat(seat);
                        m_statusMessage = "Assigned to seat " + std::to_string(seat + 1);
                    }
                    break;
                }
                case MessageType::GameStarted: {
                    m_gameStarted = true;
                    m_app->screenManager().transitionTo(AppState::InRound);
                    return;
                }
                case MessageType::LobbyUpdate: {
                    m_statusMessage = "Waiting for host to start...";
                    break;
                }
                case MessageType::Error: {
                    m_statusMessage = msg.payload.value("message", "Error");
                    m_state = JoinState::Error;
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void NetworkJoinScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "Join Room");

    if (m_state == JoinState::EnterCode || m_state == JoinState::Error) {
        drawTextCentered(renderer, m_app->font(), "Enter host address (IP:port):",
                         640, 180, 200, 200, 200);

        // Input box
        fillRect(renderer, 340, 230, 600, 60, {60, 60, 70, 255});
        drawRect(renderer, 340, 230, 600, 60, {200, 200, 200, 255});

        std::string displayText = m_roomCodeInput;
        if ((SDL_GetTicks() / 500) % 2 == 0) displayText += "_";
        if (displayText.empty()) displayText = "e.g. 192.168.1.5:37015";
        unsigned char textR = m_roomCodeInput.empty() ? 150 : 255;
        drawTextCentered(renderer, m_app->font(), displayText, 640, 260, textR, textR, textR);
    }

    if (!m_statusMessage.empty()) {
        unsigned char r = (m_state == JoinState::Error) ? 212 : 200;
        unsigned char g = (m_state == JoinState::Error) ? 0 : 200;
        unsigned char b = (m_state == JoinState::Error) ? 0 : 200;
        drawTextCentered(renderer, m_app->font(), m_statusMessage, 640, 340, r, g, b);
    }

    renderButtons(renderer, m_buttons);
    drawEscHint(renderer, m_app->font());
}


}  // namespace blackjack
