#include <blackjack/persistence.h>
#include <blackjack/card.h>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace blackjack {

// ============================================================================
// Card string conversion helpers
// ============================================================================

static std::string cardToString(const Card& card) {
    char rank = card.rankChar();
    char suit = card.suitChar();
    return std::string{rank, suit};
}

static Card cardFromString(const std::string& str) {
    if (str.size() != 2) return Card();
    char r = str[0];
    char s = str[1];

    Rank rank;
    switch (r) {
        case '2': rank = Rank::Two; break;
        case '3': rank = Rank::Three; break;
        case '4': rank = Rank::Four; break;
        case '5': rank = Rank::Five; break;
        case '6': rank = Rank::Six; break;
        case '7': rank = Rank::Seven; break;
        case '8': rank = Rank::Eight; break;
        case '9': rank = Rank::Nine; break;
        case 'T': rank = Rank::Ten; break;
        case 'J': rank = Rank::Jack; break;
        case 'Q': rank = Rank::Queen; break;
        case 'K': rank = Rank::King; break;
        case 'A': rank = Rank::Ace; break;
        default: rank = Rank::Ace; break;
    }

    Suit suit;
    switch (s) {
        case 'H': suit = Suit::Hearts; break;
        case 'D': suit = Suit::Diamonds; break;
        case 'C': suit = Suit::Clubs; break;
        case 'S': suit = Suit::Spades; break;
        default: suit = Suit::Hearts; break;
    }

    return Card(suit, rank);
}

// ============================================================================
// SaveState custom JSON serialization
// ============================================================================

nlohmann::json SaveState::toJson() const {
    nlohmann::json j;
    j["profileName"] = profileName;
    j["timestamp"] = timestamp;
    j["version"] = version;
    j["currentBankroll"] = currentBankroll;
    j["rules"] = rules;
    j["phase"] = toString(phase);
    j["currentSeatIndex"] = currentSeatIndex;
    j["currentHandIndex"] = currentHandIndex;
    j["shoeCards"] = shoeCards;
    j["shoeRunningCount"] = shoeRunningCount;
    j["dealerUpCards"] = dealerUpCards;
    j["dealerHoleCard"] = dealerHoleCard;
    j["dealerHoleVisible"] = dealerHoleVisible;

    nlohmann::json handsJson = nlohmann::json::array();
    for (const auto& hand : playerHands) {
        nlohmann::json h;
        h["cards"] = hand.cards;
        h["mainBet"] = hand.mainBet;
        h["insuranceBet"] = hand.insuranceBet;
        h["doubled"] = hand.doubled;
        h["surrendered"] = hand.surrendered;
        h["isSplit"] = hand.isSplit;
        h["finished"] = hand.finished;
        h["outcome"] = hand.outcome;
        handsJson.push_back(h);
    }
    j["playerHands"] = handsJson;

    j["stats"] = stats;
    j["achievements"] = achievements;
    return j;
}

SaveState SaveState::fromJson(const nlohmann::json& j) {
    SaveState state;
    state.profileName = j.value("profileName", "");
    state.timestamp = j.value("timestamp", 0ULL);
    state.version = j.value("version", 1);
    state.currentBankroll = j.value("currentBankroll", 0);

    if (j.contains("rules") && j["rules"].is_object()) {
        state.rules = j["rules"].get<RuleSet>();
    }

    state.phase = roundPhaseFromString(j.value("phase", "WaitingForBets"));
    state.currentSeatIndex = j.value("currentSeatIndex", -1);
    state.currentHandIndex = j.value("currentHandIndex", -1);

    if (j.contains("shoeCards") && j["shoeCards"].is_array()) {
        state.shoeCards = j["shoeCards"].get<std::vector<std::string>>();
    }
    state.shoeRunningCount = j.value("shoeRunningCount", 0);

    if (j.contains("dealerUpCards") && j["dealerUpCards"].is_array()) {
        state.dealerUpCards = j["dealerUpCards"].get<std::vector<std::string>>();
    }
    state.dealerHoleCard = j.value("dealerHoleCard", "");
    state.dealerHoleVisible = j.value("dealerHoleVisible", false);

    if (j.contains("playerHands") && j["playerHands"].is_array()) {
        for (const auto& h : j["playerHands"]) {
            SavedHand hand;
            hand.cards = h.value("cards", std::vector<std::string>{});
            hand.mainBet = h.value("mainBet", 0);
            hand.insuranceBet = h.value("insuranceBet", 0);
            hand.doubled = h.value("doubled", false);
            hand.surrendered = h.value("surrendered", false);
            hand.isSplit = h.value("isSplit", false);
            hand.finished = h.value("finished", false);
            hand.outcome = h.value("outcome", "Pending");
            state.playerHands.push_back(hand);
        }
    }

    if (j.contains("stats") && j["stats"].is_object()) {
        state.stats = j["stats"].get<PlayerStats>();
    }

    if (j.contains("achievements") && j["achievements"].is_array()) {
        state.achievements = j["achievements"].get<std::vector<Achievement>>();
    }

    return state;
}

// ============================================================================
// PersistenceManager
// ============================================================================

PersistenceManager::PersistenceManager() = default;

std::string PersistenceManager::getSaveDirectory() const {
    if (!m_saveDirectoryOverride.empty()) {
        return m_saveDirectoryOverride;
    }

#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        return std::string(appData) + "/BlackjackGame";
    }
    return "./BlackjackGame";
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        home = ".";
    }
#ifdef __APPLE__
    return std::string(home) + "/Library/Application Support/BlackjackGame";
#else
    return std::string(home) + "/.local/share/BlackjackGame";
#endif
#endif
}

std::string PersistenceManager::getProfilePath(const std::string& name) const {
    return getSaveDirectory() + "/profiles/" + name + ".json";
}

std::string PersistenceManager::getSaveFilePath(const std::string& profileName) const {
    return getSaveDirectory() + "/saves/" + profileName + "_save.json";
}

bool PersistenceManager::ensureDirectoryExists(const std::string& path) const {
    try {
        if (!std::filesystem::exists(path)) {
            return std::filesystem::create_directories(path);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool PersistenceManager::createProfile(const std::string& name) {
    if (name.empty() || name.size() > 16) return false;
    if (profileExists(name)) return false;

    std::string dir = getSaveDirectory() + "/profiles";
    if (!ensureDirectoryExists(dir)) return false;

    PlayerProfile profile;
    profile.name = name;
    return saveProfile(profile);
}

bool PersistenceManager::deleteProfile(const std::string& name) {
    try {
        std::string path = getProfilePath(name);
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> PersistenceManager::listProfiles() const {
    std::vector<std::string> profiles;
    std::string dir = getSaveDirectory() + "/profiles";

    try {
        if (!std::filesystem::exists(dir)) return profiles;

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                profiles.push_back(entry.path().stem().string());
            }
        }
    } catch (...) {
        // Return empty list on error
    }

    return profiles;
}

bool PersistenceManager::profileExists(const std::string& name) const {
    try {
        return std::filesystem::exists(getProfilePath(name));
    } catch (...) {
        return false;
    }
}

PlayerProfile PersistenceManager::loadProfile(const std::string& name) const {
    PlayerProfile profile;
    profile.name = name;

    try {
        std::string path = getProfilePath(name);
        if (!std::filesystem::exists(path)) return profile;

        std::ifstream file(path);
        if (!file.is_open()) return profile;

        nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
        if (j.is_discarded()) return profile;

        profile = j.get<PlayerProfile>();
    } catch (...) {
        // Return default profile on error
    }

    return profile;
}

bool PersistenceManager::saveProfile(const PlayerProfile& profile) {
    try {
        std::string dir = getSaveDirectory() + "/profiles";
        if (!ensureDirectoryExists(dir)) return false;

        std::string path = getProfilePath(profile.name);
        std::ofstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j = profile;
        file << j.dump(4);
        return file.good();
    } catch (...) {
        return false;
    }
}

bool PersistenceManager::saveGame(const std::string& profileName, const SaveState& state) {
    try {
        std::string dir = getSaveDirectory() + "/saves";
        if (!ensureDirectoryExists(dir)) return false;

        std::string path = getSaveFilePath(profileName);
        std::ofstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j = state.toJson();
        file << j.dump(4);
        return file.good();
    } catch (...) {
        return false;
    }
}

SaveState PersistenceManager::loadGame(const std::string& profileName) const {
    SaveState state;
    state.profileName = profileName;

    try {
        std::string path = getSaveFilePath(profileName);
        if (!std::filesystem::exists(path)) return state;

        std::ifstream file(path);
        if (!file.is_open()) return state;

        nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
        if (j.is_discarded()) return state;

        state = SaveState::fromJson(j);
    } catch (...) {
        // Return default state on error
    }

    return state;
}

bool PersistenceManager::deleteSave(const std::string& profileName) {
    try {
        std::string path = getSaveFilePath(profileName);
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool PersistenceManager::hasSave(const std::string& profileName) const {
    try {
        return std::filesystem::exists(getSaveFilePath(profileName));
    } catch (...) {
        return false;
    }
}

void PersistenceManager::setAutoSaveEnabled(bool enabled) {
    m_autoSave = enabled;
}

bool PersistenceManager::isAutoSaveEnabled() const {
    return m_autoSave;
}

void PersistenceManager::setSaveDirectoryOverride(const std::string& path) {
    m_saveDirectoryOverride = path;
}

// ============================================================================
// GameSession
// ============================================================================

GameSession::GameSession()
    : m_round(RuleSet())
{
    initAchievements();
}

void GameSession::initAchievements() {
    m_achievements = {
        {AchievementId::FirstBlackjack, "First Blackjack", "Get your first natural blackjack.", false},
        {AchievementId::ThousandBankroll, "High Roller", "Reach a bankroll of $1,000.", false},
        {AchievementId::FiveBlackjacksSession, "Lucky Streak", "Get 5 blackjacks in one session.", false},
        {AchievementId::TenWinStreak, "Unstoppable", "Win 10 games in a row.", false},
        {AchievementId::HundredHands, "Veteran", "Play 100 hands total.", false},
        {AchievementId::NeverBustSession, "Iron Stomach", "Play 20 hands without busting.", false},
    };
}

void GameSession::newGame(const std::string& profileName, const RuleSet& rules) {
    m_profileName = profileName;
    m_round = RoundState(rules);
    m_stats = PlayerStats();
    m_sessionBlackjacks = 0;
    initAchievements();
}

bool GameSession::loadGame(const std::string& profileName) {
    if (!m_persistence.hasSave(profileName)) {
        return false;
    }
    SaveState state = m_persistence.loadGame(profileName);
    m_profileName = profileName;
    restoreFromSaveState(state);
    return true;
}

bool GameSession::saveGame() {
    if (m_profileName.empty()) return false;
    SaveState state = buildSaveState();
    bool ok = m_persistence.saveGame(m_profileName, state);
    if (ok) {
        // Also save updated profile stats
        PlayerProfile profile = m_persistence.loadProfile(m_profileName);
        profile.name = m_profileName;
        m_persistence.saveProfile(profile);
    }
    return ok;
}

SaveState GameSession::buildSaveState() const {
    SaveState state;
    state.profileName = m_profileName;
    state.timestamp = static_cast<uint64_t>(std::time(nullptr));
    state.version = 1;
    state.currentBankroll = m_round.seats().empty() ? 0 : m_round.seats()[0].bankroll;
    state.rules = m_round.rules();
    state.phase = m_round.phase();
    state.currentSeatIndex = m_round.currentSeatIndex();
    state.currentHandIndex = m_round.currentHandIndex();

    // Serialize shoe
    for (const auto& card : m_round.shoe().cards()) {
        state.shoeCards.push_back(cardToString(card));
    }
    state.shoeRunningCount = m_round.shoe().getRunningCount();

    // Serialize dealer
    for (const auto& card : m_round.dealer().hand.cards()) {
        state.dealerUpCards.push_back(cardToString(card));
    }
    state.dealerHoleCard = cardToString(m_round.dealer().holeCard);
    state.dealerHoleVisible = m_round.dealer().holeCardVisible;

    // Serialize player hands
    if (!m_round.seats().empty()) {
        for (const auto& handState : m_round.seats()[0].hands) {
            SaveState::SavedHand saved;
            for (const auto& card : handState.hand.cards()) {
                saved.cards.push_back(cardToString(card));
            }
            saved.mainBet = handState.bet.mainBet;
            saved.insuranceBet = handState.bet.insuranceBet;
            saved.doubled = handState.doubled;
            saved.surrendered = handState.surrendered;
            saved.isSplit = handState.isSplit;
            saved.finished = handState.finished;
            saved.outcome = toString(handState.outcome);
            state.playerHands.push_back(saved);
        }
    }

    state.stats = m_stats;
    state.achievements = m_achievements;
    return state;
}

void GameSession::setSaveDirectoryOverride(const std::string& path) {
    m_persistence.setSaveDirectoryOverride(path);
}

void GameSession::restoreFromSaveState(const SaveState& state) {
    m_profileName = state.profileName;
    m_stats = state.stats;
    m_achievements = state.achievements.empty() ? m_achievements : state.achievements;

    RuleSet rules = state.rules;
    m_round = RoundState(rules);
    m_round.startRound();

    // Restore bankroll
    if (!m_round.seats().empty()) {
        m_round.seats()[0].bankroll = state.currentBankroll;
    }

    // Restore shoe
    std::vector<Card> shoeCards;
    for (const auto& s : state.shoeCards) {
        shoeCards.push_back(cardFromString(s));
    }
    m_round.shoe().setCards(shoeCards);
    m_round.shoe().setRunningCount(state.shoeRunningCount);

    // Restore dealer
    m_round.dealer().hand.clear();
    for (const auto& s : state.dealerUpCards) {
        m_round.dealer().hand.addCard(cardFromString(s));
    }
    m_round.dealer().holeCard = cardFromString(state.dealerHoleCard);
    m_round.dealer().holeCardVisible = state.dealerHoleVisible;

    // Restore player hands
    if (!m_round.seats().empty()) {
        m_round.seats()[0].hands.clear();
        for (const auto& saved : state.playerHands) {
            PlayerHandState handState;
            for (const auto& s : saved.cards) {
                handState.hand.addCard(cardFromString(s));
            }
            handState.bet.mainBet = saved.mainBet;
            handState.bet.insuranceBet = saved.insuranceBet;
            handState.doubled = saved.doubled;
            handState.surrendered = saved.surrendered;
            handState.isSplit = saved.isSplit;
            handState.finished = saved.finished;
            handState.outcome = handOutcomeFromString(saved.outcome);
            m_round.seats()[0].hands.push_back(handState);
        }
    }

    m_round.phase() = state.phase;
    m_round.currentSeatIndex() = state.currentSeatIndex;
    m_round.currentHandIndex() = state.currentHandIndex;
}

void GameSession::recordHandResult(HandOutcome outcome, int bet, int payout) {
    m_stats.handsPlayed++;
    m_stats.totalWagered += bet;
    m_stats.totalWon += payout - bet;

    switch (outcome) {
        case HandOutcome::Win:
            m_stats.gamesWon++;
            m_stats.currentStreak = std::max(1, m_stats.currentStreak + 1);
            m_stats.bestWinStreak = std::max(m_stats.bestWinStreak, m_stats.currentStreak);
            m_stats.biggestWin = std::max(m_stats.biggestWin, payout - bet);
            break;
        case HandOutcome::Blackjack:
            m_stats.gamesWon++;
            m_stats.blackjacks++;
            m_stats.currentStreak = std::max(1, m_stats.currentStreak + 1);
            m_stats.bestWinStreak = std::max(m_stats.bestWinStreak, m_stats.currentStreak);
            m_stats.biggestWin = std::max(m_stats.biggestWin, payout - bet);
            break;
        case HandOutcome::Lose:
        case HandOutcome::Bust:
            m_stats.gamesLost++;
            m_stats.currentStreak = std::min(-1, m_stats.currentStreak - 1);
            break;
        case HandOutcome::Push:
            m_stats.gamesPushed++;
            m_stats.currentStreak = 0;
            break;
        case HandOutcome::Surrender:
            m_stats.gamesLost++;
            m_stats.currentStreak = std::min(-1, m_stats.currentStreak - 1);
            break;
        default:
            break;
    }

    if (outcome != HandOutcome::Bust) {
        m_stats.sessionsWithoutBust++;
    } else {
        m_stats.sessionsWithoutBust = 0;
    }

    if (m_persistence.isAutoSaveEnabled()) {
        saveGame();
    }
}

void GameSession::recordRoundEnd() {
    m_stats.gamesPlayed++;
}

void GameSession::recordBlackjack() {
    m_sessionBlackjacks++;
    // Note: m_stats.blackjacks is incremented by recordHandResult(Blackjack)
}

void GameSession::checkAchievements() {
    for (auto& ach : m_achievements) {
        if (ach.unlocked) continue;

        switch (ach.id) {
            case AchievementId::FirstBlackjack:
                if (m_stats.blackjacks >= 1) ach.unlocked = true;
                break;
            case AchievementId::ThousandBankroll:
                if (!m_round.seats().empty() && m_round.seats()[0].bankroll >= 1000) {
                    ach.unlocked = true;
                }
                break;
            case AchievementId::FiveBlackjacksSession:
                if (m_sessionBlackjacks >= 5) ach.unlocked = true;
                break;
            case AchievementId::TenWinStreak:
                if (m_stats.bestWinStreak >= 10) ach.unlocked = true;
                break;
            case AchievementId::HundredHands:
                if (m_stats.handsPlayed >= 100) ach.unlocked = true;
                break;
            case AchievementId::NeverBustSession:
                if (m_stats.sessionsWithoutBust >= 20) ach.unlocked = true;
                break;
        }
    }
}

}  // namespace blackjack
