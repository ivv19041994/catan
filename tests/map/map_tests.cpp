#include "fixture.hpp"

#include <map>

using namespace ivv::catan;

int main() { return test::Run({
    {"standard map has 19 hexes, 54 numbered nodes and 72 facets", [] {
        Map map;
        test::Equal(map.GetGexes().size(),size_t{19},"map has 19 hexes");
        test::Equal(map.GetNodes().size(),size_t{54},"map has 54 nodes");
        test::Equal(map.GetFacets().size(),size_t{72},"map has 72 facets");
        const auto nodes=map.GetNodes();
        for(size_t i=0;i<nodes.size();++i) test::Equal(nodes[i].index,static_cast<int>(i),"node ids are consecutive 0..53");
    }},
    {"standard map resource and number-token composition is exact", [] {
        Map map; std::map<Resurse,size_t> resources; std::map<int,size_t> numbers;
        for(const auto& hex:map.GetGexes()) { ++resources[hex.getType()]; ++numbers[hex.getDice()]; }
        test::Equal(resources[Resurse::Wood],size_t{4},"four wood hexes");
        test::Equal(resources[Resurse::Clay],size_t{3},"three clay hexes");
        test::Equal(resources[Resurse::Hay],size_t{4},"four hay hexes");
        test::Equal(resources[Resurse::Sheep],size_t{4},"four sheep hexes");
        test::Equal(resources[Resurse::Stone],size_t{3},"three stone hexes");
        test::Equal(resources[Resurse::Not],size_t{1},"one desert");
        const std::map<int,size_t> expected{{0,1},{2,1},{3,2},{4,2},{5,2},{6,2},{8,2},{9,2},{10,2},{11,2},{12,1}};
        test::Equal(numbers,expected,"number tokens match standard composition and desert has no number");
    }},
    {"settlement produces one, city two, and robber blocks only its own hex", [] {
        Map map; Player blocked("blocked",0), open("open",1); test::SeedResources(blocked,{}); test::SeedResources(open,{});
        map.placeStartBuilding(0,&blocked); map.placeCastle(0,blocked);
        map.placeStartBuilding(3,&open);
        auto& hexes=map.GetGexes(); hexes[0].setType(Resurse::Wood); hexes[1].setType(Resurse::Wood);
        Bandit robber; hexes[0].setBandit(robber);
        hexes[0].diceEvent(); hexes[1].diceEvent();
        test::Equal(blocked.getCountResurses(Resurse::Wood),size_t{0},"robber blocks city production on its hex");
        test::Equal(open.getCountResurses(Resurse::Wood),size_t{1},"other hex still produces one for settlement");
        hexes[1].setBandit(robber); hexes[0].diceEvent();
        test::Equal(blocked.getCountResurses(Resurse::Wood),size_t{2},"unblocked city production is two");
    }},
    {"road cannot continue through an opponent building", [] {
        Map map; Player a("a",0), b("b",1);
        map.placeStartBuilding(0,&a); map.placeStartBuilding(2,&b);
        map.placeRoad(0,&a);
        test::Check(map.canPlaceRoad(1,&a),"road may terminate at opponent building");
        map.placeRoad(1,&a);
        test::Check(!map.canPlaceRoad(7,&a),"opponent building blocks continuation through its node");
    }},
}); }
