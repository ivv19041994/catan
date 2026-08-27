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
    {"progress cards may be played before dice and preserve the roll phase", [] {
        test::ControlledGame plenty({"a","b"}); test::CompleteSetup(*plenty.game,2);
        const auto plenty_player=plenty.game->GetCurrentPlayer();
        Ready(*plenty.game,plenty_player,DevelopmentCard::YearOfPlenty);
        plenty.game->UseDevCard(plenty_player,DevelopmentCard::YearOfPlenty,
            std::array<Resurse,2>{Resurse::Wood,Resurse::Clay});
        test::Equal(plenty.game->GetStep(),GameController::GameStep::DiceDrop,
            "Year of Plenty before roll returns to dice");

        test::ControlledGame monopoly({"a","b"}); test::CompleteSetup(*monopoly.game,2);
        const auto monopoly_player=monopoly.game->GetCurrentPlayer();
        const auto other=monopoly_player=="a"?"b":"a";
        test::SeedResources(*monopoly.game,other,{{Resurse::Stone,2}});
        Ready(*monopoly.game,monopoly_player,DevelopmentCard::Monopoly);
        monopoly.game->UseDevCard(monopoly_player,DevelopmentCard::Monopoly,Resurse::Stone);
        test::Equal(monopoly.game->GetStep(),GameController::GameStep::DiceDrop,
            "Monopoly before roll returns to dice");

        test::ControlledGame roads({"a","b"}); test::CompleteSetup(*roads.game,2);
        const auto road_player=roads.game->GetCurrentPlayer();
        Ready(*roads.game,road_player,DevelopmentCard::RoadBuilding);
        roads.game->UseDevCard(road_player,DevelopmentCard::RoadBuilding,std::nullopt);
        roads.game->BuildRoad(road_player,1); roads.game->BuildRoad(road_player,7);
        test::Equal(roads.game->GetStep(),GameController::GameStep::DiceDrop,
            "Road Building before roll returns to dice after two roads");
    }},
    {"Year of Plenty is atomic when the bank cannot supply both cards", [] {
        test::ControlledGame controlled({"a","b"}); test::CompleteSetup(*controlled.game,2); auto& game=*controlled.game;
        size_t safe_sum=0;
        for(size_t candidate=2;candidate<=12;++candidate) {
            if(candidate==7) continue;
            bool wood=false;
            for(const auto& hex:game.GetMap().GetGexes())
                wood=wood||(hex.getType()==Resurse::Wood&&hex.getDice()==static_cast<int>(candidate));
            if(!wood){safe_sum=candidate;break;}
        }
        test::Check(safe_sum!=0,"map has a roll that does not produce wood");
        while(game.GetResourceBank().Count(Resurse::Wood)>1) {
            const auto current=game.GetCurrentPlayer();
            Ready(game,current,DevelopmentCard::YearOfPlenty);
            const bool take_two=game.GetResourceBank().Count(Resurse::Wood)>2;
            game.UseDevCard(current,DevelopmentCard::YearOfPlenty,
                std::array<Resurse,2>{Resurse::Wood,take_two?Resurse::Wood:Resurse::Clay});
            const size_t first=safe_sum>7?6:1;
            test::AddRoll(controlled,first,safe_sum-first); game.Dice(current); game.Pass(current);
        }
        const auto current=game.GetCurrentPlayer();
        Ready(game,current,DevelopmentCard::YearOfPlenty);
        const auto before=test::ResourceCounts(game.GetPlayer(current));
        test::Throws([&]{game.UseDevCard(current,DevelopmentCard::YearOfPlenty,
            std::array<Resurse,2>{Resurse::Wood,Resurse::Wood});},"two cards cannot be created from one");
        test::Equal(game.GetResourceBank().Count(Resurse::Wood),size_t{1},"failed card does not drain bank");
        test::Equal(test::ResourceCounts(game.GetPlayer(current)),before,"failed card grants nothing");
        test::Equal(game.GetPlayer(current).GetReadyForUseCardCount(DevelopmentCard::YearOfPlenty),size_t{1},
            "failed card is not consumed");
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
    {"RoadBuilding is not consumed when no road piece can be placed", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); auto& game=*controlled.game;
        const auto current=game.GetCurrentPlayer();
        test::SeedResources(game,current,{{Resurse::Wood,20},{Resurse::Clay,20}});
        while(game.GetPlayer(current).getFreeRoadCount()>0) {
            size_t target=Map::facets_count;
            for(size_t road=0;road<Map::facets_count;++road) if(game.CanBuildRoad(road)){target=road;break;}
            test::Check(target<Map::facets_count,"connected network can consume every road piece");
            game.BuildRoad(current,target);
        }
        Ready(game,current,DevelopmentCard::RoadBuilding);
        test::Throws([&]{game.UseDevCard(current,DevelopmentCard::RoadBuilding,std::nullopt);},
            "card cannot start without a legal placement");
        test::Equal(game.GetPlayer(current).GetReadyForUseCardCount(DevelopmentCard::RoadBuilding),size_t{1},
            "rejected card stays in hand");
    }},
    {"RoadBuilding ends after one road when that is the final piece", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); auto& game=*controlled.game;
        const auto current=game.GetCurrentPlayer();
        test::SeedResources(game,current,{{Resurse::Wood,20},{Resurse::Clay,20}});
        while(game.GetPlayer(current).getFreeRoadCount()>1) {
            size_t target=Map::facets_count;
            for(size_t road=0;road<Map::facets_count;++road) if(game.CanBuildRoad(road)){target=road;break;}
            test::Check(target<Map::facets_count,"road network remains expandable");
            game.BuildRoad(current,target);
        }
        Ready(game,current,DevelopmentCard::RoadBuilding);
        game.UseDevCard(current,DevelopmentCard::RoadBuilding,std::nullopt);
        size_t target=Map::facets_count;
        for(size_t road=0;road<Map::facets_count;++road) if(game.CanBuildRoad(road)){target=road;break;}
        test::Check(target<Map::facets_count,"last road has a legal target");
        game.BuildRoad(current,target);
        test::Equal(game.GetStep(),GameController::GameStep::CommonPlay,
            "effect ends early instead of waiting for an impossible second road");
    }},
}); }
