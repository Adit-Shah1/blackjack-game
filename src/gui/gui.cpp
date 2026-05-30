// GUI layer — Application shell, ScreenManager, and stub screens
#include <blackjack/gui.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>

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
        SDL_Surface* surface = TTF_RenderText_Blended(m_app->font(), "Phase 4 — SDL2 App Shell", color);
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

void GameTableScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
}

void GameTableScreen::update(float /*deltaTime*/) {}

void GameTableScreen::render(SDL_Renderer* renderer) {
    // Green felt
    SDL_SetRenderDrawColor(renderer, 45, 90, 39, 255);
    SDL_RenderClear(renderer);

    // Dealer area (darker oval-ish rectangle)
    SDL_SetRenderDrawColor(renderer, 35, 70, 30, 255);
    SDL_Rect dealerArea{ 440, 40, 400, 180 };
    SDL_RenderFillRect(renderer, &dealerArea);

    // Player area
    SDL_SetRenderDrawColor(renderer, 35, 70, 30, 255);
    SDL_Rect playerArea{ 440, 480, 400, 180 };
    SDL_RenderFillRect(renderer, &playerArea);

    drawScreenLabel(renderer, m_app->font(), "Game Table");
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
