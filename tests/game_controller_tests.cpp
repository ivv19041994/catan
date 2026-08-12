#include "catan.hpp"
#include "dice.hpp"
#include "exception.hpp"
#include "game_controller.hpp"
#include "map.hpp"
#include "player.hpp"

#include <array>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using ivv::catan::DevelopmentCard;
using ivv::catan::GameController;
using ivv::catan::Map;
using ivv::catan::Player;
using ivv::catan::Resurse;

class TestFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void Check(bool condition, const std::string& message) {
    if (!condition) {
        throw TestFailure(message);
    }
}

template <typename Function>
void CheckThrows(Function&& function, const std::string& message) {
    try {
        std::forward<Function>(function)();
    } catch (const std::exception&) {
        return;
    }
    throw TestFailure(message);
}

std::string OtherPlayer(const std::string& current) {
    return current == "alice" ? "bob" : "alice";
}

void TestInitialPlacementAndTurns() {
    GameController game({"alice", "bob"});
    const std::string first_player = game.GetCurrentPlayer();

    CheckThrows(
        [&] { game.BuildSettlement(OtherPlayer(first_player), 9); },
        "a player must not place a settlement out of turn");

    game.BuildSettlement(first_player, 9);
    CheckThrows(
        [&] { game.BuildRoad(first_player, 71); },
        "the initial road must touch the just-built settlement");
    game.BuildRoad(first_player, 19);

    constexpr std::array<std::pair<size_t, size_t>, 3> remaining_placements{{
        {11, 14},
        {13, 16},
        {42, 58},
    }};
    for (const auto [node_id, facet_id] : remaining_placements) {
        game.BuildSettlement(game.GetCurrentPlayer(), node_id);
        game.BuildRoad(game.GetCurrentPlayer(), facet_id);
    }

    std::ostringstream step;
    game.PrintStep(step);
    Check(step.str().find("DiceDrop") != std::string::npos,
          "the game must enter DiceDrop after initial placement");

    for (const std::string name : {"alice", "bob"}) {
        const Player& player = game.GetPlayer(name);
        Check(player.getFreeSettlementCount() == 3,
              "each player must place two initial settlements");
        Check(player.getFreeRoadCount() == 13,
              "each player must place two initial roads");
    }

    const std::string active_player = game.GetCurrentPlayer();
    game.Dice(active_player);
    const auto [first_die, second_die] = game.GetLastDice();
    Check(first_die >= 1 && first_die <= 6 && second_die >= 1 && second_die <= 6,
          "both dice must be in the range 1..6");
}

void TestMapGenerationAndPortPricing() {
    Map map;
    std::array<size_t, 6> resource_counts{};

    for (const auto& gex : map.GetGexes()) {
        ++resource_counts.at(static_cast<size_t>(gex.getType()));
    }

    Check(resource_counts.at(static_cast<size_t>(Resurse::Wood)) == 4,
          "map must contain four wood hexes");
    Check(resource_counts.at(static_cast<size_t>(Resurse::Clay)) == 3,
          "map must contain three clay hexes");
    Check(resource_counts.at(static_cast<size_t>(Resurse::Hay)) == 4,
          "map must contain four hay hexes");
    Check(resource_counts.at(static_cast<size_t>(Resurse::Sheep)) == 4,
          "map must contain four sheep hexes");
    Check(resource_counts.at(static_cast<size_t>(Resurse::Stone)) == 3,
          "map must contain three stone hexes");
    Check(resource_counts.at(static_cast<size_t>(Resurse::Not)) == 1,
          "map must contain one desert");

    Player player("port-owner", 0);
    const size_t before_market = player.getCountResurses();
    map.placeStartBuilding(0, &player);  // Generic 3:1 port.
    player.Market(Resurse::Wood, Resurse::Clay);
    Check(player.getCountResurses() == before_market - 2,
          "a generic port must change the bank trade price from 4:1 to 3:1");
}

void TestPlayerDevelopmentCards() {
    Player player("developer", 0);

    player.PutCard(DevelopmentCard::Knights);
    Check(player.GetPurchasedCardCount(DevelopmentCard::Knights) == 1,
          "action cards must be unavailable during the purchase turn");
    player.OnEndTurn();
    Check(player.GetReadyForUseCardCount(DevelopmentCard::Knights) == 1,
          "an action card must become available on the next turn");

    player.Use(DevelopmentCard::Knights);
    Check(player.GetUsedCardCount(DevelopmentCard::Knights) == 1,
          "using a knight must record it as used");
    CheckThrows(
        [&] { player.Use(DevelopmentCard::Knights); },
        "a second development action in one turn must be rejected");

    player.PutCard(DevelopmentCard::University);
    player.Use(DevelopmentCard::University);
    Check(player.GetWinPoints() == 1,
          "using a victory card must add one victory point");
}

void TestDice() {
    ivv::game::Dice dice(2);
    for (size_t attempt = 0; attempt < 100; ++attempt) {
        const auto result = dice.Drop();
        Check(result.each.size() == 2, "two dice must produce two individual values");
        Check(result.each[0] >= 1 && result.each[0] <= 6,
              "first die must be in the range 1..6");
        Check(result.each[1] >= 1 && result.each[1] <= 6,
              "second die must be in the range 1..6");
        Check(result.result == result.each[0] + result.each[1],
              "the total must be the sum of individual dice");
    }
}

}  // namespace

int main() {
    const std::array<std::pair<const char*, std::function<void()>>, 4> tests{{
        {"initial placement and turn transitions", TestInitialPlacementAndTurns},
        {"map generation and ports", TestMapGenerationAndPortPricing},
        {"player development cards", TestPlayerDevelopmentCards},
        {"dice", TestDice},
    }};

    size_t failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    return failed == 0 ? 0 : 1;
}
