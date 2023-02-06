#include "catan.hpp"

#include "map_rander.h"

#include <deque>
#include <cassert>
#include <utility>
#include <iostream>
#include <algorithm>
#include "player.hpp"
#include "exception.hpp"


namespace ivv{
namespace catan{

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

void Bandit::setGex(Gex* gex) {
	gex_ = gex;
}

Gex* Bandit::getGex() {
	return gex_;
}
}
}
