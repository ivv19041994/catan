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
		};

		class Node
		{
			std::set<Gex*> gex_neighbor{};
			std::set<Facet*> facet_neighbor{};
			std::set<Node*> node_neighbor{};
			Building *building = nullptr;

			std::function<void(Player*)> change_builder_func_;

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
			std::set<Facet*> getNeighborFacets();

			template<typename Func>
			void SetBuilderChanger(Func func);
		};

		template<typename Func>
		void Node::SetBuilderChanger(Func func) {
			change_builder_func_ = func;
		}

		class Map
		{
		public:

			static constexpr unsigned int gexs_count = 19;
		private:
			std::random_device rd;
			std::default_random_engine rng;
			
			std::array<Gex, gexs_count> gexs;
			std::array<Facet, 72> facets;
			std::array<Node, 54> nodes;

			std::map<unsigned int, std::set<Gex*>> dices;

			void initNode(int node, std::vector<int> nodes, std::vector<int> roads, std::vector<int> gexs);
			void initNodes();
			void initRandomTypeForGexs();
			void initPorts();


		public:

			Map();

			void diceEvent(std::pair<unsigned int, unsigned int> d);
			void diceEvent(size_t val);

			bool canPlaceStartBuilding(unsigned int nodeId);
			void placeStartBuilding(unsigned int nodeId, Player* p);
			void placeSettlement(unsigned int nodeId, Player* p);

			bool canPlaceCastle(unsigned int nodeId, const Player& p) const;
			void placeCastle(unsigned int nodeId, Player& p);

			bool canPlaceBuilding(unsigned int nodeId, const Player& player);
			bool canPlaceRoad(unsigned int facetId, Player* p) const;
			void placeRoad(unsigned int id, Player* p);

			std::set<Gex*> getGexsByNodeId(unsigned int nodeId);
			std::set<Facet*> getFacetsByNodeId(unsigned int nodeId);
			bool isNodeAndFacetNeighbor(unsigned int nodeId, unsigned int facetId);

			std::array<Gex, gexs_count>& GetGexes();
			const std::array<Gex, gexs_count>& GetGexes() const;
			const std::array<Node, 54> GetNodes() const;
			const std::array<Facet, 72> GetFacets() const;
		};


		std::ostream& operator<<(std::ostream& os, const Player& player);

		class Player
		{
			std::string name;
			std::map<Resurse, unsigned int> resurses_;
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
		public:
			explicit Player(std::string name, size_t id);
			const std::string& getName() const;
			size_t getId() const;
			void addResurse(Resurse resurse, unsigned int count = 1);

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

			bool HaveCastleResurses() const;
			void FreeCastleResurses();

			size_t getCountResurses() const;

			void Print(std::ostream&) const;

			bool Have(const std::map<Resurse, unsigned int>& resurses) const;
			void Drop(const std::map<Resurse, unsigned int>& resurses);

			void Market(Resurse from, Resurse to);

			void DownPriceOnMarket(Resurse resurse, size_t price);

			std::optional<Resurse> Still();
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
