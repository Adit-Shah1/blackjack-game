#include <blackjack/gui.h>
#include <blackjack/audio.h>

#include "gui_helpers.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace blackjack {

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


}  // namespace blackjack
