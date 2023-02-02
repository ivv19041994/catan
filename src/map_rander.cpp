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

void MapRenderer::RenderGex(svg::Document& doc, const Gex& gex, svg::Point center) const {

	svg::Point scale_center{
		center.x* scale_ + offset_.x, center.y* scale_ + offset_.y
	};

	shapes::Hexagon(
		scale_center,
		scale_, degreeToRad(30))
		.SetFillColor(GexTypeToColor(gex.getType()))
		.SetStrokeColor("black")
		.SetStrokeWidth(scale_ / 20)
		.Draw(doc);
	double font_size = scale_ / 1.3;

	int dice = gex.getDice();
	std::string gex_text = gex.isBandit() ? "B" : (dice ? std::to_string(dice) : "");

	doc.Add(
		svg::Text()
		.SetFontSize(font_size)
		.SetFillColor("black")
		.SetData(gex_text)
		.SetPosition(scale_center)
		.SetOffset({ -font_size  / 4, font_size / 4})
	);
	
}

void MapRenderer::RenderBuilding(svg::Document& doc, const Building& building, svg::Point center) const {

	svg::Point scale_center{
		center.x * scale_ + offset_.x, center.y * scale_ + offset_.y
	};

	shapes::Hexagon(
		scale_center,
		scale_ / 4, degreeToRad(30))
		.SetFillColor(PlayerIdToColor(building.getPlayer()->getId()))
		.SetStrokeColor("black")
		.SetStrokeWidth(scale_ / 20)
		.Draw(doc);
	double font_size = scale_ / 1.3;
}

static std::vector<svg::Point> getGexCenters() {
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
	return centers;
}

static std::vector<svg::Point> getNodesCenters() {
	std::vector<svg::Point> centers{};
	const double left_to_center = std::cos(degreeToRad(30));
	const double sin30 = std::sin(degreeToRad(30));
	const double weight = left_to_center * 2;

	for (size_t i = 0; i < 7; ++i) {
		centers.push_back({ 
			left_to_center * (2 + i),
			0.0 + (i % 2) ? 0.0 : sin30
			}
		);
	}

	for (size_t i = 0; i < 9; ++i) {
		centers.push_back({
			left_to_center * (1 + i),
			sin30 + 1.0 + ((i % 2) ? 0.0 : sin30)
			}
		);
	}

	for (size_t i = 0; i < 11; ++i) {
		centers.push_back({
			left_to_center * (0 + i),
			2 *(sin30 + 1.0) + ((i % 2) ? 0.0 : sin30)
			}
		);
	}

	for (size_t i = 0; i < 11; ++i) {
		centers.push_back({
			left_to_center * (0 + i),
			3 * (sin30 + 1.0) + ((i % 2 == 0) ? 0.0 : sin30)
			}
		);
	}

	for (size_t i = 0; i < 9; ++i) {
		centers.push_back({
			left_to_center * (1 + i),
			4 * (sin30 + 1.0) + ((i % 2 == 0) ? 0.0 : sin30)
			}
		);
	}

	for (size_t i = 0; i < 7; ++i) {
		centers.push_back({
			left_to_center * (2 + i),
			5 * (sin30 + 1.0) + ((i % 2 == 0) ? 0.0 : sin30)
			}
		);
	}

	return centers;
}

struct RoadSettings {
	svg::Point center;
	double angele;
};

static std::vector<RoadSettings> getRoadCenters() {
	std::vector<RoadSettings> centers{};
	const double left_to_center = std::cos(degreeToRad(30));
	const double sin30 = std::sin(degreeToRad(30));
	const double weight = left_to_center * 2;

	for (size_t i = 0; i < 6; ++i) {
		centers.push_back({
			{
				left_to_center / 2 + left_to_center * (2 + i),
				sin30 / 2
			},
			(i % 2) ? degreeToRad(150) : degreeToRad(30)
			}
		);
	}

	for (size_t i = 0; i < 4; ++i) {
		centers.push_back({
			{
				left_to_center * 2 + i * weight,
				sin30 + 0.5
			},
			degreeToRad(90)
			}
		);
	}

	for (size_t i = 0; i < 8; ++i) {
		centers.push_back({
			{
				left_to_center / 2 + left_to_center * (1 + i),
				sin30 / 2 + 1 * (sin30 + 1.0)
			},
			(i % 2) ? degreeToRad(150) : degreeToRad(30)
			}
		);
	}

	for (size_t i = 0; i < 5; ++i) {
		centers.push_back({
			{
				left_to_center * 1 + i * weight,
				sin30 + 0.5 + 1.0 * (sin30 + 1.0)
			},
			degreeToRad(90)
			}
		);
	}

	for (size_t i = 0; i < 10; ++i) {
		centers.push_back({
			{
				left_to_center / 2 + left_to_center * i,
				sin30 / 2 + 2 * (sin30 + 1.0)
			},
			(i % 2) ? degreeToRad(150) : degreeToRad(30)
			}
		);
	}

	for (size_t i = 0; i < 6; ++i) {
		centers.push_back({
			{
				left_to_center * 0 + i * weight,
				sin30 + 0.5 + 2.0 * (sin30 + 1.0)
			},
			degreeToRad(90)
			}
		);
	}

	for (size_t i = 0; i < 10; ++i) {
		centers.push_back({
			{
				left_to_center / 2 + left_to_center * i,
				sin30 / 2 + 3 * (sin30 + 1.0)
			},
			(i % 2) ? degreeToRad(30) : degreeToRad(150)
			}
		);
	}

	for (size_t i = 0; i < 5; ++i) {
		centers.push_back({
			{
				left_to_center * 1 + i * weight,
				sin30 + 0.5 + 3.0 * (sin30 + 1.0)
			},
			degreeToRad(90)
			}
		);
	}

	for (size_t i = 0; i < 8; ++i) {
		centers.push_back({
			{
				left_to_center / 2 + left_to_center * (1 + i),
				sin30 / 2 + 4 * (sin30 + 1.0)
			},
			(i % 2) ? degreeToRad(30) : degreeToRad(150)
			}
		);
	}

	for (size_t i = 0; i < 4; ++i) {
		centers.push_back({
			{
				left_to_center * 2 + i * weight,
				sin30 + 0.5 + 4.0 * (sin30 + 1.0)
			},
			degreeToRad(90)
			}
		);
	}

	for (size_t i = 0; i < 6; ++i) {
		centers.push_back({
			{
				left_to_center / 2 + left_to_center * (2 + i),
				sin30 / 2 + 5 * (sin30 + 1.0)
			},
			(i % 2) ? degreeToRad(30) : degreeToRad(150)
			}
		);
	}

	return centers;
}

void MapRenderer::RenderGexs(svg::Document& doc) const {
	auto& gexes = map_.GetGexes();
	std::vector<svg::Point> centers = getGexCenters();

	for (size_t i = 0; i < gexes.size(); ++i) {
		RenderGex(doc, gexes[i], centers[i]);
	}
}

void MapRenderer::RenderBuildings(svg::Document& doc) const {
	auto centers = getNodesCenters();
	auto& nodes = map_.GetNodes();
	for (size_t i = 0; i < centers.size(); ++i) {
		const Building* b = nodes[i].getBuilding();
		if (b) {
			RenderBuilding(doc, *b, centers[i]);
		}
	}
}

void MapRenderer::RenderRoad(svg::Document& doc, const Road& road, svg::Point center, double angele) const {
	svg::Point scale_center{
		center.x * scale_ + offset_.x, center.y * scale_ + offset_.y
	};

	shapes::Road(scale_center, scale_ / 5, angele)
		.SetFillColor(PlayerIdToColor(road.getPlayer()->getId()))
		.SetStrokeColor("black")
		.SetStrokeWidth(scale_ / 20)
		.Draw(doc);
}
void MapRenderer::RenderRoads(svg::Document& doc) const {
	auto centers = getRoadCenters();
	auto& facets = map_.GetFacets();
	for (size_t i = 0; i < centers.size(); ++i) {
		const Road* r = facets[i].getRoad();
		if (r) {
			RenderRoad(doc, *r, centers[i].center, centers[i].angele);
		}
	}
}

void MapRenderer::Render(std::ostream& os) const {
	svg::Document doc;

	RenderGexs(doc);
	RenderBuildings(doc);
	RenderRoads(doc);
	doc.Render(os);
}

svg::Color MapRenderer::GexTypeToColor(Resurse type) const {
	switch (type) {
	case Resurse::Clay: return "brown";
	case Resurse::Hay: return "yellow";
	case Resurse::Not: return "rgb(255, 252, 179)";
	case Resurse::Sheep: return "rgb(124,252,0)";
	case Resurse::Stone: return "rgb(201, 201, 201)";
	case Resurse::Wood: return "rgb(0,160,0)";
	}
}

svg::Color MapRenderer::PlayerIdToColor(size_t id) const {
	switch (id) {
	case 0: return "red";
	case 1: return "green";
	case 2: return "blue";
	case 3: return "yellow";
	default:
		return "white";
	}
}

}//namespace renderer {
}//namespace catan {
}//namespace ivv{