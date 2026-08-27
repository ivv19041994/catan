#include "game_controller.hpp"

#include "map_rander.h"
#include "catan.hpp"
#include "exception.hpp"

#include <cassert>
#include <algorithm>

namespace ivv {
namespace catan {

namespace {

GameController::Dependencies MakeProductionDependencies() {
	GameController::Dependencies dependencies;
	dependencies.dice[0] = std::make_unique<game::Dice>();
	dependencies.dice[1] = std::make_unique<game::Dice>();
	dependencies.development_cards = std::make_unique<DevelopmentCardDeck>();
	return dependencies;
}

template <typename Counter>
Player* SelectAwardHolder(std::vector<Player>& players, Player* incumbent,
		size_t threshold, Counter count) {
	size_t maximum = 0;
	for (const Player& player : players) {
		maximum = std::max(maximum, count(player));
	}

	if (maximum < threshold) {
		return nullptr;
	}
	if (incumbent && count(*incumbent) == maximum) {
		return incumbent;
	}

	Player* leader = nullptr;
	for (Player& player : players) {
		if (count(player) != maximum) {
			continue;
		}
		if (leader) {
			return nullptr;
		}
		leader = &player;
	}
	return leader;
}

void ValidateDealSide(const std::map<Resurse, size_t>& side) {
	if (side.empty()) throw logic_error("Both deal sides must contain resources");
	for (const auto& [resource, count] : side) {
		if (resource == Resurse::Not || count == 0)
			throw logic_error("Deal resources must have positive quantities");
	}
}

void ValidateDeal(const std::map<Resurse, size_t>& sell,
		const std::map<Resurse, size_t>& buy) {
	ValidateDealSide(sell);
	ValidateDealSide(buy);
	for (const auto& [resource, count] : sell) {
		(void)count;
		if (buy.contains(resource))
			throw logic_error("The same resource cannot be on both deal sides");
	}
}

}//namespace

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

void GameController::DevCard(std::string_view player) {
	using namespace std::string_literals;

	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Buy dev card is not aviable on this game step!"s);
	}
	Player& p = CheckCurrentPlayer(player);
	if (development_cards_->Empty()) {
		throw logic_error("Dev card deque is empty!"s);
	}

	if (!p.HaveDevCardResurses()) {
		throw logic_error("Player " + p.getName() + "haven't reusrse!"s);
	}
	

	DevelopmentCard card = development_cards_->Draw();
	p.PutCard(card);
	p.FreeDevCardResurses();
	ReturnToBank(Resurse::Hay);
	ReturnToBank(Resurse::Sheep);
	ReturnToBank(Resurse::Stone);
	CheckWinner();
}

void GameController::UseDevCard(std::string_view player, DevelopmentCard card, UseDevCardParam param) {
	using namespace std::string_literals;
	if (step_ != GameStep::CommonPlay && step_ != GameStep::DiceDrop) {
		throw logic_error("Use dev card is not aviable on this game step!"s);
	}
	Player& p = CheckCurrentPlayer(player);

	switch (card) {
	case DevelopmentCard::YearOfPlenty:
		if (!param || !std::holds_alternative<std::array<Resurse, 2>>(*param)) {
			throw logic_error("Invalid use dev card param!"s);
		}
		for (Resurse resource : std::get<std::array<Resurse, 2>>(*param)) {
			if (resource == Resurse::Not) {
				throw logic_error("Invalid use dev card resource!"s);
			}
		}
		{
			const auto resources = std::get<std::array<Resurse, 2>>(*param);
			const size_t first_count = resources[0] == resources[1] ? 2 : 1;
			if (!resource_bank_.CanTake(resources[0], first_count)
				|| (resources[0] != resources[1] && !resource_bank_.CanTake(resources[1])))
				throw logic_error("Resource bank cannot supply Year of Plenty");
		}
		break;
	case DevelopmentCard::Monopoly:
		if (!param || !std::holds_alternative<Resurse>(*param) ||
			std::get<Resurse>(*param) == Resurse::Not) {
			throw logic_error("Invalid use dev card param!"s);
		}
		break;
	default:
		break;
	}
	if (card == DevelopmentCard::RoadBuilding && !HasLegalRoadPlacement(p))
		throw logic_error("Road Building has no legal road placement");

	p.Use(card);

	switch (card) {
	case DevelopmentCard::Knights:
		step_after_bandit_ = step_;
		step_ = GameStep::BanditMove;
		CheckKnightsCard();
		break;
	case DevelopmentCard::RoadBuilding:
		road_building_return_step_ = step_;
		step_ = GameStep::RoadBuilding;
		road_building_count_ = 0;
		break;
	case DevelopmentCard::YearOfPlenty:
	{
		auto& res = std::get<std::array<Resurse, 2>>(*param);
		GiveFromBank(p, res[0]);
		GiveFromBank(p, res[1]);
	}
		break;
	case DevelopmentCard::Monopoly:
		{
			Resurse res = std::get<Resurse>(*param);

			for (Player& other_player : players_) {
				if (&p != &other_player) {
					p.addResurse(res, other_player.StillAll(res));
				}
			}
		}
		break;
	case DevelopmentCard::University:
	case DevelopmentCard::Market:
	case DevelopmentCard::GreatHall:
	case DevelopmentCard::Chapel:
	case DevelopmentCard::Library:
		break;
	}

	CheckWinner();
}

void GameController::SetDeal(std::string_view player, std::map<Resurse, size_t> sell, std::map<Resurse, size_t> buy) {
	using namespace std::string_literals;

	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Deal is not aviable on this game step!"s);
	}

	ValidateDeal(sell, buy);

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

void GameController::CancelDeal(std::string_view player) {
	using namespace std::string_literals;

	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Deal is not aviable on this game step!"s);
	}

	CheckAnyPlayer(player);
	if (!activ_deal_) {
		throw logic_error("There is no active deal!"s);
	}

	activ_deal_.reset();
}

const Player& GameController::GetPlayer(std::string_view player) const {
	return *(player_by_name_.at(player));
}

const ResourceBank& GameController::GetResourceBank() const {
	return resource_bank_;
}

std::vector<std::string> GameController::GetPlayerNames() const {
	std::vector<std::string> result;
	result.reserve(players_.size());
	for (const Player& player : players_) result.push_back(player.getName());
	return result;
}

const std::optional<GameController::Deal>& GameController::GetActivDeal() const {
	return activ_deal_;
}

void GameController::CheckKnightsCard() {
	Player* condidate = SelectAwardHolder(players_, player_knights_, 3,
		[](const Player& player) {
			return player.GetUsedCardCount(DevelopmentCard::Knights);
		});

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

void GameController::CheckRoadLen() {
	Player* condidate = SelectAwardHolder(players_, player_roads_, 5,
		[](const Player& player) {
			return player.GetRoadSize();
		});

	if (player_roads_ != condidate) {
		if (player_roads_) {
			player_roads_->ResetRoadCard();
		}
	}
	player_roads_ = condidate;
	if (player_roads_) {
		player_roads_->SetRoadCard();
	}

	CheckWinner();
}

void GameController::CheckWinner() {
	Player& player = players_[current_player_];
	if (player.GetWinPoints() >= 10) {
		step_ = GameStep::Finish;
		winner_ = player.getName();
	}
}

GameController::GameController(std::vector<std::string> players)
	: GameController(std::move(players), MakeProductionDependencies()) {
}

GameController::GameController(std::vector<std::string> players, Dependencies dependencies)
	: dice_(std::move(dependencies.dice))
	, development_cards_(std::move(dependencies.development_cards))
{
	if (!dice_[0] || !dice_[1] || !development_cards_) {
		throw invalid_argument("Game dependencies must not be null");
	}
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
	CheckRoadLen();
}

void GameController::BuildSettlement(Player& player, size_t settlement_id) {
	using namespace std::string_literals;

	if (step_ == GameStep::ForwardBuildingSettlement ||
		step_ == GameStep::BackwardBuildingSettlement) {
		if (!map.canPlaceStartBuilding(settlement_id)) {
			throw logic_error("Settlement id "s + std::to_string(settlement_id) + " is busy!"s);
		}
		map.placeStartBuilding(settlement_id, &player);
		setup_settlement_id_ = settlement_id;

		if (step_ == GameStep::ForwardBuildingSettlement) {
			step_ = GameStep::ForwardBuildingRoad;
		}
		else {
			step_ = GameStep::BackwardBuildingRoad;
			for (const auto pgex : map.getGexsByNodeId(settlement_id))
				GiveFromBank(player, pgex->getType());
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
		ReturnToBank(Resurse::Wood);
		ReturnToBank(Resurse::Clay);
		ReturnToBank(Resurse::Hay);
		ReturnToBank(Resurse::Sheep);
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
	CheckRoadLen();
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
	ReturnToBank(Resurse::Hay, 2);
	ReturnToBank(Resurse::Stone, 3);
}

std::string GameController::GetCurrentPlayer() const {
	if (step_ == GameStep::DropCards) {
		return players_.at(current_drop_cards_player_).getName();
	}
	return players_.at(current_player_).getName();
}

GameController::GameStep GameController::GetStep() const {
	return step_;
}

void GameController::BuildRoad(Player& player, size_t road_id) {
	using namespace std::string_literals;

	if (step_ == GameStep::ForwardBuildingRoad ||
		step_ == GameStep::BackwardBuildingRoad) {

		if (!setup_settlement_id_ || !map.isNodeAndFacetNeighbor(*setup_settlement_id_, road_id)
			|| !map.canPlaceRoad(road_id, &player)) {
			throw logic_error("Initial road id "s + std::to_string(road_id)
				+ " must touch settlement id "s
				+ (setup_settlement_id_ ? std::to_string(*setup_settlement_id_) : "none") + "!"s);
		}
		map.placeRoad(road_id, &player);
		setup_settlement_id_.reset();

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
		ReturnToBank(Resurse::Wood);
		ReturnToBank(Resurse::Clay);

	}
	else if (step_ == GameStep::RoadBuilding) {
		if (!map.canPlaceRoad(road_id, &player)) {
			throw logic_error("Road id "s + std::to_string(road_id) + " is busy / far by other building!"s);
		}
		map.placeRoad(road_id, &player);
		++road_building_count_;
		if (road_building_count_ >= 2 || !HasLegalRoadPlacement(player)) {
			step_ = road_building_return_step_;
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

	last_dice_ = { dice_[0]->Roll(), dice_[1]->Roll() };
	const size_t dice_result = last_dice_.first + last_dice_.second;
	ResolveProduction(dice_result);

	if (dice_result == 7) {
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

	if (bandit_.getGex() == &map.GetGexes()[gex_id]) {
		throw logic_error("Bandit must move to another gex!"s);
	}

	auto pplayer = player_by_name_.find(other_payer);
	if (pplayer == player_by_name_.end()) {
		BanditMove(current, map.GetGexes()[gex_id], nullptr);
		return;
	}
	BanditMove(current, map.GetGexes()[gex_id], pplayer->second);
}

void GameController::BanditMove(Player& player, Gex& gex, Player* other_payer) {

	size_t eligible_buildings = 0;
	bool find = false;
	for (auto& node : gex.GetNodes()) {
		const Building* building = node->getBuilding();
		if (building && building->getPlayer() != &player) {
			++eligible_buildings;

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
	}else if (eligible_buildings || other_payer) {
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
	for (const auto& [resource, count] : resurses) ReturnToBank(resource, count);
	CheckNextDropCard();
	return;
}

void GameController::Market(std::string_view player, Resurse from, Resurse to) {
	using namespace std::string_literals;
	if (step_ != GameStep::CommonPlay) {
		throw logic_error("Market is not aviable on this game step!"s);
	}

	if (from == Resurse::Not || to == Resurse::Not || from == to)
		throw logic_error("Bank trade requires two different resources");
	Player& current = CheckCurrentPlayer(player);
	if (!resource_bank_.CanTake(to))
		throw logic_error("Resource bank does not contain requested card");
	const size_t price = current.GetMarketPrice(from);
	current.Market(from, to);
	ReturnToBank(from, price);
	resource_bank_.Take(to);
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
	CheckWinner();
}

bool GameController::HasLegalRoadPlacement(const Player& player) const {
	if (player.getFreeRoadCount() == 0) return false;
	for (size_t road_id = 0; road_id < Map::facets_count; ++road_id)
		if (map.canPlaceRoad(road_id, &player)) return true;
	return false;
}

void GameController::GiveFromBank(Player& player, Resurse resource, size_t count) {
	if (resource == Resurse::Not || count == 0) return;
	const size_t granted = std::min(count, resource_bank_.Count(resource));
	if (granted == 0) return;
	resource_bank_.Take(resource, granted);
	player.addResurse(resource, granted);
}

void GameController::ReturnToBank(Resurse resource, size_t count) {
	if (resource != Resurse::Not && count != 0) resource_bank_.Return(resource, count);
}

void GameController::ResolveProduction(size_t dice_result) {
	std::array<std::array<size_t, ResourceBank::Resources.size()>, 4> before{};
	for (size_t player_index = 0; player_index < players_.size(); ++player_index)
		for (size_t resource_index = 0; resource_index < ResourceBank::Resources.size(); ++resource_index)
			before[player_index][resource_index] = players_[player_index].getCountResurses(
				ResourceBank::Resources[resource_index]);

	map.diceEvent(dice_result);

	for (size_t resource_index = 0; resource_index < ResourceBank::Resources.size(); ++resource_index) {
		const Resurse resource = ResourceBank::Resources[resource_index];
		std::array<size_t, 4> gains{};
		for (size_t player_index = 0; player_index < players_.size(); ++player_index) {
			gains[player_index] = players_[player_index].getCountResurses(resource)
				- before[player_index][resource_index];
		}
		for (size_t player_index = 0; player_index < players_.size(); ++player_index)
			if (gains[player_index] != 0) players_[player_index].Drop({{resource, gains[player_index]}});

		const std::vector<size_t> grants = resource_bank_.DistributeProduction(
			resource, std::span<const size_t>(gains.data(), players_.size()));
		for (size_t player_index = 0; player_index < players_.size(); ++player_index)
			players_[player_index].addResurse(resource, grants[player_index]);
	}
}

const Map& GameController::GetMap() const {
	return map;
}

bool GameController::CanBuildSettlement(size_t node_id) const {
	if (node_id >= Map::nodes_count) return false;
	const Player& player = players_[current_player_];
	if (step_ == GameStep::ForwardBuildingSettlement || step_ == GameStep::BackwardBuildingSettlement) {
		return player.getFreeSettlementCount() > 0 && map.canPlaceStartBuilding(node_id);
	}
	return step_ == GameStep::CommonPlay && player.getFreeSettlementCount() > 0
		&& player.HaveSettlemenResurses() && map.canPlaceBuilding(node_id, player);
}

bool GameController::CanBuildRoad(size_t road_id) const {
	if (road_id >= Map::facets_count) return false;
	const Player& player = players_[current_player_];
	if (step_ == GameStep::ForwardBuildingRoad || step_ == GameStep::BackwardBuildingRoad
		|| step_ == GameStep::RoadBuilding) {
		if ((step_ == GameStep::ForwardBuildingRoad || step_ == GameStep::BackwardBuildingRoad)
			&& (!setup_settlement_id_ || !map.isNodeAndFacetNeighbor(*setup_settlement_id_, road_id))) {
			return false;
		}
		return map.canPlaceRoad(road_id, &player);
	}
	return step_ == GameStep::CommonPlay && player.HaveRoadResurses()
		&& map.canPlaceRoad(road_id, &player);
}

bool GameController::CanBuildCastle(size_t node_id) const {
	if (node_id >= Map::nodes_count) return false;
	const Player& player = players_[current_player_];
	return step_ == GameStep::CommonPlay && player.getFreeCastleCount() > 0
		&& player.HaveCastleResurses() && map.canPlaceCastle(node_id, player);
}

bool GameController::CanMoveBandit(size_t gex_id) const {
	return step_ == GameStep::BanditMove && gex_id < map.GetGexes().size()
		&& !map.GetGexes()[gex_id].isBandit();
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
	case GameController::GameStep::Finish: return os << "Finish";
		
	}
	return os << "unknown";
}

}//namespace ivv::catan {
}//namespace ivv {
