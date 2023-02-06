#pragma once

#include <vector>
#include <set>
#include <initializer_list>
#include <array>
#include <map>
#include <random>
#include <stdexcept>
#include <span>
#include <functional>
#include <optional>

namespace ivv{
	namespace catan{

		class logic_error : public std::logic_error {
		public:
			logic_error(std::string what) : std::logic_error{ std::move(what) } {

			}
		};

		class Gex;
		class Facet;
		class Node;
		class Player;
		class Bandit;
		class Building;
		class Settlement;
		class Castle;
		class Road;
		class Map;
		class GameController;

		enum class Resurse
		{
			Wood,
			Clay,
			Hay,
			Sheep,
			Stone,
			Not
		};

		enum class DevelopmentCard {
			Knights,
			RoadBuilding,
			YearOfPlenty,
			Monopoly,
			University, 
			Market,
			GreatHall,
			Chapel,
			Library
		};

		class Bandit {
		public:
			void setGex(Gex* gex);
			Gex* getGex();
		private:
			Gex* gex_;
		};

		class Construction
		{
		protected:
			Player *player = nullptr;
			bool _free = true;
		public:
			Player *getPlayer() const;
			void setPlayer(Player *p);
			void setBusy();
			bool isFree() const;
		};

		class Road: public Construction
		{
		public:
			void SetFacet(Facet* facet);
			Facet* GetFacet() const ;
		private:
			Facet* facet_;
		};

		class Building: public Construction
		{
		public:
			virtual void diceEvent(Resurse r) = 0;
			void setFree();
		};

		class Settlement: public Building
		{
		public:
			void diceEvent(Resurse r) override;
		};
		class Castle: public Building
		{
		public:
			void diceEvent(Resurse r) override;
		};

		class Gex
		{
			std::set<Node*> node_neighbor{};
			Bandit *bandit_ = nullptr;
			Resurse resurse{};
			int dice_{};
		public:
			Gex();
			void addNeighbor(Node* node);
			void setBandit(Bandit& b);
			void setType(Resurse r);
			Resurse getType() const;
			void setDice(int dice);
			int getDice() const;

			void diceEvent();
			bool isBandit() const;

			const std::set<Node*>& GetNodes() const;
		};

		class Facet
		{
			std::set<Facet*> facet_neighbor{};
			std::set<Node*> node_neighbor{};
			Road *road = nullptr;

		public:

			Facet() = default;
			void addNeighbor(Facet* facet);
			void addNeighbor(Node* node);
			bool isFree() const;
			bool isBusyBy(Player *p) const;
			void setRoad(Road *r);
			bool haveNeighborFacetWith(Player *p) const;
			bool haveNeighborNodeWith(Player *p) const;
			const Road* getRoad() const;
			const std::set<Node*>& GetNeighborNodes() const;
		};

		class Node
		{
			std::set<Gex*> gex_neighbor{};
			std::set<Facet*> facet_neighbor{};
			std::set<Node*> node_neighbor{};
			Building *building = nullptr;

			std::function<void(Player*)> change_builder_func_{};

		public:
			int index;
			Node() = default;
			void addNeighbor(Gex* gex);
			void addNeighbor(Facet* facet);
			void addNeighbor(Node* node);

			void diceEvent(Resurse r);
			void setBuilding(Building* b);
			const Building* getBuilding() const;

			bool neighborsNodeIsFree() const;
			bool isFree() const;
			bool isBusyBy(Player *p) const;

			std::set<Gex*> getNeighborGexs();
			const std::set<Facet*>& getNeighborFacets() const;

			template<typename Func>
			void SetBuilderChanger(Func func);
		};

		template<typename Func>
		void Node::SetBuilderChanger(Func func) {
			change_builder_func_ = func;
		}

		std::ostream& operator<<(std::ostream& os, const Player& player);

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

			Settlement *getFreeSettlement();
			Castle *getFreeCastle();
			Road *getFreeRoad();

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

		class out_of_range: public std::out_of_range
		{
		public:
			out_of_range(const std::string& what);
		};

		class invalid_argument: public std::invalid_argument
		{
		public:
			invalid_argument(const std::string& what);
		};
	}
}
