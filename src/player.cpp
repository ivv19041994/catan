#include "player.hpp"

#include "exception.hpp"
#include <cassert>
#include <iostream>


namespace ivv {
namespace catan {

size_t Player::GetRoadSize() const {
	size_t res = 0;
	for (const Road& road : roads) {
		if (road.isFree() == false) {
			size_t current = GetLongWay(road.GetFacet()).size();
			//std::cout << "current = " << current << std::endl;
			res = std::max(res, current);
		}
	}
	return res;
}

std::set<const Facet*> Player::GetLongWay(const Facet* from, std::set<const Node*> ban_node, std::set<const Facet*> already, size_t deep)  const {

	std::vector<const Node*> nodes{};
	std::vector<std::vector<const Facet*>> neighbors{};
	size_t neighbors_count = 0;

	//std::cout << std::string(deep, '\t') << "Current facet = " << std::distance(facets.data(), from) << std::endl;

	for (const Node* node : from->GetNeighborNodes()) {
		if (ban_node.count(node) == 0 &&
			(node->getBuilding() == nullptr || node->getBuilding()->getPlayer() == this)) {
			nodes.push_back(node);
			neighbors.push_back({});
			auto& node_facets = node->getNeighborFacets();
			for (const Facet* f : node_facets) {
				if (f != from && already.count(f) == 0 && f->getRoad() && f->getRoad()->getPlayer() == this) {
					neighbors.back().push_back(f);
					++neighbors_count;
				}
			}
		}
	}

	already.insert(from);

	if (neighbors_count == 0) {
		//std::cout << std::string(deep, '\t') << "return " << already.size() << " " << SetFacetToString(facets.data(), already) << std::endl;
		return std::move(already);
	}

	if (neighbors_count == 1) {
		for (size_t i = 0; i < neighbors.size(); ++i) {
			if (neighbors[i].size()) {
				ban_node.insert(nodes[i]);
				auto ret = GetLongWay(neighbors[i][0], std::move(ban_node), std::move(already), deep + 1);
				//std::cout << std::string(deep, '\t') << "return " << ret.size() << " " << SetFacetToString(facets.data(), ret) << std::endl;
				return ret;
			}
		}
		assert(false);
	}

	std::vector< std::vector<std::set< const Facet*>>> result_by_neighbors(neighbors.size());
	for (size_t i = 0; i < neighbors.size(); ++i) {
		for (const Facet* facet : neighbors[i]) {
			auto ban_node_copy = ban_node;
			ban_node_copy.insert(nodes[i]);
			result_by_neighbors[i].push_back(GetLongWay(facet, std::move(ban_node_copy), { from }, deep + 1));
			result_by_neighbors[i].back().erase(from);
		}
	}

	//теперь надо найти лучшую комбинацию
	assert(result_by_neighbors.size() <= 2 && result_by_neighbors.size() != 0);

	if (result_by_neighbors.size() == 1) {
		//тут все просто, кто длиннее тот и лучше
		std::sort(result_by_neighbors[0].begin(), result_by_neighbors[0].end(), [](const auto& lhs, const auto& rhs) {
			return lhs.size() > rhs.size();
			}
		);

		already.insert(result_by_neighbors[0][0].begin(), result_by_neighbors[0][0].end());
		//std::cout << std::string(deep, '\t') << "return(1) " << already.size() << " " << SetFacetToString(facets.data(), already) << std::endl;
		return std::move(already);
	}
	//result_by_neighbors.size() == 2
	//тут надо перебрать пары и найти лучшую не пересекающуюся сумму
	std::set<const Facet*> condidate;

	for (size_t i = 0; i < result_by_neighbors[0].size(); ++i) {
		for (size_t j = 0; j < result_by_neighbors[1].size(); ++j) {
			std::set<const Facet*> current;

			bool not_inter = std::none_of(result_by_neighbors[0][i].begin(), result_by_neighbors[0][i].end(),
				[&result_by_neighbors, j](const Facet* facet) {
					return result_by_neighbors[1][j].count(facet) ? true : false;
				});
			if (not_inter) {
				current.insert(result_by_neighbors[0][i].begin(), result_by_neighbors[0][i].end());
				current.insert(result_by_neighbors[1][j].begin(), result_by_neighbors[1][j].end());
			}
			else {
				if (result_by_neighbors[0][i].size() >= result_by_neighbors[1][j].size()) {
					current = result_by_neighbors[0][i];
				}
				else {
					current = result_by_neighbors[1][j];
				}
			}

			if (current.size() > condidate.size()) {
				std::swap(current, condidate);
			}
		}
	}

	already.insert(condidate.begin(), condidate.end());

	//std::cout << std::string(deep, '\t') << "return(2) " << already.size() << " " << SetFacetToString(facets.data(), already) << std::endl;
	return std::move(already);
}

Player::Player(std::string n, size_t id) : name{ n }, id_{ id }
{
	resurses_[Resurse::Wood] = 100;
	resurses_[Resurse::Clay] = 100;
	resurses_[Resurse::Hay] = 100;
	resurses_[Resurse::Sheep] = 100;
	resurses_[Resurse::Stone] = 100;

	cards_[DevelopmentCard::Knights] = 0;
	cards_[DevelopmentCard::RoadBuilding] = 0;
	cards_[DevelopmentCard::YearOfPlenty] = 0;
	cards_[DevelopmentCard::Monopoly] = 0;
	cards_[DevelopmentCard::University] = 0;
	cards_[DevelopmentCard::Market] = 0;
	cards_[DevelopmentCard::GreatHall] = 0;
	cards_[DevelopmentCard::Chapel] = 0;
	cards_[DevelopmentCard::Library] = 0;
	cards_[DevelopmentCard::Knights] = 0;

	cards_buy_on_this_turn_[DevelopmentCard::Knights] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::RoadBuilding] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::YearOfPlenty] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::Monopoly] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::University] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::Market] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::GreatHall] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::Chapel] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::Library] = 0;
	cards_buy_on_this_turn_[DevelopmentCard::Knights] = 0;

	cards_used_[DevelopmentCard::Knights] = 0;
	cards_used_[DevelopmentCard::RoadBuilding] = 0;
	cards_used_[DevelopmentCard::YearOfPlenty] = 0;
	cards_used_[DevelopmentCard::Monopoly] = 0;
	cards_used_[DevelopmentCard::University] = 0;
	cards_used_[DevelopmentCard::Market] = 0;
	cards_used_[DevelopmentCard::GreatHall] = 0;
	cards_used_[DevelopmentCard::Chapel] = 0;
	cards_used_[DevelopmentCard::Library] = 0;
	cards_used_[DevelopmentCard::Knights] = 0;
}

void Player::addResurse(Resurse resurse, size_t count)
{
	if (Resurse::Not == resurse)
		return;
	resurses_[resurse] += count;
	//std::cout << "resurses["<< static_cast<int>(resurse) << "] = " << resurses[resurse] << std::endl;
}

void Player::addResurse(const std::map<Resurse, size_t>& resurses) {
	for (auto& [name, count] : resurses) {
		addResurse(name, count);
	}
}

bool Player::HaveSettlemenResurses() const {
	return
		resurses_.at(Resurse::Wood) >= 1 &&
		resurses_.at(Resurse::Clay) >= 1 &&
		resurses_.at(Resurse::Hay) >= 1 &&
		resurses_.at(Resurse::Sheep) >= 1;
}
void Player::FreeSettlemenResurses() {
	--resurses_.at(Resurse::Wood);
	--resurses_.at(Resurse::Clay);
	--resurses_.at(Resurse::Hay);
	--resurses_.at(Resurse::Sheep);
}

bool Player::HaveRoadResurses() const {
	return
		resurses_.at(Resurse::Wood) >= 1 &&
		resurses_.at(Resurse::Clay) >= 1;
}
void Player::FreeRoadResurses() {
	--resurses_.at(Resurse::Wood);
	--resurses_.at(Resurse::Clay);
}

bool Player::HaveDevCardResurses() const {
	return
		resurses_.at(Resurse::Hay) >= 1 &&
		resurses_.at(Resurse::Sheep) >= 1 &&
		resurses_.at(Resurse::Stone) >= 1;
}
void Player::FreeDevCardResurses() {
	--resurses_.at(Resurse::Hay);
	--resurses_.at(Resurse::Sheep);
	--resurses_.at(Resurse::Stone);
}

bool Player::HaveCastleResurses() const {
	return
		resurses_.at(Resurse::Hay) >= 2 &&
		resurses_.at(Resurse::Stone) >= 3;
}
void Player::FreeCastleResurses() {
	resurses_.at(Resurse::Hay) -= 2;
	resurses_.at(Resurse::Stone) -= 3;
}

size_t Player::getCountResurses() const {
	size_t ret = 0;
	for (const auto& [name, count] : resurses_) {
		ret += count;
	}
	return ret;
}

size_t Player::getCountResurses(Resurse reusrse) const {
	auto find = resurses_.find(reusrse);
	if (find == resurses_.end()) {
		return 0;
	}
	return find->second;
}

bool Player::Have(const std::map<Resurse, size_t>& resurses) const {
	for (auto& [name, count] : resurses) {
		auto resurse = resurses_.find(name);
		if (resurse == resurses_.end()) {
			return false;
		}
		if (resurse->second < count) {
			return false;
		}
	}
	return true;
}

void Player::Drop(const std::map<Resurse, size_t>& resurses) {
	using namespace std::string_literals;

	if (!Have(resurses)) {
		throw logic_error("Player "s + this->name + " haven't resurses for drop!"s);
	}

	for (auto& [name, count] : resurses) {
		resurses_[name] -= count;
	}
}

void Player::Market(Resurse from, Resurse to) {
	using namespace std::string_literals;
	auto price = resurses_market_price_.at(from);

	if (resurses_.at(from) < price) {
		throw logic_error("Player "s + this->name + " haven't resurses for market!"s);
	}
	++resurses_.at(to);
	resurses_.at(from) -= price;
}

const std::string& Player::getName() const {
	return name;
}

Settlement* Player::getFreeSettlement()
{
	for (auto& settlement : settlements)
		if (settlement.isFree())
		{
			settlement.setPlayer(this);
			return &settlement;
		}
	return nullptr;
}
Castle* Player::getFreeCastle()
{
	for (auto& castle : castles)
		if (castle.isFree())
		{
			castle.setPlayer(this);
			return &castle;
		}
	return nullptr;
}
Road* Player::getFreeRoad()
{
	for (auto& road : roads)
		if (road.isFree())
		{
			road.setPlayer(this);
			return &road;
		}
	return nullptr;
}

size_t Player::getId() const {
	return id_;
}

size_t Player::getFreeSettlementCount() const {
	size_t ret = 0;
	for (const auto& element : settlements) {
		if (element.isFree()) {
			++ret;
		}
	}
	return ret;
}
size_t Player::getFreeCastleCount() const {
	size_t ret = 0;
	for (const auto& element : castles) {
		if (element.isFree()) {
			++ret;
		}
	}
	return ret;
}
size_t Player::getFreeRoadCount() const {
	size_t ret = 0;
	for (const auto& element : roads) {
		if (element.isFree()) {
			++ret;
		}
	}
	return ret;
}

void Player::Print(std::ostream& os) const {
	os << "Player " << name << std::endl
		<< "Id: " << id_ << std::endl
		<< "Free settlements: " << getFreeSettlementCount() << std::endl
		<< "Free castles: " << getFreeCastleCount() << std::endl
		<< "Free roads: " << getFreeRoadCount() << std::endl
		<< "Wood: " << (resurses_.count(Resurse::Wood) ? resurses_.at(Resurse::Wood) : 0) << std::endl
		<< "Clay: " << (resurses_.count(Resurse::Clay) ? resurses_.at(Resurse::Clay) : 0) << std::endl
		<< "Hay: " << (resurses_.count(Resurse::Hay) ? resurses_.at(Resurse::Hay) : 0) << std::endl
		<< "Sheep: " << (resurses_.count(Resurse::Sheep) ? resurses_.at(Resurse::Sheep) : 0) << std::endl
		<< "Stone: " << (resurses_.count(Resurse::Stone) ? resurses_.at(Resurse::Stone) : 0) << std::endl
		<< "Price Wood: " << (resurses_market_price_.count(Resurse::Wood) ? resurses_market_price_.at(Resurse::Wood) : 0) << std::endl
		<< "Price Clay: " << (resurses_market_price_.count(Resurse::Clay) ? resurses_market_price_.at(Resurse::Clay) : 0) << std::endl
		<< "Price Hay: " << (resurses_market_price_.count(Resurse::Hay) ? resurses_market_price_.at(Resurse::Hay) : 0) << std::endl
		<< "Price Sheep: " << (resurses_market_price_.count(Resurse::Sheep) ? resurses_market_price_.at(Resurse::Sheep) : 0) << std::endl
		<< "Price Stone: " << (resurses_market_price_.count(Resurse::Stone) ? resurses_market_price_.at(Resurse::Stone) : 0) << std::endl
		<< "Win points: " << GetWinPoints() << std::endl
		<< "Knights: " << cards_.at(DevelopmentCard::Knights) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Knights) << "/" << cards_used_.at(DevelopmentCard::Knights) << std::endl
		<< "RoadBuilding: " << cards_.at(DevelopmentCard::RoadBuilding) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::RoadBuilding) << "/" << cards_used_.at(DevelopmentCard::RoadBuilding) << std::endl
		<< "YearOfPlenty: " << cards_.at(DevelopmentCard::YearOfPlenty) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::YearOfPlenty) << "/" << cards_used_.at(DevelopmentCard::YearOfPlenty) << std::endl
		<< "Monopoly: " << cards_.at(DevelopmentCard::Monopoly) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Monopoly) << "/" << cards_used_.at(DevelopmentCard::Monopoly) << std::endl
		<< "University: " << cards_.at(DevelopmentCard::University) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::University) << "/" << cards_used_.at(DevelopmentCard::University) << std::endl
		<< "Market: " << cards_.at(DevelopmentCard::Market) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Market) << "/" << cards_used_.at(DevelopmentCard::Market) << std::endl
		<< "GreatHall: " << cards_.at(DevelopmentCard::GreatHall) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::GreatHall) << "/" << cards_used_.at(DevelopmentCard::GreatHall) << std::endl
		<< "Chapel: " << cards_.at(DevelopmentCard::Chapel) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Chapel) << "/" << cards_used_.at(DevelopmentCard::Chapel) << std::endl
		<< "Library: " << cards_.at(DevelopmentCard::Library) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Library) << "/" << cards_used_.at(DevelopmentCard::Library) << std::endl
		<< "Road size: " << GetRoadSize() << std::endl;

}

void Player::DownPriceOnMarket(Resurse resurse, size_t price) {
	size_t& price_ = resurses_market_price_[resurse];
	if (price_ > price) {
		price_ = price;
	}
}

std::optional<Resurse> Player::Still() {
	size_t total = getCountResurses();
	if (!total) {
		return {};
	}

	std::random_device rd{};
	std::default_random_engine rng{ rd() };

	size_t r = rng() % total;

	for (auto& [name, count] : resurses_) {
		if (r < count) {
			--count;
			return name;
		}
		r -= count;
	}
	assert(false);
	return {};
}

static bool IsWinnerCard(DevelopmentCard card) {
	switch (card) {
	case DevelopmentCard::Knights:
	case DevelopmentCard::RoadBuilding:
	case DevelopmentCard::YearOfPlenty:
	case DevelopmentCard::Monopoly:
		return false;
	case DevelopmentCard::University:
	case DevelopmentCard::Market:
	case DevelopmentCard::GreatHall:
	case DevelopmentCard::Chapel:
	case DevelopmentCard::Library:
		return true;
	}
	return false;
}

void Player::PutCard(DevelopmentCard card) {
	if (IsWinnerCard(card)) {
		++cards_[card];
	}
	else {
		++cards_buy_on_this_turn_[card];
	}



}

void Player::OnEndTurn() {
	already_use_dev_card_on_this_turn_ = false;

	for (auto& [name, count] : cards_buy_on_this_turn_) {
		cards_[name] += count;
		cards_count_ += count;
		count = 0;
	}
}

size_t Player::StillAll(Resurse resurse) {
	size_t& count = resurses_[resurse];
	size_t ret = count;
	count = 0;
	return ret;
}

size_t Player::GetCardCount(DevelopmentCard card, const std::map<DevelopmentCard, size_t>& card_deque) const {
	auto find = card_deque.find(card);
	if (find == card_deque.end()) {
		return 0;
	}
	return find->second;
}

size_t Player::GetReadyForUseCardCount(DevelopmentCard card) const {
	return GetCardCount(card, cards_);
}
size_t Player::GetPurchasedCardCount(DevelopmentCard card) const {
	return GetCardCount(card, cards_buy_on_this_turn_);
}

size_t Player::GetUsedCardCount(DevelopmentCard card) const {
	return GetCardCount(card, cards_used_);
}

void Player::SetKnightCard() {
	knight_card_ = true;
}
void Player::ResetKnightCard() {
	knight_card_ = false;
}

void Player::SetRoadCard() {
	road_card_ = true;
}
void Player::ResetRoadCard() {
	road_card_ = false;
}

size_t Player::GetWinPoints() const {
	size_t res = 0;
	res += 5 - getFreeSettlementCount();
	res += (4 - getFreeCastleCount()) * 2;
	res += win_cards_used_;
	if (knight_card_) {
		res += 2;
	}
	if (road_card_) {
		res += 2;
	}
	return res;
}

void Player::Use(DevelopmentCard card) {
	using namespace std::string_literals;
	auto& count = cards_[card];
	if (count == 0) {
		throw logic_error("Player "s + this->name + " haven't this card!"s);
	}

	switch (card) {
	case DevelopmentCard::Knights:
	case DevelopmentCard::RoadBuilding:
	case DevelopmentCard::YearOfPlenty:
	case DevelopmentCard::Monopoly:
		if (already_use_dev_card_on_this_turn_) {
			throw logic_error("Player "s + this->name + " already use card in this turn!"s);
		}
		already_use_dev_card_on_this_turn_ = true;
		break;
	case DevelopmentCard::University:
	case DevelopmentCard::Market:
	case DevelopmentCard::GreatHall:
	case DevelopmentCard::Chapel:
	case DevelopmentCard::Library:
		++win_cards_used_;
		break;
	}
	++cards_used_[card];
	--count;
	--cards_count_;
}

std::ostream& operator<<(std::ostream& os, const Player& player) {
	player.Print(os);
	return os;
}

}//namespace ivv::catan {
}//namespace ivv {