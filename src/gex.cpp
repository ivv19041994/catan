#include "gex.hpp"

#include "node.hpp"

namespace ivv {
namespace catan {

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
	for (auto& node : node_neighbor)
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

}//namespace ivv::catan {
}//namespace ivv {