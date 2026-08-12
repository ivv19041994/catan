#include "fixture.hpp"

using namespace ivv::catan;

int main() { return test::Run({
    {"settlements, cities, largest army and longest road score correctly", [] {
        Map map; Player p("scorer",0);
        map.placeStartBuilding(0,&p); map.placeStartBuilding(3,&p);
        test::Equal(p.GetWinPoints(),size_t{2},"each settlement scores one point");
        map.placeCastle(0,p);
        test::Equal(p.GetWinPoints(),size_t{3},"upgrading settlement to city adds one net point");
        p.SetKnightCard(); test::Equal(p.GetWinPoints(),size_t{5},"largest army adds two points");
        p.SetRoadCard(); test::Equal(p.GetWinPoints(),size_t{7},"longest road adds two points");
        p.ResetKnightCard(); p.ResetRoadCard(); test::Equal(p.GetWinPoints(),size_t{3},"losing awards removes their points");
    }},
    {"five connected roads earn longest road", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); auto& game=*controlled.game;
        const auto current=game.GetCurrentPlayer(); const auto before=game.GetPlayer(current).GetWinPoints();
        test::SeedResources(game,current,{{Resurse::Wood,4},{Resurse::Clay,4}});
        for(size_t road:{1u,7u,12u,19u}) game.BuildRoad(current,road);
        test::Check(game.GetPlayer(current).GetRoadSize()>=5,"road algorithm sees a chain of five");
        test::Equal(game.GetPlayer(current).GetWinPoints(),before+2,"unique road length five owns the award");
    }},
    {"longest-road incumbent survives a tie and strict lead transfers award", [] {
        test::ControlledGame controlled({"a","b"}); test::EnterCommonPlay(controlled); auto& game=*controlled.game;
        const std::string incumbent=game.GetCurrentPlayer(), challenger=incumbent=="a"?"b":"a";
        test::SeedResources(game,incumbent,{{Resurse::Wood,4},{Resurse::Clay,4}});
        for(size_t road:{1u,7u,12u,19u}) game.BuildRoad(incumbent,road);
        game.Pass(incumbent); test::AddRoll(controlled,1,1); game.Dice(challenger);
        test::SeedResources(game,challenger,{{Resurse::Wood,4},{Resurse::Clay,4}});
        for(size_t road:{3u,4u,9u}) game.BuildRoad(challenger,road);
        const bool tie_preserved=game.GetPlayer(incumbent).GetWinPoints()>=4 && game.GetPlayer(challenger).GetWinPoints()==2;
        game.BuildRoad(challenger,16);
        const bool lead_transferred=game.GetPlayer(incumbent).GetWinPoints()==2 && game.GetPlayer(challenger).GetWinPoints()>=4;
        test::Check(tie_preserved,"equal length preserves incumbent");
        test::Check(lead_transferred,"strictly longer road transfers award");
    }},
    {"victory cards can finish game and expose winner", [] {
        std::deque<DevelopmentCard> cards(8,DevelopmentCard::University);
        test::ControlledGame controlled({"a","b"},{},{},cards); test::EnterCommonPlay(controlled); auto& game=*controlled.game;
        const auto current=game.GetCurrentPlayer();
        test::SeedResources(game,current,{{Resurse::Hay,8},{Resurse::Sheep,8},{Resurse::Stone,8}});
        for(size_t i=0;i<8 && !game.Finish();++i) game.DevCard(current);
        test::Equal(game.GetPlayer(current).GetReadyForUseCardCount(DevelopmentCard::University),size_t{8},"victory point cards are owned immediately");
        test::Equal(game.GetPlayer(current).GetUsedCardCount(DevelopmentCard::University),size_t{0},"victory point cards are never played");
        test::Check(game.Finish(),"ten points finish the game");
        test::Check(game.GetWinner().has_value(),"winner is exposed");
        test::Equal(*game.GetWinner(),current,"ten-point player is winner");
        test::Check(test::Step(game).find("Finish")!=std::string::npos,"controller enters Finish step");
        const auto before=test::ResourceCounts(game.GetPlayer(current));
        test::Throws([&]{game.Dice(current);},"dice is rejected after Finish");
        test::Throws([&]{game.Market(current,Resurse::Hay,Resurse::Wood);},"market is rejected after Finish");
        test::Throws([&]{game.BuildRoad(current,1);},"building is rejected after Finish");
        test::Throws([&]{game.DevCard(current);},"card purchase is rejected after Finish");
        test::Equal(test::ResourceCounts(game.GetPlayer(current)),before,"post-finish actions leave resources unchanged");
    }},
    {"three used knights are threshold for largest army", [] {
        Player p("army",0);
        for(size_t i=0;i<3;++i) { p.PutCard(DevelopmentCard::Knights); p.OnEndTurn(); p.Use(DevelopmentCard::Knights); p.OnEndTurn(); }
        test::Equal(p.GetUsedCardCount(DevelopmentCard::Knights),size_t{3},"three knights are recorded");
        p.SetKnightCard(); test::Equal(p.GetWinPoints(),size_t{2},"largest-army award is worth two");
    }},
    {"largest-army incumbent survives a tie and strict lead transfers award", [] {
        test::ControlledGame controlled({"a","b"}); test::CompleteSetup(*controlled.game,2); auto& game=*controlled.game;
        test::SeedResources(game,"a",{}); test::SeedResources(game,"b",{});
        const std::string incumbent=game.GetCurrentPlayer(), challenger=incumbent=="a"?"b":"a";
        auto turn=[&](bool knight) {
            const auto player=game.GetCurrentPlayer();
            if(knight) {
                auto& p=test::MutablePlayer(game,player); p.PutCard(DevelopmentCard::Knights); p.OnEndTurn();
                game.UseDevCard(player,DevelopmentCard::Knights,std::nullopt);
                size_t target=0;
                for(size_t i=0;i<game.GetMap().GetGexes().size();++i) {
                    const auto& hex=game.GetMap().GetGexes()[i]; bool occupied=false;
                    for(const auto* node:hex.GetNodes()) occupied|=node->getBuilding()!=nullptr;
                    if(!hex.isBandit()&&!occupied) { target=i; break; }
                }
                game.BanditMove(player,target);
            }
            test::AddRoll(controlled,1,1); game.Dice(player); game.Pass(player);
        };
        for(size_t i=0;i<3;++i) { turn(true); turn(false); }
        test::Check(game.GetPlayer(incumbent).GetWinPoints()>=4,"three knights award incumbent two points");
        for(size_t i=0;i<3;++i) { turn(false); turn(true); }
        const bool tie_preserved=game.GetPlayer(incumbent).GetWinPoints()>=4 && game.GetPlayer(challenger).GetWinPoints()==2;
        turn(false); turn(true);
        const bool lead_transferred=game.GetPlayer(incumbent).GetWinPoints()==2 && game.GetPlayer(challenger).GetWinPoints()>=4;
        test::Check(tie_preserved,"equal armies preserve incumbent");
        test::Check(lead_transferred,"strictly larger army transfers award");
    }},
}); }
