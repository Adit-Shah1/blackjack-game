#pragma once

namespace blackjack {

struct RuleSet {
    int deckCount = 6;
    bool dealerHitsSoft17 = false;
    bool surrenderAllowed = true;
    bool insuranceAllowed = true;
    bool doubleAfterSplit = true;
    int maxSplitHands = 4;
    int blackjackPayoutNumerator = 3;
    int blackjackPayoutDenominator = 2;
    int minBet = 10;
    int maxBet = 500;
    int startingBankroll = 1000;
    float reshuffleThreshold = 0.25f;
};

}  // namespace blackjack