#include "fixture.hpp"

using namespace ivv::catan;

int main() { return test::Run({
    {"game accepts two, three, and four distinct players", [] {
        for (size_t count : {2u, 3u, 4u}) {
            std::vector<std::string> names;
            for (size_t i = 0; i < count; ++i) names.push_back("p" + std::to_string(i));
            test::ControlledGame controlled(names);
            test::Equal(controlled.game->GetPlayer("p0").getName(), std::string("p0"), "player must be addressable by name");
        }
    }},
    {"game rejects player counts outside 2..4", [] {
        test::Throws([] { test::ControlledGame game({"solo"}); }, "one player must be rejected");
        test::Throws([] { test::ControlledGame game({"a","b","c","d","e"}); }, "five players must be rejected");
    }},
    {"game rejects duplicate names and null dependencies", [] {
        test::Throws([] { test::ControlledGame game({"same","same"}); }, "duplicate names must be rejected");
        GameController::Dependencies deps;
        test::Throws([&] { GameController game(std::vector<std::string>{"a","b"}, std::move(deps)); }, "null dependencies must be rejected");
    }},
    {"initial placement follows forward then reverse snake", [] {
        test::ControlledGame controlled({"a","b","c","d"});
        const auto order = test::CompleteSetup(*controlled.game, 4);
        test::Equal(order.size(), size_t{8}, "four players place twice");
        test::Equal(order[0], order[7], "first player must also place last");
        test::Equal(order[1], order[6], "second player's return order is reversed");
        test::Equal(order[2], order[5], "third player's return order is reversed");
        test::Equal(order[3], order[4], "last player places twice at the turn-around");
        test::Check(test::Step(*controlled.game).find("DiceDrop") != std::string::npos, "setup must end at DiceDrop");
        for (const char* name : {"a","b","c","d"}) {
            test::Equal(controlled.game->GetPlayer(name).getFreeSettlementCount(), size_t{3}, "each player uses two settlements");
            test::Equal(controlled.game->GetPlayer(name).getFreeRoadCount(), size_t{13}, "each player uses two roads");
        }
    }},
    {"initial road must touch the settlement just placed", [] {
        test::ControlledGame controlled({"a","b"});
        const auto current = controlled.game->GetCurrentPlayer();
        controlled.game->BuildSettlement(current, 0);
        test::Throws([&] { controlled.game->BuildRoad(current, 71); }, "remote initial road must be rejected");
    }},
    {"reverse-round road cannot attach to the player's older settlement", [] {
        test::ControlledGame controlled({"a","b"}); auto& game=*controlled.game;
        const auto first=game.GetCurrentPlayer();
        game.BuildSettlement(first,0); game.BuildRoad(first,0);
        const auto second=game.GetCurrentPlayer();
        game.BuildSettlement(second,3); game.BuildRoad(second,2);

        test::Equal(game.GetCurrentPlayer(),second,"last player starts reverse round");
        game.BuildSettlement(second,16);
        test::Check(!game.CanBuildRoad(1),"road beside the first settlement is not a valid return-round target");
        test::Throws([&]{game.BuildRoad(second,1);},"return-round road beside old settlement must be rejected");
        test::Check(game.CanBuildRoad(23),"road beside the settlement built this turn remains valid");
        game.BuildRoad(second,23);

        test::Equal(game.GetCurrentPlayer(),first,"reverse round continues to first player");
        game.BuildSettlement(first,6);
        test::Check(!game.CanBuildRoad(1),"first player's older network cannot anchor second setup road");
        test::Check(game.CanBuildRoad(5),"first player's newly placed settlement anchors its road");
    }},
    {"only current player may place and ids are range checked", [] {
        test::ControlledGame controlled({"a","b"});
        const auto current = controlled.game->GetCurrentPlayer();
        const auto other = current == "a" ? "b" : "a";
        test::Throws([&] { controlled.game->BuildSettlement(other, 0); }, "out-of-turn setup action must fail");
        test::Throws([&] { controlled.game->BuildSettlement(current, 54); }, "invalid node id must fail");
    }},
    {"each second initial settlement grants its adjacent non-desert resources", [] {
        test::ControlledGame controlled({"a","b"}); auto& game=*controlled.game;
        for(const auto [node,road]:std::array<std::pair<size_t,size_t>,2>{{{0,0},{3,2}}}) {
            const auto p=game.GetCurrentPlayer(); game.BuildSettlement(p,node); game.BuildRoad(p,road);
        }
        for(const auto [node,road]:std::array<std::pair<size_t,size_t>,2>{{{6,5},{16,23}}}) {
            const auto p=game.GetCurrentPlayer(); const auto before=test::ResourceCounts(game.GetPlayer(p));
            auto expected=before;
            auto nodes=game.GetMap().GetNodes();
            for(const auto* hex:nodes[node].getNeighborGexs()) {
                if(hex->getType()!=Resurse::Not) ++expected[static_cast<size_t>(hex->getType())];
            }
            game.BuildSettlement(p,node);
            test::Equal(test::ResourceCounts(game.GetPlayer(p)),expected,"second settlement grants one card from each adjacent producing hex");
            game.BuildRoad(p,road);
        }
    }},
}); }
