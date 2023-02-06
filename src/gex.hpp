#pragma once

#include "catan.hpp"
#include <set>

namespace ivv {
namespace catan {
class Gex
{
	std::set<Node*> node_neighbor{};
	Bandit* bandit_ = nullptr;
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

}//namespace ivv::catan {
}//namespace ivv {