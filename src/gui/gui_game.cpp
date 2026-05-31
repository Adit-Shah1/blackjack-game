#include <blackjack/gui.h>
#include <blackjack/audio.h>
#include <blackjack/network.h>

#include "gui_helpers.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <cmath>

namespace blackjack {

// GameTableScreen
// ============================================================================

GameTableScreen::GameTableScreen(Application* app)
    : Screen(AppState::InRound), m_app(app) {}

GameTableScreen::~GameTableScreen() = default;

void GameTableScreen::onEnter() {
    if (!m_round) {
        RuleSet rules;
        m_round = std::make_unique<RoundState>(rules);
    }

    // Reset stats and achievements for a fresh session
    m_stats = PlayerStats();
    m_sessionBlackjacks = 0;
    m_sessionHandsWithoutBust = 0;
    m_toasts.clear();
    m_showHelpOverlay = false;
    initAchievements();

    if (m_app->networkConfig().mode == NetworkConfig::Mode::Host) {
        setupNetworkHost();
    } else if (m_app->networkConfig().mode == NetworkConfig::Mode::Client) {
        setupNetworkClient();
    } else if (m_app->localMPConfig().enabled) {
        setupLocalMultiplayer();
    } else {
        setupAIOpponents();
    }

    m_round->startRound();
    m_currentBet = m_round->rules().minBet;
    m_lastPhase = RoundPhase::RoundComplete;
    m_autoAdvanceTimer = 0.0f;
    m_needsUIRebuild = false;
    m_message.clear();
    m_subMessage.clear();
    m_flyingCards.clear();
    m_holeCardFlipping = false;
    m_holeCardFlipTimer = 0.0f;
    m_screenFlash.active = false;
    m_lastDealerCardCount = 0;
    m_outcomeTexts.clear();
    m_aiTurnTimer = 0.0f;
    m_aiInsuranceResolved = false;
    m_passScreenActive = false;
    m_lastActiveSeat = m_app->localMPConfig().enabled ? 0 : -1;
    m_localMultiplayer = m_app->localMPConfig().enabled;
    m_app->localMPConfig().enabled = false; // Reset for next transition
    m_app->networkConfig().mode = NetworkConfig::Mode::None; // Reset network mode

    if (m_localMultiplayer) {
        m_currentBettingSeat = 0;
        int seatCount = static_cast<int>(m_round->seats().size());
        m_displayedBankrolls.resize(seatCount);
        for (int i = 0; i < seatCount; ++i) {
            m_displayedBankrolls[i] = m_round->seats()[i].bankroll;
        }
    } else {
        m_displayedBankroll = m_round->seats()[0].bankroll;
    }

    rebuildUI();
    updateMessage();
    m_app->audioManager().playAmbient("casino_ambient");
}


void GameTableScreen::setupAIOpponents() {
    if (!m_round || !m_aiControllers.empty()) return;
    // Default: 2 AI opponents (3 total players)
    const int aiCount = 2;
    for (int i = 0; i < aiCount; ++i) {
        std::string name = "AI " + std::to_string(i + 1);
        m_round->addSeat(name, m_round->rules().startingBankroll);
        int strategyType = i % 4;
        std::unique_ptr<IAIStrategy> strategy;
        switch (strategyType) {
            case 0: strategy = std::make_unique<BasicStrategy>(); break;
            case 1: strategy = std::make_unique<ConservativeStrategy>(); break;
            case 2: strategy = std::make_unique<AggressiveStrategy>(); break;
            case 3: strategy = std::make_unique<CardCounterStrategy>(&m_round->shoe()); break;
        }
        m_aiControllers.push_back(std::make_unique<AIController>(std::move(strategy)));
    }
}

void GameTableScreen::setupNetworkHost() {
    m_networkMode = NetworkMode::Host;
    m_aiControllers.clear();

    // Try to reuse the HostServer created in NetworkCreateScreen
    auto* createScreen = dynamic_cast<NetworkCreateScreen*>(
        m_app->screenManager().getScreen(AppState::NetworkCreate));
    if (createScreen && createScreen->hostServer() && createScreen->hostServer()->isRunning()) {
        m_hostServer = createScreen->takeHostServer();
    }

    if (!m_hostServer) {
        int port = m_app->networkConfig().port;
        if (port == 0) port = 37015;
        m_hostServer = std::make_unique<HostServer>(port);
        if (!m_hostServer->start()) {
            m_networkMode = NetworkMode::None;
            m_networkStatusMsg = "Failed to start host server";
            return;
        }
    }

    // Ensure seats exist for all expected players (host + joined clients)
    while (m_round->seats().size() < 4) {
        m_round->addSeat("", m_round->rules().startingBankroll);
    }
    m_round->seats()[0].name = "Host";
    // Restore client names to their seats
    for (const auto& [id, client] : m_hostServer->clients()) {
        if (client.assignedSeat >= 0 && client.assignedSeat < static_cast<int>(m_round->seats().size())) {
            m_round->seats()[client.assignedSeat].name = client.name;
        }
    }

    m_networkStatusMsg = "Hosting room " + m_hostServer->getRoomCode();
}

void GameTableScreen::setupNetworkClient() {
    m_networkMode = NetworkMode::Client;
    m_aiControllers.clear();

    // Try to reuse the GameClient from NetworkJoinScreen
    auto* joinScreen = dynamic_cast<NetworkJoinScreen*>(
        m_app->screenManager().getScreen(AppState::NetworkJoin));
    if (joinScreen && joinScreen->gameClient() && joinScreen->gameClient()->isConnected()) {
        int seat = joinScreen->gameClient()->assignedSeat();
        m_networkClientSession = std::make_unique<NetworkClientSession>();
        m_networkClientSession->setGameClient(joinScreen->takeGameClient());
        m_networkSeatIndex = seat;
        m_networkClientSession->update(0.0f); // Process any pending messages
    } else {
        // Fallback: create fresh connection
        if (!m_networkClientSession) {
            m_networkClientSession = std::make_unique<NetworkClientSession>();
        }

        if (!m_networkClientSession->isConnected()) {
            std::string host = m_app->networkConfig().hostAddress.empty()
                ? "127.0.0.1" : m_app->networkConfig().hostAddress;
            int port = m_app->networkConfig().port;
            if (port == 0) port = 37015;
            std::string name = m_app->networkConfig().playerName.empty()
                ? "Player" : m_app->networkConfig().playerName;

            if (!m_networkClientSession->connect(host, port, name)) {
                m_networkMode = NetworkMode::None;
                m_networkStatusMsg = "Failed to connect to host";
                return;
            }
        }

        m_networkSeatIndex = m_networkClientSession->mySeatIndex();
    }

    if (m_networkSeatIndex >= 0) {
        m_networkStatusMsg = "Connected as seat " + std::to_string(m_networkSeatIndex + 1);
    }
}

void GameTableScreen::processNetworkMessages(float deltaTime) {
    if (m_networkMode == NetworkMode::Host && m_hostServer) {
        m_hostServer->update(deltaTime);

        auto messages = m_hostServer->getMessages();
        for (auto& msg : messages) {
            int clientId = msg.senderId;
            if (clientId < 0) continue;

            int seatIdx = -1;
            for (const auto& [id, client] : m_hostServer->clients()) {
                if (id == clientId) {
                    seatIdx = client.assignedSeat;
                    break;
                }
            }
            if (seatIdx < 0) continue;

            switch (msg.type) {
                case MessageType::PlayerAction: {
                    std::string actionStr = msg.payload.value("action", "");
                    int handIdx = msg.payload.value("handIndex", 0);

                    PlayerAction action = PlayerAction::Stand;
                    if (actionStr == "Hit") action = PlayerAction::Hit;
                    else if (actionStr == "Stand") action = PlayerAction::Stand;
                    else if (actionStr == "DoubleDown") action = PlayerAction::DoubleDown;
                    else if (actionStr == "Split") action = PlayerAction::Split;
                    else if (actionStr == "Surrender") action = PlayerAction::Surrender;

                    auto legal = m_round->getLegalActions(seatIdx, handIdx);
                    bool valid = false;
                    switch (action) {
                        case PlayerAction::Hit: valid = legal.canHit; break;
                        case PlayerAction::Stand: valid = legal.canStand; break;
                        case PlayerAction::DoubleDown: valid = legal.canDouble; break;
                        case PlayerAction::Split: valid = legal.canSplit; break;
                        case PlayerAction::Surrender: valid = legal.canSurrender; break;
                        default: break;
                    }
                    if (!valid) continue;

                    switch (action) {
                        case PlayerAction::Hit: m_round->hit(seatIdx, handIdx); break;
                        case PlayerAction::Stand: m_round->stand(seatIdx, handIdx); break;
                        case PlayerAction::DoubleDown: m_round->doubleDown(seatIdx, handIdx); break;
                        case PlayerAction::Split: m_round->split(seatIdx, handIdx); break;
                        case PlayerAction::Surrender: m_round->surrender(seatIdx, handIdx); break;
                        default: break;
                    }

                    m_round->nextHand();
                    m_round->advancePhase();
                    broadcastStateToClients();
                    m_needsUIRebuild = true;
                    break;
                }
                case MessageType::PlaceBet: {
                    int amount = msg.payload.value("amount", 0);
                    m_round->placeBet(seatIdx, amount);
                    if (m_round->allSeatsHaveBets()) {
                        m_round->advancePhase();
                        animateInitialDealAllSeats();
                    }
                    broadcastStateToClients();
                    m_needsUIRebuild = true;
                    break;
                }
                case MessageType::TakeInsurance: {
                    bool take = msg.payload.value("take", false);
                    if (take && !m_round->seats()[seatIdx].hands.empty()) {
                        int maxInsurance = m_round->seats()[seatIdx].hands[0].bet.mainBet / 2;
                        m_round->takeInsurance(seatIdx, maxInsurance);
                    }
                    m_round->advancePhase();
                    broadcastStateToClients();
                    m_needsUIRebuild = true;
                    break;
                }
                default:
                    break;
            }
        }
    }

    if (m_networkMode == NetworkMode::Client && m_networkClientSession) {
        m_networkClientSession->update(deltaTime);
        if (m_networkClientSession->isConnected()) {
            *m_round = m_networkClientSession->round();
            m_needsUIRebuild = true;
        }
    }
}

void GameTableScreen::broadcastStateToClients() {
    if (!m_hostServer || !m_hostServer->isRunning() || !m_round) return;

    NetworkMessage msg;
    msg.type = MessageType::StateSync;

    nlohmann::json payload;
    payload["phase"] = toString(m_round->phase());
    payload["currentSeat"] = m_round->currentSeatIndex();
    payload["currentHand"] = m_round->currentHandIndex();

    nlohmann::json dealerJson;
    dealerJson["holeVisible"] = m_round->dealer().holeCardVisible;
    nlohmann::json dealerCards = nlohmann::json::array();
    // Hide hole card from clients until it's revealed
    if (!m_round->dealer().holeCardVisible && m_round->dealer().hand.cardCount() == 2) {
        if (!m_round->dealer().hand.cards().empty()) {
            dealerCards.push_back(cardToString(m_round->dealer().hand.cards()[0]));
        }
    } else {
        for (const auto& card : m_round->dealer().hand.cards()) {
            dealerCards.push_back(cardToString(card));
        }
    }
    dealerJson["cards"] = dealerCards;
    payload["dealer"] = dealerJson;

    nlohmann::json seatsJson = nlohmann::json::array();
    for (const auto& seat : m_round->seats()) {
        nlohmann::json seatJson;
        seatJson["name"] = seat.name;
        seatJson["bankroll"] = seat.bankroll;
        nlohmann::json hands = nlohmann::json::array();
        for (const auto& hand : seat.hands) {
            nlohmann::json handJson;
            nlohmann::json cards = nlohmann::json::array();
            for (const auto& c : hand.hand.cards()) {
                cards.push_back(cardToString(c));
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
        seatsJson.push_back(seatJson);
    }
    payload["seats"] = seatsJson;

    msg.payload = payload;
    m_hostServer->broadcast(msg);
}

void GameTableScreen::sendActionToHost(PlayerAction action) {
    if (m_networkMode != NetworkMode::Client || !m_networkClientSession) return;
    m_networkClientSession->sendAction(action);
}

void GameTableScreen::renderNetworkStatus(SDL_Renderer* r) {
    if (m_networkMode == NetworkMode::None || m_networkStatusMsg.empty()) return;
    drawText(r, m_app->font(), m_networkStatusMsg, 20, 680, 200, 200, 200);
}

void GameTableScreen::setupLocalMultiplayer() {
    if (!m_round) return;
    m_aiControllers.clear();
    m_round->seats().clear();

    const auto& config = m_app->localMPConfig();
    int count = std::max(2, std::min(4, config.playerCount));
    for (int i = 0; i < count; ++i) {
        std::string name = (i < static_cast<int>(config.playerNames.size()) && !config.playerNames[i].empty())
            ? config.playerNames[i]
            : "Player " + std::to_string(i + 1);
        m_round->addSeat(name, m_round->rules().startingBankroll);
    }

    m_localMultiplayer = true;
    m_currentBettingSeat = 0;
    m_passScreenActive = false;
    m_lastActiveSeat = -1;
    int seatCount = static_cast<int>(m_round->seats().size());
    m_displayedBankrolls.resize(seatCount);
    for (int i = 0; i < seatCount; ++i) {
        m_displayedBankrolls[i] = m_round->seats()[i].bankroll;
    }
}

int GameTableScreen::getLocalMPCenterX(int seatIndex, int totalSeats) const {
    if (totalSeats <= 1) return 640;
    int spacing = 1280 / (totalSeats + 1);
    return spacing * (seatIndex + 1);
}

void GameTableScreen::updateAllBankrollTickers(float deltaTime) {
    if (!m_round) return;
    int seatCount = static_cast<int>(m_round->seats().size());
    if (static_cast<int>(m_displayedBankrolls.size()) != seatCount) {
        m_displayedBankrolls.resize(seatCount);
        for (int i = 0; i < seatCount; ++i) {
            m_displayedBankrolls[i] = m_round->seats()[i].bankroll;
        }
    }
    for (int i = 0; i < seatCount; ++i) {
        int actual = m_round->seats()[i].bankroll;
        if (m_displayedBankrolls[i] != actual) {
            int diff = actual - m_displayedBankrolls[i];
            float speed = 800.0f * deltaTime;
            if (std::abs(diff) <= static_cast<int>(speed)) {
                m_displayedBankrolls[i] = actual;
            } else {
                m_displayedBankrolls[i] += (diff > 0 ? static_cast<int>(speed) : -static_cast<int>(speed));
            }
        }
    }
}

void GameTableScreen::onPlaceBet() {
    if (!m_round) return;
    if (m_networkMode == NetworkMode::Client) {
        if (m_networkClientSession) {
            m_networkClientSession->sendBet(m_currentBet);
        }
        return;
    }
    int seat = m_currentBettingSeat;
    if (seat < 0 || seat >= static_cast<int>(m_round->seats().size())) return;

    m_round->placeBet(seat, m_currentBet);
    m_currentBettingSeat++;
    m_currentBet = m_round->rules().minBet;

    if (m_round->allSeatsHaveBets()) {
        m_round->advancePhase();
        animateInitialDealAllSeats();
        m_app->audioManager().playSFX("shuffle");
    }

    if (m_networkMode == NetworkMode::Host) {
        broadcastStateToClients();
    }

    m_needsUIRebuild = true;
    updateMessage();
}

void GameTableScreen::onPassReady() {
    m_passScreenActive = false;
    m_needsUIRebuild = true;
    updateMessage();
}

void GameTableScreen::renderPassScreen(SDL_Renderer* r) {
    // Semi-transparent dark overlay
    fillRect(r, 0, 0, 1280, 720, {20, 20, 30, 255});

    if (!m_round) return;
    int seatIdx = m_round->currentSeatIndex();
    if (seatIdx < 0 || seatIdx >= static_cast<int>(m_round->seats().size())) return;

    const auto& seat = m_round->seats()[seatIdx];
    drawTextCentered(r, m_app->font(), "Pass the device", 640, 240, 255, 255, 255);
    drawTextCentered(r, m_app->font(), seat.name + "'s Turn", 640, 300, 255, 215, 0);
    drawTextCentered(r, m_app->font(), "Don't look at the screen until it's your turn!",
                     640, 360, 200, 200, 200);
}

void GameTableScreen::onExit() {
    m_app->audioManager().stopAmbient();
}

void GameTableScreen::rebuildUI() {
    m_buttons.clear();
    if (!m_round) return;

    auto addBtn = [&](const std::string& label, int x, int y, int w, int h,
                      std::function<void()> cb) {
        m_buttons.push_back(std::make_unique<Button>(
            x, y, w, h, label, std::move(cb), m_app->font()));
        m_buttons.back()->theme = &m_app->theme();
    };

    // Pass screen takes over all UI during local MP seat transitions
    if (m_passScreenActive) {
        addBtn("Ready", 565, 460, 150, 50, [this]() { onPassReady(); });
        return;
    }

    switch (m_round->phase()) {
        case RoundPhase::WaitingForBets: {
            addBtn("?", 1200, 20, 40, 40, [this]() { m_showHelpOverlay = true; });
            addBtn("-", 540, 400, 60, 40, [this]() { onBetMinus(); });
            addBtn("+", 680, 400, 60, 40, [this]() { onBetPlus(); });
            if (m_localMultiplayer) {
                addBtn("Place Bet", 540, 460, 200, 50, [this]() { onPlaceBet(); });
            } else {
                addBtn("Deal", 565, 460, 150, 50, [this]() { onDeal(); });
            }
            break;
        }
        case RoundPhase::InsuranceOffer: {
            addBtn("?", 1200, 20, 40, 40, [this]() { m_showHelpOverlay = true; });
            addBtn("Yes", 490, 400, 120, 50, [this]() { onInsuranceYes(); });
            addBtn("No", 670, 400, 120, 50, [this]() { onInsuranceNo(); });
            break;
        }
        case RoundPhase::PlayerTurns: {
            int seatIdx = m_round->currentSeatIndex();
            int handIdx = m_round->currentHandIndex();
            auto actions = m_round->getLegalActions(seatIdx, handIdx);
            int bx = 340;
            const int bw = 100;
            const int bh = 40;
            const int gap = 10;
            if (actions.canHit) {
                addBtn("Hit", bx, 620, bw, bh, [this]() { onHit(); });
                bx += bw + gap;
            }
            if (actions.canStand) {
                addBtn("Stand", bx, 620, bw, bh, [this]() { onStand(); });
                bx += bw + gap;
            }
            if (actions.canDouble) {
                addBtn("Double", bx, 620, bw, bh, [this]() { onDouble(); });
                bx += bw + gap;
            }
            if (actions.canSplit) {
                addBtn("Split", bx, 620, bw, bh, [this]() { onSplit(); });
                bx += bw + gap;
            }
            if (actions.canSurrender) {
                addBtn("Surrender", bx, 620, bw + 20, bh, [this]() { onSurrender(); });
            }
            break;
        }
        case RoundPhase::RoundComplete: {
            addBtn("?", 1200, 20, 40, 40, [this]() { m_showHelpOverlay = true; });
            addBtn("Next Round", 565, 580, 150, 50, [this]() { onNextRound(); });
            break;
        }
        default:
            break;
    }
}

void GameTableScreen::updateMessage() {
    if (!m_round) {
        m_message.clear();
        m_subMessage.clear();
        return;
    }

    switch (m_round->phase()) {
        case RoundPhase::WaitingForBets: {
            if (m_localMultiplayer) {
                if (m_currentBettingSeat >= 0 && m_currentBettingSeat < static_cast<int>(m_round->seats().size())) {
                    m_message = m_round->seats()[m_currentBettingSeat].name + ", place your bet";
                } else {
                    m_message = "Place your bets";
                }
            } else {
                m_message = "Place your bet";
            }
            m_subMessage = "Bet: $" + std::to_string(m_currentBet);
            break;
        }
        case RoundPhase::InitialDeal:
            m_message = "Dealing...";
            m_subMessage.clear();
            break;
        case RoundPhase::InsuranceOffer:
            m_message = "Dealer shows Ace. Take insurance?";
            m_subMessage.clear();
            break;
        case RoundPhase::PlayerTurns: {
            int currentSeat = m_round->currentSeatIndex();
            int handIdx = m_round->currentHandIndex();
            if (currentSeat >= 0 && currentSeat < static_cast<int>(m_round->seats().size()) &&
                handIdx >= 0 && handIdx < static_cast<int>(m_round->seats()[currentSeat].hands.size())) {
                const auto& seat = m_round->seats()[currentSeat];
                const auto& hand = seat.hands[handIdx];
                if (!m_localMultiplayer && currentSeat == 0) {
                    m_message = "Your turn — Hand " + std::to_string(handIdx + 1) + " of " +
                               std::to_string(seat.hands.size());
                } else {
                    m_message = seat.name + "'s turn — Hand " + std::to_string(handIdx + 1) +
                               " of " + std::to_string(seat.hands.size());
                }
                m_subMessage = "Total: " + std::to_string(hand.hand.bestValue());
                if (hand.hand.isSoft()) {
                    m_subMessage += " (soft)";
                }
                if (hand.hand.isBlackjack()) {
                    m_subMessage += " — Blackjack!";
                }
            } else {
                m_message.clear();
                m_subMessage.clear();
            }
            break;
        }
        case RoundPhase::DealerTurn:
            m_message = "Dealer's turn...";
            m_subMessage = "Dealer: " + std::to_string(m_round->dealer().hand.bestValue());
            break;
        case RoundPhase::EvaluateHands:
        case RoundPhase::Payout:
            m_message = "Evaluating...";
            m_subMessage.clear();
            break;
        case RoundPhase::RoundComplete: {
            m_message = "Round Complete";
            m_subMessage.clear();
            if (!m_localMultiplayer) {
                const auto& seat = m_round->seats()[0];
                for (size_t i = 0; i < seat.hands.size(); ++i) {
                    if (!m_subMessage.empty()) m_subMessage += "  |  ";
                    m_subMessage += "Hand " + std::to_string(i + 1) + ": " + toString(seat.hands[i].outcome);
                }
            }
            break;
        }
    }
}

void GameTableScreen::onBetMinus() {
    if (!m_round) return;
    int minBet = m_round->rules().minBet;
    m_currentBet = std::max(minBet, m_currentBet - 10);
    updateMessage();
}

void GameTableScreen::onBetPlus() {
    if (!m_round) return;
    int maxBet = m_round->rules().maxBet;
    int bankroll = m_round->seats()[0].bankroll;
    if (m_localMultiplayer && m_currentBettingSeat >= 0 &&
        m_currentBettingSeat < static_cast<int>(m_round->seats().size())) {
        bankroll = m_round->seats()[m_currentBettingSeat].bankroll;
    }
    m_currentBet = std::min(maxBet, std::min(bankroll, m_currentBet + 10));
    updateMessage();
}

void GameTableScreen::onDeal() {
    if (!m_round) return;
    if (m_networkMode == NetworkMode::Client) {
        if (m_networkClientSession) {
            m_networkClientSession->sendBet(m_currentBet);
        }
        return;
    }
    m_round->placeBet(0, m_currentBet);

    if (m_networkMode == NetworkMode::Host) {
        // In host mode: auto-bet for AI-controlled empty seats;
        // remote human clients must send their bets via PlaceBet messages.
        for (size_t i = 1; i < m_round->seats().size(); ++i) {
            // Check if this seat has a connected remote client
            bool hasRemoteClient = false;
            if (m_hostServer) {
                for (const auto& [id, client] : m_hostServer->clients()) {
                    if (client.connected && client.assignedSeat == static_cast<int>(i)) {
                        hasRemoteClient = true;
                        break;
                    }
                }
            }
            // Auto-bet only for seats without a remote client
            if (!hasRemoteClient) {
                // Use a simple default bet (min bet or half bankroll)
                int bet = std::min(m_round->rules().minBet * 2,
                                   m_round->seats()[i].bankroll);
                if (bet < m_round->rules().minBet) bet = m_round->rules().minBet;
                m_round->placeBet(static_cast<int>(i), bet);
            }
        }
    } else {
        // Single-player: AI opponents place their bets
        for (size_t i = 1; i < m_round->seats().size(); ++i) {
            int bet = m_aiControllers[i - 1]->chooseBet(*m_round, static_cast<int>(i));
            m_round->placeBet(static_cast<int>(i), bet);
        }
    }

    if (!m_round->allSeatsHaveBets()) return;

    m_round->advancePhase();
    animateInitialDealAllSeats();
    m_app->audioManager().playSFX("shuffle");
    if (m_networkMode == NetworkMode::Host) {
        broadcastStateToClients();
    }
    m_needsUIRebuild = true;
    m_aiInsuranceResolved = false;
}

void GameTableScreen::animateInitialDealAllSeats() {
    if (!m_round) return;
    float deckX = 640.0f - 35.0f;
    float deckY = 360.0f - 49.0f;
    float delay = 0.0f;
    const float delayStep = 0.05f;

    const auto& seats = m_round->seats();
    const auto& dealer = m_round->dealer();

    // First card to each player
    for (size_t s = 0; s < seats.size(); ++s) {
        if (!seats[s].hands.empty() && seats[s].hands[0].hand.cardCount() >= 1) {
            int tx, ty;
            getPlayerCardPosition(0, 0, tx, ty, static_cast<int>(s));
            spawnCardFly(seats[s].hands[0].hand.cards()[0], deckX, deckY,
                         static_cast<float>(tx), static_cast<float>(ty), true, delay,
                         static_cast<int>(s), 0, 0);
            delay += delayStep;
        }
    }

    // Dealer upcard
    if (dealer.hand.cardCount() >= 1) {
        int dx, dy;
        getDealerCardPosition(0, dx, dy);
        spawnCardFly(dealer.hand.cards()[0], deckX, deckY,
                     static_cast<float>(dx), static_cast<float>(dy), true, delay, -1, -1, 0);
        delay += delayStep;
    }

    // Second card to each player
    for (size_t s = 0; s < seats.size(); ++s) {
        if (!seats[s].hands.empty() && seats[s].hands[0].hand.cardCount() >= 2) {
            int tx, ty;
            getPlayerCardPosition(0, 1, tx, ty, static_cast<int>(s));
            spawnCardFly(seats[s].hands[0].hand.cards()[1], deckX, deckY,
                         static_cast<float>(tx), static_cast<float>(ty), true, delay,
                         static_cast<int>(s), 0, 1);
            delay += delayStep;
        }
    }

    m_lastDealerCardCount = dealer.hand.cardCount();
}

void GameTableScreen::onHit() {
    if (!m_round) return;
    if (m_networkMode == NetworkMode::Client) {
        sendActionToHost(PlayerAction::Hit);
        return;
    }
    int seatIdx = m_round->currentSeatIndex();
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0 || seatIdx < 0) return;
    int prevCount = m_round->seats()[seatIdx].hands[handIdx].hand.cardCount();
    m_round->hit(seatIdx, handIdx);
    int newCount = m_round->seats()[seatIdx].hands[handIdx].hand.cardCount();
    if (newCount > prevCount) {
        int tx, ty;
        getPlayerCardPosition(handIdx, newCount - 1, tx, ty, seatIdx);
        spawnCardFly(m_round->seats()[seatIdx].hands[handIdx].hand.cards().back(),
                     640.0f - 35.0f, 360.0f - 49.0f,
                     static_cast<float>(tx), static_cast<float>(ty), true, 0.0f, seatIdx, handIdx, newCount - 1);
        m_app->audioManager().playSFX("card_deal");
    }
    if (m_round->seats()[seatIdx].hands[handIdx].finished) {
        m_round->nextHand();
    }
    m_round->advancePhase();
    if (m_networkMode == NetworkMode::Host) {
        broadcastStateToClients();
    }
    m_needsUIRebuild = true;
}

void GameTableScreen::onStand() {
    if (!m_round) return;
    if (m_networkMode == NetworkMode::Client) {
        sendActionToHost(PlayerAction::Stand);
        return;
    }
    int seatIdx = m_round->currentSeatIndex();
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0 || seatIdx < 0) return;
    m_round->stand(seatIdx, handIdx);
    m_app->audioManager().playSFX("stand_click");
    m_round->nextHand();
    m_round->advancePhase();
    if (m_networkMode == NetworkMode::Host) {
        broadcastStateToClients();
    }
    m_needsUIRebuild = true;
}

void GameTableScreen::onDouble() {
    if (!m_round) return;
    if (m_networkMode == NetworkMode::Client) {
        sendActionToHost(PlayerAction::DoubleDown);
        return;
    }
    int seatIdx = m_round->currentSeatIndex();
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0 || seatIdx < 0) return;
    int prevCount = m_round->seats()[seatIdx].hands[handIdx].hand.cardCount();
    m_round->doubleDown(seatIdx, handIdx);
    int newCount = m_round->seats()[seatIdx].hands[handIdx].hand.cardCount();
    if (newCount > prevCount) {
        int tx, ty;
        getPlayerCardPosition(handIdx, newCount - 1, tx, ty, seatIdx);
        spawnCardFly(m_round->seats()[seatIdx].hands[handIdx].hand.cards().back(),
                     640.0f - 35.0f, 360.0f - 49.0f,
                     static_cast<float>(tx), static_cast<float>(ty), true, 0.0f, seatIdx, handIdx, newCount - 1);
        m_app->audioManager().playSFX("card_deal");
    }
    m_app->audioManager().playSFX("chip_stack");
    m_round->nextHand();
    m_round->advancePhase();
    if (m_networkMode == NetworkMode::Host) {
        broadcastStateToClients();
    }
    m_needsUIRebuild = true;
}

void GameTableScreen::onSplit() {
    if (!m_round) return;
    if (m_networkMode == NetworkMode::Client) {
        sendActionToHost(PlayerAction::Split);
        return;
    }
    int seatIdx = m_round->currentSeatIndex();
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0 || seatIdx < 0) return;
    m_round->split(seatIdx, handIdx);

    // Animate new cards dealt to both split hands
    const auto& hands = m_round->seats()[seatIdx].hands;
    for (size_t h = handIdx; h < hands.size() && h <= static_cast<size_t>(handIdx) + 1; ++h) {
        int cardCount = hands[h].hand.cardCount();
        if (cardCount >= 2) {
            int tx, ty;
            getPlayerCardPosition(static_cast<int>(h), cardCount - 1, tx, ty, seatIdx);
            spawnCardFly(hands[h].hand.cards().back(),
                         640.0f - 35.0f, 360.0f - 49.0f,
                         static_cast<float>(tx), static_cast<float>(ty), true,
                         static_cast<float>(h - handIdx) * 0.05f, seatIdx, static_cast<int>(h), cardCount - 1);
        }
    }
    m_app->audioManager().playSFX("card_deal");
    m_app->audioManager().playSFX("chip_stack");

    const auto& hand = m_round->seats()[seatIdx].hands[handIdx];
    if (hand.finished) {
        m_round->nextHand();
    }
    m_round->advancePhase();
    if (m_networkMode == NetworkMode::Host) {
        broadcastStateToClients();
    }
    m_needsUIRebuild = true;
}

void GameTableScreen::onSurrender() {
    if (!m_round) return;
    if (m_networkMode == NetworkMode::Client) {
        sendActionToHost(PlayerAction::Surrender);
        return;
    }
    int seatIdx = m_round->currentSeatIndex();
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0 || seatIdx < 0) return;
    m_round->surrender(seatIdx, handIdx);
    m_app->audioManager().playSFX("surrender");
    m_round->nextHand();
    m_round->advancePhase();
    if (m_networkMode == NetworkMode::Host) {
        broadcastStateToClients();
    }
    m_needsUIRebuild = true;
}

void GameTableScreen::onInsuranceYes() {
    if (!m_round) return;
    if (m_networkMode == NetworkMode::Client) {
        if (m_networkClientSession) {
            m_networkClientSession->sendInsurance(true);
        }
        return;
    }
    if (m_localMultiplayer) {
        // In local MP, auto-insure all seats to keep the game flowing
        for (size_t i = 0; i < m_round->seats().size(); ++i) {
            int maxInsurance = m_round->seats()[i].hands[0].bet.mainBet / 2;
            if (maxInsurance > 0 && maxInsurance <= m_round->seats()[i].bankroll) {
                m_round->takeInsurance(static_cast<int>(i), maxInsurance);
            }
        }
        m_app->audioManager().playSFX("chip_stack");
        m_round->advancePhase();
        m_needsUIRebuild = true;
        return;
    }
    int maxInsurance = m_round->seats()[0].hands[0].bet.mainBet / 2;
    m_round->takeInsurance(0, maxInsurance);
    m_app->audioManager().playSFX("chip_stack");
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onInsuranceNo() {
    if (!m_round) return;
    m_app->audioManager().playSFX("stand_click");
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onNextRound() {
    if (!m_round) return;
    m_round->startRound();
    m_currentBet = m_round->rules().minBet;
    m_lastPhase = RoundPhase::RoundComplete;
    m_autoAdvanceTimer = 0.0f;
    m_flyingCards.clear();
    m_holeCardFlipping = false;
    m_holeCardFlipTimer = 0.0f;
    m_screenFlash.active = false;
    m_lastDealerCardCount = 0;
    m_outcomeTexts.clear();
    m_passScreenActive = false;
    m_lastActiveSeat = m_localMultiplayer ? 0 : -1;
    m_aiTurnTimer = 0.0f;
    m_aiInsuranceResolved = false;
    if (m_localMultiplayer) {
        m_currentBettingSeat = 0;
        int seatCount = static_cast<int>(m_round->seats().size());
        m_displayedBankrolls.resize(seatCount);
        for (int i = 0; i < seatCount; ++i) {
            m_displayedBankrolls[i] = m_round->seats()[i].bankroll;
        }
    } else {
        m_displayedBankroll = m_round->seats()[0].bankroll;
    }
    if (m_networkMode == NetworkMode::Host) {
        broadcastStateToClients();
    }
    m_needsUIRebuild = true;
    rebuildUI();
    updateMessage();
    m_app->audioManager().playSFX("new_round");
}

void GameTableScreen::spawnCardFly(const Card& card, float fromX, float fromY,
                                    float toX, float toY, bool faceUp, float delay,
                                    int targetSeatIndex, int targetHandIndex,
                                    int targetCardIndex) {
    FlyingCard fc;
    fc.card = card;
    fc.startX = fromX;
    fc.startY = fromY;
    fc.targetX = toX;
    fc.targetY = toY;
    fc.x = fromX;
    fc.y = fromY;
    fc.faceUp = faceUp;
    fc.delay = delay;
    fc.duration = 0.25f;
    fc.elapsed = 0.0f;
    fc.done = false;
    fc.targetSeatIndex = targetSeatIndex;
    fc.targetHandIndex = targetHandIndex;
    fc.targetCardIndex = targetCardIndex;
    m_flyingCards.push_back(fc);
}

void GameTableScreen::spawnHoleCardFlip() {
    m_holeCardFlipping = true;
    m_holeCardFlipTimer = 0.0f;
    m_app->audioManager().playSFX("card_flip");
}

void GameTableScreen::spawnScreenFlash(const Color& color, float duration) {
    m_screenFlash.color = color;
    m_screenFlash.duration = duration;
    m_screenFlash.elapsed = 0.0f;
    m_screenFlash.active = true;
}

void GameTableScreen::getPlayerCardPosition(int handIndex, int cardIndex, int& outX, int& outY, int seatIndex) {
    const int cw = 70;
    const int overlap = 20;
    int totalSeats = static_cast<int>(m_round->seats().size());
    int centerX = m_localMultiplayer ? getLocalMPCenterX(seatIndex, totalSeats)
                                     : getSeatCenterX(seatIndex, totalSeats);
    int baseY;
    if (m_localMultiplayer) {
        baseY = 420;
    } else {
        baseY = (seatIndex == 0) ? 380 : 520;
    }

    if (seatIndex < 0 || seatIndex >= totalSeats ||
        handIndex < 0 || handIndex >= static_cast<int>(m_round->seats()[seatIndex].hands.size())) {
        outX = centerX;
        outY = baseY;
        return;
    }

    int cardCount = m_round->seats()[seatIndex].hands[handIndex].hand.cardCount();
    int totalWidth = cardCount * cw - (cardCount - 1) * overlap;
    int startX = centerX - totalWidth / 2;
    outX = startX + cardIndex * (cw - overlap);
    outY = baseY + handIndex * 110;
}

void GameTableScreen::getDealerCardPosition(int cardIndex, int& outX, int& outY) {
    const int cw = 70;
    const int overlap = 20;
    // Use fixed count of 2 when hole card exists but isn't visible,
    // so layout doesn't jump when hole card is revealed.
    int cardCount = m_round->dealer().hand.cardCount();
    bool holeVisible = m_round->dealer().holeCardVisible;
    int layoutCount = (cardCount == 1 && !holeVisible) ? 2 : cardCount;
    int totalWidth = layoutCount * cw - (layoutCount - 1) * overlap;
    int startX = (1280 - totalWidth) / 2;
    outX = startX + cardIndex * (cw - overlap);
    outY = 80;
}

void GameTableScreen::updateAnimations(float deltaTime) {
    // Flying cards
    for (auto& fc : m_flyingCards) {
        if (fc.done) continue;
        if (fc.delay > 0.0f) {
            fc.delay -= deltaTime;
            continue;
        }
        fc.elapsed += deltaTime;
        float t = std::min(1.0f, fc.elapsed / fc.duration);
        float ease = 1.0f - (1.0f - t) * (1.0f - t);  // EaseOut
        fc.x = fc.startX + (fc.targetX - fc.startX) * ease;
        fc.y = fc.startY + (fc.targetY - fc.startY) * ease;
        if (t >= 1.0f) fc.done = true;
    }
    m_flyingCards.erase(
        std::remove_if(m_flyingCards.begin(), m_flyingCards.end(),
            [](const FlyingCard& fc) { return fc.done; }),
        m_flyingCards.end());

    // Hole card flip
    if (m_holeCardFlipping) {
        m_holeCardFlipTimer += deltaTime;
        if (m_holeCardFlipTimer >= 0.3f) {
            m_holeCardFlipping = false;
            m_holeCardFlipTimer = 0.0f;
        }
    }

    // Screen flash
    if (m_screenFlash.active) {
        m_screenFlash.elapsed += deltaTime;
        if (m_screenFlash.elapsed >= m_screenFlash.duration) {
            m_screenFlash.active = false;
        }
    }
}

void GameTableScreen::playOutcomeAudio() {
    if (!m_round) return;
    const auto& seat = m_round->seats()[0];
    bool hasBlackjack = false;
    bool hasWin = false;
    bool hasBust = false;
    bool hasLose = false;
    for (const auto& hand : seat.hands) {
        switch (hand.outcome) {
            case HandOutcome::Blackjack: hasBlackjack = true; break;
            case HandOutcome::Win: hasWin = true; break;
            case HandOutcome::Bust: hasBust = true; break;
            case HandOutcome::Lose: hasLose = true; break;
            default: break;
        }
    }
    if (hasBlackjack) {
        m_app->audioManager().playSFX("blackjack_fanfare");
    } else if (hasWin) {
        m_app->audioManager().playSFX("win_chips");
    } else if (hasBust) {
        m_app->audioManager().playSFX("bust_sad");
    } else if (hasLose) {
        m_app->audioManager().playSFX("lose");
    }
}

void GameTableScreen::handleEvent(const SDL_Event& event) {
    if (handleHelpEvent(event)) return;
    if (m_showHelpOverlay) return;  // Block all input while help is open
    if (handleEscToMenu(event, m_app)) return;

    if (m_passScreenActive) {
        if (routeButtons(event, m_buttons)) return;
        return;
    }

    if (routeButtons(event, m_buttons)) return;

    if (event.type == SDL_KEYDOWN && m_round) {
        switch (m_round->phase()) {
            case RoundPhase::PlayerTurns: {
                int seatIdx = m_round->currentSeatIndex();
                int handIdx = m_round->currentHandIndex();
                auto actions = m_round->getLegalActions(seatIdx, handIdx);
                switch (event.key.keysym.sym) {
                    case SDLK_h: if (actions.canHit) onHit(); break;
                    case SDLK_s: if (actions.canStand) onStand(); break;
                    case SDLK_d: if (actions.canDouble) onDouble(); break;
                    case SDLK_p: if (actions.canSplit) onSplit(); break;
                    case SDLK_r: if (actions.canSurrender) onSurrender(); break;
                }
                break;
            }
            case RoundPhase::WaitingForBets:
                if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) {
                    if (m_localMultiplayer) {
                        onPlaceBet();
                    } else {
                        onDeal();
                    }
                } else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_DOWN) {
                    onBetMinus();
                } else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_UP) {
                    onBetPlus();
                }
                break;
            case RoundPhase::RoundComplete:
                if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) {
                    onNextRound();
                }
                break;
            case RoundPhase::InsuranceOffer:
                if (event.key.keysym.sym == SDLK_y) onInsuranceYes();
                else if (event.key.keysym.sym == SDLK_n) onInsuranceNo();
                break;
            default:
                break;
        }
    }
}

void GameTableScreen::update(float deltaTime) {
    if (!m_round) return;

    processNetworkMessages(deltaTime);

    updateAnimations(deltaTime);
    updateOutcomeTexts(deltaTime);
    updateToasts(deltaTime);

    if (m_localMultiplayer) {
        updateAllBankrollTickers(deltaTime);
    } else {
        updateBankrollTicker(deltaTime);
    }

    RoundPhase currentPhase = m_round->phase();

    // Pass screen handling for local multiplayer
    if (m_localMultiplayer && currentPhase == RoundPhase::PlayerTurns) {
        int currentSeat = m_round->currentSeatIndex();
        if (currentSeat >= 0 && currentSeat != m_lastActiveSeat && m_lastActiveSeat >= 0) {
            m_passScreenActive = true;
            m_lastActiveSeat = currentSeat;
            m_needsUIRebuild = true;
        }
        if (m_passScreenActive) {
            if (m_needsUIRebuild) {
                m_needsUIRebuild = false;
                rebuildUI();
            }
            return; // Block gameplay while pass screen is active
        }
    }

    if (currentPhase != m_lastPhase || m_needsUIRebuild) {
        RoundPhase prevPhase = m_lastPhase;
        m_lastPhase = currentPhase;
        m_needsUIRebuild = false;
        rebuildUI();
        updateMessage();
        m_autoAdvanceTimer = 0.0f;

        // Phase transition audio and effects
        if (currentPhase == RoundPhase::DealerTurn && prevPhase == RoundPhase::PlayerTurns) {
            spawnHoleCardFlip();
        }
        if (currentPhase == RoundPhase::Payout && prevPhase == RoundPhase::EvaluateHands) {
            playOutcomeAudio();
        }
        if (currentPhase == RoundPhase::RoundComplete && prevPhase != RoundPhase::RoundComplete) {
            // Update stats and check achievements
            m_stats.gamesPlayed++;
            bool anyBust = false;
            for (const auto& hand : m_round->seats()[0].hands) {
                m_stats.handsPlayed++;
                m_stats.totalWagered += hand.bet.mainBet;
                int payout = 0;
                switch (hand.outcome) {
                    case HandOutcome::Blackjack:
                        m_stats.blackjacks++;
                        m_stats.gamesWon++;
                        m_sessionBlackjacks++;
                        payout = hand.bet.mainBet * 3 / 2 + hand.bet.mainBet;
                        m_stats.currentStreak = std::max(1, m_stats.currentStreak + 1);
                        break;
                    case HandOutcome::Win:
                        m_stats.gamesWon++;
                        payout = hand.bet.mainBet * 2;
                        m_stats.currentStreak = std::max(1, m_stats.currentStreak + 1);
                        break;
                    case HandOutcome::Push:
                        m_stats.gamesPushed++;
                        payout = hand.bet.mainBet;
                        m_stats.currentStreak = 0;
                        break;
                    case HandOutcome::Bust:
                        m_stats.gamesLost++;
                        anyBust = true;
                        m_stats.currentStreak = std::min(-1, m_stats.currentStreak - 1);
                        break;
                    case HandOutcome::Lose:
                        m_stats.gamesLost++;
                        m_stats.currentStreak = std::min(-1, m_stats.currentStreak - 1);
                        break;
                    case HandOutcome::Surrender:
                        m_stats.gamesLost++;
                        payout = hand.bet.mainBet / 2;
                        m_stats.currentStreak = std::min(-1, m_stats.currentStreak - 1);
                        break;
                    default:
                        break;
                }
                m_stats.totalWon += payout - hand.bet.mainBet;
                if (payout > hand.bet.mainBet) {
                    m_stats.biggestWin = std::max(m_stats.biggestWin, payout - hand.bet.mainBet);
                }
            }
            m_stats.bestWinStreak = std::max(m_stats.bestWinStreak, m_stats.currentStreak);
            if (!anyBust) {
                m_sessionHandsWithoutBust++;
            } else {
                m_sessionHandsWithoutBust = 0;
            }
            checkAchievements();

            // Trigger visual effects based on outcomes
            for (size_t s = 0; s < m_round->seats().size(); ++s) {
                const auto& seat = m_round->seats()[s];
                bool hasBlackjack = false;
                bool hasWin = false;
                bool hasBust = false;
                bool hasLose = false;
                for (const auto& hand : seat.hands) {
                    switch (hand.outcome) {
                        case HandOutcome::Blackjack: hasBlackjack = true; break;
                        case HandOutcome::Win: hasWin = true; break;
                        case HandOutcome::Bust: hasBust = true; break;
                        case HandOutcome::Lose: hasLose = true; break;
                        default: break;
                    }
                }
                if (hasBlackjack) {
                    spawnScreenFlash({255, 215, 0, 120}, 1.0f);
                } else if (hasWin) {
                    spawnScreenFlash({255, 215, 0, 80}, 0.5f);
                } else if (hasBust) {
                    spawnScreenFlash({212, 0, 0, 80}, 0.5f);
                } else if (hasLose) {
                    spawnScreenFlash({212, 0, 0, 40}, 0.3f);
                }

                // Outcome text pop-in for all seats
                for (size_t i = 0; i < seat.hands.size(); ++i) {
                    const auto& hand = seat.hands[i];
                    std::string text;
                    Color color{255, 255, 255, 255};
                    switch (hand.outcome) {
                        case HandOutcome::Blackjack: text = "BLACKJACK!"; color = {255, 215, 0, 255}; break;
                        case HandOutcome::Win: text = "WIN!"; color = {0, 255, 100, 255}; break;
                        case HandOutcome::Push: text = "PUSH"; color = {180, 180, 180, 255}; break;
                        case HandOutcome::Bust: text = "BUST!"; color = {212, 0, 0, 255}; break;
                        case HandOutcome::Lose: text = "LOSE"; color = {212, 0, 0, 255}; break;
                        case HandOutcome::Surrender: text = "SURRENDER"; color = {180, 140, 80, 255}; break;
                        default: break;
                    }
                    if (!text.empty()) {
                        int tx, ty;
                        getPlayerCardPosition(static_cast<int>(i), 0, tx, ty, static_cast<int>(s));
                        ty -= 30;
                        spawnOutcomeText(text, static_cast<float>(tx) + 35.0f, static_cast<float>(ty), color);
                    }
                }
            }
        }
    }

    // Dealer hit card fly animations
    if (currentPhase == RoundPhase::DealerTurn) {
        int dealerCardCount = m_round->dealer().hand.cardCount();
        if (dealerCardCount > m_lastDealerCardCount && m_lastDealerCardCount >= 2) {
            for (int i = m_lastDealerCardCount; i < dealerCardCount; ++i) {
                int tx, ty;
                getDealerCardPosition(i, tx, ty);
                spawnCardFly(m_round->dealer().hand.cards()[i],
                             640.0f - 35.0f, 360.0f - 49.0f,
                             static_cast<float>(tx), static_cast<float>(ty), true, 0.0f, -1, -1, i);
                m_app->audioManager().playSFX("card_deal");
            }
        }
        if (dealerCardCount > 0) {
            m_lastDealerCardCount = dealerCardCount;
        }
    }

    // AI insurance resolution during InsuranceOffer phase
    if (currentPhase == RoundPhase::InsuranceOffer && !m_aiInsuranceResolved && !m_localMultiplayer) {
        m_aiInsuranceResolved = true;
        resolveAllAIInsurance();
    }

    // AI turn execution during PlayerTurns phase (only in single player)
    if (currentPhase == RoundPhase::PlayerTurns && !m_localMultiplayer) {
        int currentSeat = m_round->currentSeatIndex();
        if (currentSeat > 0 && currentSeat < static_cast<int>(m_round->seats().size())) {
            m_aiTurnTimer += deltaTime;
            if (m_aiTurnTimer >= m_aiTurnDelay) {
                m_aiTurnTimer = 0.0f;
                int handIdx = m_round->currentHandIndex();
                if (handIdx >= 0) {
                    executeAIAction(currentSeat, handIdx);
                }
            }
        }
    }

    switch (currentPhase) {
        case RoundPhase::PlayerTurns: {
            int seatIdx = m_round->currentSeatIndex();
            int handIdx = m_round->currentHandIndex();
            if (handIdx >= 0 && seatIdx >= 0 &&
                handIdx < static_cast<int>(m_round->seats()[seatIdx].hands.size())) {
                const auto& hand = m_round->seats()[seatIdx].hands[handIdx];
                if (hand.hand.isBlackjack()) {
                    m_round->stand(seatIdx, handIdx);
                    m_round->nextHand();
                    m_round->advancePhase();
                }
            }
            break;
        }
        case RoundPhase::DealerTurn:
        case RoundPhase::EvaluateHands:
        case RoundPhase::Payout:
            m_autoAdvanceTimer += deltaTime;
            if (m_autoAdvanceTimer >= 1.5f) {
                m_autoAdvanceTimer = 0.0f;
                m_round->advancePhase();
                if (m_networkMode == NetworkMode::Host) {
                    broadcastStateToClients();
                }
            }
            break;
        default:
            break;
    }
}

void GameTableScreen::renderCard(SDL_Renderer* r, const Card& card, int x, int y, bool faceUp, float scaleX) {
    const int cw = static_cast<int>(70 * scaleX);
    const int ch = 98;
    int cx = x + (70 - cw) / 2;

    if (!faceUp) {
        renderCardBack(r, cx, y, scaleX);
        return;
    }

    // Shadow
    drawShadow(r, cx, y, cw, ch, 2, {0, 0, 0, 100});

    // White rounded card face
    drawRoundedRect(r, cx, y, cw, ch, 4, {255, 255, 255, 255});
    drawRoundedRectOutline(r, cx, y, cw, ch, 4, {180, 180, 180, 255});

    bool red = isRedSuit(card);
    unsigned char rc = red ? 212 : 26;
    unsigned char gc = red ? 0 : 26;
    unsigned char bc = red ? 0 : 26;

    std::string rank = rankDisplay(card);
    Color suitColor{rc, gc, bc, 255};

    drawText(r, m_app->font(), rank, cx + 4, y + 2, rc, gc, bc);
    if (scaleX >= 0.5f) {
        drawSuitSymbol(r, card.suit(), cx + 10, y + 29, 1, suitColor);
        drawSuitSymbol(r, card.suit(), cx + cw / 2, y + ch / 2, 2, suitColor);
        drawSuitSymbol(r, card.suit(), cx + cw - 8, y + ch - 12, 1, suitColor);
    }
}

void GameTableScreen::renderCardBack(SDL_Renderer* r, int x, int y, float scaleX) {
    const int cw = static_cast<int>(70 * scaleX);
    const int ch = 98;

    // Shadow
    drawShadow(r, x, y, cw, ch, 2, {0, 0, 0, 100});

    drawRoundedRect(r, x, y, cw, ch, 4, {26, 58, 110, 255});
    drawRoundedRectOutline(r, x, y, cw, ch, 4, {60, 100, 160, 255});

    SDL_SetRenderDrawColor(r, 60, 100, 160, 255);
    for (int i = 0; i < cw; i += 8) {
        SDL_RenderDrawLine(r, x + i, y, x + i, y + ch);
    }
    for (int i = 0; i < ch; i += 8) {
        SDL_RenderDrawLine(r, x, y + i, x + cw, y + i);
    }
}

void GameTableScreen::renderDealer(SDL_Renderer* r) {
    if (!m_round) return;
    const auto& dealer = m_round->dealer();
    int cardCount = dealer.hand.cardCount();
    bool holeVisible = dealer.holeCardVisible;

    const int cw = 70;
    const int overlap = 20;

    // Use fixed count of 2 for layout when hole card exists but isn't visible yet,
    // to prevent upcard from jumping when hole card is revealed.
    int layoutCount = (dealer.hand.cardCount() == 1 && !holeVisible) ? 2 : cardCount;
    int totalWidth = layoutCount * cw - (layoutCount - 1) * overlap;
    int startX = (1280 - totalWidth) / 2;

    for (int i = 0; i < cardCount; ++i) {
        int cx = startX + i * (cw - overlap);

        // Skip rendering cards that are currently animating as flying cards
        bool skip = false;
        for (const auto& fc : m_flyingCards) {
            if (fc.targetSeatIndex == -1 && fc.targetHandIndex == -1 &&
                fc.targetCardIndex == i && !fc.done) {
                skip = true;
                break;
            }
        }
        if (skip) continue;

        if (m_holeCardFlipping && i == 1) {
            float t = m_holeCardFlipTimer / 0.3f;
            if (t < 0.5f) {
                // First half: card back shrinking
                float scale = 1.0f - t * 2.0f;
                renderCardBack(r, cx, 80, scale);
            } else {
                // Second half: face-up card growing
                float scale = (t - 0.5f) * 2.0f;
                renderCard(r, dealer.hand.cards()[i], cx, 80, true, scale);
            }
        } else if (!holeVisible && i == 1) {
            renderCardBack(r, cx, 80);
        } else {
            renderCard(r, dealer.hand.cards()[i], cx, 80, true);
        }
    }
}

void GameTableScreen::renderStatus(SDL_Renderer* r) {
    if (!m_round) return;
    if (m_localMultiplayer) {
        // Each seat shows its own bankroll via renderPlayerSeat
        return;
    }
    std::string bankroll = "Bankroll: $" + std::to_string(m_displayedBankroll);
    drawText(r, m_app->font(), bankroll, 1050, 20, 255, 255, 255);
}

void GameTableScreen::renderFlyingCards(SDL_Renderer* r) {
    for (const auto& fc : m_flyingCards) {
        renderCard(r, fc.card, static_cast<int>(fc.x), static_cast<int>(fc.y), fc.faceUp);
    }
}

void GameTableScreen::renderScreenFlash(SDL_Renderer* r) {
    if (!m_screenFlash.active) return;
    float t = m_screenFlash.elapsed / m_screenFlash.duration;
    float fade = 1.0f - t;
    uint8_t alpha = static_cast<uint8_t>(fade * m_screenFlash.color.a);
    fillRect(r, 0, 0, 1280, 720,
             {m_screenFlash.color.r, m_screenFlash.color.g, m_screenFlash.color.b, alpha});
}

void GameTableScreen::render(SDL_Renderer* renderer) {
    // Table felt background with subtle crosshatch
    SDL_SetRenderDrawColor(renderer, 45, 90, 39, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 40, 82, 35, 255);
    for (int i = 0; i < 1280; i += 24) {
        SDL_RenderDrawLine(renderer, i, 0, i, 720);
    }
    for (int i = 0; i < 720; i += 24) {
        SDL_RenderDrawLine(renderer, 0, i, 1280, i);
    }

    // Oval dealer area with shadow
    drawShadow(renderer, 340, 30, 600, 170, 6, {0, 0, 0, 120});
    drawRoundedRect(renderer, 340, 30, 600, 170, 12, {35, 70, 30, 255});
    drawRoundedRectOutline(renderer, 340, 30, 600, 170, 12, {60, 110, 50, 255});

    // Player area with shadow
    drawShadow(renderer, 340, 350, 600, 220, 6, {0, 0, 0, 120});
    drawRoundedRect(renderer, 340, 350, 600, 220, 12, {35, 70, 30, 255});
    drawRoundedRectOutline(renderer, 340, 350, 600, 220, 12, {60, 110, 50, 255});

    renderDealer(renderer);
    renderAllPlayers(renderer);
    renderFlyingCards(renderer);
    renderStatus(renderer);
    renderBetChips(renderer);
    renderNetworkStatus(renderer);

    // Message panel
    if (!m_message.empty() || !m_subMessage.empty()) {
        drawShadow(renderer, 440, 220, 400, 80, 4, {0, 0, 0, 100});
        drawRoundedRect(renderer, 440, 220, 400, 80, 8, {20, 20, 25, 200});
        drawRoundedRectOutline(renderer, 440, 220, 400, 80, 8, {60, 60, 70, 200});
    }

    if (!m_message.empty()) {
        drawTextCentered(renderer, m_app->font(), m_message, 640, 240, 255, 255, 255);
    }
    if (!m_subMessage.empty()) {
        drawTextCentered(renderer, m_app->font(), m_subMessage, 640, 270, 200, 200, 200);
    }

    if (m_passScreenActive) {
        renderPassScreen(renderer);
    }
    renderButtons(renderer, m_buttons);
    drawEscHint(renderer, m_app->font());
    renderScreenFlash(renderer);
    renderOutcomeTexts(renderer);
    renderHelpOverlay(renderer);
    renderToasts(renderer);
}

void GameTableScreen::spawnOutcomeText(const std::string& text, float x, float y, const Color& color) {
    OutcomeText ot;
    ot.text = text;
    ot.x = x;
    ot.y = y;
    ot.color = color;
    ot.scale = 0.0f;
    ot.elapsed = 0.0f;
    ot.done = false;
    m_outcomeTexts.push_back(ot);
}

void GameTableScreen::updateOutcomeTexts(float deltaTime) {
    for (auto& ot : m_outcomeTexts) {
        if (ot.done) continue;
        ot.elapsed += deltaTime;
        if (ot.elapsed >= ot.duration) {
            ot.done = true;
            ot.scale = 1.0f;
        } else {
            float t = ot.elapsed / ot.duration;
            // Bounce effect: overshoot then settle
            float bounce;
            if (t < 0.4f) {
                bounce = 1.3f * (t / 0.4f);
            } else if (t < 0.6f) {
                float sub = (t - 0.4f) / 0.2f;
                bounce = 1.3f - 0.4f * sub;
            } else {
                float sub = (t - 0.6f) / 0.4f;
                bounce = 0.9f + 0.1f * sub;
            }
            ot.scale = bounce;
        }
    }
    m_outcomeTexts.erase(
        std::remove_if(m_outcomeTexts.begin(), m_outcomeTexts.end(),
            [](const OutcomeText& ot) { return ot.done; }),
        m_outcomeTexts.end());
}

void GameTableScreen::renderOutcomeTexts(SDL_Renderer* r) {
    for (const auto& ot : m_outcomeTexts) {
        if (!m_app->font() || ot.text.empty()) continue;

        int tw = 0, th = 0;
        TTF_SizeText(m_app->font(), ot.text.c_str(), &tw, &th);

        int w = static_cast<int>(tw * ot.scale);
        int h = static_cast<int>(th * ot.scale);
        int x = static_cast<int>(ot.x) - w / 2;
        int y = static_cast<int>(ot.y) - h / 2;

        SDL_Color color{ot.color.r, ot.color.g, ot.color.b, ot.color.a};
        SDL_Surface* surface = TTF_RenderText_Blended(m_app->font(), ot.text.c_str(), color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(r, surface);
            if (texture) {
                SDL_Rect dst{x, y, w, h};
                SDL_RenderCopy(r, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void GameTableScreen::renderBetChips(SDL_Renderer* r) {
    if (!m_round) return;

    if (m_localMultiplayer) {
        // Render chips for each seat at their position
        int totalSeats = static_cast<int>(m_round->seats().size());
        for (int s = 0; s < totalSeats; ++s) {
            int bet = 0;
            if (m_round->phase() == RoundPhase::WaitingForBets && s == m_currentBettingSeat) {
                bet = m_currentBet;
            } else if (m_round->phase() != RoundPhase::RoundComplete && !m_round->seats()[s].hands.empty()) {
                bet = m_round->seats()[s].hands[0].bet.mainBet;
            }
            if (bet <= 0) continue;

            int cx = getLocalMPCenterX(s, totalSeats);
            int cy = 390;
            renderChipStack(r, cx, cy, bet);
        }
        return;
    }

    int bet = 0;
    if (m_round->phase() == RoundPhase::WaitingForBets) {
        bet = m_currentBet;
    } else if (m_round->phase() != RoundPhase::RoundComplete) {
        bet = m_round->seats()[0].hands[0].bet.mainBet;
    }
    if (bet <= 0) return;

    int cx = 640;
    int cy = 330;
    renderChipStack(r, cx, cy, bet);

    std::string betText = "$" + std::to_string(
        (m_round->phase() == RoundPhase::WaitingForBets) ? m_currentBet : m_round->seats()[0].hands[0].bet.mainBet);
    drawTextCentered(r, m_app->font(), betText, cx, cy + 18, 255, 255, 255);
}

void GameTableScreen::renderChipStack(SDL_Renderer* r, int cx, int cy, int bet) {
    int originalBet = bet;
    const int chipW = 28;
    const int chipH = 18;

    struct ChipDenom { int value; Color color; Color border; };
    static const ChipDenom chips[] = {
        {100, {26, 26, 26, 255}, {120, 120, 120, 255}},
        {25, {0, 170, 68, 255}, {100, 255, 150, 255}},
        {5, {212, 0, 0, 255}, {255, 150, 150, 255}},
        {1, {240, 240, 240, 255}, {180, 180, 180, 255}},
    };

    int stackY = cy;
    for (const auto& chip : chips) {
        int count = bet / chip.value;
        bet %= chip.value;
        for (int i = 0; i < count; ++i) {
            int y = stackY - i * 6;
            drawRoundedRect(r, cx - chipW / 2, y - chipH / 2, chipW, chipH, 4, chip.color);
            drawRoundedRectOutline(r, cx - chipW / 2, y - chipH / 2, chipW, chipH, 4, chip.border);
            // White accent dots
            SDL_SetRenderDrawColor(r, 255, 255, 255, 200);
            SDL_Rect dot1{cx - chipW / 2 + 2, y - 1, 3, 3};
            SDL_Rect dot2{cx + chipW / 2 - 5, y - 1, 3, 3};
            SDL_RenderFillRect(r, &dot1);
            SDL_RenderFillRect(r, &dot2);
        }
        if (count > 0) stackY -= 8;
    }

    std::string betText = "$" + std::to_string(originalBet);
    drawTextCentered(r, m_app->font(), betText, cx, cy + 18, 255, 255, 255);
}

// ============================================================================
// AI Helpers
// ============================================================================

// Achievement helpers
void GameTableScreen::initAchievements() {
    m_achievements = {
        {AchievementId::FirstBlackjack, "First Blackjack", "Get your first natural blackjack.", false},
        {AchievementId::ThousandBankroll, "High Roller", "Reach a bankroll of $1,000.", false},
        {AchievementId::FiveBlackjacksSession, "Lucky Streak", "Get 5 blackjacks in one session.", false},
        {AchievementId::TenWinStreak, "Unstoppable", "Win 10 games in a row.", false},
        {AchievementId::HundredHands, "Veteran", "Play 100 hands total.", false},
        {AchievementId::NeverBustSession, "Iron Stomach", "Play 20 hands without busting.", false},
    };
}

void GameTableScreen::checkAchievements() {
    if (!m_round || m_round->seats().empty()) return;
    int bankroll = m_round->seats()[0].bankroll;

    for (auto& ach : m_achievements) {
        if (ach.unlocked) continue;

        bool newlyUnlocked = false;
        switch (ach.id) {
            case AchievementId::FirstBlackjack:
                if (m_stats.blackjacks >= 1) newlyUnlocked = true;
                break;
            case AchievementId::ThousandBankroll:
                if (bankroll >= 1000) newlyUnlocked = true;
                break;
            case AchievementId::FiveBlackjacksSession:
                if (m_sessionBlackjacks >= 5) newlyUnlocked = true;
                break;
            case AchievementId::TenWinStreak:
                if (m_stats.bestWinStreak >= 10) newlyUnlocked = true;
                break;
            case AchievementId::HundredHands:
                if (m_stats.handsPlayed >= 100) newlyUnlocked = true;
                break;
            case AchievementId::NeverBustSession:
                if (m_sessionHandsWithoutBust >= 20) newlyUnlocked = true;
                break;
        }
        if (newlyUnlocked) {
            ach.unlocked = true;
            showToast("Achievement Unlocked: " + ach.name);
            m_app->audioManager().playSFX("win_chips");
        }
    }
}

void GameTableScreen::showToast(const std::string& message) {
    int y = 580 - static_cast<int>(m_toasts.size()) * 60;
    int tw = 0, th = 0;
    if (m_app->font()) {
        TTF_SizeText(m_app->font(), message.c_str(), &tw, &th);
    }
    int x = 640 - (tw + 40) / 2;
    auto toast = std::make_unique<Toast>(x, y, message, m_app->font(), 3.0f);
    m_toasts.push_back(std::move(toast));
}

void GameTableScreen::updateToasts(float deltaTime) {
    for (auto& toast : m_toasts) {
        toast->update(deltaTime);
    }
    m_toasts.erase(
        std::remove_if(m_toasts.begin(), m_toasts.end(),
            [](const std::unique_ptr<Toast>& t) { return t->isExpired(); }),
        m_toasts.end());
}

void GameTableScreen::renderToasts(SDL_Renderer* r) {
    for (auto& toast : m_toasts) {
        toast->render(r);
    }
}

void GameTableScreen::renderHelpOverlay(SDL_Renderer* r) {
    if (!m_showHelpOverlay) return;

    // Dim background
    fillRect(r, 0, 0, 1280, 720, {0, 0, 0, 200});

    // Panel
    int px = 240, py = 80, pw = 800, ph = 560;
    drawShadow(r, px, py, pw, ph, 6, {0, 0, 0, 120});
    drawRoundedRect(r, px, py, pw, ph, 12, {35, 35, 42, 255});
    drawRoundedRectOutline(r, px, py, pw, ph, 12, {80, 80, 90, 255});

    drawTextCentered(r, m_app->font(), "Help & Rules", px + pw / 2, py + 30, 255, 215, 0);

    struct HelpLine { std::string text; int y; unsigned char r, g, b; };
    std::vector<HelpLine> lines = {
        {"Objective: Beat the dealer by getting closer to 21 without busting.", py + 80, 255, 255, 255},
        {"", py + 105, 200, 200, 200},
        {"Card Values: Number cards = face value, Face cards = 10, Aces = 1 or 11.", py + 120, 220, 220, 220},
        {"", py + 145, 200, 200, 200},
        {"Keyboard Shortcuts:", py + 170, 255, 215, 0},
        {"  H  = Hit (take another card)", py + 200, 220, 220, 220},
        {"  S  = Stand (keep current hand)", py + 225, 220, 220, 220},
        {"  D  = Double Down (double bet, take one card)", py + 250, 220, 220, 220},
        {"  P  = Split (split pair into two hands)", py + 275, 220, 220, 220},
        {"  R  = Surrender (forfeit half your bet)", py + 300, 220, 220, 220},
        {"", py + 325, 200, 200, 200},
        {"Payouts: Blackjack pays 3:2, Win pays 1:1, Push returns bet.", py + 350, 220, 220, 220},
        {"Insurance: If dealer shows Ace, you can insure against blackjack (pays 2:1).", py + 375, 220, 220, 220},
        {"", py + 400, 200, 200, 200},
        {"Press ESC or click outside this panel to close.", py + 430, 180, 180, 180},
    };

    for (const auto& line : lines) {
        drawTextCentered(r, m_app->font(), line.text, px + pw / 2, line.y, line.r, line.g, line.b);
    }
}

bool GameTableScreen::handleHelpEvent(const SDL_Event& event) {
    if (!m_showHelpOverlay) return false;

    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        m_showHelpOverlay = false;
        return true;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int px = 240, py = 80, pw = 800, ph = 560;
        int mx = event.button.x, my = event.button.y;
        if (mx < px || mx > px + pw || my < py || my > py + ph) {
            m_showHelpOverlay = false;
            return true;
        }
    }
    return false;
}


void GameTableScreen::executeAIAction(int seatIndex, int handIndex) {
    if (!m_round || seatIndex <= 0 || seatIndex >= static_cast<int>(m_round->seats().size())) {
        return;
    }
    if (handIndex < 0 || handIndex >= static_cast<int>(m_round->seats()[seatIndex].hands.size())) {
        return;
    }

    int aiIdx = seatIndex - 1;
    if (aiIdx < 0 || aiIdx >= static_cast<int>(m_aiControllers.size())) {
        return;
    }

    PlayerAction action = m_aiControllers[aiIdx]->chooseAction(*m_round, seatIndex, handIndex);
    int prevCardCount = m_round->seats()[seatIndex].hands[handIndex].hand.cardCount();

    bool actionSucceeded = false;
    switch (action) {
        case PlayerAction::Hit:
            actionSucceeded = m_round->hit(seatIndex, handIndex);
            break;
        case PlayerAction::Stand:
            actionSucceeded = m_round->stand(seatIndex, handIndex);
            break;
        case PlayerAction::DoubleDown:
            actionSucceeded = m_round->doubleDown(seatIndex, handIndex);
            break;
        case PlayerAction::Split:
            actionSucceeded = m_round->split(seatIndex, handIndex);
            break;
        case PlayerAction::Surrender:
            actionSucceeded = m_round->surrender(seatIndex, handIndex);
            break;
        default:
            actionSucceeded = m_round->stand(seatIndex, handIndex);
            break;
    }

    if (!actionSucceeded) {
        // Fallback: stand
        m_round->stand(seatIndex, handIndex);
    }

    // Animate new card if drawn
    int newCardCount = m_round->seats()[seatIndex].hands[handIndex].hand.cardCount();
    if (newCardCount > prevCardCount) {
        int tx, ty;
        getPlayerCardPosition(handIndex, newCardCount - 1, tx, ty, seatIndex);
        spawnCardFly(m_round->seats()[seatIndex].hands[handIndex].hand.cards().back(),
                     640.0f - 35.0f, 360.0f - 49.0f,
                     static_cast<float>(tx), static_cast<float>(ty), true, 0.0f,
                     seatIndex, handIndex, newCardCount - 1);
        m_app->audioManager().playSFX("card_deal");
    }

    // If split created a new hand, animate the new card for the second hand too
    int handCount = static_cast<int>(m_round->seats()[seatIndex].hands.size());
    if (action == PlayerAction::Split && handCount > handIndex + 1) {
        int splitHandIdx = handIndex + 1;
        int splitCardCount = m_round->seats()[seatIndex].hands[splitHandIdx].hand.cardCount();
        if (splitCardCount > 0) {
            int tx, ty;
            getPlayerCardPosition(splitHandIdx, splitCardCount - 1, tx, ty, seatIndex);
            spawnCardFly(m_round->seats()[seatIndex].hands[splitHandIdx].hand.cards().back(),
                         640.0f - 35.0f, 360.0f - 49.0f,
                         static_cast<float>(tx), static_cast<float>(ty), true, 0.05f,
                         seatIndex, splitHandIdx, splitCardCount - 1);
        }
        m_app->audioManager().playSFX("card_deal");
    }

    if (action == PlayerAction::DoubleDown) {
        m_app->audioManager().playSFX("chip_stack");
    }

    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::resolveAllAIInsurance() {
    if (!m_round) return;
    for (size_t i = 1; i < m_round->seats().size() && i - 1 < m_aiControllers.size(); ++i) {
        const auto& seat = m_round->seats()[i];
        if (seat.hands.empty() || seat.hands[0].bet.mainBet <= 0) continue;
        bool takeIns = m_aiControllers[i - 1]->chooseInsurance(*m_round, static_cast<int>(i));
        if (takeIns) {
            int maxInsurance = seat.hands[0].bet.mainBet / 2;
            m_round->takeInsurance(static_cast<int>(i), maxInsurance);
        }
    }
}

int GameTableScreen::getSeatCenterX(int seatIndex, int totalSeats) const {
    if (totalSeats <= 1) return 640;
    // Seat 0 (human) is always centered at the bottom.
    if (seatIndex == 0) return 640;

    const int margin = 160;  // enough for 5-card hands (~270px) centered
    int aiCount = totalSeats - 1;
    int aiIdx = seatIndex - 1;  // 0-based among AI seats

    // Distribute AI seats evenly across the width above the human player.
    // With 1 AI: centered at top. With 2+ AI: spread left/right.
    if (aiCount == 1) {
        return 640;
    }

    int aiSpacing = (1280 - margin * 2) / (aiCount + 1);
    return margin + (aiIdx + 1) * aiSpacing;
}

void GameTableScreen::renderAllPlayers(SDL_Renderer* r) {
    if (!m_round) return;
    int totalSeats = static_cast<int>(m_round->seats().size());
    if (totalSeats == 0) return;

    if (m_localMultiplayer) {
        // All seats are human, distributed evenly across the bottom
        for (int s = 0; s < totalSeats; ++s) {
            int cx = getLocalMPCenterX(s, totalSeats);
            int baseY = 420;
            renderPlayerSeat(r, s, cx, baseY, true);
        }
    } else {
        for (int s = 0; s < totalSeats; ++s) {
            int cx = getSeatCenterX(s, totalSeats);
            int baseY = (s == 0) ? 380 : 520;  // Human at bottom, AI above
            bool isHuman = (s == 0);
            renderPlayerSeat(r, s, cx, baseY, isHuman);
        }
    }
}

void GameTableScreen::renderPlayerSeat(SDL_Renderer* r, int seatIndex, int centerX, int baseY, bool isHuman) {
    if (!m_round) return;
    const auto& seat = m_round->seats()[seatIndex];
    const int cw = 70;
    const int ch = 98;
    const int overlap = 20;
    int currentHandIdx = m_round->currentHandIndex();
    int currentSeatIdx = m_round->currentSeatIndex();

    // Draw seat name label
    Color nameColor = isHuman ? Color{255, 215, 0, 255} : Color{200, 200, 200, 255};
    drawTextCentered(r, m_app->font(), seat.name, centerX, baseY - 30,
                     nameColor.r, nameColor.g, nameColor.b);

    // Draw bankroll
    std::string bankrollText = "$" + std::to_string(seat.bankroll);
    drawTextCentered(r, m_app->font(), bankrollText, centerX, baseY - 12,
                     180, 180, 180);

    for (size_t h = 0; h < seat.hands.size(); ++h) {
        const auto& handState = seat.hands[h];
        int cardCount = handState.hand.cardCount();
        if (cardCount == 0) continue;

        int totalWidth = cardCount * cw - (cardCount - 1) * overlap;
        int startX = centerX - totalWidth / 2;
        int y = baseY + static_cast<int>(h) * 110;

        // Highlight active hand during PlayerTurns with pulsing glow
        if (m_round->phase() == RoundPhase::PlayerTurns &&
            seatIndex == currentSeatIdx &&
            static_cast<int>(h) == currentHandIdx && !handState.finished) {
            uint32_t ticks = SDL_GetTicks();
            float pulse = (std::sin(ticks * 0.005f) + 1.0f) * 0.5f;
            uint8_t alpha = static_cast<uint8_t>(120 + pulse * 80);
            SDL_SetRenderDrawColor(r, 255, 215, 0, 255);
            SDL_Rect highlight{ startX - 6, y - 6, totalWidth + 12, ch + 12 };
            SDL_RenderDrawRect(r, &highlight);
            SDL_SetRenderDrawColor(r, 255, 215, 0, alpha);
            SDL_RenderFillRect(r, &highlight);
        }

        for (int i = 0; i < cardCount; ++i) {
            // Skip rendering cards that are currently animating as flying cards
            bool skip = false;
            for (const auto& fc : m_flyingCards) {
                if (fc.targetSeatIndex == seatIndex &&
                    fc.targetHandIndex == static_cast<int>(h) &&
                    fc.targetCardIndex == i && !fc.done) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            int cx = startX + i * (cw - overlap);
            renderCard(r, handState.hand.cards()[i], cx, y, true);
        }

        // Hand total / outcome text
        std::string labelText = std::to_string(handState.hand.bestValue());
        if (handState.hand.isSoft()) labelText += " (soft)";
        if (handState.hand.isBlackjack()) labelText += " — BJ!";
        if (handState.hand.isBust()) labelText += " — Bust!";

        // Show outcome text for completed hands
        if (handState.outcome != HandOutcome::Pending) {
            labelText += " [" + toString(handState.outcome) + "]";
        }

        int tw = 0, th = 0;
        if (m_app->font()) {
            TTF_SizeText(m_app->font(), labelText.c_str(), &tw, &th);
        }
        drawText(r, m_app->font(), labelText, centerX - tw / 2, y + ch + 4,
                 255, 255, 255);
    }
}

void GameTableScreen::updateBankrollTicker(float deltaTime) {
    if (!m_round) return;
    int actual = m_round->seats()[0].bankroll;
    if (m_displayedBankroll != actual) {
        int diff = actual - m_displayedBankroll;
        float speed = 800.0f * deltaTime; // 800 chips per second
        if (std::abs(diff) <= static_cast<int>(speed)) {
            m_displayedBankroll = actual;
        } else {
            m_displayedBankroll += (diff > 0 ? static_cast<int>(speed) : -static_cast<int>(speed));
        }
    }
}


}  // namespace blackjack
