#pragma once

#include "catan.hpp"
#include "svg.h"

namespace ivv {
namespace catan {
namespace renderer {

class PlayerRanderer {
public:
	PlayerRanderer(const Player& player, svg::Point offset = { 0.0, 0.0 }, double scale = 1.0);
	void Render(std::ostream& os) const;
private:
	const Player& player_;
	svg::Point offset_;
	double scale_;

	svg::Color GexTypeToColor(Resurse type) const;
	svg::Color PlayerIdToColor(size_t id) const;
};

}//namespace ivv::catan::renderer {
}//namespace ivv::catan {
}//namespace ivv{