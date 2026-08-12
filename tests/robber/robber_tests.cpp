#include "fixture.hpp"

using namespace ivv::catan;

namespace {
std::map<Resurse,size_t> Half(const Player& p) {
    size_t remaining = p.getCountResurses() / 2;
    std::map<Resurse,size_t> result;
    for (auto r : {Resurse::Wood,Resurse::Clay,Resurse::Hay,Resurse::Sheep,Resurse::Stone}) {
        const size_t take = std::min(remaining, p.getCountResurses(r));
        if (take) result[r] = take;
        remaining -= take;
    }
    return result;
}
}

int main() { return test::Run({
    {"players with eight or more cards discard exactly half on seven", [] {
        test::ControlledGame controlled({"a","b"}); test::CompleteSetup(*controlled.game,2);
        test::SeedResources(*controlled.game,"a",{{Resurse::Wood,8}}); test::SeedResources(*controlled.game,"b",{{Resurse::Clay,8}}); test::AddRoll(controlled,3,4);
        controlled.game->Dice(controlled.game->GetCurrentPlayer());
        for (size_t i=0; i<2; ++i) {
            const auto name = controlled.game->GetCurrentPlayer();
            const size_t before = controlled.game->GetPlayer(name).getCountResurses();
            controlled.game->DropCards(name, Half(controlled.game->GetPlayer(name)));
            test::Equal(controlled.game->GetPlayer(name).getCountResurses(), before - before/2, "discard removes floor(total/2)");
        }
        test::Check(test::Step(*controlled.game).find("BanditMove") != std::string::npos, "all discards lead to robber move");
    }},
    {"wrong discard count and out-of-turn discard are rejected atomically", [] {
        test::ControlledGame controlled({"a","b"}); test::CompleteSetup(*controlled.game,2);
        test::SeedResources(*controlled.game,"a",{{Resurse::Wood,8}}); test::SeedResources(*controlled.game,"b",{{Resurse::Clay,8}}); test::AddRoll(controlled,3,4);
        controlled.game->Dice(controlled.game->GetCurrentPlayer());
        const auto current = controlled.game->GetCurrentPlayer(); const auto other = current=="a"?"b":"a";
        const size_t before = controlled.game->GetPlayer(current).getCountResurses();
        test::Throws([&] { controlled.game->DropCards(other, Half(controlled.game->GetPlayer(other))); }, "discard order is enforced");
        test::Throws([&] { controlled.game->DropCards(current, {{Resurse::Wood,1}}); }, "must discard exact half");
        test::Equal(controlled.game->GetPlayer(current).getCountResurses(), before, "invalid discard changes nothing");
    }},
    {"robber must move to a different hex", [] {
        test::ControlledGame controlled({"a","b"}); test::CompleteSetup(*controlled.game,2);
        test::SeedResources(*controlled.game,"a",{{Resurse::Wood,8}}); test::SeedResources(*controlled.game,"b",{{Resurse::Clay,8}}); test::AddRoll(controlled,3,4);
        controlled.game->Dice(controlled.game->GetCurrentPlayer());
        for (size_t i=0; i<2; ++i) { const auto n=controlled.game->GetCurrentPlayer(); controlled.game->DropCards(n,Half(controlled.game->GetPlayer(n))); }
        const auto current = controlled.game->GetCurrentPlayer();
        size_t robber_hex=0;
        for(size_t i=0;i<controlled.game->GetMap().GetGexes().size();++i) if(controlled.game->GetMap().GetGexes()[i].isBandit()) { robber_hex=i; break; }
        test::Throws([&] { controlled.game->BanditMove(current,robber_hex); }, "robber cannot stay on its current hex");
        test::Check(test::Step(*controlled.game).find("BanditMove") != std::string::npos, "invalid move keeps robber phase active");
    }},
    {"robber validates ids, self-theft and victim adjacency", [] {
        test::ControlledGame controlled({"a","b"}); test::CompleteSetup(*controlled.game,2);
        test::SeedResources(*controlled.game,"a",{{Resurse::Wood,8}}); test::SeedResources(*controlled.game,"b",{{Resurse::Clay,8}}); test::AddRoll(controlled,3,4);
        controlled.game->Dice(controlled.game->GetCurrentPlayer());
        for (size_t i=0; i<2; ++i) { const auto n=controlled.game->GetCurrentPlayer(); controlled.game->DropCards(n,Half(controlled.game->GetPlayer(n))); }
        const std::string current=controlled.game->GetCurrentPlayer();
        const std::string other=current=="a"?"b":"a";
        test::Throws([&] { controlled.game->BanditMove(current,19); }, "hex id is range checked");
        test::Throws([&] { controlled.game->BanditMove(current,0,current); }, "self-theft is forbidden");
        size_t occupied=0; for (size_t i=0;i<19;++i) { for (auto* n:controlled.game->GetMap().GetGexes()[i].GetNodes()) if(n->getBuilding()){occupied=i;break;} }
        test::Throws([&] { controlled.game->BanditMove(current,occupied,other); }, "named victim must have adjacent building");
    }},
}); }
