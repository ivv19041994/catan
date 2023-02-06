#include "game_controller.hpp"

#include "map_rander.h"
#include "catan.hpp"

#include <cassert>
#include <algorithm>

namespace ivv {
namespace catan {

GameController::GameController(std::initializer_list<std::string> players_il) : GameController(std::vector<std::string>(players_il.begin(), players_il.end()))
{

}

void GameController::MixPlayers() {
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(players_.begin(), players_.end(), g);
}

void GameController::BindPlayers() {
	for (auto& player : players_) {
		player_by_name_[player.getName()] = &player;
	}
	if (player_by_name_.size() != players_.size()) {
		throw out_of_range("Players names must be different");
	}
}

void GameController::InitCardsDeque() {
	cards_deque_.insert(cards_deque_.end(), 14, DevelopmentCard::Knights);
	cards_deque_.insert(cards_deque_.end(), 2, DevelopmentCard::RoadBuilding);
	cards_deque_.insert(cards_deque_.end(), 2, DevelopmentCard::YearOfPlenty);
	cards_deque_.insert(cards_deque_.end(), 2, DevelopmentCard::Monopoly);
	cards_deque_.insert(cards_deque_.end(), 1, DevelopmentCard::University);
	cards_deque_.insert(cards_deque_.end(), 1, DevelopmentCard::Market);
	cards_deque_.insert(cards_deque_.end(), 1, DevelopmentCard::GreatHall);
	cards_deque_.insert(cards_deque_.end(), 1, DevelopmentCard::Chapel);
	cards_deque_.insert(cards_deque_.end(), 1, DevelopmentCard::Library);

	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(cards_deque_.begin(), cards_deque_.end(), g);
}

void GameController::DevCard(std::string_view player) {
	using namespace std::string_literals;

	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Buy dev card is not aviable on this game step!"s);
	}
	Player& p = CheckCurrentPlayer(player);
	if (cards_deque_.size() == 0) {
		throw logic_error("Dev card deque is empty!"s);
	}

	if (!p.HaveDevCardResurses()) {
		throw logic_error("Player " + p.getName() + "haven't reusrse!"s);
	}
	

	DevelopmentCard card = cards_deque_.front();
	p.PutCard(card);
	p.FreeDevCardResurses();
	cards_deque_.pop_front();
}

void GameController::UseDevCard(std::string_view player, DevelopmentCard card, UseDevCardParam param) {
	using namespace std::string_literals;
	if (step_ != GameStep::CommonPlay && 
		(step_ != GameStep::DiceDrop || card != DevelopmentCard::Knights)
		) {
		throw logic_error("Use dev card is not aviable on this game step!"s);
	}
	Player& p = CheckCurrentPlayer(player);

	switch (card) {
	case DevelopmentCard::YearOfPlenty:
		if (!param && std::holds_alternative<std::array<Resurse, 2>>(*param)) {
			throw logic_error("Invalid use dev card param!"s);
		}
		break;
	case DevelopmentCard::Monopoly:
		if (!param && std::holds_alternative<Resurse>(*param)) {
			throw logic_error("Invalid use dev card param!"s);
		}
		break;
	}

	p.Use(card);

	switch (card) {
	case DevelopmentCard::Knights:
		step_after_bandit_ = step_;
		step_ = GameStep::BanditMove;
		CheckKnightsCard();
		break;
	case DevelopmentCard::RoadBuilding:
		if (p.getFreeRoad()) {
			step_ = GameStep::RoadBuilding;
			road_building_count_ = 0;
		}
		break;
	case DevelopmentCard::YearOfPlenty:
	{
		auto& res = std::get<std::array<Resurse, 2>>(*param);
		p.addResurse(res[0]);
		p.addResurse(res[1]);
	}
		break;
	case DevelopmentCard::Monopoly:
		{
			Resurse res = std::get<Resurse>(*param);

			for (Player& player : players_) {
				if (&p != &player) {
					p.addResurse(res, player.StillAll(res));
				}
			}
		}
		break;
	}

	CheckWinner();
}

void GameController::SetDeal(std::string_view player, std::map<Resurse, size_t> sell, std::map<Resurse, size_t> buy) {
	using namespace std::string_literals;

	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Deal is not aviable on this game step!"s);
	}

	if (sell.size() == 0 || buy.size() == 0) {
		throw logic_error("Deal must be fair!");
	}

	auto pplayer = player_by_name_.find(player);
	if (pplayer == player_by_name_.end()) {
		throw logic_error("Player "s + std::string(player) + " is not created!"s);
	}
	Player& p = *pplayer->second;

	if (!p.Have(sell)) {
		throw logic_error("Player "s + std::string(player) + " haven't resurses for deal!"s);
	}

	if (&p == &players_[current_player_]) {
		activ_deal_ = { std::move(sell), std::move(buy) };
		return;
	}

	if (!activ_deal_) {
		throw logic_error("Player "s + players_[current_player_].getName() + " does not sell anything!"s);
	}

	if (!players_[current_player_].Have(buy)) {
		activ_deal_.reset();
		throw logic_error("Player "s + players_[current_player_].getName() + " haven't resurses for deal!"s);
	}

	if (activ_deal_->sell != buy ||
		activ_deal_->buy != sell) {
		throw logic_error("Player "s + players_[current_player_].getName() + " does not sell this!"s);
	}

	p.addResurse(buy);
	try {
		players_[current_player_].addResurse(sell);
	}
	catch (std::exception& e) {
		p.Drop(buy);
		throw e;
	}

	p.Drop(sell);
	players_[current_player_].Drop(buy);
	activ_deal_.reset();
}

const Player& GameController::GetPlayer(std::string_view player) const {
	return *(player_by_name_.at(player));
}

const std::optional<GameController::Deal>& GameController::GetActivDeal() const {
	return activ_deal_;
}

void GameController::CheckKnightsCard() {
	Player* condidate = nullptr;
	size_t condidate_count = 2;

	for (Player& player : players_) {
		size_t count = player.GetUsedCardCount(DevelopmentCard::Knights);
		if (count == condidate_count) {
			condidate = nullptr;
		}
		else if(count > condidate_count) {
			condidate = &player;
			condidate_count = count;
		}
	}

	if (player_knights_ != condidate) {
		if (player_knights_) {
			player_knights_->ResetKnightCard();
		}
	}
	player_knights_ = condidate;
	if (player_knights_) {
		player_knights_->SetKnightCard();
	}

	CheckWinner();
}

void GameController::CheckWinner() {
	for (Player& player : players_) {
		if (player.GetWinPoints() >= 10) {
			step_ = GameStep::Finish;
			winner_ = player.getName();
			return;
		}
	}
}

GameController::GameController(std::vector<std::string> players)
{
	using DropResult = game::Dice::DropResult;
	if (players.size() > 4 || players.size() < 2)
		throw out_of_range("Players must by 2 - 4");

	for (size_t i = 0; i < players.size(); ++i) {
		players_.push_back(Player{ players [i], i});
	}

	MixPlayers();
	BindPlayers();
	InitCardsDeque();

	current_player_ = 0;
	map.GetGexes()[9].setBandit(bandit_);
}
/*
void GameController::startPlace()
{
	for (size_t i = 0; i < players.size(); ++i)
	{
		unsigned int id;
		do
		{
			std::cout << players[i].getName() << " place first building:" << std::endl;
			std::cin >> id;
		} while (!map.canPlaceStartBuilding(id));
		map.placeStartBuilding(id, &players[i]);

		do
		{
			std::cout << players[i].getName() << " place first road:" << std::endl;
			std::cin >> id;
		} while (!map.canPlaceRoad(id, &players[i]));
		map.placeRoad(id, &players[i]);
	}

	for (size_t i = 0, j = players.size() - 1; i < players.size(); ++i, --j)
	{
		unsigned int building_id;
		do
		{
			std::cout << players[j].getName() << " place second building:" << std::endl;
			std::cin >> building_id;
		} while (!map.canPlaceStartBuilding(building_id));
		map.placeStartBuilding(building_id, &players[j]);

		for (const auto pgex : map.getGexsByNodeId(building_id))
			players[j].addResurse(pgex->getType());

		unsigned int road_id;
		do
		{
			std::cout << players[j].getName() << " place second road:" << std::endl;
			std::cin >> road_id;

		} while (!map.isNodeAndFacetNeighbor(building_id, road_id) || !map.canPlaceRoad(road_id, &players[j]));
		map.placeRoad(road_id, &players[j]);
	}
}*/

Player& GameController::CheckCurrentPlayer(std::string_view player) {
	using namespace std::string_literals;
	auto pplayer = player_by_name_.find(player);
	if (pplayer == player_by_name_.end()) {
		throw logic_error("Player "s + std::string(player) + " is not created!"s);
	}

	if (GameStep::DropCards == step_) {
		if (&players_[current_drop_cards_player_] != pplayer->second) {
			throw logic_error("Player "s + std::string(player) + " out of turn!"s);
		}
		return players_[current_drop_cards_player_];
	}

	if (&players_[current_player_] != pplayer->second) {
		throw logic_error("Player "s + std::string(player) + " out of turn!"s);
	}
	return players_[current_player_];
}

Player& GameController::CheckAnyPlayer(std::string_view player) {
	using namespace std::string_literals;
	auto pplayer = player_by_name_.find(player);
	if (pplayer == player_by_name_.end()) {
		throw logic_error("Player "s + std::string(player) + " is not created!"s);
	}

	return *(pplayer->second);
}

void GameController::BuildSettlement(std::string_view player, size_t settlement_id) {
	using namespace std::string_literals;
	Player& p = CheckCurrentPlayer(player);
	if (settlement_id >= map.GetNodes().size()) {
		throw logic_error("Settlement id "s + std::to_string(settlement_id) + " >= "s + std::to_string(map.GetNodes().size()) + "!");
	}

	BuildSettlement(p, settlement_id);
	CheckWinner();
}

void GameController::BuildSettlement(Player& player, size_t settlement_id) {
	using namespace std::string_literals;

	if (step_ == GameStep::ForwardBuildingSettlement ||
		step_ == GameStep::BackwardBuildingSettlement) {
		if (!map.canPlaceStartBuilding(settlement_id)) {
			throw logic_error("Settlement id "s + std::to_string(settlement_id) + " is busy!"s);
		}
		map.placeStartBuilding(settlement_id, &player);

		if (step_ == GameStep::ForwardBuildingSettlement) {
			step_ = GameStep::ForwardBuildingRoad;
		}
		else {
			step_ = GameStep::BackwardBuildingRoad;
			for (const auto pgex : map.getGexsByNodeId(settlement_id))
				player.addResurse(pgex->getType());
		}
	} else if(step_ == GameStep::CommonPlay) {
		if (!map.canPlaceBuilding(settlement_id, player)) {
			throw logic_error("Settlement id "s + std::to_string(settlement_id) + " is busy / no road!"s);
		}
		if (!player.HaveSettlemenResurses()) {
			throw logic_error("Build settlement "s + player.getName() + " havent resurses!"s);
		}

		map.placeSettlement(settlement_id, &player);
		player.FreeSettlemenResurses();
	} else {
		throw logic_error("Build settlement is not aviable on this game step!"s);
	}
}

void GameController::BuildRoad(std::string_view player, size_t road_id) {
	using namespace std::string_literals;
	Player& p = CheckCurrentPlayer(player);
	if (road_id >= map.GetFacets().size()) {
		throw logic_error("Road id "s + std::to_string(road_id) + " >= "s + std::to_string(map.GetFacets().size()) + "!");
	}

	BuildRoad(p, road_id);
	CheckWinner();
}

void GameController::BuildCastle(std::string_view player, size_t settlement_id) {
	using namespace std::string_literals;
	Player& p = CheckCurrentPlayer(player);
	if (settlement_id >= map.GetNodes().size()) {
		throw logic_error("Settlement id "s + std::to_string(settlement_id) + " >= "s + std::to_string(map.GetNodes().size()) + "!");
	}
	BuildCastle(p, settlement_id);
	CheckWinner();
}

void GameController::BuildCastle(Player& player, size_t settlement_id) {
	using namespace std::string_literals;

	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Build castle is not aviable on this game step!"s);
	}

	if (!map.canPlaceCastle(settlement_id, player)) {
		throw logic_error("Castle id "s + std::to_string(settlement_id) + " is not ready!"s);
	}
	if (!player.HaveCastleResurses()) {
		throw logic_error("Build castle fail: "s + player.getName() + " haven't resurses!"s);
	}

	map.placeCastle(settlement_id, player);
	player.FreeCastleResurses();
}

std::string GameController::GetCurrentPlayer() const {
	if (step_ == GameStep::DropCards) {
		return players_.at(current_drop_cards_player_).getName();
	}
	return players_.at(current_player_).getName();
}

void GameController::BuildRoad(Player& player, size_t road_id) {
	using namespace std::string_literals;

	if (step_ == GameStep::ForwardBuildingRoad ||
		step_ == GameStep::BackwardBuildingRoad) {

		if (!map.canPlaceRoad(road_id, &player)) {
			throw logic_error("Road id "s + std::to_string(road_id) + " is busy / far by other building!"s);
		}
		map.placeRoad(road_id, &player);

		if (step_ == GameStep::ForwardBuildingRoad) {
			++current_player_;
			if (current_player_ == players_.size()) {
				step_ = GameStep::BackwardBuildingSettlement;
				--current_player_;
			} else {
				step_ = GameStep::ForwardBuildingSettlement;
			}
		} else {
			if (current_player_ == 0) {
				step_ = GameStep::DiceDrop;
				return;
			}
			--current_player_;
			step_ = GameStep::BackwardBuildingSettlement;
		}
	} else if(step_ == GameStep::CommonPlay) {
		if (!map.canPlaceRoad(road_id, &player)) {
			throw logic_error("Road id "s + std::to_string(road_id) + " is busy / far by other building!"s);
		}
		if (!player.HaveRoadResurses()) {
			throw logic_error("Build road "s + player.getName() + " havent resurses!"s);
		}

		map.placeRoad(road_id, &player);
		
		player.FreeRoadResurses();

	}
	else if (step_ == GameStep::RoadBuilding) {
		if (!map.canPlaceRoad(road_id, &player)) {
			throw logic_error("Road id "s + std::to_string(road_id) + " is busy / far by other building!"s);
		}
		map.placeRoad(road_id, &player);
		if (++road_building_count_ >= 2 || player.getFreeRoad() == nullptr) {
			step_ = GameStep::CommonPlay;
		}
	}
	else {
		throw logic_error("Build road is not aviable on this game step!"s);
	}
}

void GameController::Dice(std::string_view player) {
	using namespace std::string_literals;
	
	if (step_ != GameStep::DiceDrop) {
		throw logic_error("Dice drop is not aviable on this game step!"s);
	}

	CheckCurrentPlayer(player);

	auto dice = dice_.Drop();
	map.diceEvent(dice.result);
	last_dice_ = { dice.each[0], dice.each[1] };

	if (dice.result == 7) {
		step_ = GameStep::DropCards;
		current_drop_cards_player_ = 0;
		DropCards(players_[0], {});
	}
	else {
		step_ = GameStep::CommonPlay;
	}
}

void GameController::DropCards(std::string_view player, const std::map<Resurse, size_t>& resurses) {

	using namespace std::string_literals;
	if (step_ == GameStep::DropCards) {
		auto& p = CheckCurrentPlayer(player);
		DropCards(p, resurses);
	}
	else {
		throw logic_error("Drop cards is not aviable on this game step!"s);
	}
}

void GameController::CheckNextDropCard() {
	++current_drop_cards_player_;
	if (current_drop_cards_player_ >= players_.size()) {
		step_after_bandit_ = GameStep::CommonPlay;
		step_ = GameStep::BanditMove;
		return;
	}
	DropCards(players_[current_drop_cards_player_], {});
}

void GameController::BanditMove(std::string_view player, size_t gex_id, std::string_view other_payer) {
	using namespace std::string_literals;
	
	if (step_ != GameStep::BanditMove) {
		throw logic_error("Bandit move is not aviable on this game step!"s);
	}

	if (player == other_payer) {
		throw logic_error("Can't still yourself!"s);
	}

	if (gex_id >= map.GetGexes().size()) {
		throw logic_error("Gex id "s + std::to_string(gex_id) + " >= "s + std::to_string(map.GetGexes().size()) + "!");
	}

	Player& current = CheckCurrentPlayer(player);

	if (gex_id == 9) {
		map.GetGexes()[gex_id].setBandit(bandit_);
		step_ = step_after_bandit_;
		return;
	}

	auto pplayer = player_by_name_.find(other_payer);
	if (pplayer == player_by_name_.end()) {
		BanditMove(current, map.GetGexes()[gex_id], nullptr);
		return;
	}
	BanditMove(current, map.GetGexes()[gex_id], pplayer->second);
}

void GameController::BanditMove(Player& player, Gex& gex, Player* other_payer) {

	size_t count_building = 0;
	bool find = false;
	for (auto& node : gex.GetNodes()) {
		const Building* building = node->getBuilding();
		if (building) {
			++count_building;

			if (other_payer && building->getPlayer() == other_payer) {
				find = true;
				break;
			}
		}
	}

	if (find) {
		auto still = other_payer->Still();
		if (still) {
			player.addResurse(*still);
		}
	}else if (count_building) {
		throw logic_error("Can't still on this gex");
	}
	gex.setBandit(bandit_);
	step_ = step_after_bandit_;
	return;
}

void GameController::DropCards(Player& player, const std::map<Resurse, size_t>& resurses) {
	using namespace std::string_literals;
	size_t total = player.getCountResurses();

	if (total < 8) {
		CheckNextDropCard();
		return;
	}

	size_t need_drop = total / 2;

	size_t drop_count = 0;
	for (auto& [name, count] : resurses) {
		drop_count += count;
	}

	if (need_drop != drop_count) {
		if (drop_count) {
			throw logic_error("Drop count mast be half of total cards!"s);
		}
		return;
	}

	player.Drop(resurses);
	CheckNextDropCard();
	return;
}

void GameController::Market(std::string_view player, Resurse from, Resurse to) {
	using namespace std::string_literals;
	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Market is not aviable on this game step!"s);
	}

	CheckCurrentPlayer(player).Market(from, to);
}

std::pair<size_t, size_t> GameController::GetLastDice() const {
	return last_dice_;
}

void GameController::Pass(std::string_view player) {
	using namespace std::string_literals;
	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Pass is not aviable on this game step!"s);
	}

	CheckCurrentPlayer(player).OnEndTurn();
	current_player_ = (++current_player_) % players_.size();
	step_ = GameStep::DiceDrop;
	activ_deal_.reset();
}

const Map& GameController::GetMap() const {
	return map;
}

bool GameController::Finish() {
	return winner_.has_value();
}

std::optional<std::string> GameController::GetWinner() {
	return winner_;
}

void GameController::PrintPlayer(std::ostream& os, std::string_view player) {


	using namespace std::string_literals;
	auto pplayer = player_by_name_.find(player);
	if (pplayer == player_by_name_.end()) {
		throw logic_error("Player "s + std::string(player) + " is not created!"s);
	}

	os << *pplayer->second << std::endl;
	//os << "Road len = " << std::endl  << map.GetRoadSize(pplayer->second);
}

void GameController::PrintStep(std::ostream& os) {
	os << step_ << " by " << GetCurrentPlayer();
}

std::ostream& operator<<(std::ostream& os, GameController::GameStep step) {
	switch (step) {
	case GameController::GameStep::ForwardBuildingSettlement: return os << "ForwardBuildingSettlement";
	case GameController::GameStep::ForwardBuildingRoad: return os << "ForwardBuildingRoad";
	case GameController::GameStep::BackwardBuildingSettlement: return os << "BackwardBuildingSettlement";
	case GameController::GameStep::BackwardBuildingRoad: return os << "BackwardBuildingRoad";
	case GameController::GameStep::DiceDrop: return os << "DiceDrop";
	case GameController::GameStep::CommonPlay: return os << "CommonPlay";
	case GameController::GameStep::DropCards: return os << "DropCards";
	case GameController::GameStep::BanditMove: return os << "BanditMove";
	case GameController::GameStep::RoadBuilding: return os << "RoadBuilding";
		
	}
	return os << "unknown";
}

}//namespace ivv::catan {
}//namespace ivv {