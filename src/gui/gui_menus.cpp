#include <blackjack/gui.h>

#include "gui_helpers.h"

#include <SDL.h>

namespace blackjack {

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
    addBtn("Local Multiplayer", AppState::Lobby);
    addBtn("Create Room", AppState::NetworkCreate);
    addBtn("Join Room", AppState::NetworkJoin);
    addBtn("Tutorial", AppState::Tutorial);
    addBtn("Achievements", AppState::Achievements);
    addBtn("Settings", AppState::Settings);

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
    drawTextCentered(renderer, m_app->font(), "Phase 11 — Tutorial, Help & Achievements",
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


}  // namespace blackjack
