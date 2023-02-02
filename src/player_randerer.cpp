#include "player_randerer.hpp"

namespace ivv {
namespace catan {
namespace renderer {

PlayerRanderer::PlayerRanderer(const Player& player, svg::Point offset, double scale)
	: player_{ player}
	, offset_{ offset }
	, scale_{scale} {
}

void PlayerRanderer::Render(std::ostream& os) const {

};

svg::Color PlayerRanderer::GexTypeToColor(Resurse type) const {
	switch (type) {
	case Resurse::Clay: return "brown";
	case Resurse::Hay: return "yellow";
	case Resurse::Not: return "rgb(255, 252, 179)";
	case Resurse::Sheep: return "rgb(124,252,0)";
	case Resurse::Stone: return "rgb(201, 201, 201)";
	case Resurse::Wood: return "rgb(0,160,0)";
	}
}

svg::Color PlayerRanderer::PlayerIdToColor(size_t id) const {
	switch (id) {
	case 0: return "red";
	case 1: return "green";
	case 2: return "blue";
	case 3: return "yellow";
	default:
		return "white";
	}
}

}//namespace ivv::catan::renderer {
}//namespace ivv::catan {
}//namespace ivv{