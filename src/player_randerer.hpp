#pragma once

#include "catan.hpp"
#include "svg.h"
#include "player.hpp"

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

	void RenderRoad(svg::Document& doc, svg::Point center) const;
	void RenderSett(svg::Document& doc, svg::Point center) const;
	void RenderCastle(svg::Document& doc, svg::Point center) const;
	void RenderWinPoint(svg::Document& doc, svg::Point center) const;
	
	void RenderResurse(svg::Document& doc, svg::Point center, Resurse resurse) const;
	void RenderDevCard(svg::Document& doc, svg::Point center, DevelopmentCard card) const;
};

}//namespace ivv::catan::renderer {
}//namespace ivv::catan {
}//namespace ivv{