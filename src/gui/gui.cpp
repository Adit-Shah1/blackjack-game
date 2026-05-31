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
    m_flyingCards.clear();
    m_holeCardFlipping = false;
    m_holeCardFlipTimer = 0.0f;
    m_screenFlash.active = false;
    m_lastDealerCardCount = 0;
    m_outcomeTexts.clear();
    m_displayedBankroll = m_round->seats()[0].bankroll;
    rebuildUI();
    updateMessage();
    m_app->audioManager().playAmbient("casino_ambient");
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

    // Animate initial deal: 4 cards from deck
    float deckX = 640.0f - 35.0f;
    float deckY = 360.0f - 49.0f;

    const auto& playerHand = m_round->seats()[0].hands[0].hand;
    if (playerHand.cardCount() >= 2) {
        int px1, py1, px2, py2;
        getPlayerCardPosition(0, 0, px1, py1);
        getPlayerCardPosition(0, 1, px2, py2);
        spawnCardFly(playerHand.cards()[0], deckX, deckY, static_cast<float>(px1), static_cast<float>(py1), true, 0.0f, 0, 0);
        spawnCardFly(playerHand.cards()[1], deckX, deckY, static_cast<float>(px2), static_cast<float>(py2), true, 0.1f, 0, 1);
    }

    const auto& dealer = m_round->dealer();
    if (dealer.hand.cardCount() >= 1) {
        int dx, dy;
        getDealerCardPosition(0, dx, dy);
        spawnCardFly(dealer.hand.cards()[0], deckX, deckY, static_cast<float>(dx), static_cast<float>(dy), true, 0.05f, -1, 0);
        // Hole card is dealt face down — no fly animation for it
    }

    m_lastDealerCardCount = m_round->dealer().hand.cardCount();
    m_app->audioManager().playSFX("shuffle");
    m_needsUIRebuild = true;
}

void GameTableScreen::onHit() {
    if (!m_round) return;
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0) return;
    int prevCount = m_round->seats()[0].hands[handIdx].hand.cardCount();
    m_round->hit(0, handIdx);
    int newCount = m_round->seats()[0].hands[handIdx].hand.cardCount();
    if (newCount > prevCount) {
        int tx, ty;
        getPlayerCardPosition(handIdx, newCount - 1, tx, ty);
        spawnCardFly(m_round->seats()[0].hands[handIdx].hand.cards().back(),
                     640.0f - 35.0f, 360.0f - 49.0f,
                     static_cast<float>(tx), static_cast<float>(ty), true, 0.0f, handIdx, newCount - 1);
        m_app->audioManager().playSFX("card_deal");
    }
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
    m_app->audioManager().playSFX("stand_click");
    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onDouble() {
    if (!m_round) return;
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0) return;
    int prevCount = m_round->seats()[0].hands[handIdx].hand.cardCount();
    m_round->doubleDown(0, handIdx);
    int newCount = m_round->seats()[0].hands[handIdx].hand.cardCount();
    if (newCount > prevCount) {
        int tx, ty;
        getPlayerCardPosition(handIdx, newCount - 1, tx, ty);
        spawnCardFly(m_round->seats()[0].hands[handIdx].hand.cards().back(),
                     640.0f - 35.0f, 360.0f - 49.0f,
                     static_cast<float>(tx), static_cast<float>(ty), true, 0.0f, handIdx, newCount - 1);
        m_app->audioManager().playSFX("card_deal");
    }
    m_app->audioManager().playSFX("chip_stack");
    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onSplit() {
    if (!m_round) return;
    int handIdx = m_round->currentHandIndex();
    if (handIdx < 0) return;
    m_round->split(0, handIdx);

    // Animate new cards dealt to both split hands
    const auto& hands = m_round->seats()[0].hands;
    for (size_t h = handIdx; h < hands.size() && h <= static_cast<size_t>(handIdx) + 1; ++h) {
        int cardCount = hands[h].hand.cardCount();
        if (cardCount >= 2) {
            int tx, ty;
            getPlayerCardPosition(static_cast<int>(h), cardCount - 1, tx, ty);
            spawnCardFly(hands[h].hand.cards().back(),
                         640.0f - 35.0f, 360.0f - 49.0f,
                         static_cast<float>(tx), static_cast<float>(ty), true,
                         static_cast<float>(h - handIdx) * 0.05f, static_cast<int>(h), cardCount - 1);
        }
    }
    m_app->audioManager().playSFX("card_deal");
    m_app->audioManager().playSFX("chip_stack");

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
    m_app->audioManager().playSFX("surrender");
    m_round->nextHand();
    m_round->advancePhase();
    m_needsUIRebuild = true;
}

void GameTableScreen::onInsuranceYes() {
    if (!m_round) return;
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
    m_displayedBankroll = m_round->seats()[0].bankroll;
    m_needsUIRebuild = true;
    rebuildUI();
    updateMessage();
    m_app->audioManager().playSFX("new_round");
}

void GameTableScreen::spawnCardFly(const Card& card, float fromX, float fromY,
                                    float toX, float toY, bool faceUp, float delay,
                                    int targetHandIndex, int targetCardIndex) {
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

void GameTableScreen::getPlayerCardPosition(int handIndex, int cardIndex, int& outX, int& outY) {
    const int cw = 70;
    const int overlap = 20;
    int cardCount = m_round->seats()[0].hands[handIndex].hand.cardCount();
    int totalWidth = cardCount * cw - (cardCount - 1) * overlap;
    int startX = (1280 - totalWidth) / 2;
    outX = startX + cardIndex * (cw - overlap);
    outY = 380 + handIndex * 110;
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

    updateAnimations(deltaTime);

    RoundPhase currentPhase = m_round->phase();

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
                spawnScreenFlash({255, 215, 0, 120}, 1.0f);
            } else if (hasWin) {
                spawnScreenFlash({255, 215, 0, 80}, 0.5f);
            } else if (hasBust) {
                spawnScreenFlash({212, 0, 0, 80}, 0.5f);
            } else if (hasLose) {
                spawnScreenFlash({212, 0, 0, 40}, 0.3f);
            }

            // Outcome text pop-in
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
                    int tx = 0, ty = 0;
                    getPlayerCardPosition(static_cast<int>(i), 0, tx, ty);
                    ty -= 30;
                    spawnOutcomeText(text, static_cast<float>(tx) + 35.0f, static_cast<float>(ty), color);
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
                             static_cast<float>(tx), static_cast<float>(ty), true, 0.0f, -1, i);
                m_app->audioManager().playSFX("card_deal");
            }
        }
        if (dealerCardCount > 0) {
            m_lastDealerCardCount = dealerCardCount;
        }
    }

    updateOutcomeTexts(deltaTime);
    updateBankrollTicker(deltaTime);

    switch (currentPhase) {
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
            if (fc.targetHandIndex == -1 && fc.targetCardIndex == i && !fc.done) {
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

        // Highlight active hand during PlayerTurns with pulsing glow
        if (m_round->phase() == RoundPhase::PlayerTurns &&
            static_cast<int>(h) == currentHandIdx && !handState.finished) {
            uint32_t ticks = SDL_GetTicks();
            float pulse = (std::sin(ticks * 0.005f) + 1.0f) * 0.5f; // 0 to 1
            uint8_t alpha = static_cast<uint8_t>(120 + pulse * 80); // 120 to 200
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
                if (fc.targetHandIndex == static_cast<int>(h) &&
                    fc.targetCardIndex == i && !fc.done) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

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
    renderPlayer(renderer);
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

    int bet = 0;
    if (m_round->phase() == RoundPhase::WaitingForBets) {
        bet = m_currentBet;
    } else if (m_round->phase() != RoundPhase::RoundComplete) {
        bet = m_round->seats()[0].hands[0].bet.mainBet;
    }
    if (bet <= 0) return;

    int cx = 640;
    int cy = 330;
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

    std::string betText = "$" + std::to_string(
        (m_round->phase() == RoundPhase::WaitingForBets) ? m_currentBet : m_round->seats()[0].hands[0].bet.mainBet);
    drawTextCentered(r, m_app->font(), betText, cx, cy + 18, 255, 255, 255);
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
