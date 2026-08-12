#pragma once

#include "catan.hpp"
#include <set>

namespace ivv {
namespace catan {

class Facet
{
	std::set<Facet*> facet_neighbor{};
	std::set<Node*> node_neighbor{};
	Road* road = nullptr;

public:

	Facet() = default;
	void addNeighbor(Facet* facet);
	void addNeighbor(Node* node);
	bool isFree() const;
	bool isBusyBy(const Player* p) const;
	void setRoad(Road* r);
	bool haveNeighborFacetWith(const Player* p) const;
	bool haveNeighborNodeWith(const Player* p) const;
	const Road* getRoad() const;
	const std::set<Node*>& GetNeighborNodes() const;
};
}//namespace ivv::catan {
}//namespace ivv {
