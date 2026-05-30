// GUI layer — Application shell, ScreenManager, managers, widget toolkit, and screens
#include <blackjack/gui.h>
#include <blackjack/audio.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <algorithm>
#include <iostream>

namespace blackjack {

// ============================================================================
// Helpers
// ============================================================================

static SDL_Color toSDL(const Color& c) {
    SDL_Color sc = {};
    sc.r = c.r;
    sc.g = c.g;
    sc.b = c.b;
    sc.a = c.a;
    return sc;
}

static void fillRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void drawRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderDrawRect(r, &rect);
}

static void drawText(SDL_Renderer* renderer, TTF_Font* font,
                     const std::string& text, int x, int y,
                     unsigned char r, unsigned char g, unsigned char b) {
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

static void drawTextCentered(SDL_Renderer* renderer, TTF_Font* font,
                             const std::string& text, int cx, int cy,
                             unsigned char r, unsigned char g, unsigned char b) {
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

static void drawScreenLabel(SDL_Renderer* renderer, TTF_Font* font,
                            const std::string& title) {
    drawTextCentered(renderer, font, title, 640, 80, 255, 255, 255);
}

static void drawEscHint(SDL_Renderer* renderer, TTF_Font* font) {
    drawTextCentered(renderer, font, "Press ESC for Main Menu", 640, 660, 180, 180, 180);
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

static void renderButtons(SDL_Renderer* renderer,
                          std::vector<std::unique_ptr<Button>>& buttons) {
    for (auto& btn : buttons) {
        if (btn->visible) btn->render(renderer);
    }
}

static bool routeButtons(const SDL_Event& event,
                         std::vector<std::unique_ptr<Button>>& buttons) {
    for (auto& btn : buttons) {
        if (btn->handleEvent(event)) return true;
    }
    return false;
}

// ============================================================================
// TextureManager
// ============================================================================

TextureManager::~TextureManager() {
    clear();
}

SDL_Texture* TextureManager::load(SDL_Renderer* renderer,
                                  const std::string& key,
                                  const std::string& filepath) {
    auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second;

    SDL_Texture* texture = IMG_LoadTexture(renderer, filepath.c_str());
    if (!texture) {
        SDL_Surface* surface = SDL_LoadBMP(filepath.c_str());
        if (surface) {
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        }
    }
    if (texture) {
        m_textures[key] = texture;
    }
    return texture;
}

SDL_Texture* TextureManager::get(const std::string& key) {
    auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second;
    return nullptr;
}

void TextureManager::clear() {
    for (auto& [k, texture] : m_textures) {
        (void)k;
        if (texture) SDL_DestroyTexture(texture);
    }
    m_textures.clear();
}

// ============================================================================
// FontManager
// ============================================================================

FontManager::~FontManager() {
    clear();
}

TTF_Font* FontManager::load(const std::string& key,
                            const std::string& filepath,
                            int size) {
    auto it = m_fonts.find(key);
    if (it != m_fonts.end() && it->second != nullptr) {
        return it->second;
    }
    TTF_Font* font = TTF_OpenFont(filepath.c_str(), size);
    if (font) {
        m_fonts[key] = font;
    }
    return font;
}

TTF_Font* FontManager::get(const std::string& key) {
    auto it = m_fonts.find(key);
    if (it != m_fonts.end()) return it->second;
    return nullptr;
}

void FontManager::registerFont(const std::string& key, TTF_Font* font) {
    m_fonts[key] = font;
}

void FontManager::clear() {
    for (auto& [k, font] : m_fonts) {
        (void)k;
        if (font) TTF_CloseFont(font);
    }
    m_fonts.clear();
}

// ============================================================================
// Button
// ============================================================================

Button::Button(int x, int y, int w, int h, const std::string& label,
               std::function<void()> onClick, TTF_Font* font)
    : label(label), onClick(std::move(onClick)), m_font(font) {
    bounds = {x, y, w, h};
}

void Button::setFont(TTF_Font* font) {
    m_font = font;
}

void Button::resetState() {
    m_hovered = false;
    m_pressed = false;
}

void Button::render(SDL_Renderer* renderer) {
    if (!visible) return;

    float scale = 1.0f;
    if (m_pressed && enabled) {
        scale = 0.95f;
    } else if (m_hovered && enabled) {
        scale = 1.05f;
    }

    int w = static_cast<int>(bounds.w * scale);
    int h = static_cast<int>(bounds.h * scale);
    int x = bounds.x + (bounds.w - w) / 2;
    int y = bounds.y + (bounds.h - h) / 2;

    const Theme t = theme ? *theme : Theme{};

    Color bg = !enabled ? Color{40, 40, 40, 200} :
               (m_pressed && enabled ? t.buttonPressed :
                (m_hovered && enabled ? t.buttonHover : t.buttonNormal));

    SDL_Color sdlBg = toSDL(bg);
    SDL_SetRenderDrawColor(renderer, sdlBg.r, sdlBg.g, sdlBg.b, sdlBg.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);

    // Border
    if (m_hovered && enabled) {
        SDL_Color sdlGold = toSDL(t.goldAccent);
        SDL_SetRenderDrawColor(renderer, sdlGold.r, sdlGold.g, sdlGold.b, 180);
        SDL_RenderDrawRect(renderer, &rect);
    } else {
        SDL_Color sdlBorder = toSDL(t.textSecondary);
        SDL_SetRenderDrawColor(renderer, sdlBorder.r, sdlBorder.g, sdlBorder.b, sdlBorder.a);
        SDL_RenderDrawRect(renderer, &rect);
    }

    // Label
    if (m_font && !label.empty()) {
        Color textColor = !enabled ? t.textSecondary : t.textPrimary;
        SDL_Color sdlText = toSDL(textColor);
        SDL_Surface* surface = TTF_RenderText_Blended(m_font, label.c_str(), sdlText);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                int tw = surface->w;
                int th = surface->h;
                SDL_Rect dst{ x + (w - tw) / 2, y + (h - th) / 2, tw, th };
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

bool Button::handleEvent(const SDL_Event& event) {
    if (!enabled || !visible) return false;

    if (event.type == SDL_MOUSEMOTION) {
        bool inside = contains(event.motion.x, event.motion.y);
        if (inside != m_hovered) {
            m_hovered = inside;
        }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (contains(event.button.x, event.button.y)) {
            m_pressed = true;
            return true;
        }
    }
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        if (m_pressed) {
            m_pressed = false;
            if (contains(event.button.x, event.button.y) && onClick) {
                onClick();
                return true;
            }
        }
    }
    return false;
}

// ============================================================================
// Label
// ============================================================================

Label::Label(int x, int y, const std::string& text, TTF_Font* font)
    : text(text), m_font(font) {
    bounds = {x, y, 0, 0};
    color = {255, 255, 255, 255};
}

void Label::setFont(TTF_Font* font) {
    m_font = font;
}

void Label::render(SDL_Renderer* renderer) {
    if (!visible || text.empty() || !m_font) return;

    Color c = color;
    SDL_Color sdlColor = toSDL(c);
    SDL_Surface* surface = TTF_RenderText_Blended(m_font, text.c_str(), sdlColor);
    if (surface) {
        bounds.w = surface->w;
        bounds.h = surface->h;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect dst{bounds.x, bounds.y, bounds.w, bounds.h};
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
}

bool Label::handleEvent(const SDL_Event& /*event*/) {
    return false;
}

// ============================================================================
// Panel
// ============================================================================

Panel::Panel(int x, int y, int w, int h) {
    bounds = {x, y, w, h};
    backgroundColor = {0, 0, 0, 170};
}

void Panel::addWidget(std::unique_ptr<Widget> widget) {
    children.push_back(std::move(widget));
}

void Panel::render(SDL_Renderer* renderer) {
    if (!visible) return;

    if (backgroundColor.a > 0) {
        fillRect(renderer, bounds.x, bounds.y, bounds.w, bounds.h, backgroundColor);
    }

    for (auto& child : children) {
        if (child->visible) child->render(renderer);
    }
}

bool Panel::handleEvent(const SDL_Event& event) {
    if (!enabled || !visible) return false;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if ((*it)->handleEvent(event)) return true;
    }
    return false;
}

// ============================================================================
// Slider
// ============================================================================

Slider::Slider(int x, int y, int w, int h, int minVal, int maxVal, int initialVal)
    : m_minVal(minVal), m_maxVal(maxVal) {
    bounds = {x, y, w, h};
    minValue = minVal;
    maxValue = maxVal;
    value = initialVal;
}

void Slider::updateValueFromMouse(int mx) {
    int trackX = mx - bounds.x;
    float t = static_cast<float>(trackX) / bounds.w;
    t = std::max(0.0f, std::min(1.0f, t));
    int newValue = m_minVal + static_cast<int>(t * (m_maxVal - m_minVal));
    if (newValue != value) {
        value = newValue;
        if (onValueChanged) onValueChanged(value);
    }
}

void Slider::render(SDL_Renderer* renderer) {
    if (!visible) return;

    // Track
    int trackY = bounds.y + bounds.h / 2 - 2;
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_Rect track{bounds.x, trackY, bounds.w, 4};
    SDL_RenderFillRect(renderer, &track);

    // Handle
    float t = static_cast<float>(value - m_minVal) / (m_maxVal - m_minVal);
    int handleX = bounds.x + static_cast<int>(t * (bounds.w - 20));
    int handleY = bounds.y + (bounds.h - 20) / 2;
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_Rect handle{handleX, handleY, 20, 20};
    SDL_RenderFillRect(renderer, &handle);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &handle);
}

bool Slider::handleEvent(const SDL_Event& event) {
    if (!enabled || !visible) return false;

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (contains(event.button.x, event.button.y)) {
            m_dragging = true;
            updateValueFromMouse(event.button.x);
            return true;
        }
    }
    if (event.type == SDL_MOUSEMOTION && m_dragging) {
        updateValueFromMouse(event.motion.x);
        return true;
    }
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        if (m_dragging) {
            m_dragging = false;
            return true;
        }
    }
    return false;
}

// ============================================================================
// Modal
// ============================================================================

Modal::Modal(int x, int y, int w, int h,
             const std::string& title, const std::string& message,
             TTF_Font* font)
    : title(title), message(message), m_font(font) {
    bounds = {x, y, w, h};
}

void Modal::rebuildButtons() {
    m_buttons.clear();
    if (buttonLabels.empty()) {
        buttonLabels.push_back("OK");
    }

    int btnW = 100;
    int btnH = 40;
    int gap = 10;
    int totalW = static_cast<int>(buttonLabels.size()) * btnW +
                 (static_cast<int>(buttonLabels.size()) - 1) * gap;
    int startX = bounds.x + (bounds.w - totalW) / 2;
    int y = bounds.y + bounds.h - btnH - 20;

    for (size_t i = 0; i < buttonLabels.size(); ++i) {
        auto btn = std::make_unique<Button>(
            startX + static_cast<int>(i) * (btnW + gap), y, btnW, btnH,
            buttonLabels[i],
            [this, i]() { if (onResult) onResult(static_cast<int>(i)); },
            m_font);
        if (theme) btn->theme = theme;
        m_buttons.push_back(std::move(btn));
    }
}

void Modal::render(SDL_Renderer* renderer) {
    if (!visible) return;

    // Semi-transparent overlay
    fillRect(renderer, 0, 0, 1280, 720, {0, 0, 0, 170});

    // Modal panel
    fillRect(renderer, bounds.x, bounds.y, bounds.w, bounds.h, {40, 40, 45, 255});
    drawRect(renderer, bounds.x, bounds.y, bounds.w, bounds.h, {200, 200, 200, 255});

    // Title
    drawTextCentered(renderer, m_font, title, bounds.x + bounds.w / 2,
                       bounds.y + 30, 255, 255, 255);

    // Message
    drawTextCentered(renderer, m_font, message, bounds.x + bounds.w / 2,
                       bounds.y + 70, 200, 200, 200);

    // Buttons
    if (m_buttons.empty() && !buttonLabels.empty()) {
        rebuildButtons();
    }
    for (auto& btn : m_buttons) {
        if (btn->visible) btn->render(renderer);
    }
}

bool Modal::handleEvent(const SDL_Event& event) {
    if (!enabled || !visible) return false;
    for (auto& btn : m_buttons) {
        if (btn->handleEvent(event)) return true;
    }
    return false;
}

// ============================================================================
// Toast
// ============================================================================

Toast::Toast(int x, int y, const std::string& message, TTF_Font* font,
             float duration)
    : message(message), duration(duration), m_font(font) {
    bounds = {x, y, 300, 50};
}

void Toast::update(float deltaTime) {
    elapsed += deltaTime;
}

bool Toast::isExpired() const {
    return elapsed >= duration;
}

void Toast::render(SDL_Renderer* renderer) {
    if (!visible || isExpired()) return;

    float t = elapsed / duration;
    uint8_t alpha = static_cast<uint8_t>((1.0f - t) * 255);

    // Compute dynamic size from text
    if (m_font && !message.empty()) {
        int tw = 0, th = 0;
        TTF_SizeText(m_font, message.c_str(), &tw, &th);
        bounds.w = tw + 40;
        bounds.h = th + 20;
    }

    // Background
    fillRect(renderer, bounds.x, bounds.y, bounds.w, bounds.h,
             {40, 40, 40, alpha});

    // Border
    drawRect(renderer, bounds.x, bounds.y, bounds.w, bounds.h,
             {255, 215, 0, alpha});

    // Text
    if (m_font && !message.empty()) {
        SDL_Color color{255, 255, 255, alpha};
        SDL_Surface* surface = TTF_RenderText_Blended(m_font, message.c_str(), color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect dst{
                    bounds.x + (bounds.w - surface->w) / 2,
                    bounds.y + (bounds.h - surface->h) / 2,
                    surface->w, surface->h
                };
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

bool Toast::handleEvent(const SDL_Event& /*event*/) {
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
    m_textureManager.clear();
    m_fontManager.clear();
    m_font = nullptr;
    m_audioManager.reset();
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
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    m_audioManager = std::make_unique<AudioManager>();
    if (!m_audioManager->init()) {
        std::cerr << "Warning: Audio initialization failed." << std::endl;
    }

    if (!loadFont()) {
        std::cerr << "Warning: failed to load any font; text will not render." << std::endl;
    }

    return true;
}

bool Application::loadFont() {
    static const std::vector<std::string> fontPaths = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Monaco.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
    };

    for (const auto& path : fontPaths) {
        TTF_Font* font = TTF_OpenFont(path.c_str(), 24);
        if (font) {
            m_font = font;
            m_fontManager.registerFont("default", font);
            return true;
        }
    }

    return false;
}

void Application::renderText(const std::string& text, int x, int y,
                             unsigned char r, unsigned char g, unsigned char b) {
    if (!m_font || text.empty()) return;

    SDL_Color color{r, g, b, 255};
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

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                m_running = false;
            }
            m_screenManager.handleEvent(event);
        }

        while (m_accumulator >= FIXED_DT) {
            m_screenManager.update(FIXED_DT);
            m_accumulator -= FIXED_DT;
        }

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

    auto addBtn = [&](const std::string& label, AppState target) {
        m_buttons.push_back(std::make_unique<Button>(
            bx, by, bw, bh, label,
            [this, target]() { m_app->screenManager().transitionTo(target); },
            m_app->font()));
        m_buttons.back()->theme = &m_app->theme();
        by += bh + gap;
    };

    addBtn("Single Player", AppState::InRound);
    addBtn("Multiplayer", AppState::Lobby);
    addBtn("Settings", AppState::Settings);
    addBtn("Achievements", AppState::Achievements);

    // Exit button
    m_buttons.push_back(std::make_unique<Button>(
        bx, by, bw, bh, "Exit",
        [this]() { m_app->quit(); },
        m_app->font()));
    m_buttons.back()->theme = &m_app->theme();
}

void MainMenuScreen::onEnter() {
    for (auto& btn : m_buttons) {
        btn->resetState();
    }
}

void MainMenuScreen::handleEvent(const SDL_Event& event) {
    if (routeButtons(event, m_buttons)) return;
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        m_app->quit();
    }
}

void MainMenuScreen::update(float /*deltaTime*/) {}

void MainMenuScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "BLACKJACK");
    drawTextCentered(renderer, m_app->font(), "Phase 6 — Asset & Rendering Systems",
                     640, 150, 200, 180, 120);

    renderButtons(renderer, m_buttons);
}

// ============================================================================
// LobbyScreen
// ============================================================================

LobbyScreen::LobbyScreen(Application* app)
    : Screen(AppState::Lobby), m_app(app) {}

void LobbyScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    if (routeButtons(event, m_buttons)) return;
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
    if (routeButtons(event, m_buttons)) return;
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

    auto addBtn = [&](const std::string& label, int x, int y, int w, int h,
                      std::function<void()> cb) {
        m_buttons.push_back(std::make_unique<Button>(
            x, y, w, h, label, std::move(cb), m_app->font()));
        m_buttons.back()->theme = &m_app->theme();
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
            addBtn("No", 670, 400, 120, 50, [this]() { onInsuranceNo(); });
            break;
        }
        case RoundPhase::PlayerTurns: {
            auto actions = m_round->getLegalActions(0, m_round->currentHandIndex());
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

    int totalWidth = cardCount * cw - (cardCount - 1) * overlap;
    int startX = (1280 - totalWidth) / 2;

    for (int i = 0; i < cardCount; ++i) {
        int cx = startX + i * (cw - overlap);
        if (!holeVisible && i == 1) {
            renderCardBack(r, cx, 80);
        } else {
            renderCard(r, dealer.hand.cards()[i], cx, 80, true);
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

    renderButtons(renderer, m_buttons);
    drawEscHint(renderer, m_app->font());
}

// ============================================================================
// RoundResultsScreen
// ============================================================================

RoundResultsScreen::RoundResultsScreen(Application* app)
    : Screen(AppState::RoundResults), m_app(app) {}

void RoundResultsScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    if (routeButtons(event, m_buttons)) return;
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
    if (routeButtons(event, m_buttons)) return;
}

void AchievementsScreen::update(float /*deltaTime*/) {}

void AchievementsScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "Achievements");
    drawEscHint(renderer, m_app->font());
}

}  // namespace blackjack
