#pragma once

#include <vector>
#include <set>
#include <initializer_list>
#include <array>
#include <map>
#include <random>
#include <stdexcept>
#include <span>

namespace ivv{
	namespace catan{

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
		class Game;

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
		};

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

		public:

			Map();

			void diceEvent(std::pair<unsigned int, unsigned int> d);

			bool canPlaceStartBuilding(unsigned int nodeId);
			void placeStartBuilding(unsigned int nodeId, Player* p);
			bool canPlaceBuilding(unsigned int nodeId);
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

		class Game
		{
			std::vector <Player> players;
			unsigned int currentPlayer;
			std::random_device rd;
			std::default_random_engine rng;
			Map map;

			Bandit bandit_;

			void startPlace();
		public:
			Game(std::initializer_list<std::string> players);
			Game(std::vector<std::string> players);

			std::pair<unsigned int, unsigned int> diceDrop();
			static unsigned int diceToUInt(std::pair<unsigned int, unsigned int> d);

		};


		std::ostream& operator<<(std::ostream& os, const Player& player);

		class Player
		{
			std::string name;
			std::map<Resurse, unsigned int> resurses;
			std::array<Settlement, 5> settlements;
			std::array<Castle, 4> castles;
			std::array<Road, 15 > roads;
			size_t id_;
		public:
			explicit Player(std::string name, size_t id);
			std::string getName();
			size_t getId() const;
			void addResurse(Resurse resurse, unsigned int count = 1);

			Settlement *getFreeSettlement();
			Castle *getFreeCastle();
			Road *getFreeRoad();

			size_t getFreeSettlementCount() const;
			size_t getFreeCastleCount() const;
			size_t getFreeRoadCount() const;

			void Print(std::ostream&) const;
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
