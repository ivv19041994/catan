#pragma once

#include "catan.hpp"
#include <set>

namespace ivv {
namespace catan {

class Node
{
	std::set<Gex*> gex_neighbor{};
	std::set<Facet*> facet_neighbor{};
	std::set<Node*> node_neighbor{};
	Building* building = nullptr;

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
	bool isBusyBy(Player* p) const;

	std::set<Gex*> getNeighborGexs();
	const std::set<Facet*>& getNeighborFacets() const;

	template<typename Func>
	void SetBuilderChanger(Func func);
};

template<typename Func>
void Node::SetBuilderChanger(Func func) {
	change_builder_func_ = func;
}

}//namespace ivv::catan {
}//namespace ivv {