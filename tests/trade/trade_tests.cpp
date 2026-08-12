#include "fixture.hpp"

using namespace ivv::catan;

int main() { return test::Run({
    {"bank trade is 4:1 without a port", [] {
        Player p("trader",0);
        test::SeedResources(p, {{Resurse::Wood,4}});
        const auto wood = p.getCountResurses(Resurse::Wood);
        const auto clay = p.getCountResurses(Resurse::Clay);
        p.Market(Resurse::Wood, Resurse::Clay);
        test::Equal(p.getCountResurses(Resurse::Wood), wood - 4, "bank consumes four offered cards");
        test::Equal(p.getCountResurses(Resurse::Clay), clay + 1, "bank gives one requested card");
    }},
    {"generic and specialized ports reduce only the correct prices", [] {
        Map map; Player generic("generic",0), specialized("specialized",1);
        map.placeStartBuilding(0, &generic);
        test::SeedResources(generic, {{Resurse::Stone,3}});
        auto before = generic.getCountResurses(); generic.Market(Resurse::Stone, Resurse::Hay);
        test::Equal(generic.getCountResurses(), before - 2, "generic port trades 3:1");
        map.placeStartBuilding(3, &specialized);
        test::SeedResources(specialized, {{Resurse::Sheep,2},{Resurse::Stone,4}});
        const auto sheep = specialized.getCountResurses(Resurse::Sheep);
        specialized.Market(Resurse::Sheep, Resurse::Wood);
        test::Equal(specialized.getCountResurses(Resurse::Sheep), sheep - 2, "sheep port trades sheep 2:1");
        const auto stone = specialized.getCountResurses(Resurse::Stone);
        specialized.Market(Resurse::Stone, Resurse::Wood);
        test::Equal(specialized.getCountResurses(Resurse::Stone), stone - 4, "specialized port leaves other rates at 4:1");
    }},
    {"matching player deal transfers both sides atomically", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled);
        auto& game = *controlled.game; const auto seller = game.GetCurrentPlayer(); const auto buyer = seller == "a" ? "b" : "a";
        test::SeedResources(game, seller, {{Resurse::Wood,2}});
        test::SeedResources(game, buyer, {{Resurse::Clay,1}});
        const auto seller_wood = game.GetPlayer(seller).getCountResurses(Resurse::Wood);
        const auto buyer_clay = game.GetPlayer(buyer).getCountResurses(Resurse::Clay);
        game.SetDeal(seller, {{Resurse::Wood,2}}, {{Resurse::Clay,1}});
        test::Check(game.GetActivDeal().has_value(), "current player's offer becomes active");
        game.SetDeal(buyer, {{Resurse::Clay,1}}, {{Resurse::Wood,2}});
        test::Check(!game.GetActivDeal(), "accepted deal is cleared");
        test::Equal(game.GetPlayer(seller).getCountResurses(Resurse::Wood), seller_wood - 2, "seller pays offer");
        test::Equal(game.GetPlayer(buyer).getCountResurses(Resurse::Clay), buyer_clay - 1, "buyer pays counter-resource");
    }},
    {"empty, mismatched and unaffordable deals are rejected without transfer", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled);
        auto& game = *controlled.game; const auto seller = game.GetCurrentPlayer(); const auto buyer = seller == "a" ? "b" : "a";
        test::SeedResources(game, seller, {{Resurse::Wood,2}});
        test::SeedResources(game, buyer, {{Resurse::Clay,2}});
        test::Throws([&] { game.SetDeal(seller, {}, {{Resurse::Clay,1}}); }, "empty side is invalid");
        game.SetDeal(seller, {{Resurse::Wood,2}}, {{Resurse::Clay,1}});
        const auto before = game.GetPlayer(buyer).getCountResurses();
        test::Throws([&] { game.SetDeal(buyer, {{Resurse::Clay,2}}, {{Resurse::Wood,2}}); }, "mismatched acceptance is invalid");
        test::Equal(game.GetPlayer(buyer).getCountResurses(), before, "rejected deal is atomic");
        test::SeedResources(game, buyer, {});
        test::Throws([&] { game.SetDeal(buyer, {{Resurse::Clay,1}}, {{Resurse::Wood,2}}); }, "buyer cannot offer absent cards");
    }},
    {"passing clears an unaccepted deal", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled);
        auto& game = *controlled.game; const auto seller = game.GetCurrentPlayer();
        test::SeedResources(game, seller, {{Resurse::Wood,1}});
        game.SetDeal(seller, {{Resurse::Wood,1}}, {{Resurse::Clay,1}}); game.Pass(seller);
        test::Check(!game.GetActivDeal(), "offer expires at end of turn");
    }},
}); }
