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

void GameController::BuildSettlement(std::string_view player, size_t settlement_id) {
	using namespace std::string_literals;
	Player& p = CheckCurrentPlayer(player);
	if (settlement_id >= map.GetNodes().size()) {
		throw logic_error("Settlement id "s + std::to_string(settlement_id) + " >= "s + std::to_string(map.GetNodes().size()) + "!");
	}

	BuildSettlement(p, settlement_id);
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
}

void GameController::BuildCastle(std::string_view player, size_t settlement_id) {
	using namespace std::string_literals;
	Player& p = CheckCurrentPlayer(player);
	if (settlement_id >= map.GetNodes().size()) {
		throw logic_error("Settlement id "s + std::to_string(settlement_id) + " >= "s + std::to_string(map.GetNodes().size()) + "!");
	}
	BuildCastle(p, settlement_id);
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

void GameController::DropCards(std::string_view player, const std::map<Resurse, unsigned int>& resurses) {

	using namespace std::string_literals;
	if (step_ == GameStep::DropCards) {
		auto p = CheckCurrentPlayer(player);
		DropCards(p, resurses);
	}
	else {
		throw logic_error("Drop cards is not aviable on this game step!"s);
	}
}

void GameController::DropCards(Player& player, const std::map<Resurse, unsigned int>& resurses) {
	size_t total = player.getCountResurses();
	if (total >= 8) {
		player.Drop(resurses);
	}
	
	++current_drop_cards_player_;
	if (current_drop_cards_player_ == players_.size()) {
		step_ == GameStep::CommonPlay;
		return;
	}
	DropCards(players_[current_drop_cards_player_], {});
	return;
}

std::pair<size_t, size_t> GameController::GetLastDice() const {
	return last_dice_;
}

void GameController::Pass(std::string_view player) {
	using namespace std::string_literals;
	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Pass is not aviable on this game step!"s);
	}

	CheckCurrentPlayer(player);
	current_player_ = (++current_player_) % players_.size();
	step_ = GameStep::DiceDrop;
}

const Map& GameController::GetMap() const {
	return map;
}

bool GameController::Finish() {
	return winner_.has_value();
}

void GameController::PrintPlayer(std::ostream& os, std::string_view player) {


	using namespace std::string_literals;
	auto pplayer = player_by_name_.find(player);
	if (pplayer == player_by_name_.end()) {
		throw logic_error("Player "s + std::string(player) + " is not created!"s);
	}

	os << *pplayer->second;
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
	}
	return os << "unknown";
}

}//namespace ivv::catan {
}//namespace ivv {