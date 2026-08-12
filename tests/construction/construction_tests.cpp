#include "fixture.hpp"

using namespace ivv::catan;

int main() { return test::Run({
    {"placement queries expose only currently actionable board targets", [] {
        test::ControlledGame controlled({"a","b"}); auto& game=*controlled.game;
        size_t settlement=54;
        for(size_t i=0;i<54;++i) if(game.CanBuildSettlement(i)){settlement=i;break;}
        test::Check(settlement<54,"setup exposes a settlement target");
        test::Check(!game.CanBuildSettlement(54) && !game.CanBuildRoad(72) && !game.CanBuildCastle(54),"query ids are range checked");
        game.BuildSettlement(game.GetCurrentPlayer(),settlement);
        size_t road=72;
        for(size_t i=0;i<72;++i) if(game.CanBuildRoad(i)){road=i;break;}
        test::Check(road<72,"road query follows the placed setup settlement");
        const auto resources=test::ResourceCounts(game.GetPlayer(game.GetCurrentPlayer()));
        test::Check(game.CanBuildRoad(road),"repeated query is stable");
        test::Check(test::ResourceCounts(game.GetPlayer(game.GetCurrentPlayer()))==resources,"query does not consume resources");
    }},
    {"new player starts without resource cards", [] {
        Player player("builder", 0);
        test::Equal(player.getCountResurses(), size_t{0}, "Catan players start with no bank resources before setup production");
    }},
    {"road, settlement, city and development card prices are exact", [] {
        Player p("builder", 0); test::SeedResources(p, {{Resurse::Wood,2},{Resurse::Clay,2},{Resurse::Hay,4},{Resurse::Sheep,2},{Resurse::Stone,4}});
        test::Check(p.HaveRoadResurses(), "road costs one wood and one clay");
        auto wood=p.getCountResurses(Resurse::Wood), clay=p.getCountResurses(Resurse::Clay);
        p.FreeRoadResurses();
        test::Equal(p.getCountResurses(Resurse::Wood),wood-1,"road consumes one wood");
        test::Equal(p.getCountResurses(Resurse::Clay),clay-1,"road consumes one clay");
        test::Check(p.HaveSettlemenResurses(), "settlement price must be recognized"); p.FreeSettlemenResurses();
        test::Equal(p.getCountResurses(Resurse::Wood),wood-2,"settlement consumes one wood");
        test::Equal(p.getCountResurses(Resurse::Clay),clay-2,"settlement consumes one clay");
        auto hay=p.getCountResurses(Resurse::Hay), stone=p.getCountResurses(Resurse::Stone);
        test::Check(p.HaveCastleResurses(), "city costs two hay and three stone"); p.FreeCastleResurses();
        test::Equal(p.getCountResurses(Resurse::Hay),hay-2,"city consumes two hay");
        test::Equal(p.getCountResurses(Resurse::Stone),stone-3,"city consumes three stone");
        hay=p.getCountResurses(Resurse::Hay); stone=p.getCountResurses(Resurse::Stone);
        auto sheep=p.getCountResurses(Resurse::Sheep);
        test::Check(p.HaveDevCardResurses(), "development card costs hay, sheep, stone"); p.FreeDevCardResurses();
        test::Equal(p.getCountResurses(Resurse::Hay),hay-1,"card consumes one hay");
        test::Equal(p.getCountResurses(Resurse::Sheep),sheep-1,"card consumes one sheep");
        test::Equal(p.getCountResurses(Resurse::Stone),stone-1,"card consumes one stone");
    }},
    {"settlements obey distance rule and ordinary settlement needs own road", [] {
        Map map; Player a("a",0), b("b",1);
        map.placeStartBuilding(0, &a);
        test::Check(!map.canPlaceStartBuilding(1), "adjacent intersection violates distance rule");
        test::Check(map.canPlaceStartBuilding(2), "intersection two edges away is legal");
        test::Check(!map.canPlaceBuilding(2, b), "ordinary building requires player's connected road");
        map.placeRoad(0, &a);
        test::Check(!map.canPlaceBuilding(2, a), "a non-adjacent owned road is insufficient");
    }},
    {"city upgrade requires owner's settlement", [] {
        Map map; Player a("a",0), b("b",1);
        map.placeStartBuilding(0, &a);
        test::Check(map.canPlaceCastle(0, a), "owner may upgrade settlement");
        test::Check(!map.canPlaceCastle(0, b), "opponent may not upgrade it");
        test::Check(!map.canPlaceCastle(2, a), "empty intersection cannot become city");
    }},
    {"building, market and dice enforce phase and current player", [] {
        test::ControlledGame controlled({"a","b"}); auto& game=*controlled.game; const auto current=game.GetCurrentPlayer(); const auto other=current=="a"?"b":"a";
        test::Throws([&]{game.Dice(current);},"dice is forbidden during initial placement");
        test::Throws([&]{game.Market(current,Resurse::Wood,Resurse::Clay);},"market is forbidden during setup");
        test::Throws([&]{game.BuildRoad(current,0);},"road cannot precede setup settlement");
        test::CompleteSetup(game,2); test::AddRoll(controlled,1,1);
        test::Throws([&]{game.Dice(other);},"only current player rolls dice");
        game.Dice(current);
        test::SeedResources(game,current,{{Resurse::Wood,4}});
        test::Throws([&]{game.Market(other,Resurse::Wood,Resurse::Clay);},"only current player trades with bank");
        test::Throws([&]{game.BuildSettlement(other,54);},"out-of-turn player cannot build");
    }},
    {"invalid settlement, road and city ids are rejected without spending", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); auto& game=*controlled.game; const auto current=game.GetCurrentPlayer();
        test::SeedResources(game,current,{{Resurse::Wood,2},{Resurse::Clay,2},{Resurse::Hay,2},{Resurse::Sheep,1},{Resurse::Stone,3}});
        const auto before=test::ResourceCounts(game.GetPlayer(current)); const auto settlements=game.GetPlayer(current).getFreeSettlementCount(); const auto roads=game.GetPlayer(current).getFreeRoadCount(); const auto cities=game.GetPlayer(current).getFreeCastleCount();
        test::Throws([&]{game.BuildSettlement(current,54);},"node 54 is invalid");
        test::Throws([&]{game.BuildRoad(current,72);},"facet 72 is invalid");
        test::Throws([&]{game.BuildCastle(current,54);},"city node 54 is invalid");
        test::Equal(test::ResourceCounts(game.GetPlayer(current)),before,"invalid ids spend nothing");
        test::Equal(game.GetPlayer(current).getFreeSettlementCount(),settlements,"invalid settlement leaks no piece");
        test::Equal(game.GetPlayer(current).getFreeRoadCount(),roads,"invalid road leaks no piece");
        test::Equal(game.GetPlayer(current).getFreeCastleCount(),cities,"invalid city leaks no piece");
    }},
}); }
