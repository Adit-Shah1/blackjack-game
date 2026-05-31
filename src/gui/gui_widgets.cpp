#include <blackjack/gui.h>

#include "gui_helpers.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

namespace blackjack {

// TextureManager
// ============================================================================

TextureManager::~TextureManager() {
    clear();
}

SDL_Texture* TextureManager::load(SDL_Renderer* renderer,
                                  const std::string& key,
                                  const std::string& filepath) {
    auto it = m_textures.find(key);
    if (it != m_textures.end()) {
        return it->second;
    }

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
    if (it != m_textures.end()) {
        return it->second;
    }
    return nullptr;
}

void TextureManager::clear() {
    for (auto& [k, texture] : m_textures) {
        (void)k;
        if (texture) {
            SDL_DestroyTexture(texture);
        }
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
    if (it != m_fonts.end()) {
        return it->second;
    }
    return nullptr;
}

void FontManager::registerFont(const std::string& key, TTF_Font* font) {
    m_fonts[key] = font;
}

void FontManager::clear() {
    for (auto& [k, font] : m_fonts) {
        (void)k;
        if (font) {
            TTF_CloseFont(font);
        }
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
    if (!visible) {
        return;
    }

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
    if (!enabled || !visible) {
        return false;
    }

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
    if (!visible || text.empty() || !m_font) {
        return;
    }

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
    if (!visible) {
        return;
    }

    if (backgroundColor.a > 0) {
        fillRect(renderer, bounds.x, bounds.y, bounds.w, bounds.h, backgroundColor);
    }

    for (auto& child : children) {
        if (child->visible) {
            child->render(renderer);
        }
    }
}

bool Panel::handleEvent(const SDL_Event& event) {
    if (!enabled || !visible) {
        return false;
    }
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if ((*it)->handleEvent(event)) {
            return true;
        }
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
        if (onValueChanged) {
            onValueChanged(value);
        }
    }
}

void Slider::render(SDL_Renderer* renderer) {
    if (!visible) {
        return;
    }

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
    if (!enabled || !visible) {
        return false;
    }

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
            startX + static_cast<int>(i) * (btnW + gap), y, btnW, btnH, buttonLabels[i],
            [this, i]() {
                if (onResult) {
                    onResult(static_cast<int>(i));
                }
            },
            m_font);
        if (theme) {
            btn->theme = theme;
        }
        m_buttons.push_back(std::move(btn));
    }
}

void Modal::render(SDL_Renderer* renderer) {
    if (!visible) {
        return;
    }

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
        if (btn->visible) {
            btn->render(renderer);
        }
    }
}

bool Modal::handleEvent(const SDL_Event& event) {
    if (!enabled || !visible) {
        return false;
    }
    for (auto& btn : m_buttons) {
        if (btn->handleEvent(event)) {
            return true;
        }
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
    if (!visible || isExpired()) {
        return;
    }

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


}  // namespace blackjack
