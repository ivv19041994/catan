#include "fixture.hpp"

using namespace ivv::catan;

int main() { return test::Run({
    {"each physical d6 is rolled exactly once and last dice are preserved", [] {
        test::ControlledGame controlled({"a","b"});
        test::CompleteSetup(*controlled.game, 2);
        test::AddRoll(controlled, 2, 4);
        const auto current = controlled.game->GetCurrentPlayer();
        controlled.game->Dice(current);
        test::Equal(controlled.first_die->calls, size_t{1}, "first die Roll must be called once");
        test::Equal(controlled.second_die->calls, size_t{1}, "second die Roll must be called once");
        test::Equal(controlled.game->GetLastDice(), std::pair<size_t,size_t>{2,4}, "GetLastDice must expose individual values");
        test::Check(test::Step(*controlled.game).find("CommonPlay") != std::string::npos, "non-seven sum enters common play");
    }},
    {"production gives one matching resource per settlement", [] {
        test::ControlledGame controlled({"a","b"});
        test::CompleteSetup(*controlled.game, 2);
        const auto current = controlled.game->GetCurrentPlayer();
        const auto& map = controlled.game->GetMap();
        test::SeedResources(*controlled.game, "a", {});
        test::SeedResources(*controlled.game, "b", {});
        const Gex* target = nullptr;
        const Player* owner = nullptr;
        for (const auto& gex : map.GetGexes()) {
            if (gex.getDice() == 0 || gex.getDice() == 7 || gex.isBandit()) continue;
            for (const Node* node : gex.GetNodes()) if (node->getBuilding()) { target = &gex; owner = node->getBuilding()->getPlayer(); break; }
            if (target) break;
        }
        test::Check(target && owner, "setup must own a producing intersection");
        const auto resource = target->getType();
        const size_t before = owner->getCountResurses(resource);
        const size_t total = static_cast<size_t>(target->getDice());
        const size_t first = total > 6 ? 6 : 1;
        test::AddRoll(controlled, first, total - first);
        controlled.game->Dice(current);
        test::Equal(owner->getCountResurses(resource), before + 1, "settlement produces exactly one card");
    }},
    {"a seven produces nothing and starts discard flow", [] {
        test::ControlledGame controlled({"a","b"});
        test::CompleteSetup(*controlled.game, 2);
        test::SeedResources(*controlled.game, "a", {{Resurse::Wood,8}});
        test::SeedResources(*controlled.game, "b", {{Resurse::Clay,8}});
        const size_t total = controlled.game->GetPlayer("a").getCountResurses() + controlled.game->GetPlayer("b").getCountResurses();
        test::AddRoll(controlled, 3, 4);
        controlled.game->Dice(controlled.game->GetCurrentPlayer());
        test::Equal(controlled.game->GetLastDice(), std::pair<size_t,size_t>{3,4}, "seven is the actual sum of both dice");
        test::Equal(controlled.game->GetPlayer("a").getCountResurses() + controlled.game->GetPlayer("b").getCountResurses(), total, "seven itself produces no resource");
        test::Check(test::Step(*controlled.game).find("DropCards") != std::string::npos, "seven starts discards when a player has more than seven cards");
    }},
}); }
