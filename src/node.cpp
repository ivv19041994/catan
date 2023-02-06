#include "node.hpp"

namespace ivv {
namespace catan {


void Node::diceEvent(Resurse r)
{
	if (building == nullptr)
		return;
	building->diceEvent(r);
}

bool Node::neighborsNodeIsFree() const
{
	for (auto nn : node_neighbor)
		if (!nn->isFree())
			return false;
	return true;
}

bool Node::isFree() const
{
	return building == nullptr;
}

bool Node::isBusyBy(Player* p) const
{
	if (isFree())
	{
		return false;
	}
	return building->getPlayer() == p;
}

void Node::setBuilding(Building* b)
{
	if (building != nullptr)
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

}//namespace ivv::catan {
}//namespace ivv {