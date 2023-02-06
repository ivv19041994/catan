#pragma once

#include "catan.hpp"
#include "svg.h"
#include "map.hpp"
#include "player.hpp"

#include <ostream>

namespace ivv{
namespace catan {
namespace renderer {

	class MapRenderer {
	public:
		MapRenderer(const Map& map, svg::Point offset = {0.0, 0.0}, double scale = 1.0);
		void Render(std::ostream& os) const ;
	private:
		const Map& map_;
		svg::Point offset_;
		double scale_;
		
		svg::Color GexTypeToColor(Resurse type) const;
		svg::Color PlayerIdToColor(size_t id) const;

		void RenderGex(svg::Document& doc, const Gex& gex, svg::Point center) const;
		void RenderGexs(svg::Document& doc) const;
		void RenderBuilding(svg::Document& doc, const Building& building, svg::Point center) const;
		void RenderBuildings(svg::Document& doc) const;

		void RenderPort(svg::Document& doc, Resurse resurse, svg::Point center) const;

		void RenderRoad(svg::Document& doc, const Road& road, svg::Point center, double angele) const;
		void RenderRoads(svg::Document& doc) const;
	};
}//namespace renderer {
}//namespace catan {
}//namespace ivv{