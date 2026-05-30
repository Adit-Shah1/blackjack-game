#pragma once

#include "rules.h"
#include "round.h"
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace blackjack {

// --- Player Profile ---
struct PlayerProfile {
    std::string name;
    int avatarId = 0;
    RuleSet preferredRules;
    int masterVolume = 100;
    int sfxVolume = 100;
    int ambientVolume = 100;
    bool muted = false;
};

// --- Statistics ---
struct PlayerStats {
    int gamesPlayed = 0;
    int gamesWon = 0;
    int gamesLost = 0;
    int gamesPushed = 0;
    int blackjacks = 0;
    int biggestWin = 0;
    int totalWagered = 0;
    int totalWon = 0;
    int currentStreak = 0;
    int bestWinStreak = 0;
    int handsPlayed = 0;
    int sessionsWithoutBust = 0;
};

// --- Achievement ---
enum class AchievementId {
    FirstBlackjack,
    ThousandBankroll,
    FiveBlackjacksSession,
    TenWinStreak,
    HundredHands,
    NeverBustSession
};

struct Achievement {
    AchievementId id = AchievementId::FirstBlackjack;
    std::string name;
    std::string description;
    bool unlocked = false;
};

// --- Full Save State ---
struct SaveState {
    std::string profileName;
    uint64_t timestamp = 0;
    int version = 1;

    int currentBankroll = 0;
    RuleSet rules;
    RoundPhase phase = RoundPhase::WaitingForBets;
    int currentSeatIndex = -1;
    int currentHandIndex = -1;

    std::vector<std::string> shoeCards;
    int shoeRunningCount = 0;

    std::vector<std::string> dealerUpCards;
    std::string dealerHoleCard;
    bool dealerHoleVisible = false;

    struct SavedHand {
        std::vector<std::string> cards;
        int mainBet = 0;
        int insuranceBet = 0;
        bool doubled = false;
        bool surrendered = false;
        bool isSplit = false;
        bool finished = false;
        std::string outcome = "Pending";
    };
    std::vector<SavedHand> playerHands;

    PlayerStats stats;
    std::vector<Achievement> achievements;

    nlohmann::json toJson() const;
    static SaveState fromJson(const nlohmann::json& j);
};

// ============================================================================
// NLOHMANN JSON serialization macros (must be in header for visibility)
// ============================================================================

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RuleSet,
    deckCount, dealerHitsSoft17, surrenderAllowed, insuranceAllowed,
    doubleAfterSplit, maxSplitHands, blackjackPayoutNumerator,
    blackjackPayoutDenominator, minBet, maxBet, startingBankroll,
    reshuffleThreshold)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerProfile,
    name, avatarId, preferredRules, masterVolume, sfxVolume,
    ambientVolume, muted)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerStats,
    gamesPlayed, gamesWon, gamesLost, gamesPushed, blackjacks,
    biggestWin, totalWagered, totalWon, currentStreak,
    bestWinStreak, handsPlayed, sessionsWithoutBust)

NLOHMANN_JSON_SERIALIZE_ENUM(AchievementId, {
    {AchievementId::FirstBlackjack, "FirstBlackjack"},
    {AchievementId::ThousandBankroll, "ThousandBankroll"},
    {AchievementId::FiveBlackjacksSession, "FiveBlackjacksSession"},
    {AchievementId::TenWinStreak, "TenWinStreak"},
    {AchievementId::HundredHands, "HundredHands"},
    {AchievementId::NeverBustSession, "NeverBustSession"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Achievement,
    id, name, description, unlocked)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SaveState::SavedHand,
    cards, mainBet, insuranceBet, doubled, surrendered,
    isSplit, finished, outcome)

// --- Persistence Manager ---
class PersistenceManager {
public:
    PersistenceManager();

    bool createProfile(const std::string& name);
    bool deleteProfile(const std::string& name);
    std::vector<std::string> listProfiles() const;
    bool profileExists(const std::string& name) const;
    PlayerProfile loadProfile(const std::string& name) const;
    bool saveProfile(const PlayerProfile& profile);

    bool saveGame(const std::string& profileName, const SaveState& state);
    SaveState loadGame(const std::string& profileName) const;
    bool deleteSave(const std::string& profileName);
    bool hasSave(const std::string& profileName) const;

    void setAutoSaveEnabled(bool enabled);
    void setSaveDirectoryOverride(const std::string& path);
    bool isAutoSaveEnabled() const;

private:
    std::string getSaveDirectory() const;
    std::string getProfilePath(const std::string& name) const;
    std::string getSaveFilePath(const std::string& profileName) const;
    bool ensureDirectoryExists(const std::string& path) const;

    bool m_autoSave = true;
    std::string m_saveDirectoryOverride;
};

// --- Game Session ---
class GameSession {
public:
    GameSession();

    void newGame(const std::string& profileName, const RuleSet& rules);
    bool loadGame(const std::string& profileName);
    bool saveGame();

    void setSaveDirectoryOverride(const std::string& path);

    RoundState& round() { return m_round; }
    const RoundState& round() const { return m_round; }
    PlayerStats& stats() { return m_stats; }
    const PlayerStats& stats() const { return m_stats; }

    void recordHandResult(HandOutcome outcome, int bet, int payout);
    void recordRoundEnd();
    void recordBlackjack();
    void checkAchievements();

    const std::vector<Achievement>& achievements() const { return m_achievements; }

private:
    RoundState m_round;
    PersistenceManager m_persistence;
    PlayerStats m_stats;
    std::vector<Achievement> m_achievements;
    std::string m_profileName;
    int m_sessionBlackjacks = 0;

    void initAchievements();
    SaveState buildSaveState() const;
    void restoreFromSaveState(const SaveState& state);
};

}  // namespace blackjack
