#include <blackjack/gui.h>

#include "gui_helpers.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace blackjack {

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

void AchievementsScreen::onEnter() {
    m_buttons.clear();
    m_scrollY = 0.0f;
    loadAchievements();

    auto backBtn = std::make_unique<Button>(
        40, 640, 140, 44, "Back",
        [this]() { m_app->screenManager().transitionTo(AppState::MainMenu); },
        m_app->font());
    backBtn->theme = &m_app->theme();
    m_buttons.push_back(std::move(backBtn));
}

void AchievementsScreen::loadAchievements() {
    // Default achievement definitions
    m_achievements = {
        {AchievementId::FirstBlackjack, "First Blackjack", "Get your first natural blackjack.", false},
        {AchievementId::ThousandBankroll, "High Roller", "Reach a bankroll of $1,000.", false},
        {AchievementId::FiveBlackjacksSession, "Lucky Streak", "Get 5 blackjacks in one session.", false},
        {AchievementId::TenWinStreak, "Unstoppable", "Win 10 games in a row.", false},
        {AchievementId::HundredHands, "Veteran", "Play 100 hands total.", false},
        {AchievementId::NeverBustSession, "Iron Stomach", "Play 20 hands without busting.", false},
    };

    // Try to load saved progress from a simple achievements file
    try {
        std::filesystem::path path = std::filesystem::path(
            std::getenv("HOME") ? std::getenv("HOME") : ".") / ".local/share/BlackjackGame/achievements.json";
#ifdef __APPLE__
        path = std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / "Library/Application Support/BlackjackGame/achievements.json";
#endif
        if (std::filesystem::exists(path)) {
            std::ifstream file(path);
            if (file.is_open()) {
                nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
                if (!j.is_discarded() && j.contains("achievements")) {
                    auto saved = j["achievements"].get<std::vector<Achievement>>();
                    for (auto& ach : m_achievements) {
                        for (const auto& s : saved) {
                            if (s.id == ach.id) {
                                ach.unlocked = s.unlocked;
                                break;
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {  // NOLINT(bugprone-empty-catch)
        // Use defaults on any error
    }
}

void AchievementsScreen::renderAchievementCard(SDL_Renderer* r, int x, int y, int w, int h,
                                                const Achievement& ach, int progress, int target) {
    Color bg = ach.unlocked ? Color{45, 55, 45, 255} : Color{35, 35, 40, 255};
    Color border = ach.unlocked ? Color{0, 200, 80, 255} : Color{80, 80, 90, 255};
    drawShadow(r, x, y, w, h, 4, {0, 0, 0, 80});
    drawRoundedRect(r, x, y, w, h, 8, bg);
    drawRoundedRectOutline(r, x, y, w, h, 8, border);

    // Icon area (left side)
    int iconX = x + 16;
    int iconY = y + h / 2 - 20;
    if (ach.unlocked) {
        // Gold star-ish shape
        fillRect(r, iconX, iconY, 40, 40, {255, 215, 0, 255});
        drawRoundedRectOutline(r, iconX, iconY, 40, 40, 4, {255, 255, 150, 255});
        drawTextCentered(r, m_app->font(), "*", iconX + 20, iconY + 18, 0, 0, 0);
    } else {
        fillRect(r, iconX, iconY, 40, 40, {60, 60, 65, 255});
        drawRoundedRectOutline(r, iconX, iconY, 40, 40, 4, {100, 100, 110, 255});
        drawTextCentered(r, m_app->font(), "L", iconX + 20, iconY + 18, 150, 150, 150);
    }

    // Name
    drawText(r, m_app->font(), ach.name, x + 70, y + 12,
             ach.unlocked ? 255 : 180, ach.unlocked ? 255 : 180, ach.unlocked ? 255 : 180);

    // Description
    drawText(r, m_app->font(), ach.description, x + 70, y + 38, 180, 180, 180);

    // Progress bar (if not unlocked and target > 0)
    if (!ach.unlocked && target > 0) {
        int barY = y + h - 18;
        int barW = w - 86;
        fillRect(r, x + 70, barY, barW, 8, {50, 50, 55, 255});
        int fillW = static_cast<int>(barW * std::min(1.0f, static_cast<float>(progress) / target));
        fillRect(r, x + 70, barY, fillW, 8, {0, 170, 68, 255});
        std::string prog = std::to_string(progress) + "/" + std::to_string(target);
        drawText(r, m_app->font(), prog, x + 70 + barW + 6, barY - 4, 150, 150, 150);
    }

    // Unlocked badge
    if (ach.unlocked) {
        drawText(r, m_app->font(), "UNLOCKED", x + w - 90, y + h - 22, 0, 200, 80);
    }
}

void AchievementsScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    if (routeButtons(event, m_buttons)) return;

    // Simple scroll handling
    if (event.type == SDL_MOUSEWHEEL) {
        m_scrollY += event.wheel.y * -30;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_draggingScroll = true;
        m_lastMouseY = event.button.y;
    }
    if (event.type == SDL_MOUSEMOTION && m_draggingScroll) {
        m_scrollY += event.motion.yrel;
        m_lastMouseY = event.motion.y;
    }
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        m_draggingScroll = false;
    }
}

void AchievementsScreen::update(float /*deltaTime*/) {
    // Clamp scroll
    int contentH = static_cast<int>(m_achievements.size()) * 110 + 160;
    int minScroll = 720 - contentH;
    if (minScroll > 0) minScroll = 0;
    if (m_scrollY > 0) m_scrollY = 0;
    if (m_scrollY < minScroll) m_scrollY = static_cast<float>(minScroll);
}

void AchievementsScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
    SDL_RenderClear(renderer);

    drawScreenLabel(renderer, m_app->font(), "Achievements");

    int startY = 140 + static_cast<int>(m_scrollY);
    const int cardW = 740;
    const int cardH = 90;
    const int cardX = (1280 - cardW) / 2;
    const int gap = 16;

    // Progress mapping for each achievement
    struct Prog { int current; int target; };
    std::unordered_map<AchievementId, Prog> progMap;
    // These would normally come from saved stats; using placeholders here
    progMap[AchievementId::FirstBlackjack] = {0, 1};
    progMap[AchievementId::ThousandBankroll] = {0, 1000};
    progMap[AchievementId::FiveBlackjacksSession] = {0, 5};
    progMap[AchievementId::TenWinStreak] = {0, 10};
    progMap[AchievementId::HundredHands] = {0, 100};
    progMap[AchievementId::NeverBustSession] = {0, 20};

    for (size_t i = 0; i < m_achievements.size(); ++i) {
        int y = startY + static_cast<int>(i) * (cardH + gap);
        if (y + cardH < 0 || y > 720) continue;  // culling
        const auto& ach = m_achievements[i];
        auto it = progMap.find(ach.id);
        int prog = 0, target = 0;
        if (it != progMap.end()) {
            prog = it->second.current;
            target = it->second.target;
            if (ach.unlocked) { prog = target; }
        }
        renderAchievementCard(renderer, cardX, y, cardW, cardH, ach, prog, target);
    }

    renderButtons(renderer, m_buttons);
    drawEscHint(renderer, m_app->font());
}


// ============================================================================
// TutorialScreen
// ============================================================================

TutorialScreen::TutorialScreen(Application* app)
    : Screen(AppState::Tutorial), m_app(app) {
    setupButtons();
}

void TutorialScreen::setupButtons() {
    m_buttons.clear();
    const int bw = 160;
    const int bh = 44;
    const int by = 620;

    // Previous button
    auto prevBtn = std::make_unique<Button>(
        340, by, bw, bh, "Previous",
        [this]() { previousStep(); },
        m_app->font());
    prevBtn->theme = &m_app->theme();
    m_buttons.push_back(std::move(prevBtn));

    // Next button
    auto nextBtn = std::make_unique<Button>(
        560, by, bw, bh, "Next",
        [this]() { advanceStep(); },
        m_app->font());
    nextBtn->theme = &m_app->theme();
    m_buttons.push_back(std::move(nextBtn));

    // Skip button
    auto skipBtn = std::make_unique<Button>(
        780, by, bw, bh, "Skip",
        [this]() { m_app->screenManager().transitionTo(AppState::MainMenu); },
        m_app->font());
    skipBtn->theme = &m_app->theme();
    m_buttons.push_back(std::move(skipBtn));
}

void TutorialScreen::onEnter() {
    m_step = Step::Welcome;
    for (auto& btn : m_buttons) {
        btn->resetState();
    }
}

void TutorialScreen::advanceStep() {
    switch (m_step) {
        case Step::Welcome:    m_step = Step::Objective; break;
        case Step::Objective:  m_step = Step::Betting; break;
        case Step::Betting:    m_step = Step::Deal; break;
        case Step::Deal:       m_step = Step::HitStand; break;
        case Step::HitStand:   m_step = Step::DoubleSplit; break;
        case Step::DoubleSplit:m_step = Step::Insurance; break;
        case Step::Insurance:  m_step = Step::WrapUp; break;
        case Step::WrapUp:
            m_app->screenManager().transitionTo(AppState::MainMenu);
            return;
    }
    for (auto& btn : m_buttons) btn->resetState();
}

void TutorialScreen::previousStep() {
    switch (m_step) {
        case Step::Objective:  m_step = Step::Welcome; break;
        case Step::Betting:    m_step = Step::Objective; break;
        case Step::Deal:       m_step = Step::Betting; break;
        case Step::HitStand:   m_step = Step::Deal; break;
        case Step::DoubleSplit:m_step = Step::HitStand; break;
        case Step::Insurance:  m_step = Step::DoubleSplit; break;
        case Step::WrapUp:     m_step = Step::Insurance; break;
        case Step::Welcome:    break;
    }
    for (auto& btn : m_buttons) btn->resetState();
}

void TutorialScreen::handleEvent(const SDL_Event& event) {
    if (handleEscToMenu(event, m_app)) return;
    if (routeButtons(event, m_buttons)) return;

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_SPACE) {
            advanceStep();
            return;
        }
        if (event.key.keysym.sym == SDLK_LEFT) {
            previousStep();
            return;
        }
    }
}

void TutorialScreen::update(float /*deltaTime*/) {}

static const std::unordered_map<TutorialScreen::Step, std::pair<std::string, std::vector<std::string>>>& getStepInfo() {
    static const std::unordered_map<TutorialScreen::Step, std::pair<std::string, std::vector<std::string>>> info = {
        {TutorialScreen::Step::Welcome, {
            "Welcome to Blackjack!",
            {
                "Blackjack is one of the most popular casino card games in the world.",
                "The goal is simple: beat the dealer's hand without going over 21.",
                "This tutorial will guide you through the basics in just a few steps.",
                "",
                "Press Next to begin, or use the Right Arrow key.",
            }
        }},
        {TutorialScreen::Step::Objective, {
            "The Objective",
            {
                "Your goal is to have a hand value closer to 21 than the dealer's hand.",
                "If your hand exceeds 21, you 'bust' and automatically lose.",
                "If the dealer busts and you don't, you win!",
                "A 'Blackjack' is an Ace + a 10-value card — the best possible hand.",
                "",
                "Card values: Numbers = face value, Face cards (J, Q, K) = 10, Ace = 1 or 11.",
            }
        }},
        {TutorialScreen::Step::Betting, {
            "Placing Your Bet",
            {
                "Before each round, you place a bet using your bankroll.",
                "Use the + and - buttons (or arrow keys) to adjust your bet.",
                "The minimum and maximum bets are set by the table rules.",
                "You can only bet what you have — manage your bankroll wisely!",
                "",
                "Once you're happy with your bet, press Deal to start the round.",
            }
        }},
        {TutorialScreen::Step::Deal, {
            "The Deal",
            {
                "Each player (including you) receives two cards face-up.",
                "The dealer receives one card face-up and one card face-down (the 'hole card').",
                "If the dealer's up-card is an Ace, you'll be offered Insurance.",
                "",
                "After the deal, it's your turn to act on your hand.",
            }
        }},
        {TutorialScreen::Step::HitStand, {
            "Hit or Stand",
            {
                "HIT  (H)  — Take another card. Use this when your total is low.",
                "STAND (S) — Keep your current hand and end your turn.",
                "",
                "Example: You have 16. Hitting might get you to 21... or bust you at 26.",
                "Standing means you're hoping the dealer busts or ends with a lower total.",
                "",
                "Tip: The dealer must hit until they reach at least 17.",
            }
        }},
        {TutorialScreen::Step::DoubleSplit, {
            "Double Down & Split",
            {
                "DOUBLE DOWN (D) — Double your bet, take exactly one more card, then stand.",
                "Best used when you have 10 or 11 and the dealer shows a weak card.",
                "",
                "SPLIT (P) — If your first two cards have the same rank, split them into two hands.",
                "Each new hand gets its own bet. You play each hand separately.",
                "",
                "Note: Not all tables allow doubling after a split.",
            }
        }},
        {TutorialScreen::Step::Insurance, {
            "Insurance",
            {
                "When the dealer's up-card is an Ace, you may take Insurance.",
                "Insurance is a side bet equal to half your original bet.",
                "If the dealer has Blackjack, Insurance pays 2:1 — covering your original loss.",
                "If the dealer does not have Blackjack, you lose the Insurance bet.",
                "",
                "Most strategy guides advise against taking Insurance in the long run.",
            }
        }},
        {TutorialScreen::Step::WrapUp, {
            "You're Ready!",
            {
                "You now know the basics of Blackjack:",
                "  • Place your bet, then press Deal",
                "  • Choose Hit, Stand, Double, or Split on your turn",
                "  • Watch the dealer play and see if you win!",
                "",
                "For a quick reminder during play, press the ? button on the table.",
                "",
                "Good luck, and may the cards be in your favor!",
            }
        }},
    };
    return info;
}

void TutorialScreen::renderStep(SDL_Renderer* r) {
    const auto& info = getStepInfo();
    auto it = info.find(m_step);
    if (it == info.end()) return;

    const auto& stepInfo = it->second;
    drawTextCentered(r, m_app->font(), stepInfo.first, 640, 120, 255, 215, 0);

    int y = 200;
    for (const auto& line : stepInfo.second) {
        if (line.empty()) {
            y += 20;
            continue;
        }
        drawTextCentered(r, m_app->font(), line, 640, y, 230, 230, 230);
        y += 28;
    }
}

void TutorialScreen::renderMiniTable(SDL_Renderer* r) {
    // Decorative mini table visual for ambiance
    int cx = 640, cy = 520, rw = 500, rh = 140;
    drawShadow(r, cx - rw / 2, cy - rh / 2, rw, rh, 5, {0, 0, 0, 100});
    drawRoundedRect(r, cx - rw / 2, cy - rh / 2, rw, rh, 10, {45, 90, 39, 255});
    drawRoundedRectOutline(r, cx - rw / 2, cy - rh / 2, rw, rh, 10, {60, 110, 50, 255});
    drawTextCentered(r, m_app->font(), "Blackjack Table", cx, cy - 40, 150, 180, 140);
}

void TutorialScreen::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
    SDL_RenderClear(renderer);

    renderStep(renderer);
    renderMiniTable(renderer);

    // Step counter
    int stepNum = static_cast<int>(m_step) + 1;
    int totalSteps = 8;
    drawTextCentered(renderer, m_app->font(),
                     "Step " + std::to_string(stepNum) + " of " + std::to_string(totalSteps),
                     640, 680, 180, 180, 180);

    renderButtons(renderer, m_buttons);
    drawEscHint(renderer, m_app->font());
}


}  // namespace blackjack
