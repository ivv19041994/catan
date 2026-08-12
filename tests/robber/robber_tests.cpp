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
        test::Check(!controlled.game->CanMoveBandit(robber_hex), "current robber hex is not a valid target");
        test::Check(controlled.game->CanMoveBandit((robber_hex+1)%19), "a different robber hex is a valid target");
        test::Check(!controlled.game->CanMoveBandit(19), "robber target query checks range");
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
        size_t non_adjacent=19;
        for (size_t i=0;i<19;++i) {
            if (controlled.game->GetMap().GetGexes()[i].isBandit()) continue;
            bool occupied=false, other_adjacent=false;
            for (auto* node : controlled.game->GetMap().GetGexes()[i].GetNodes()) {
                if (const Building* building=node->getBuilding()) {
                    occupied=true;
                    other_adjacent |= building->getPlayer()->getName()==other;
                }
            }
            if (occupied && !other_adjacent) { non_adjacent=i; break; }
        }
        test::Check(non_adjacent<19, "fixture provides an occupied hex away from the named victim");
        test::Throws([&] { controlled.game->BanditMove(current,non_adjacent,other); }, "named victim must have adjacent building");
    }},
    {"robber may move to a hex occupied only by the current player", [] {
        test::ControlledGame controlled({"a","b"}); test::CompleteSetup(*controlled.game,2);
        test::SeedResources(*controlled.game,"a",{{Resurse::Wood,8}}); test::SeedResources(*controlled.game,"b",{{Resurse::Clay,8}}); test::AddRoll(controlled,3,4);
        controlled.game->Dice(controlled.game->GetCurrentPlayer());
        for (size_t i=0; i<2; ++i) { const auto n=controlled.game->GetCurrentPlayer(); controlled.game->DropCards(n,Half(controlled.game->GetPlayer(n))); }
        const std::string current=controlled.game->GetCurrentPlayer();
        size_t target=19;
        for (size_t i=0;i<19;++i) {
            if (controlled.game->GetMap().GetGexes()[i].isBandit()) continue;
            bool own=false, opponent=false;
            for (auto* node : controlled.game->GetMap().GetGexes()[i].GetNodes()) {
                if (const Building* building=node->getBuilding()) {
                    own |= building->getPlayer()->getName()==current;
                    opponent |= building->getPlayer()->getName()!=current;
                }
            }
            if (own && !opponent) { target=i; break; }
        }
        test::Check(target<19, "fixture provides a current-player-only target hex");
        controlled.game->BanditMove(current,target);
        test::Check(controlled.game->GetMap().GetGexes()[target].isBandit(), "robber moved without stealing from self");
        test::Check(test::Step(*controlled.game).find("CommonPlay") != std::string::npos, "robber move completes");
    }},
}); }
