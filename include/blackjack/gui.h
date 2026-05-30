#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

#include <blackjack/round.h>

// SDL forward declarations
struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;
struct TTF_Font;
struct SDL_Texture;

namespace blackjack {

// Forward declarations
class AudioManager;
class Application;

// ---------------------------------------------------------------------------
// AppState
// ---------------------------------------------------------------------------
enum class AppState {
    MainMenu,
    Lobby,
    Settings,
    Tutorial,
    InRound,
    RoundResults,
    Achievements,
    Loading,
    Exiting
};

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool contains(int mx, int my) const {
        return mx >= x && mx < x + w && my >= y && my < y + h;
    }
};

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------
struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------
struct Theme {
    Color tableFelt{45, 90, 39, 255};
    Color cardFace{255, 255, 255, 255};
    Color cardBack{26, 58, 110, 255};
    Color redSuit{212, 0, 0, 255};
    Color blackSuit{26, 26, 26, 255};
    Color goldAccent{255, 215, 0, 255};
    Color panelBg{0, 0, 0, 170};
    Color buttonNormal{60, 60, 60, 255};
    Color buttonHover{80, 80, 80, 255};
    Color buttonPressed{40, 40, 40, 255};
    Color textPrimary{255, 255, 255, 255};
    Color textSecondary{180, 180, 180, 255};
    Color chipWhite{255, 255, 255, 255};
    Color chipRed{212, 0, 0, 255};
    Color chipBlue{0, 102, 204, 255};
    Color chipGreen{0, 170, 68, 255};
    Color chipBlack{26, 26, 26, 255};
};

// ---------------------------------------------------------------------------
// TextureManager
// ---------------------------------------------------------------------------
class TextureManager {
public:
    ~TextureManager();

    SDL_Texture* load(SDL_Renderer* renderer, const std::string& key,
                      const std::string& filepath);
    SDL_Texture* get(const std::string& key);
    void clear();

private:
    std::unordered_map<std::string, SDL_Texture*> m_textures;
};

// ---------------------------------------------------------------------------
// FontManager
// ---------------------------------------------------------------------------
class FontManager {
public:
    ~FontManager();

    TTF_Font* load(const std::string& key, const std::string& filepath, int size);
    TTF_Font* get(const std::string& key);
    void registerFont(const std::string& key, TTF_Font* font);
    void clear();

private:
    std::unordered_map<std::string, TTF_Font*> m_fonts;
};

// ---------------------------------------------------------------------------
// Widget base class
// ---------------------------------------------------------------------------
class Widget {
public:
    virtual ~Widget() = default;
    virtual void render(SDL_Renderer* renderer) = 0;
    virtual bool handleEvent(const SDL_Event& event) = 0;

    bool contains(int mx, int my) const {
        return bounds.contains(mx, my);
    }

    Rect bounds;
    bool visible = true;
    bool enabled = true;
    const Theme* theme = nullptr;
};

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
class Button : public Widget {
public:
    Button(int x, int y, int w, int h, const std::string& label,
           std::function<void()> onClick, TTF_Font* font);

    void render(SDL_Renderer* renderer) override;
    bool handleEvent(const SDL_Event& event) override;

    std::string label;
    std::function<void()> onClick;

    void setFont(TTF_Font* font);
    void resetState();

private:
    TTF_Font* m_font = nullptr;
    bool m_hovered = false;
    bool m_pressed = false;
};

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------
class Label : public Widget {
public:
    Label(int x, int y, const std::string& text, TTF_Font* font);

    void render(SDL_Renderer* renderer) override;
    bool handleEvent(const SDL_Event& event) override;

    std::string text;
    Color color;

    void setFont(TTF_Font* font);

private:
    TTF_Font* m_font = nullptr;
};

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------
class Panel : public Widget {
public:
    Panel(int x, int y, int w, int h);

    void render(SDL_Renderer* renderer) override;
    bool handleEvent(const SDL_Event& event) override;

    Color backgroundColor;
    std::vector<std::unique_ptr<Widget>> children;

    void addWidget(std::unique_ptr<Widget> widget);
};

// ---------------------------------------------------------------------------
// Slider
// ---------------------------------------------------------------------------
class Slider : public Widget {
public:
    Slider(int x, int y, int w, int h, int minVal, int maxVal, int initialVal);

    void render(SDL_Renderer* renderer) override;
    bool handleEvent(const SDL_Event& event) override;

    int minValue = 0;
    int maxValue = 100;
    int value = 50;
    std::function<void(int)> onValueChanged;

private:
    bool m_dragging = false;
    int m_minVal = 0;
    int m_maxVal = 100;

    void updateValueFromMouse(int mx);
};

// ---------------------------------------------------------------------------
// Modal
// ---------------------------------------------------------------------------
class Modal : public Widget {
public:
    Modal(int x, int y, int w, int h,
          const std::string& title, const std::string& message,
          TTF_Font* font);

    void render(SDL_Renderer* renderer) override;
    bool handleEvent(const SDL_Event& event) override;

    std::string title;
    std::string message;
    std::vector<std::string> buttonLabels;
    std::function<void(int)> onResult; // index of clicked button

    void rebuildButtons();

private:
    TTF_Font* m_font = nullptr;
    std::vector<std::unique_ptr<Button>> m_buttons;
};

// ---------------------------------------------------------------------------
// Toast
// ---------------------------------------------------------------------------
class Toast : public Widget {
public:
    Toast(int x, int y, const std::string& message, TTF_Font* font,
          float duration = 2.0f);

    void render(SDL_Renderer* renderer) override;
    bool handleEvent(const SDL_Event& event) override;

    void update(float deltaTime);
    bool isExpired() const;

    std::string message;
    float duration = 2.0f;
    float elapsed = 0.0f;

private:
    TTF_Font* m_font = nullptr;
};

// ---------------------------------------------------------------------------
// Screen base class
// ---------------------------------------------------------------------------
class Screen {
public:
    virtual ~Screen() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void handleEvent(const SDL_Event& event) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;

    AppState getState() const { return m_state; }

protected:
    explicit Screen(AppState state) : m_state(state) {}
    AppState m_state;
};

// ---------------------------------------------------------------------------
// Screen Manager
// ---------------------------------------------------------------------------
class ScreenManager {
public:
    ScreenManager();

    void registerScreen(AppState state, std::unique_ptr<Screen> screen);
    void transitionTo(AppState state);
    AppState currentState() const { return m_currentState; }

    void handleEvent(const SDL_Event& event);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);

private:
    std::unordered_map<AppState, std::unique_ptr<Screen>> m_screens;
    AppState m_currentState = AppState::MainMenu;
    Screen* m_currentScreen = nullptr;
};

// ---------------------------------------------------------------------------
// Application (RAII SDL wrapper)
// ---------------------------------------------------------------------------
class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    bool init();
    void run();
    void quit();

    SDL_Renderer* renderer() { return m_renderer; }
    ScreenManager& screenManager() { return m_screenManager; }
    TTF_Font* font() { return m_font; }

    TextureManager& textureManager() { return m_textureManager; }
    FontManager& fontManager() { return m_fontManager; }
    AudioManager& audioManager() { return *m_audioManager; }
    const Theme& theme() const { return m_theme; }

    // Render text centered at (x, y). If font is missing, this is a no-op.
    void renderText(const std::string& text, int x, int y,
                    unsigned char r = 255, unsigned char g = 255, unsigned char b = 255);

private:
    std::string m_title;
    int m_width;
    int m_height;
    bool m_running = false;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    TTF_Font* m_font = nullptr;

    TextureManager m_textureManager;
    FontManager m_fontManager;
    std::unique_ptr<AudioManager> m_audioManager;
    Theme m_theme;

    ScreenManager m_screenManager;

    bool loadFont();

    static constexpr float FIXED_DT = 1.0f / 60.0f;
    float m_accumulator = 0.0f;
    uint64_t m_lastTime = 0;
};

// ---------------------------------------------------------------------------
// Concrete Screens
// ---------------------------------------------------------------------------

class MainMenuScreen : public Screen {
public:
    explicit MainMenuScreen(Application* app);

    void onEnter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;

    void setupButtons();
};

class LobbyScreen : public Screen {
public:
    explicit LobbyScreen(Application* app);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
};

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(Application* app);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
};

class GameTableScreen : public Screen {
public:
    explicit GameTableScreen(Application* app);
    ~GameTableScreen() override;

    void onEnter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::unique_ptr<RoundState> m_round;
    RoundPhase m_lastPhase = RoundPhase::RoundComplete;
    int m_currentBet = 100;
    std::vector<std::unique_ptr<Button>> m_buttons;
    float m_autoAdvanceTimer = 0.0f;
    bool m_needsUIRebuild = false;
    std::string m_message;
    std::string m_subMessage;

    void rebuildUI();
    void updateMessage();

    void onBetMinus();
    void onBetPlus();
    void onDeal();
    void onHit();
    void onStand();
    void onDouble();
    void onSplit();
    void onSurrender();
    void onInsuranceYes();
    void onInsuranceNo();
    void onNextRound();

    void renderCard(SDL_Renderer* r, const Card& card, int x, int y, bool faceUp);
    void renderCardBack(SDL_Renderer* r, int x, int y);
    void renderDealer(SDL_Renderer* r);
    void renderPlayer(SDL_Renderer* r);
    void renderStatus(SDL_Renderer* r);
};

class RoundResultsScreen : public Screen {
public:
    explicit RoundResultsScreen(Application* app);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
};

class AchievementsScreen : public Screen {
public:
    explicit AchievementsScreen(Application* app);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
};

}  // namespace blackjack
