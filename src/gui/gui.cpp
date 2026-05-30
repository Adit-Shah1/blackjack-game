// GUI layer — Application shell, ScreenManager, and stub screens
#include <blackjack/gui.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>
#include <algorithm>

namespace blackjack {

// ============================================================================
// Button
// ============================================================================

bool Button::contains(int mx, int my) const {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

void Button::render(SDL_Renderer* renderer, TTF_Font* font) {
    // Background
    if (hovered) {
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    }
    SDL_Rect rect{ x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);

    // Border
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &rect);

    // Label (centered)
    if (font && !label.empty()) {
        SDL_Color color{ 255, 255, 255, 255 };
        SDL_Surface* surface = TTF_RenderText_Blended(font, label.c_str(), color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            int tw = surface->w;
            int th = surface->h;
            SDL_Rect dst{ x + (w - tw) / 2, y + (h - th) / 2, tw, th };
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }
}

bool Button::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION) {
        hovered = contains(event.motion.x, event.motion.y);
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (contains(event.button.x, event.button.y) && onClick) {
            onClick();
            return true;
        }
    }
    return false;
}

// ============================================================================
// ScreenManager
// ============================================================================

ScreenManager::ScreenManager() = default;

void ScreenManager::registerScreen(AppState state, std::unique_ptr<Screen> screen) {
    m_screens[state] = std::move(screen);
}

void ScreenManager::transitionTo(AppState state) {
    if (m_currentScreen) {
        m_currentScreen->onExit();
    }

    auto it = m_screens.find(state);
    if (it != m_screens.end()) {
        m_currentScreen = it->second.get();
        m_currentState = state;
        m_currentScreen->onEnter();
    }
}

void ScreenManager::handleEvent(const SDL_Event& event) {
    if (m_currentScreen) {
        m_currentScreen->handleEvent(event);
    }
}

void ScreenManager::update(float deltaTime) {
    if (m_currentScreen) {
        m_currentScreen->update(deltaTime);
    }
}

void ScreenManager::render(SDL_Renderer* renderer) {
    if (m_currentScreen) {
        m_currentScreen->render(renderer);
    }
}

// ============================================================================
// Application
// ============================================================================

Application::Application(const std::string& title, int width, int height)
    : m_title(title), m_width(width), m_height(height) {}

Application::~Application() {
    if (m_font) TTF_CloseFont(m_font);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    TTF_Quit();
    SDL_Quit();
}

bool Application::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    m_window = SDL_CreateWindow(
        m_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    m_renderer = SDL_CreateRenderer(
        m_window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!m_renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_RenderSetLogicalSize(m_renderer, 1280, 720);

    if (!loadFont()) {
        std::cerr << "Warning: failed to load any font; text will not render." << std::endl;
    }

    return true;
}

bool Application::loadFont() {
    // Try common system font paths on macOS, Linux, and Windows
    static const std::vector<std::string> fontPaths = {
        // macOS
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Monaco.ttf",
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        // Windows
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
    };

    for (const auto& path : fontPaths) {
        m_font = TTF_OpenFont(path.c_str(), 24);
        if (m_font) return true;
    }

    return false;
}

void Application::renderText(const std::string& text, int x, int y,
                             unsigned char r, unsigned char g, unsigned char b) {
    if (!m_font || text.empty()) return;

    SDL_Color color{ r, g, b, 255 };
    SDL_Surface* surface = TTF_RenderText_Blended(m_font, text.c_str(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    if (texture) {
        SDL_Rect dst{ x, y, surface->w, surface->h };
        SDL_RenderCopy(m_renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void Application::run() {
    m_running = true;
    m_lastTime = SDL_GetTicks64();

    SDL_Event event;
    while (m_running) {
        uint64_t now = SDL_GetTicks64();
        float frameTime = static_cast<float>(now - m_lastTime) / 1000.0f;
        m_lastTime = now;
        m_accumulator += frameTime;

        // Process input every frame for responsiveness
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                m_running = false;
            }
            m_screenManager.handleEvent(event);
        }

        // Fixed-timestep updates
        while (m_accumulator >= FIXED_DT) {
            m_screenManager.update(FIXED_DT);
            m_accumulator -= FIXED_DT;
        }

        // Render
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(m_renderer);
        m_screenManager.render(m_renderer);
        SDL_RenderPresent(m_renderer);
    }
}

void Application::quit() {
    m_running = false;
}

// ============================================================================
// Helper: draw a centered title and an ESC hint
// ============================================================================

static void drawScreenLabel(SDL_Renderer* renderer, TTF_Font* font,
                            const std::string& title) {
    if (!font) return;
    SDL_Color color{ 255, 255, 255, 255 };
    SDL_Surface* surface = TTF_RenderText_Blended(font, title.c_str(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        int w = surface->w;
        int h = surface->h;
        SDL_Rect dst{ (1280 - w) / 2, 80, w, h };
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void drawEscHint(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;
    SDL_Color color{ 180, 180, 180, 255 };
    SDL_Surface* surface = TTF_RenderText_Blended(font, "Press ESC for Main Menu", color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        int w = surface->w;
        int h = surface->h;
        SDL_Rect dst{ (1280 - w) / 2, 660, w, h };
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void drawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                     int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (!font || text.empty()) return;
    SDL_Color color{r, g, b, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst{x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void drawTextCentered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                             int cx, int cy, unsigned char r, unsigned char g, unsigned char b) {
    if (!font || text.empty()) return;
    SDL_Color color{r, g, b, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        int x = cx - surface->w / 2;
        int y = cy - surface->h / 2;
        SDL_Rect dst{x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static std::string rankDisplay(const Card& card) {
    char c = card.rankChar();
    if (c == 'T') return "10";
    return std::string(1, c);
}

static bool isRedSuit(const Card& card) {
    return card.suit() == Suit::Hearts || card.suit() == Suit::Diamonds;
}

static bool handleEscToMenu(const SDL_Event& event, Application* app) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        app->screenManager().transitionTo(AppState::MainMenu);
        return true;
    }
    return false;
}

static void renderButtons(SDL_Renderer* renderer, TTF_Font* font,
                          std::vector<Button>& buttons) {
    for (auto& btn : buttons) {
        btn.render(renderer, font);
    }
}

static bool routeButtons(const SDL_Event& event, std::vector<Button>& buttons) {
    for (auto& btn : buttons) {
        if (btn.handleEvent(event)) return true;
    }
    return false;
}

// ============================================================================
// MainMenuScreen
// ============================================================================

MainMenuScreen::MainMenuScreen(Application* app)
    : Screen(AppState::MainMenu), m_app(app) {
    setupButtons();
}

void MainMenuScreen::setupButtons() {
    const int bw = 300;
    const int bh = 50;
    const int bx = (1280 - bw) / 2;
    int by = 280;
    const int gap = 20;

    auto makeBtn = [&](const std::string& label, AppState target) -> Button {
        Button b;
        b.x = bx; b.y = by; b.w = bw; b.h = bh;
        b.label = label;
        b.onClick = [this, target]() { m_app->screenManager().transitionTo(target); };
        by += bh + gap;
        return b;
    };

    m_buttons.push_back(makeBtn("Single Player", AppState::InRound));
    m_buttons.push_back(makeBtn("Multiplayer",   AppState::Lobby));
    m_buttons.push_back(makeBtn("Settings",      AppState::Settings));
    m_buttons.push_back(makeBtn("Achievements",    AppState::Achievements));
    m_buttons.push_back(makeBtn("Exit",            AppState::MainMenu));
    // Override the last button's onClick to actually quit
    m_buttons.back().onClick = [this]() { m_app->quit(); };
}

void MainMenuScreen::onEnter() {
    for (auto& btn : m_buttons) btn.hovered = false;
}

void MainMenuScreen::handleEvent(const SDL_Event& event) {
    if (routeButtons(event, m_buttons)) return;
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        m_app->quit();
    }
}

void MainMenuScreen::update(float /*deltaTime*/) {}

void MainMenuScreen::render(SDL_Renderer* renderer) {
    // Dark background
    SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
    SDL_RenderClear(renderer);

    // Title
    drawScreenLabel(renderer, m_app->font(), "BLACKJACK");

    // Subtitle
    if (m_app->font()) {
        SDL_Color color{ 200, 180, 120, 255 };
        SDL_Surface* surface = TTF_RenderText_Blended(m_app->font(), "Phase 5 — Basic Gameplay UI", color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                int w = surface->w;
                int h = surface->h;
                SDL_Rect dst{ (1280 - w) / 2, 150, w, h };
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    renderButtons(renderer, m_app->font(), m_buttons);
}

// ============================================================================
// LobbyScreen
// ============================================================================

LobbyScreen::LobbyScreen(Application* app)
    : Screen(AppState::Lobby), m_app(app) {}

void LobbyScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    routeButtons(event, m_buttons);
}

void LobbyScreen::update(float /*deltaTime*/) {}

void LobbyScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "Multiplayer Lobby");
    drawEscHint(renderer, m_app->font());
}

// ============================================================================
// SettingsScreen
// ============================================================================

SettingsScreen::SettingsScreen(Application* app)
    : Screen(AppState::Settings), m_app(app) {}

void SettingsScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    routeButtons(event, m_buttons);
}

void SettingsScreen::update(float /*deltaTime*/) {}

void SettingsScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "Settings");
    drawEscHint(renderer, m_app->font());
}

// ============================================================================
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
    m_round->startRound();
    m_currentBet = m_round->rules().minBet;
    m_lastPhase = RoundPhase::RoundComplete;
    m_autoAdvanceTimer = 0.0f;
    m_needsUIRebuild = false;
    m_message.clear();
    m_subMessage.clear();
    rebuildUI();
    updateMessage();
}

void GameTableScreen::rebuildUI() {
    m_buttons.clear();
    if (!m_round) return;

    const int bw = 100;
    const int bh = 40;
    const int gap = 10;
    int startX = 340;

    auto addBtn = [&](const std::string& label, int x, int y, int w, int h,
                      std::function<void()> cb) {
        Button b;
        b.x = x; b.y = y; b.w = w; b.h = h;
        b.label = label;
        b.onClick = std::move(cb);
        m_buttons.push_back(b);
    };

    switch (m_round->phase()) {
        case RoundPhase::WaitingForBets: {
            addBtn("-", 540, 400, 60, 40, [this]() { onBetMinus(); });
            addBtn("+", 680, 400, 60, 40, [this]() { onBetPlus(); });
            addBtn("Deal", 565, 460, 150, 50, [this]() { onDeal(); });
            break;
        }
        case RoundPhase::InsuranceOffer: {
            addBtn("Yes", 490, 400, 120, 50, [this]() { onInsuranceYes(); });
            addBtn("No",  670, 400, 120, 50, [this]() { onInsuranceNo(); });
            break;
        }
        case RoundPhase::PlayerTurns: {
            auto actions = m_round->getLegalActions(0, m_round->currentHandIndex());
            int bx = startX;
            if (actions.canHit) {
                addBtn("Hit", bx, 620, bw, bh, [this]() { onHit(); }); bx += bw + gap;
            }
            if (actions.canStand) {
                addBtn("Stand", bx, 620, bw, bh, [this]() { onStand(); }); bx += bw + gap;
            }
            if (actions.canDouble) {
                addBtn("Double", bx, 620, bw, bh, [this]() { onDouble(); }); bx += bw + gap;
            }
            if (actions.canSplit) {
                addBtn("Split", bx, 620, bw, bh, [this]() { onSplit(); }); bx += bw + gap;
            }
            if (actions.canSurrender) {
                addBtn("Surrender", bx, 620, bw + 20, bh, [this]() { onSurrender(); });
            }
            break;
        }
        case RoundPhase::RoundComplete: {
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
        case RoundPhase::WaitingForBets:
            m_message = "Place your bet";
            m_subMessage = "Bet: $" + std::to_string(m_currentBet);
            break;
        case RoundPhase::InitialDeal:
            m_message = "Dealing...";
            m_subMessage.clear();
            break;
        case RoundPhase::InsuranceOffer:
            m_message = "Dealer shows Ace. Take insurance?";
            m_subMessage.clear();
            break;
        case RoundPhase::PlayerTurns: {
            int handIdx = m_round->currentHandIndex();
            if (handIdx >= 0 && handIdx < static_cast<int>(m_round->seats()[0].hands.size())) {
                const auto& hand = m_round->seats()[0].hands[handIdx];
                m_message = "Hand " + std::to_string(handIdx + 1) + " of " +
                           std::to_string(m_round->seats()[0].hands.size());
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
            const auto& seat = m_round->seats()[0];
            for (size_t i = 0; i < seat.hands.size(); ++i) {
                if (!m_subMessage.empty()) m_subMessage += "  |  ";
                m_subMessage += "Hand " + std::to_string(i + 1) + ": " + toString(seat.hands[i].outcome);
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
    m_currentBet = std::min(maxBet, std::min(bankroll, m_currentBet + 10));
    updateMessage();
}

void GameTableScreen::onDeal() {
    if (!m_round) return;
    m_round->placeBet(0, m_currentBet);
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onHit() {
    if (!m_round) return;
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0) return;
    m_round->hit(0, handIdx);
    if (m_round->seats()[0].hands[handIdx].finished) {
        m_round->nextHand();
    }
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onStand() {
    if (!m_round) return;
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0) return;
    m_round->stand(0, handIdx);
    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onDouble() {
    if (!m_round) return;
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0) return;
    m_round->doubleDown(0, handIdx);
    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onSplit() {
    if (!m_round) return;
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0) return;
    m_round->split(0, handIdx);
    const auto& hand = m_round->seats()[0].hands[handIdx];
    if (hand.finished) {
        m_round->nextHand();
    }
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onSurrender() {
    if (!m_round) return;
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0) return;
    m_round->surrender(0, handIdx);
    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onInsuranceYes() {
    if (!m_round) return;
    int maxInsurance = m_round->seats()[0].hands[0].bet.mainBet / 2;
    m_round->takeInsurance(0, maxInsurance);
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onInsuranceNo() {
    if (!m_round) return;
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onNextRound() {
    if (!m_round) return;
    m_round->startRound();
    m_currentBet = m_round->rules().minBet;
    m_lastPhase = RoundPhase::RoundComplete;
    m_autoAdvanceTimer = 0.0f;
    m_needsUIRebuild = true;
    rebuildUI();
    updateMessage();
}

void GameTableScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    if (routeButtons(event, m_buttons)) return;

    if (event.type == SDL_KEYDOWN && m_round) {
        switch (m_round->phase()) {
            case RoundPhase::PlayerTurns: {
                auto actions = m_round->getLegalActions(0, m_round->currentHandIndex());
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
                    onDeal();
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

    if (m_round->phase() != m_lastPhase || m_needsUIRebuild) {
        m_lastPhase = m_round->phase();
        m_needsUIRebuild = false;
        rebuildUI();
        updateMessage();
        m_autoAdvanceTimer = 0.0f;
    }

    switch (m_round->phase()) {
        case RoundPhase::PlayerTurns: {
            int handIdx = m_round->currentHandIndex();
            if (handIdx >= 0 && handIdx < static_cast<int>(m_round->seats()[0].hands.size())) {
                const auto& hand = m_round->seats()[0].hands[handIdx];
                if (hand.hand.isBlackjack()) {
                    m_round->stand(0, handIdx);
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
            }
            break;
        default:
            break;
    }
}

void GameTableScreen::renderCard(SDL_Renderer* r, const Card& card, int x, int y, bool faceUp) {
    const int cw = 70;
    const int ch = 98;

    if (!faceUp) {
        renderCardBack(r, x, y);
        return;
    }

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_Rect bg{x, y, cw, ch};
    SDL_RenderFillRect(r, &bg);

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderDrawRect(r, &bg);

    bool red = isRedSuit(card);
    unsigned char rc = red ? 212 : 26;
    unsigned char gc = red ? 0 : 26;
    unsigned char bc = red ? 0 : 26;

    std::string rank = rankDisplay(card);
    std::string suit(1, card.suitChar());

    drawText(r, m_app->font(), rank, x + 4, y + 2, rc, gc, bc);
    drawText(r, m_app->font(), suit, x + 4, y + 24, rc, gc, bc);
    drawTextCentered(r, m_app->font(), rank, x + cw / 2, y + ch / 2, rc, gc, bc);
}

void GameTableScreen::renderCardBack(SDL_Renderer* r, int x, int y) {
    const int cw = 70;
    const int ch = 98;

    SDL_SetRenderDrawColor(r, 26, 58, 110, 255);
    SDL_Rect bg{x, y, cw, ch};
    SDL_RenderFillRect(r, &bg);

    SDL_SetRenderDrawColor(r, 60, 100, 160, 255);
    SDL_RenderDrawRect(r, &bg);

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

    int totalCards = holeVisible ? cardCount : cardCount + 1;
    int totalWidth = totalCards * cw - (totalCards - 1) * overlap;
    int startX = (1280 - totalWidth) / 2;

    for (int i = 0; i < totalCards; ++i) {
        int cx = startX + i * (cw - overlap);
        if (!holeVisible && i == 1) {
            renderCardBack(r, cx, 80);
        } else {
            int handIdx = holeVisible ? i : 0;
            if (handIdx < cardCount) {
                renderCard(r, dealer.hand.cards()[handIdx], cx, 80, true);
            }
        }
    }
}

void GameTableScreen::renderPlayer(SDL_Renderer* r) {
    if (!m_round) return;
    const auto& seat = m_round->seats()[0];
    const int cw = 70;
    const int ch = 98;
    const int overlap = 20;
    int currentHandIdx = m_round->currentHandIndex();

    for (size_t h = 0; h < seat.hands.size(); ++h) {
        const auto& handState = seat.hands[h];
        int cardCount = handState.hand.cardCount();
        int totalWidth = cardCount * cw - (cardCount - 1) * overlap;
        int startX = (1280 - totalWidth) / 2;
        int y = 380 + static_cast<int>(h) * 110;

        // Highlight active hand during PlayerTurns
        if (m_round->phase() == RoundPhase::PlayerTurns &&
            static_cast<int>(h) == currentHandIdx && !handState.finished) {
            SDL_SetRenderDrawColor(r, 255, 215, 0, 255);
            SDL_Rect highlight{ startX - 6, y - 6, totalWidth + 12, ch + 12 };
            SDL_RenderDrawRect(r, &highlight);
            SDL_SetRenderDrawColor(r, 255, 215, 0, 120);
            SDL_RenderFillRect(r, &highlight);
        }

        for (int i = 0; i < cardCount; ++i) {
            int cx = startX + i * (cw - overlap);
            renderCard(r, handState.hand.cards()[i], cx, y, true);
        }

        std::string total = std::to_string(handState.hand.bestValue());
        if (handState.hand.isSoft()) total += " (soft)";
        if (handState.hand.isBlackjack()) total += " — BJ!";
        if (handState.hand.isBust()) total += " — Bust!";
        int tw = 0, th = 0;
        if (m_app->font()) {
            TTF_SizeText(m_app->font(), total.c_str(), &tw, &th);
        }
        drawText(r, m_app->font(), total, (1280 - tw) / 2, y + ch + 4, 255, 255, 255);
    }
}

void GameTableScreen::renderStatus(SDL_Renderer* r) {
    if (!m_round) return;
    const auto& seat = m_round->seats()[0];
    std::string bankroll = "Bankroll: $" + std::to_string(seat.bankroll);
    drawText(r, m_app->font(), bankroll, 1050, 20, 255, 255, 255);
}

void GameTableScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 45, 90, 39, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 35, 70, 30, 255);
    SDL_Rect dealerArea{ 340, 30, 600, 170 };
    SDL_RenderFillRect(renderer, &dealerArea);

    SDL_SetRenderDrawColor(renderer, 35, 70, 30, 255);
    SDL_Rect playerArea{ 340, 350, 600, 220 };
    SDL_RenderFillRect(renderer, &playerArea);

    renderDealer(renderer);
    renderPlayer(renderer);
    renderStatus(renderer);

    if (!m_message.empty()) {
        drawTextCentered(renderer, m_app->font(), m_message, 640, 240, 255, 255, 255);
    }
    if (!m_subMessage.empty()) {
        drawTextCentered(renderer, m_app->font(), m_subMessage, 640, 270, 200, 200, 200);
    }

    renderButtons(renderer, m_app->font(), m_buttons);
    drawEscHint(renderer, m_app->font());
}

// ============================================================================
// RoundResultsScreen
// ============================================================================

RoundResultsScreen::RoundResultsScreen(Application* app)
    : Screen(AppState::RoundResults), m_app(app) {}

void RoundResultsScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    routeButtons(event, m_buttons);
}

void RoundResultsScreen::update(float /*deltaTime*/) {}

void RoundResultsScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "Round Results");
    drawEscHint(renderer, m_app->font());
}

// ============================================================================
// AchievementsScreen
// ============================================================================

AchievementsScreen::AchievementsScreen(Application* app)
    : Screen(AppState::Achievements), m_app(app) {}

void AchievementsScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    routeButtons(event, m_buttons);
}

void AchievementsScreen::update(float /*deltaTime*/) {}

void AchievementsScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "Achievements");
    drawEscHint(renderer, m_app->font());
}

}  // namespace blackjack
