#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

#include <blackjack/round.h>
#include <blackjack/ai.h>
#include <blackjack/persistence.h>

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
class HostServer;
class GameClient;
class NetworkGameSession;
class NetworkClientSession;

// ---------------------------------------------------------------------------
// Local Multiplayer Configuration
// ---------------------------------------------------------------------------
struct LocalMultiplayerConfig {
    bool enabled = false;
    int playerCount = 1;
    std::vector<std::string> playerNames;
};

// ---------------------------------------------------------------------------
// Network Configuration
// ---------------------------------------------------------------------------
struct NetworkConfig {
    enum class Mode { None, Host, Client };
    Mode mode = Mode::None;
    std::string roomCode;
    std::string playerName;
    int port = 0;
    std::string hostAddress;
};

// ---------------------------------------------------------------------------
// AppState
// ---------------------------------------------------------------------------
enum class AppState {
    MainMenu,
    Lobby,
    NetworkCreate,
    NetworkJoin,
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
    void setLabel(const std::string& newLabel);
    void resetState();
    void setHovered(bool hovered);
    bool isHovered() const;

    ~Button();

private:
    TTF_Font* m_font = nullptr;
    bool m_hovered = false;
    bool m_pressed = false;

    SDL_Texture* m_labelTexture = nullptr;
    int m_labelTextureW = 0;
    int m_labelTextureH = 0;
    std::string m_cachedLabel;
    TTF_Font* m_cachedFont = nullptr;
    bool m_cachedEnabled = false;

    void invalidateLabelCache();
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
// Animation
// ---------------------------------------------------------------------------
enum class Easing { Linear, EaseIn, EaseOut, EaseInOut };

struct Tween {
    float* target = nullptr;
    float from = 0.0f;
    float to = 0.0f;
    float duration = 0.0f;
    float elapsed = 0.0f;
    Easing easing = Easing::EaseOut;
    bool done = false;
    std::function<void()> onComplete;
};

class AnimationSystem {
public:
    void addTween(Tween tween);
    void update(float dt);
    void clear();
    bool hasActiveTweens() const;
    size_t activeCount() const;
private:
    std::vector<Tween> m_tweens;
    float applyEasing(float t, Easing e) const;
};

// ---------------------------------------------------------------------------
// Screen base class
// ---------------------------------------------------------------------------
class Screen {
public:
    virtual ~Screen() = default;
    virtual void onEnter() { m_focusedButtonIndex = 0; }
    virtual void onExit() {}
    virtual void handleEvent(const SDL_Event& event) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;

    AppState getState() const { return m_state; }

protected:
    explicit Screen(AppState state) : m_state(state) {}
    AppState m_state;
    int m_focusedButtonIndex = -1;  // Keyboard navigation focus index for buttons
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
    Screen* getScreen(AppState state);

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
    LocalMultiplayerConfig& localMPConfig() { return m_localMPConfig; }
    NetworkConfig& networkConfig() { return m_networkConfig; }

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
    LocalMultiplayerConfig m_localMPConfig;
    NetworkConfig m_networkConfig;

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

    void onEnter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;

    enum class LobbyState { ModeSelect, PlayerCount, NameEntry, Ready };
    LobbyState m_state = LobbyState::ModeSelect;
    int m_selectedPlayerCount = 2;
    int m_currentNameEntry = 0;
    std::vector<std::string> m_playerNames;
    std::string m_currentNameInput;

    void setupButtons();
    void resetToModeSelect();
};

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(Application* app);
    void onEnter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
    std::vector<std::unique_ptr<Slider>> m_sliders;
};

class GameTableScreen : public Screen {
public:
    explicit GameTableScreen(Application* app);
    ~GameTableScreen() override;

    void onEnter() override;
    void onExit() override;
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

    void renderCard(SDL_Renderer* r, const Card& card, int x, int y, bool faceUp, float scaleX = 1.0f);
    void renderCardBack(SDL_Renderer* r, int x, int y, float scaleX = 1.0f);
    void renderDealer(SDL_Renderer* r);

    void renderStatus(SDL_Renderer* r);

    // Animation helpers
    struct FlyingCard {
        Card card;
        float x = 0.0f;
        float y = 0.0f;
        float startX = 0.0f;
        float startY = 0.0f;
        float targetX = 0.0f;
        float targetY = 0.0f;
        float elapsed = 0.0f;
        float delay = 0.0f;
        float duration = 0.2f;
        bool faceUp = true;
        bool done = false;
        int targetSeatIndex = -1;  // -1 for dealer, 0+ for player seat
        int targetHandIndex = -1;  // hand index within seat
        int targetCardIndex = -1;
    };
    std::vector<FlyingCard> m_flyingCards;

    struct ScreenFlash {
        Color color{255, 255, 255, 0};
        float duration = 0.0f;
        float elapsed = 0.0f;
        bool active = false;
    };
    ScreenFlash m_screenFlash;

    float m_holeCardFlipTimer = 0.0f;
    bool m_holeCardFlipping = false;

    // Dealer card count tracking for hit animations
    int m_lastDealerCardCount = 0;

    // Outcome text pop-in animation
    struct OutcomeText {
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
        float scale = 0.0f;
        float elapsed = 0.0f;
        float duration = 1.2f;
        Color color{255,255,255,255};
        bool done = false;
    };
    std::vector<OutcomeText> m_outcomeTexts;

    // AI opponents
    std::vector<std::unique_ptr<AIController>> m_aiControllers;
    float m_aiTurnTimer = 0.0f;
    float m_aiTurnDelay = 0.6f;
    bool m_aiInsuranceResolved = false;

    // Local multiplayer
    bool m_localMultiplayer = false;
    int m_currentBettingSeat = 0;
    bool m_passScreenActive = false;
    int m_lastActiveSeat = -1;
    std::vector<int> m_displayedBankrolls;

    // Bankroll ticker animation
    int m_displayedBankroll = 0;

    void spawnCardFly(const Card& card, float fromX, float fromY,
                      float toX, float toY, bool faceUp, float delay = 0.0f,
                      int targetSeatIndex = -1, int targetHandIndex = -1,
                      int targetCardIndex = -1);
    void spawnHoleCardFlip();
    void spawnScreenFlash(const Color& color, float duration);
    void updateAnimations(float deltaTime);
    void renderFlyingCards(SDL_Renderer* r);
    void renderScreenFlash(SDL_Renderer* r);
    void playOutcomeAudio();
    void getPlayerCardPosition(int handIndex, int cardIndex, int& outX, int& outY, int seatIndex = 0);
    void getDealerCardPosition(int cardIndex, int& outX, int& outY);

    void spawnOutcomeText(const std::string& text, float x, float y, const Color& color);
    void updateOutcomeTexts(float deltaTime);
    void renderOutcomeTexts(SDL_Renderer* r);
    void renderBetChips(SDL_Renderer* r);
    void renderChipStack(SDL_Renderer* r, int cx, int cy, int bet);
    void updateBankrollTicker(float deltaTime);

    // Multi-seat rendering
    void renderAllPlayers(SDL_Renderer* r);
    void renderPlayerSeat(SDL_Renderer* r, int seatIndex, int centerX, int baseY, bool isHuman);
    int getSeatCenterX(int seatIndex, int totalSeats) const;

    // Achievement & stats
    void initAchievements();
    void checkAchievements();
    void showToast(const std::string& message);
    void renderToasts(SDL_Renderer* r);
    void updateToasts(float deltaTime);

    // Help overlay
    void renderHelpOverlay(SDL_Renderer* r);
    bool handleHelpEvent(const SDL_Event& event);

    // AI helpers
    void executeAIAction(int seatIndex, int handIndex);
    void resolveAllAIInsurance();
    void setupAIOpponents();
    void animateInitialDealAllSeats();

    // Local multiplayer helpers
    void setupLocalMultiplayer();
    void onPlaceBet();
    void onPassReady();
    void renderPassScreen(SDL_Renderer* r);
    int getLocalMPCenterX(int seatIndex, int totalSeats) const;
    void updateAllBankrollTickers(float deltaTime);

    // Network multiplayer
    enum class NetworkMode { None, Host, Client };
    void setupNetworkHost();
    void setupNetworkClient();
    void processNetworkMessages(float deltaTime);
    void sendActionToHost(PlayerAction action);
    void broadcastStateToClients();
    void renderNetworkStatus(SDL_Renderer* r);

    NetworkMode m_networkMode = NetworkMode::None;
    std::unique_ptr<HostServer> m_hostServer;
    std::unique_ptr<NetworkClientSession> m_networkClientSession;
    int m_networkSeatIndex = -1;   // seat assigned to this client (-1 for host)
    std::string m_networkStatusMsg;

    // Stats & achievements
    PlayerStats m_stats;
    std::vector<Achievement> m_achievements;
    int m_sessionBlackjacks = 0;
    int m_sessionHandsWithoutBust = 0;
    std::vector<std::unique_ptr<Toast>> m_toasts;

    // Help overlay
    bool m_showHelpOverlay = false;

    // ESC confirmation modal
    std::unique_ptr<Modal> m_quitConfirmModal;
};

class NetworkCreateScreen : public Screen {
public:
    explicit NetworkCreateScreen(Application* app);
    ~NetworkCreateScreen() override;

    void onEnter() override;
    void onExit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

    HostServer* hostServer() const { return m_hostServer.get(); }
    std::unique_ptr<HostServer> takeHostServer() { return std::move(m_hostServer); }

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
    std::unique_ptr<HostServer> m_hostServer;
    std::string m_roomCode;
    std::vector<std::string> m_connectedPlayers;
    bool m_gameStarted = false;
    float m_pollTimer = 0.0f;

    void setupButtons();
    void refreshPlayerList();
};

class NetworkJoinScreen : public Screen {
public:
    explicit NetworkJoinScreen(Application* app);
    ~NetworkJoinScreen() override;

    void onEnter() override;
    void onExit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

    GameClient* gameClient() const { return m_gameClient.get(); }
    std::unique_ptr<GameClient> takeGameClient() { return std::move(m_gameClient); }

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
    std::unique_ptr<GameClient> m_gameClient;

    enum class JoinState { EnterCode, Connecting, Waiting, Ready, Error };
    JoinState m_state = JoinState::EnterCode;
    std::string m_roomCodeInput;
    std::string m_statusMessage;
    float m_connectTimer = 0.0f;
    bool m_gameStarted = false;

    void setupButtons();
    void tryConnect();
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

    void onEnter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
    std::vector<Achievement> m_achievements;
    float m_scrollY = 0.0f;
    bool m_draggingScroll = false;
    int m_lastMouseY = 0;

    void loadAchievements();
    void renderAchievementCard(SDL_Renderer* r, int x, int y, int w, int h,
                                const Achievement& ach, int progress, int target);
};

class TutorialScreen : public Screen {
public:
    explicit TutorialScreen(Application* app);

    void onEnter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

    enum class Step {
        Welcome,
        Objective,
        Betting,
        Deal,
        HitStand,
        DoubleSplit,
        Insurance,
        WrapUp
    };

private:
    Application* m_app;
    std::vector<std::unique_ptr<Button>> m_buttons;
    Step m_step = Step::Welcome;

    void setupButtons();
    void advanceStep();
    void previousStep();
    void renderStep(SDL_Renderer* r);
    void renderMiniTable(SDL_Renderer* r);
};

}  // namespace blackjack
