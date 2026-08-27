#include "fixture.hpp"

#include <map>

using namespace ivv::catan;

int main() { return test::Run({
    {"piece pools contain exactly 5 settlements, 4 cities and 15 roads", [] {
        Player p("pieces",0);
        test::Equal(p.getFreeSettlementCount(),size_t{5},"five settlements");
        test::Equal(p.getFreeCastleCount(),size_t{4},"four cities");
        test::Equal(p.getFreeRoadCount(),size_t{15},"fifteen roads");
        for(size_t i=0;i<5;++i) p.getFreeSettlement()->setBusy();
        for(size_t i=0;i<4;++i) p.getFreeCastle()->setBusy();
        for(size_t i=0;i<15;++i) p.getFreeRoad()->setBusy();
        test::Check(p.getFreeSettlement()==nullptr,"sixth settlement is refused");
        test::Check(p.getFreeCastle()==nullptr,"fifth city is refused");
        test::Check(p.getFreeRoad()==nullptr,"sixteenth road is refused");
        test::Equal(p.getFreeSettlementCount(),size_t{0},"refusal does not corrupt settlement pool");
        test::Equal(p.getFreeCastleCount(),size_t{0},"refusal does not corrupt city pool");
        test::Equal(p.getFreeRoadCount(),size_t{0},"refusal does not corrupt road pool");
    }},
    {"standard development deck has exact 25-card composition", [] {
        DevelopmentCardDeck deck; std::map<DevelopmentCard,size_t> cards; size_t count=0;
        while(!deck.Empty()) { ++cards[deck.Draw()]; ++count; }
        test::Equal(count,size_t{25},"deck contains 25 cards");
        test::Equal(cards[DevelopmentCard::Knights],size_t{14},"fourteen knights");
        test::Equal(cards[DevelopmentCard::RoadBuilding],size_t{2},"two RoadBuilding cards");
        test::Equal(cards[DevelopmentCard::YearOfPlenty],size_t{2},"two YearOfPlenty cards");
        test::Equal(cards[DevelopmentCard::Monopoly],size_t{2},"two Monopoly cards");
        for(auto card:{DevelopmentCard::University,DevelopmentCard::Market,DevelopmentCard::GreatHall,DevelopmentCard::Chapel,DevelopmentCard::Library})
            test::Equal(cards[card],size_t{1},"each victory card appears once");
        test::Throws([&]{deck.Draw();},"drawing exhausted deck is rejected");
    }},
    {"victory card scores immediately and cannot be used", [] {
        Player p("winner",0); p.PutCard(DevelopmentCard::University);
        const auto points=p.GetWinPoints(); const auto owned=p.GetReadyForUseCardCount(DevelopmentCard::University);
        test::Equal(points,size_t{1},"owned victory card scores immediately");
        test::Equal(p.GetPublicWinPoints(),size_t{0},"hidden victory card is absent from public score");
        test::Equal(p.GetVictoryPointCardCount(),size_t{1},"owner can inspect hidden victory-card count");
        test::Throws([&]{p.Use(DevelopmentCard::University);},"victory card is not played");
        test::Equal(p.GetWinPoints(),points,"rejected use preserves points");
        test::Equal(p.GetReadyForUseCardCount(DevelopmentCard::University),owned,"rejected use preserves ownership");
        test::Equal(p.GetUsedCardCount(DevelopmentCard::University),size_t{0},"rejected use is not recorded");
    }},
}); }
