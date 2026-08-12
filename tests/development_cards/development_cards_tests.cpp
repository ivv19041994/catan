#include "fixture.hpp"

using namespace ivv::catan;

namespace {
void Ready(GameController& game, const std::string& player, DevelopmentCard card) {
    auto& p = test::MutablePlayer(game, player);
    p.PutCard(card); p.OnEndTurn();
}
}

int main() { return test::Run({
    {"buy draws fixed top card and pays exact price", [] {
        test::ControlledGame controlled({"a","b"},{},{},{DevelopmentCard::Monopoly}); test::EnterCommonPlay(controlled);
        const auto current=controlled.game->GetCurrentPlayer();
        test::SeedResources(*controlled.game,current,{{Resurse::Hay,1},{Resurse::Sheep,1},{Resurse::Stone,1}});
        controlled.game->DevCard(current);
        test::Equal(controlled.deck->draws,size_t{1},"deck Draw called once");
        test::Equal(controlled.game->GetPlayer(current).GetPurchasedCardCount(DevelopmentCard::Monopoly),size_t{1},"fixed top card is purchased");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(current)),std::array<size_t,5>{},"development card consumes its exact seeded price");
        test::Throws([&]{controlled.game->UseDevCard(current,DevelopmentCard::Monopoly,Resurse::Wood);},"action card cannot be used in purchase turn");
    }},
    {"empty development deck rejects purchase without payment", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled);
        const auto current=controlled.game->GetCurrentPlayer();
        test::SeedResources(*controlled.game,current,{{Resurse::Hay,1},{Resurse::Sheep,1},{Resurse::Stone,1}});
        const auto before=test::ResourceCounts(controlled.game->GetPlayer(current));
        test::Throws([&]{controlled.game->DevCard(current);},"empty deck must reject purchase");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(current)),before,"failed purchase is atomic");
    }},
    {"YearOfPlenty grants its two requested resources including duplicates", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); const auto current=controlled.game->GetCurrentPlayer();
        test::SeedResources(*controlled.game,current,{});
        Ready(*controlled.game,current,DevelopmentCard::YearOfPlenty);
        const auto before=controlled.game->GetPlayer(current).getCountResurses(Resurse::Stone);
        controlled.game->UseDevCard(current,DevelopmentCard::YearOfPlenty,std::array<Resurse,2>{Resurse::Stone,Resurse::Stone});
        test::Equal(controlled.game->GetPlayer(current).getCountResurses(Resurse::Stone),before+2,"two identical requested cards are both granted");
    }},
    {"Monopoly transfers every selected resource from every opponent", [] {
        test::ControlledGame controlled({"a","b","c"}); test::EnterCommonPlay(controlled,3); const auto current=controlled.game->GetCurrentPlayer();
        for(const char* n:{"a","b","c"}) test::SeedResources(*controlled.game,n,current==n ? std::map<Resurse,size_t>{} : std::map<Resurse,size_t>{{Resurse::Wood,3}});
        Ready(*controlled.game,current,DevelopmentCard::Monopoly);
        size_t expected=controlled.game->GetPlayer(current).getCountResurses(Resurse::Wood);
        for(const char* n:{"a","b","c"}) if(current!=n) expected+=controlled.game->GetPlayer(n).getCountResurses(Resurse::Wood);
        controlled.game->UseDevCard(current,DevelopmentCard::Monopoly,Resurse::Wood);
        test::Equal(controlled.game->GetPlayer(current).getCountResurses(Resurse::Wood),expected,"current player receives all selected cards");
        for(const char* n:{"a","b","c"}) if(current!=n) test::Equal(controlled.game->GetPlayer(n).getCountResurses(Resurse::Wood),size_t{0},"opponents surrender all selected cards");
    }},
    {"wrong YearOfPlenty variant throws and does not consume card", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); const auto current=controlled.game->GetCurrentPlayer();
        test::SeedResources(*controlled.game,current,{{Resurse::Clay,2}});
        Ready(*controlled.game,current,DevelopmentCard::YearOfPlenty); const auto before=test::ResourceCounts(controlled.game->GetPlayer(current));
        test::Throws([&]{controlled.game->UseDevCard(current,DevelopmentCard::YearOfPlenty,Resurse::Wood);},"wrong variant must be rejected");
        test::Equal(controlled.game->GetPlayer(current).GetReadyForUseCardCount(DevelopmentCard::YearOfPlenty),size_t{1},"invalid parameter must not consume card");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(current)),before,"invalid parameter must not change resources");
    }},
    {"wrong Monopoly variant throws and does not consume card", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); const auto current=controlled.game->GetCurrentPlayer();
        const auto other=current=="a"?"b":"a"; test::SeedResources(*controlled.game,current,{{Resurse::Clay,2}}); test::SeedResources(*controlled.game,other,{{Resurse::Wood,3}});
        Ready(*controlled.game,current,DevelopmentCard::Monopoly);
        const auto current_before=test::ResourceCounts(controlled.game->GetPlayer(current)); const auto other_before=test::ResourceCounts(controlled.game->GetPlayer(other));
        test::Throws([&]{controlled.game->UseDevCard(current,DevelopmentCard::Monopoly,std::array<Resurse,2>{Resurse::Wood,Resurse::Clay});},"wrong variant must be rejected");
        test::Equal(controlled.game->GetPlayer(current).GetReadyForUseCardCount(DevelopmentCard::Monopoly),size_t{1},"invalid parameter must not consume card");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(current)),current_before,"wrong variant must not change current resources");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(other)),other_before,"wrong variant must not change opponent resources");
    }},
    {"null parameter throws and is atomic", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); const auto current=controlled.game->GetCurrentPlayer();
        test::SeedResources(*controlled.game,current,{{Resurse::Clay,2}}); const auto before=test::ResourceCounts(controlled.game->GetPlayer(current));
        Ready(*controlled.game,current,DevelopmentCard::YearOfPlenty);
        test::Throws([&]{controlled.game->UseDevCard(current,DevelopmentCard::YearOfPlenty,std::nullopt);},"null parameter must throw a normal exception");
        test::Equal(controlled.game->GetPlayer(current).GetReadyForUseCardCount(DevelopmentCard::YearOfPlenty),size_t{1},"null must not consume card");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(current)),before,"null must not change resources");
    }},
    {"Not resource parameters are rejected atomically", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); const auto current=controlled.game->GetCurrentPlayer();
        test::SeedResources(*controlled.game,current,{{Resurse::Clay,2}}); const auto before=test::ResourceCounts(controlled.game->GetPlayer(current));
        Ready(*controlled.game,current,DevelopmentCard::YearOfPlenty);
        test::Throws([&]{controlled.game->UseDevCard(current,DevelopmentCard::YearOfPlenty,std::array<Resurse,2>{Resurse::Wood,Resurse::Not});},"Not is not a resource card");
        test::Equal(controlled.game->GetPlayer(current).GetReadyForUseCardCount(DevelopmentCard::YearOfPlenty),size_t{1},"Not must not consume card");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(current)),before,"Not must not change resources");
    }},
    {"Monopoly rejects Not without changing any player", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); const auto current=controlled.game->GetCurrentPlayer(); const auto other=current=="a"?"b":"a";
        test::SeedResources(*controlled.game,current,{{Resurse::Clay,2}}); test::SeedResources(*controlled.game,other,{{Resurse::Wood,3}}); Ready(*controlled.game,current,DevelopmentCard::Monopoly);
        const auto current_before=test::ResourceCounts(controlled.game->GetPlayer(current)); const auto other_before=test::ResourceCounts(controlled.game->GetPlayer(other));
        test::Throws([&]{controlled.game->UseDevCard(current,DevelopmentCard::Monopoly,Resurse::Not);},"Monopoly cannot select Not");
        test::Equal(controlled.game->GetPlayer(current).GetReadyForUseCardCount(DevelopmentCard::Monopoly),size_t{1},"Not must not consume Monopoly");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(current)),current_before,"Not leaves current resources unchanged");
        test::Equal(test::ResourceCounts(controlled.game->GetPlayer(other)),other_before,"Not leaves opponent resources unchanged");
    }},
    {"knight opens robber movement even before dice", [] {
        test::ControlledGame controlled({"a","b"}); test::CompleteSetup(*controlled.game,2); const auto current=controlled.game->GetCurrentPlayer();
        test::SeedResources(*controlled.game,current,{});
        Ready(*controlled.game,current,DevelopmentCard::Knights);
        controlled.game->UseDevCard(current,DevelopmentCard::Knights,std::nullopt);
        test::Check(test::Step(*controlled.game).find("BanditMove")!=std::string::npos,"knight enters robber movement");
    }},
    {"RoadBuilding places two roads for free then returns to common play", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); const auto current=controlled.game->GetCurrentPlayer();
        test::SeedResources(*controlled.game,current,{});
        Ready(*controlled.game,current,DevelopmentCard::RoadBuilding);
        const auto resources=controlled.game->GetPlayer(current).getCountResurses();
        const auto free_roads=controlled.game->GetPlayer(current).getFreeRoadCount();
        controlled.game->UseDevCard(current,DevelopmentCard::RoadBuilding,std::nullopt);
        controlled.game->BuildRoad(current,1); controlled.game->BuildRoad(current,7);
        test::Equal(controlled.game->GetPlayer(current).getFreeRoadCount(),free_roads-2,"card places exactly two roads");
        test::Equal(controlled.game->GetPlayer(current).getCountResurses(),resources,"free roads consume no resources");
        test::Check(test::Step(*controlled.game).find("CommonPlay")!=std::string::npos,"two roads complete the effect");
    }},
}); }
