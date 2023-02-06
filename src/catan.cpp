#include "catan.hpp"

#include "map_rander.h"

#include <deque>
#include <cassert>
#include <utility>
#include <iostream>
#include <algorithm>
#include "player.hpp"


namespace ivv{
	namespace catan{


		std::string SetFacetToString(const Facet* first, const std::set<const Facet*>& facets) {
			std::string ret;
			for (auto f : facets) {
				ret += std::to_string(std::distance(first, f)) + ", ";
			}
			return ret;
		}

		const std::set<Node*>& Facet::GetNeighborNodes() const {
			return node_neighbor;
		}

		Gex::Gex()
		{

		}

		Resurse Gex::getType() const
		{
			return resurse;
		}

		void Gex::setType(Resurse r)
		{
			resurse = r;
		}

		void Gex::addNeighbor(Node* n)
		{
			node_neighbor.insert(n);;
		}

		const std::set<Node*>& Gex::GetNodes() const {
			return node_neighbor;
		}

		void Gex::diceEvent()
		{
			if (bandit_) {
				return;
			}
			for(auto& node: node_neighbor)
			{
				node->diceEvent(resurse);
			}
		}

		void Gex::setDice(int dice) {
			dice_ = dice;
		}

		int Gex::getDice() const {
			return dice_;
		}

		void Gex::setBandit(Bandit& b) {
			if (&b == bandit_) {
				return;
			}
			Gex* gex = b.getGex();
			if (gex) {
				gex->bandit_ = nullptr;
			}
			b.setGex(this);
			bandit_ = &b;
		}

		bool Gex::isBandit() const {
			return bandit_ != nullptr;
		}

		void Node::diceEvent(Resurse r)
		{
			if(building == nullptr)
				return;
			building->diceEvent(r);
		}

		bool Node::neighborsNodeIsFree() const
		{
			for(auto nn: node_neighbor)
				if(!nn->isFree())
					return false;
			return true;
		}

		bool Node::isFree() const
		{
			return building == nullptr;
		}

		bool Node::isBusyBy(Player *p) const
		{
			if(isFree())
			{
				return false;
			}
			return building->getPlayer() == p;
		}

		void Node::setBuilding(Building* b)
		{
			if(building != nullptr)
				building->setFree();
			b->setBusy();
			building = b;
			if (change_builder_func_) {
				change_builder_func_(b->getPlayer());
			}
		}

		const Building* Node::getBuilding() const {
			return building;
		}

		std::set<Gex*> Node::getNeighborGexs()
		{
			return gex_neighbor;
		}

		const std::set<Facet*>& Node::getNeighborFacets() const
		{
			return facet_neighbor;
		}

		void Node::addNeighbor(Gex* gex)
		{
			gex_neighbor.insert(gex);
		}
		void Node::addNeighbor(Facet* facet)
		{
			facet_neighbor.insert(facet);
		}
		void Node::addNeighbor(Node* node)
		{
			node_neighbor.insert(node);
		}

		void Settlement::diceEvent(Resurse r)
		{
			player->addResurse(r);
		}

		void Castle::diceEvent(Resurse r)
		{
			player->addResurse(r, 2);
		}







		

		

		void Construction::setBusy()
		{
			_free = false;
		}

		Player* Construction::getPlayer() const
		{
			return player;
		}

		void Construction::setPlayer(Player *p)
		{
			player = p;
		}

		bool Construction::isFree() const
		{
			return _free;
		}

		void Road::SetFacet(Facet* facet) {
			facet_ = facet;
		}
		Facet* Road::GetFacet() const {
			return facet_;
		}


		void Building::setFree()
		{
			_free = true;
		}


		out_of_range::out_of_range(const std::string& what): std::out_of_range{what}
		{

		}

		invalid_argument::invalid_argument(const std::string& what): std::invalid_argument{what}
		{

		}

		bool Facet::isFree() const
		{
			return road == nullptr;
		}

		bool Facet::isBusyBy(Player *p) const
		{
			if(isFree())
				return false;
			return road->getPlayer() == p;
		}

		bool Facet::haveNeighborFacetWith(Player *p) const 
		{
			for(auto& facet: facet_neighbor)
				if(facet->isBusyBy(p))
					return true;
			return false;
		}

		bool Facet::haveNeighborNodeWith(Player *p) const
		{
			for(auto& node: node_neighbor)
			{
				if(node->isBusyBy(p))
					return true;
			}
			return false;
		}

		void Facet::setRoad(Road *r)
		{
			if(road != nullptr)
				throw invalid_argument("Facet already busy");
			road = r;
			r->setBusy();
		}

		const Road* Facet::getRoad() const {
			return road;
		}

		void Facet::addNeighbor(Facet* f)
		{
			facet_neighbor.insert(f);
		}
		void Facet::addNeighbor(Node* n)
		{
			node_neighbor.insert(n);
		}

		void Bandit::setGex(Gex* gex) {
			gex_ = gex;
		}
		Gex* Bandit::getGex() {
			return gex_;
		}

	}
}
