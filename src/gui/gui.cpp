// GUI layer — Application shell, ScreenManager, managers, widget toolkit, and screens
#include <blackjack/gui.h>
#include <blackjack/audio.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <algorithm>
#include <cmath>
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
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
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
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
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

static void drawRoundedRect(SDL_Renderer* r, int x, int y, int w, int h, int rad, const Color& c) {
    fillRect(r, x + rad, y, w - rad*2, h, c);
    fillRect(r, x, y + rad, w, h - rad*2, c);
    fillRect(r, x + rad, y + rad, rad, rad, c);
    fillRect(r, x + w - rad*2, y + rad, rad, rad, c);
    fillRect(r, x + rad, y + h - rad*2, rad, rad, c);
    fillRect(r, x + w - rad*2, y + h - rad*2, rad, rad, c);
}

static void drawRoundedRectOutline(SDL_Renderer* r, int x, int y, int w, int h, int rad, const Color& c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(r, x + rad, y, x + w - rad - 1, y);
    SDL_RenderDrawLine(r, x + rad, y + h - 1, x + w - rad - 1, y + h - 1);
    SDL_RenderDrawLine(r, x, y + rad, x, y + h - rad - 1);
    SDL_RenderDrawLine(r, x + w - 1, y + rad, x + w - 1, y + h - rad - 1);
}

static void drawShadow(SDL_Renderer* r, int x, int y, int w, int h, int offset, const Color& c) {
    fillRect(r, x + offset, y + offset, w, h, c);
}

static void drawGradientRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& top, const Color& bot) {
    for (int row = 0; row < h; ++row) {
        float t = static_cast<float>(row) / h;
        uint8_t rc = static_cast<uint8_t>(top.r + (bot.r - top.r) * t);
        uint8_t gc = static_cast<uint8_t>(top.g + (bot.g - top.g) * t);
        uint8_t bc = static_cast<uint8_t>(top.b + (bot.b - top.b) * t);
        uint8_t ac = static_cast<uint8_t>(top.a + (bot.a - top.a) * t);
        SDL_SetRenderDrawColor(r, rc, gc, bc, ac);
        SDL_RenderDrawLine(r, x, y + row, x + w - 1, y + row);
    }
}

static void drawBitmap(SDL_Renderer* r, int x, int y, int scale,
                       const char* const bitmap[], int rows, int cols,
                       const Color& color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int row = 0; row < rows; ++row) {
        const char* line = bitmap[row];
        for (int col = 0; col < cols; ++col) {
            if (line[col] == '#') {
                SDL_Rect pixel{ x + col * scale, y + row * scale, scale, scale };
                SDL_RenderFillRect(r, &pixel);
            }
        }
    }
}

static void drawSuitSymbol(SDL_Renderer* r, Suit suit, int cx, int cy, int scale, const Color& color) {
    // 11x11 bitmaps, drawn centered at (cx, cy)
    static const char* HEART[] = {
        "...........",
        "..##...##..",
        ".####.####.",
        ".#########.",
        ".#########.",
        ".#########.",
        "..#######..",
        "...#####...",
        "....###....",
        ".....#.....",
        "...........",
    };
    static const char* DIAMOND[] = {
        "...........",
        ".....#.....",
        "....###....",
        "...#####...",
        "..#######..",
        ".#########.",
        "..#######..",
        "...#####...",
        "....###....",
        ".....#.....",
        "...........",
    };
    static const char* SPADE[] = {
        "...........",
        ".....#.....",
        "....###....",
        "...#####...",
        "..#######..",
        ".#########.",
        ".#########.",
        "..#######..",
        "...#####...",
        "....###....",
        "....###....",
    };
    static const char* CLUB[] = {
        "...........",
        "....###....",
        "...#####...",
        "..#######..",
        "...#####...",
        ".#########.",
        "..#######..",
        "...#####...",
        "....###....",
        "....###....",
        "....###....",
    };

    const char* const* bitmap = nullptr;
    switch (suit) {
        case Suit::Hearts:   bitmap = HEART;   break;
        case Suit::Diamonds: bitmap = DIAMOND; break;
        case Suit::Spades:   bitmap = SPADE;   break;
        case Suit::Clubs:    bitmap = CLUB;    break;
    }
    if (!bitmap) return;

    const int rows = 11;
    const int cols = 11;
    int w = cols * scale;
    int h = rows * scale;
    int x = cx - w / 2;
    int y = cy - h / 2;
    drawBitmap(r, x, y, scale, bitmap, rows, cols, color);
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

    Color bgTop = !enabled ? Color{40, 40, 40, 200} :
                  (m_pressed && enabled ? t.buttonPressed :
                   (m_hovered && enabled ? t.buttonHover : t.buttonNormal));
    Color bgBot = bgTop;
    bgBot.r = static_cast<uint8_t>(bgBot.r * 0.85f);
    bgBot.g = static_cast<uint8_t>(bgBot.g * 0.85f);
    bgBot.b = static_cast<uint8_t>(bgBot.b * 0.85f);

    // Shadow
    drawShadow(renderer, x, y, w, h, 3, {0, 0, 0, 80});

    // Gradient rounded button body
    drawGradientRect(renderer, x, y, w, h, bgTop, bgBot);
    drawRoundedRect(renderer, x, y, w, h, 6, bgTop);
    drawRoundedRectOutline(renderer, x, y, w, h, 6,
        (m_hovered && enabled) ? Color{255, 215, 0, 180} : Color{100, 100, 100, 120});

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

Screen* ScreenManager::getScreen(AppState state) {
    auto it = m_screens.find(state);
    if (it != m_screens.end()) {
        return it->second.get();
    }
    return nullptr;
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
// AnimationSystem
// ============================================================================

float AnimationSystem::applyEasing(float t, Easing e) const {
    switch (e) {
        case Easing::Linear: return t;
        case Easing::EaseIn: return t * t;
        case Easing::EaseOut: return 1.0f - (1.0f - t) * (1.0f - t);
        case Easing::EaseInOut:
            if (t < 0.5f) return 2.0f * t * t;
            return 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }
    return t;
}

void AnimationSystem::addTween(Tween tween) {
    if (tween.target) {
        *tween.target = tween.from;
    }
    m_tweens.push_back(std::move(tween));
}

void AnimationSystem::update(float dt) {
    for (auto& tween : m_tweens) {
        if (tween.done) continue;
        tween.elapsed += dt;
        if (tween.elapsed >= tween.duration) {
            tween.elapsed = tween.duration;
            tween.done = true;
            if (tween.target) {
                *tween.target = tween.to;
            }
            if (tween.onComplete) {
                tween.onComplete();
            }
        } else {
            float t = tween.duration > 0.0f ? tween.elapsed / tween.duration : 1.0f;
            float eased = applyEasing(t, tween.easing);
            if (tween.target) {
                *tween.target = tween.from + (tween.to - tween.from) * eased;
            }
        }
    }
    m_tweens.erase(
        std::remove_if(m_tweens.begin(), m_tweens.end(),
            [](const Tween& t) { return t.done; }),
        m_tweens.end());
}

void AnimationSystem::clear() {
    m_tweens.clear();
}

bool AnimationSystem::hasActiveTweens() const {
    for (const auto& tween : m_tweens) {
        if (!tween.done) return true;
    }
    return false;
}

size_t AnimationSystem::activeCount() const {
    size_t count = 0;
    for (const auto& tween : m_tweens) {
        if (!tween.done) ++count;
    }
    return count;
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
    drawTextCentered(renderer, m_app->font(), "Phase 7 — Animations & Audio",
                     640, 150, 200, 180, 120);

    renderButtons(renderer, m_buttons);
}

// ============================================================================
// LobbyScreen
// ============================================================================

LobbyScreen::LobbyScreen(Application* app)
    : Screen(AppState::Lobby), m_app(app) {
    setupButtons();
}

void LobbyScreen::setupButtons() {
    m_buttons.clear();
    const int bw = 280;
    const int bh = 50;
    const int bx = (1280 - bw) / 2;
    int by = 280;
    const int gap = 16;

    auto addBtn = [&](const std::string& label, std::function<void()> cb) {
        m_buttons.push_back(std::make_unique<Button>(
            bx, by, bw, bh, label, std::move(cb), m_app->font()));
        m_buttons.back()->theme = &m_app->theme();
        by += bh + gap;
    };

    switch (m_state) {
        case LobbyState::ModeSelect: {
            addBtn("Local Multiplayer", [this]() {
                m_state = LobbyState::PlayerCount;
                m_app->localMPConfig().enabled = true;
                m_playerNames.clear();
                setupButtons();
            });
            addBtn("Back", [this]() {
                m_app->screenManager().transitionTo(AppState::MainMenu);
            });
            break;
        }
        case LobbyState::PlayerCount: {
            addBtn("2 Players", [this]() {
                m_selectedPlayerCount = 2;
                m_currentNameEntry = 0;
                m_playerNames.clear();
                m_currentNameInput.clear();
                m_state = LobbyState::NameEntry;
                SDL_StartTextInput();
                setupButtons();
            });
            addBtn("3 Players", [this]() {
                m_selectedPlayerCount = 3;
                m_currentNameEntry = 0;
                m_playerNames.clear();
                m_currentNameInput.clear();
                m_state = LobbyState::NameEntry;
                SDL_StartTextInput();
                setupButtons();
            });
            addBtn("4 Players", [this]() {
                m_selectedPlayerCount = 4;
                m_currentNameEntry = 0;
                m_playerNames.clear();
                m_currentNameInput.clear();
                m_state = LobbyState::NameEntry;
                SDL_StartTextInput();
                setupButtons();
            });
            addBtn("Back", [this]() {
                resetToModeSelect();
            });
            break;
        }
        case LobbyState::NameEntry: {
            // Only Next/Start button here; text input handled in handleEvent
            std::string btnLabel = (m_currentNameEntry >= m_selectedPlayerCount - 1) ? "Start Game" : "Next";
            addBtn(btnLabel, [this]() {
                std::string name = m_currentNameInput.empty()
                    ? ("Player " + std::to_string(m_currentNameEntry + 1))
                    : m_currentNameInput;
                m_playerNames.push_back(name);
                m_currentNameInput.clear();
                m_currentNameEntry++;
                if (m_currentNameEntry >= m_selectedPlayerCount) {
                    SDL_StopTextInput();
                    m_state = LobbyState::Ready;
                }
                setupButtons();
            });
            addBtn("Back", [this]() {
                if (m_currentNameEntry > 0) {
                    m_currentNameEntry--;
                    if (!m_playerNames.empty()) m_playerNames.pop_back();
                    m_currentNameInput.clear();
                } else {
                    SDL_StopTextInput();
                    resetToModeSelect();
                }
                setupButtons();
            });
            break;
        }
        case LobbyState::Ready: {
            addBtn("Start Game", [this]() {
                m_app->localMPConfig().playerCount = m_selectedPlayerCount;
                m_app->localMPConfig().playerNames = m_playerNames;
                m_app->localMPConfig().enabled = true;
                m_app->screenManager().transitionTo(AppState::InRound);
            });
            addBtn("Back", [this]() {
                m_state = LobbyState::PlayerCount;
                m_currentNameEntry = 0;
                m_playerNames.clear();
                m_currentNameInput.clear();
                setupButtons();
            });
            break;
        }
    }
}

void LobbyScreen::resetToModeSelect() {
    m_state = LobbyState::ModeSelect;
    m_selectedPlayerCount = 2;
    m_currentNameEntry = 0;
    m_playerNames.clear();
    m_currentNameInput.clear();
    m_app->localMPConfig().enabled = false;
    SDL_StopTextInput();
    setupButtons();
}

void LobbyScreen::onEnter() {
    resetToModeSelect();
    for (auto& btn : m_buttons) {
        btn->resetState();
    }
}

void LobbyScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) {
        SDL_StopTextInput();
        return;
    }

    if (m_state == LobbyState::NameEntry) {
        if (event.type == SDL_TEXTINPUT) {
            if (m_currentNameInput.size() < 16) {
                m_currentNameInput += event.text.text;
            }
            return;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_BACKSPACE && !m_currentNameInput.empty()) {
                m_currentNameInput.pop_back();
                return;
            }
            if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                // Trigger the Next/Start button
                for (auto& btn : m_buttons) {
                    if (btn->label == "Next" || btn->label == "Start Game") {
                        if (btn->onClick) btn->onClick();
                        return;
                    }
                }
                return;
            }
        }
    }

    if (routeButtons(event, m_buttons)) return;
}

void LobbyScreen::update(float /*deltaTime*/) {}

void LobbyScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderClear(renderer);

    switch (m_state) {
        case LobbyState::ModeSelect: {
            drawScreenLabel(renderer, m_app->font(), "Multiplayer");
            drawTextCentered(renderer, m_app->font(), "Choose game mode",
                             640, 180, 200, 200, 200);
            break;
        }
        case LobbyState::PlayerCount: {
            drawScreenLabel(renderer, m_app->font(), "Player Count");
            drawTextCentered(renderer, m_app->font(), "How many players?",
                             640, 180, 200, 200, 200);
            break;
        }
        case LobbyState::NameEntry: {
            drawScreenLabel(renderer, m_app->font(), "Player Names");
            std::string prompt = "Enter name for Player " + std::to_string(m_currentNameEntry + 1) +
                                 " of " + std::to_string(m_selectedPlayerCount);
            drawTextCentered(renderer, m_app->font(), prompt, 640, 180, 200, 200, 200);

            // Draw text input box
            fillRect(renderer, 440, 250, 400, 60, {60, 60, 70, 255});
            drawRect(renderer, 440, 250, 400, 60, {200, 200, 200, 255});

            std::string displayText = m_currentNameInput;
            if ((SDL_GetTicks() / 500) % 2 == 0) displayText += "_";
            if (displayText.empty()) displayText = "_";
            drawTextCentered(renderer, m_app->font(), displayText, 640, 280, 255, 255, 255);
            break;
        }
        case LobbyState::Ready: {
            drawScreenLabel(renderer, m_app->font(), "Ready to Start");
            int y = 180;
            for (size_t i = 0; i < m_playerNames.size(); ++i) {
                drawTextCentered(renderer, m_app->font(),
                                 std::to_string(i + 1) + ". " + m_playerNames[i],
                                 640, y, 255, 255, 255);
                y += 40;
            }
            break;
        }
    }

    renderButtons(renderer, m_buttons);
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

    if (m_app->localMPConfig().enabled) {
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
    m_lastActiveSeat = -1;
    m_localMultiplayer = m_app->localMPConfig().enabled;
    m_app->localMPConfig().enabled = false; // Reset for next transition

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
    m_currentBet = std::min(maxBet, std::min(bankroll, m_currentBet + 10));
    updateMessage();
}

void GameTableScreen::onDeal() {
    if (!m_round) return;
    m_round->placeBet(0, m_currentBet);

    // AI bets
    for (size_t i = 1; i < m_round->seats().size(); ++i) {
        int bet = m_aiControllers[i - 1]->chooseBet(*m_round, static_cast<int>(i));
        m_round->placeBet(static_cast<int>(i), bet);
    }

    m_round->advancePhase();
    animateInitialDealAllSeats();
    m_app->audioManager().playSFX("shuffle");
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
    m_needsUIRebuild = true;
}

void GameTableScreen::onStand() {
    if (!m_round) return;
    int seatIdx = m_round->currentSeatIndex();
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0 || seatIdx < 0) return;
    m_round->stand(seatIdx, handIdx);
    m_app->audioManager().playSFX("stand_click");
    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onDouble() {
    if (!m_round) return;
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
    m_needsUIRebuild = true;
}

void GameTableScreen::onSplit() {
    if (!m_round) return;
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
    m_needsUIRebuild = true;
}

void GameTableScreen::onSurrender() {
    if (!m_round) return;
    int seatIdx = m_round->currentSeatIndex();
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0 || seatIdx < 0) return;
    m_round->surrender(seatIdx, handIdx);
    m_app->audioManager().playSFX("surrender");
    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onInsuranceYes() {
    if (!m_round) return;
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
    m_lastActiveSeat = -1;
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
    int centerX = getSeatCenterX(seatIndex, totalSeats);
    int baseY = (seatIndex == 0) ? 380 : 520;

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

    updateAnimations(deltaTime);
    updateOutcomeTexts(deltaTime);

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

    renderButtons(renderer, m_buttons);
    drawEscHint(renderer, m_app->font());
    renderScreenFlash(renderer);
    renderOutcomeTexts(renderer);
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
