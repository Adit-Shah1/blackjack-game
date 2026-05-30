#pragma once

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

namespace blackjack {

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

// Forward declaration
class Application;

// ---------------------------------------------------------------------------
// Simple clickable button widget
// ---------------------------------------------------------------------------
struct Button {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    std::string label;
    std::function<void()> onClick;
    bool hovered = false;

    bool contains(int mx, int my) const;
    void render(SDL_Renderer* renderer, TTF_Font* font);
    bool handleEvent(const SDL_Event& event);
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

    ScreenManager m_screenManager;

    bool loadFont();

    static constexpr float FIXED_DT = 1.0f / 60.0f;
    float m_accumulator = 0.0f;
    uint64_t m_lastTime = 0;
};

// ---------------------------------------------------------------------------
// Stub Screens
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
    std::vector<Button> m_buttons;

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
    std::vector<Button> m_buttons;
};

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(Application* app);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<Button> m_buttons;
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
    std::vector<Button> m_buttons;
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
    std::vector<Button> m_buttons;
};

class AchievementsScreen : public Screen {
public:
    explicit AchievementsScreen(Application* app);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<Button> m_buttons;
};

}  // namespace blackjack
