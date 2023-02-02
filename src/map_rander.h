#pragma once

#include "catan.hpp"
#include "svg.h"

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
	};
}//namespace renderer {
}//namespace catan {
}//namespace ivv{