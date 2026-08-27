#include "fixture.hpp"

#include <string>

using namespace ivv::catan;

int main() { return test::Run({
    {"round trip preserves an in-progress game exactly and remains playable", [] {
        GameController game({"Alice", "Bob", "Carol"});
        test::CompleteSetup(game, 3);
        const std::string current = game.GetCurrentPlayer();
        test::SeedResources(game, current, {{Resurse::Wood, 3}, {Resurse::Clay, 2}});

        const std::string saved = game.SerializeState();
        auto restored = GameController::DeserializeState(saved);

        test::Equal(restored->SerializeState(), saved,
                    "every authoritative field and the remaining deck round-trips");
        test::Equal(restored->GetCurrentPlayer(), current, "current player is restored");
        restored->Dice(current);
        test::Check(restored->GetLastDice().first >= 1 && restored->GetLastDice().second >= 1,
                    "restored game accepts the next legal command");
    }},
    {"round trip preserves the settlement selected during initial placement", [] {
        GameController game({"Alice", "Bob"});
        const std::string current = game.GetCurrentPlayer();
        game.BuildSettlement(current, 0);
        const std::string saved = game.SerializeState();
        auto restored = GameController::DeserializeState(saved);

        test::Equal(restored->SerializeState(), saved, "mid-phase setup state round-trips");
        test::Throws([&] { restored->BuildRoad(current, 71); },
                     "road away from the restored setup settlement stays forbidden");
        restored->BuildRoad(current, 0);
    }},
    {"round trip preserves finite bank and pre-roll Road Building return phase", [] {
        auto first = std::make_shared<test::DiceState>();
        auto second = std::make_shared<test::DiceState>();
        GameController::Dependencies dependencies;
        dependencies.dice[0] = std::make_unique<test::ScriptedDice>(first);
        dependencies.dice[1] = std::make_unique<test::ScriptedDice>(second);
        dependencies.development_cards = std::make_unique<DevelopmentCardDeck>();
        GameController game(std::vector<std::string>{"Alice","Bob"},std::move(dependencies));
        test::CompleteSetup(game,2); const auto current=game.GetCurrentPlayer();
        auto& player=test::MutablePlayer(game,current);
        player.PutCard(DevelopmentCard::RoadBuilding); player.OnEndTurn();
        game.UseDevCard(current,DevelopmentCard::RoadBuilding,std::nullopt);
        game.BuildRoad(current,1);
        auto restored=GameController::DeserializeState(game.SerializeState());
        test::Equal(restored->GetResourceBank().Counts(),game.GetResourceBank().Counts(),
            "all five physical piles round-trip");
        restored->BuildRoad(current,7);
        test::Equal(restored->GetStep(),GameController::GameStep::DiceDrop,
            "saved pre-roll card returns to dice after its second road");
    }},
    {"round trip accepts completed discard iteration while robber is waiting", [] {
        auto first = std::make_shared<test::DiceState>();
        auto second = std::make_shared<test::DiceState>();
        first->rolls.push_back(3);
        second->rolls.push_back(4);
        GameController::Dependencies dependencies;
        dependencies.dice[0] = std::make_unique<test::ScriptedDice>(first);
        dependencies.dice[1] = std::make_unique<test::ScriptedDice>(second);
        dependencies.development_cards = std::make_unique<DevelopmentCardDeck>();
        GameController game(std::vector<std::string>{"Alice", "Bob", "Carol"},
                            std::move(dependencies));
        test::CompleteSetup(game, 3);
        game.Dice(game.GetCurrentPlayer());
        test::Equal(game.GetStep(), GameController::GameStep::BanditMove,
                    "seven advances through players with at most seven cards");

        const std::string saved = game.SerializeState();
        auto restored = GameController::DeserializeState(saved);
        test::Equal(restored->SerializeState(), saved,
                    "completed discard cursor is a valid transient state");
    }},
    {"corrupt, truncated, unsupported and extended saves are rejected", [] {
        GameController game({"Alice", "Bob"});
        const std::string saved = game.SerializeState();

        std::string corrupt = saved;
        corrupt[0] = 'X';
        test::Throws([&] { GameController::DeserializeState(corrupt); }, "bad magic is rejected");
        test::Throws([&] { GameController::DeserializeState(saved.substr(0, saved.size() - 1)); },
                     "truncated state is rejected");

        std::string unsupported = saved;
        unsupported[16] = 99; // magic is 16 bytes; version is little-endian uint32
        test::Throws([&] { GameController::DeserializeState(unsupported); },
                     "unknown version is rejected");

        std::string extended = saved + "unexpected";
        test::Throws([&] { GameController::DeserializeState(extended); },
                     "trailing unvalidated data is rejected");
    }},
}); }
