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

		class out_of_range : public std::out_of_range
		{
		public:
			out_of_range(const std::string& what);
		};

		class invalid_argument : public std::invalid_argument
		{
		public:
			invalid_argument(const std::string& what);
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


	}
}
