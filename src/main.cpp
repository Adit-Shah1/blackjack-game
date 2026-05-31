#include <blackjack/gui.h>
#include <iostream>

int main(int /*argc*/, char* /*argv*/[]) {
    blackjack::Application app("Blackjack", 1280, 720);

    if (!app.init()) {
        std::cerr << "Failed to initialize application." << std::endl;
        return 1;
    }

    // Register all screens
    app.screenManager().registerScreen(
        blackjack::AppState::MainMenu,
        std::make_unique<blackjack::MainMenuScreen>(&app));
    app.screenManager().registerScreen(
        blackjack::AppState::Lobby,
        std::make_unique<blackjack::LobbyScreen>(&app));
    app.screenManager().registerScreen(
        blackjack::AppState::Settings,
        std::make_unique<blackjack::SettingsScreen>(&app));
    app.screenManager().registerScreen(
        blackjack::AppState::InRound,
        std::make_unique<blackjack::GameTableScreen>(&app));
    app.screenManager().registerScreen(
        blackjack::AppState::RoundResults,
        std::make_unique<blackjack::RoundResultsScreen>(&app));
    app.screenManager().registerScreen(
        blackjack::AppState::NetworkCreate,
        std::make_unique<blackjack::NetworkCreateScreen>(&app));
    app.screenManager().registerScreen(
        blackjack::AppState::NetworkJoin,
        std::make_unique<blackjack::NetworkJoinScreen>(&app));
    app.screenManager().registerScreen(
        blackjack::AppState::Achievements,
        std::make_unique<blackjack::AchievementsScreen>(&app));
    app.screenManager().registerScreen(
        blackjack::AppState::Tutorial,
        std::make_unique<blackjack::TutorialScreen>(&app));

    app.screenManager().transitionTo(blackjack::AppState::MainMenu);
    app.run();

    return 0;
}