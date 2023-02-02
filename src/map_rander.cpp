#include "map_rander.h"

#include <cmath>
#include <numbers>
#include <array>

namespace ivv {
namespace catan {
namespace renderer {

static constexpr double degreeToRad(double degree) {
	return (degree / 180.0) * std::numbers::pi;
}

MapRenderer::MapRenderer(const Map& map, svg::Point offset, double scale)
	: map_{ map } 
	, offset_{ offset }
	, scale_{ scale } {

}

void MapRenderer::Render(std::ostream& os) const {
	auto gexes = map_.GetGexes();
	svg::Document doc;

	double left_to_center = std::cos(degreeToRad(30));
	double horizont_offset = 1.0 + std::sin(degreeToRad(30));
	double weight = left_to_center * 2;
	std::vector<svg::Point> centers{

	};

	for (size_t i = 0; i < 3; ++i) {
		centers.push_back({ left_to_center * 3 + weight * i, 1.0 });
	}

	for (size_t i = 0; i < 4; ++i) {
		centers.push_back({ left_to_center * 2 + weight * i, 1.0 + horizont_offset });
	}

	for (size_t i = 0; i < 5; ++i) {
		centers.push_back({ left_to_center * 1 + weight * i, 1.0 + horizont_offset * 2 });
	}

	for (size_t i = 0; i < 4; ++i) {
		centers.push_back({ left_to_center * 2 + weight * i, 1.0 + horizont_offset * 3 });
	}
	
	for (size_t i = 0; i < 3; ++i) {
		centers.push_back({ left_to_center * 3 + weight * i, 1.0 + horizont_offset * 4 });
	}

	for (size_t i = 0; i < gexes.size(); ++i) {
		shapes::Hexagon(
			{ centers[i].x * scale_ + offset_.x, centers[i].y * scale_ + offset_.y},
			scale_, degreeToRad(30)).SetColor(GexTypeToColor(gexes[i].getType())).Draw(doc);
	}
	doc.Render(os);
}

svg::Color MapRenderer::GexTypeToColor(Resurse type) const {
	switch (type) {
	case Resurse::Clay: return "brown";
	case Resurse::Hay: return "yellow";
	case Resurse::Not: return "rgb(255, 252, 179)";
	case Resurse::Sheep: return "white";
	case Resurse::Stone: return "rgb(201, 201, 201)";
	case Resurse::Wood: return "green";
	}
}

}//namespace renderer {
}//namespace catan {
}//namespace ivv{