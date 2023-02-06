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
}
}
