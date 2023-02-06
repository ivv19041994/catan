#include "facet.hpp"

#include "exception.hpp"
#include "node.hpp"

namespace ivv {
namespace catan {

const std::set<Node*>& Facet::GetNeighborNodes() const {
	return node_neighbor;
}

bool Facet::isFree() const
{
	return road == nullptr;
}

bool Facet::isBusyBy(Player* p) const
{
	if (isFree())
		return false;
	return road->getPlayer() == p;
}

bool Facet::haveNeighborFacetWith(Player* p) const
{
	for (auto& facet : facet_neighbor)
		if (facet->isBusyBy(p))
			return true;
	return false;
}

bool Facet::haveNeighborNodeWith(Player* p) const
{
	for (auto& node : node_neighbor)
	{
		if (node->isBusyBy(p))
			return true;
	}
	return false;
}

void Facet::setRoad(Road* r)
{
	if (road != nullptr)
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

}//namespace ivv::catan {
}//namespace ivv {