#pragma once

#include <map>
#include <string>
#include <set>

#include "catan.hpp"

namespace ivv {
namespace catan {
class Player
{
	std::string name;
	std::map<Resurse, size_t> resurses_;
	std::map<DevelopmentCard, size_t> cards_;
	std::map<DevelopmentCard, size_t> cards_buy_on_this_turn_;
	std::map<DevelopmentCard, size_t> cards_used_;
	size_t cards_count_ = 0;
	size_t win_cards_used_ = 0;
	bool already_use_dev_card_on_this_turn_ = false;
	bool knight_card_ = false;
	bool road_card_ = false;
	std::array<Settlement, 5> settlements;
	std::array<Castle, 4> castles;
	std::array<Road, 15 > roads;
	size_t id_;
	std::map<Resurse, size_t> resurses_market_price_ = {
		{ Resurse::Wood, 4 },
		{ Resurse::Clay, 4 },
		{ Resurse::Hay, 4 },
		{ Resurse::Sheep, 4 },
		{ Resurse::Stone, 4 }
	};

	size_t GetCardCount(DevelopmentCard card, const std::map<DevelopmentCard, size_t>& card_deque) const;

	std::set<const Facet*> GetLongWay(
		const Facet* from,
		std::set<const Node*> ban_node = std::set<const Node*>{},
		std::set<const Facet*> already = std::set<const Facet*>{},
		size_t deep = 0)  const;

public:
	explicit Player(std::string name, size_t id);
	const std::string& getName() const;
	size_t getId() const;
	void addResurse(Resurse resurse, size_t count = 1);
	void addResurse(const std::map<Resurse, size_t>& resurses);

	Settlement* getFreeSettlement();
	Castle* getFreeCastle();
	Road* getFreeRoad();

	size_t getFreeSettlementCount() const;
	size_t getFreeCastleCount() const;
	size_t getFreeRoadCount() const;

	bool HaveSettlemenResurses() const;
	void FreeSettlemenResurses();

	bool HaveRoadResurses() const;
	void FreeRoadResurses();

	bool HaveDevCardResurses() const;
	void FreeDevCardResurses();

	bool HaveCastleResurses() const;
	void FreeCastleResurses();

	size_t getCountResurses() const;
	size_t getCountResurses(Resurse reusrse) const;

	void Print(std::ostream&) const;

	bool Have(const std::map<Resurse, size_t>& resurses) const;
	void Drop(const std::map<Resurse, size_t>& resurses);

	void Market(Resurse from, Resurse to);

	void DownPriceOnMarket(Resurse resurse, size_t price);

	std::optional<Resurse> Still();

	void PutCard(DevelopmentCard card);
	void Use(DevelopmentCard card);

	void OnEndTurn();

	size_t StillAll(Resurse resurse);

	size_t GetReadyForUseCardCount(DevelopmentCard card) const;
	size_t GetPurchasedCardCount(DevelopmentCard card) const;
	size_t GetUsedCardCount(DevelopmentCard card) const;

	void SetKnightCard();
	void ResetKnightCard();
	void SetRoadCard();
	void ResetRoadCard();

	size_t GetWinPoints() const;

	size_t GetRoadSize() const;


};

std::ostream& operator<<(std::ostream& os, const Player& player);

}//namespace ivv::catan {
}//namespace ivv {