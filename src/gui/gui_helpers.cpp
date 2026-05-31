#include "gui_helpers.h"

namespace blackjack {

// Helpers
// ============================================================================

SDL_Color toSDL(const Color& c) {
    SDL_Color sc = {};
    sc.r = c.r;
    sc.g = c.g;
    sc.b = c.b;
    sc.a = c.a;
    return sc;
}

void fillRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void drawRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderDrawRect(r, &rect);
}

void drawText(SDL_Renderer* renderer, TTF_Font* font,
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

void drawTextCentered(SDL_Renderer* renderer, TTF_Font* font,
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

void drawScreenLabel(SDL_Renderer* renderer, TTF_Font* font,
                            const std::string& title) {
    drawTextCentered(renderer, font, title, 640, 80, 255, 255, 255);
}

void drawEscHint(SDL_Renderer* renderer, TTF_Font* font) {
    drawTextCentered(renderer, font, "Press ESC for Main Menu", 640, 660, 180, 180, 180);
}

std::string rankDisplay(const Card& card) {
    char c = card.rankChar();
    if (c == 'T') return "10";
    return std::string(1, c);
}

bool isRedSuit(const Card& card) {
    return card.suit() == Suit::Hearts || card.suit() == Suit::Diamonds;
}

bool handleEscToMenu(const SDL_Event& event, Application* app) {
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        app->screenManager().transitionTo(AppState::MainMenu);
        return true;
    }
    return false;
}

void renderButtons(SDL_Renderer* renderer,
                          std::vector<std::unique_ptr<Button>>& buttons) {
    for (auto& btn : buttons) {
        if (btn->visible) btn->render(renderer);
    }
}

bool routeButtons(const SDL_Event& event,
                         std::vector<std::unique_ptr<Button>>& buttons) {
    for (auto& btn : buttons) {
        if (btn->handleEvent(event)) return true;
    }
    return false;
}

void drawRoundedRect(SDL_Renderer* r, int x, int y, int w, int h, int rad, const Color& c) {
    fillRect(r, x + rad, y, w - rad*2, h, c);
    fillRect(r, x, y + rad, w, h - rad*2, c);
    fillRect(r, x + rad, y + rad, rad, rad, c);
    fillRect(r, x + w - rad*2, y + rad, rad, rad, c);
    fillRect(r, x + rad, y + h - rad*2, rad, rad, c);
    fillRect(r, x + w - rad*2, y + h - rad*2, rad, rad, c);
}

void drawRoundedRectOutline(SDL_Renderer* r, int x, int y, int w, int h, int rad, const Color& c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(r, x + rad, y, x + w - rad - 1, y);
    SDL_RenderDrawLine(r, x + rad, y + h - 1, x + w - rad - 1, y + h - 1);
    SDL_RenderDrawLine(r, x, y + rad, x, y + h - rad - 1);
    SDL_RenderDrawLine(r, x + w - 1, y + rad, x + w - 1, y + h - rad - 1);
}

void drawShadow(SDL_Renderer* r, int x, int y, int w, int h, int offset, const Color& c) {
    fillRect(r, x + offset, y + offset, w, h, c);
}

void drawGradientRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& top, const Color& bot) {
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

void drawBitmap(SDL_Renderer* r, int x, int y, int scale,
                       const char* const bitmap[], int rows, int cols,
                       const Color& color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int row = 0; row < rows; ++row) {
        const char* line = bitmap[row];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        for (int col = 0; col < cols; ++col) {
            if (line[col] == '#') {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                SDL_Rect pixel{ x + col * scale, y + row * scale, scale, scale };
                SDL_RenderFillRect(r, &pixel);
            }
        }
    }
}

bool handleButtonNavigation(const SDL_Event& event,
                            std::vector<std::unique_ptr<Button>>& buttons,
                            int& focusedIndex) {
    if (event.type != SDL_KEYDOWN) {
        return false;
    }

    // Only process navigation/activation keys
    switch (event.key.keysym.sym) {
        case SDLK_TAB: case SDLK_DOWN: case SDLK_RIGHT:
        case SDLK_UP: case SDLK_LEFT:
        case SDLK_SPACE: case SDLK_RETURN: case SDLK_KP_ENTER:
            break;
        default:
            return false;
    }

    int visibleCount = 0;
    for (const auto& btn : buttons) {
        if (btn->visible && btn->enabled) ++visibleCount;
    }
    if (visibleCount == 0) return false;

    // Ensure focusedIndex is within valid visible range
    int clampedFocus = -1;
    int idx = 0;
    for (const auto& btn : buttons) {
        if (btn->visible && btn->enabled) {
            if (clampedFocus < 0) clampedFocus = idx;
            if (idx == focusedIndex) {
                clampedFocus = focusedIndex;
                break;
            }
        }
        ++idx;
    }
    if (clampedFocus < 0) clampedFocus = 0;
    focusedIndex = clampedFocus;

    // Clear all hover states first
    for (auto& btn : buttons) {
        btn->setHovered(false);
    }

    switch (event.key.keysym.sym) {
        case SDLK_TAB:
        case SDLK_DOWN:
        case SDLK_RIGHT: {
            // Find next visible/enabled button after focusedIndex
            int start = focusedIndex;
            int count = static_cast<int>(buttons.size());
            for (int i = 1; i <= count; ++i) {
                int next = (start + i) % count;
                if (buttons[next]->visible && buttons[next]->enabled) {
                    focusedIndex = next;
                    buttons[next]->setHovered(true);
                    return true;
                }
            }
            return true;
        }
        case SDLK_UP:
        case SDLK_LEFT: {
            int start = focusedIndex;
            int count = static_cast<int>(buttons.size());
            for (int i = 1; i <= count; ++i) {
                int prev = (start - i + count) % count;
                if (buttons[prev]->visible && buttons[prev]->enabled) {
                    focusedIndex = prev;
                    buttons[prev]->setHovered(true);
                    return true;
                }
            }
            return true;
        }
        case SDLK_SPACE:
        case SDLK_RETURN:
        case SDLK_KP_ENTER: {
            if (focusedIndex >= 0 && focusedIndex < static_cast<int>(buttons.size()) &&
                buttons[focusedIndex]->visible && buttons[focusedIndex]->enabled &&
                buttons[focusedIndex]->onClick) {
                buttons[focusedIndex]->onClick();
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

void drawSuitSymbol(SDL_Renderer* r, Suit suit, int cx, int cy, int scale, const Color& color) {
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
        case Suit::Hearts:   bitmap = HEART;   break;  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        case Suit::Diamonds: bitmap = DIAMOND; break;  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        case Suit::Spades:   bitmap = SPADE;   break;  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        case Suit::Clubs:    bitmap = CLUB;    break;  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
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



}  // namespace blackjack
