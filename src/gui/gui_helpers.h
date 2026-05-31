#pragma once

// Internal GUI helper declarations — shared across all GUI source files

#include <blackjack/gui.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <SDL.h>
#include <SDL_ttf.h>

namespace blackjack {

SDL_Color toSDL(const Color& c);
void fillRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& c);
void drawRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& c);
void drawText(SDL_Renderer* renderer, TTF_Font* font,
              const std::string& text, int x, int y,
              unsigned char r, unsigned char g, unsigned char b);
void drawTextCentered(SDL_Renderer* renderer, TTF_Font* font,
                      const std::string& text, int cx, int cy,
                      unsigned char r, unsigned char g, unsigned char b);
void drawScreenLabel(SDL_Renderer* renderer, TTF_Font* font,
                     const std::string& title);
void drawEscHint(SDL_Renderer* renderer, TTF_Font* font);
std::string rankDisplay(const Card& card);
bool isRedSuit(const Card& card);
bool handleEscToMenu(const SDL_Event& event, Application* app);
void renderButtons(SDL_Renderer* renderer,
                   std::vector<std::unique_ptr<Button>>& buttons);
bool routeButtons(const SDL_Event& event,
                  std::vector<std::unique_ptr<Button>>& buttons);
void drawRoundedRect(SDL_Renderer* r, int x, int y, int w, int h, int rad, const Color& c);
void drawRoundedRectOutline(SDL_Renderer* r, int x, int y, int w, int h, int rad, const Color& c);
void drawShadow(SDL_Renderer* r, int x, int y, int w, int h, int offset, const Color& c);
void drawGradientRect(SDL_Renderer* r, int x, int y, int w, int h, const Color& top, const Color& bot);
void drawBitmap(SDL_Renderer* r, int x, int y, int scale,
                const char* const bitmap[], int rows, int cols,
                const Color& color);
void drawSuitSymbol(SDL_Renderer* r, Suit suit, int cx, int cy, int scale, const Color& color);

}  // namespace blackjack
