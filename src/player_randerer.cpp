#include "player_randerer.hpp"
#include <numbers>

namespace ivv {
namespace catan {
namespace renderer {

static constexpr double degreeToRad(double degree) {
	return (degree / 180.0) * std::numbers::pi;
}

PlayerRanderer::PlayerRanderer(const Player& player, svg::Point offset, double scale)
	: player_{ player}
	, offset_{ offset }
	, scale_{scale} {
}

void PlayerRanderer::RenderRoad(svg::Document& doc, svg::Point center) const {
	svg::Point scale_center{
		center.x * scale_ + offset_.x, center.y * scale_ + offset_.y
	};

	shapes::Road(scale_center, scale_ / 3, degreeToRad(30))
		.SetFillColor(PlayerIdToColor(player_.getId()))
		.SetStrokeColor("black")
		.SetStrokeWidth(scale_ / 20 )
		.Draw(doc);

	double font_size = scale_ / 1.3;

	doc.Add(
		svg::Text()
		.SetFontSize(font_size)
		.SetFillColor("black")
		.SetData(std::to_string(player_.getFreeRoadCount()))
		.SetPosition(scale_center)
		.SetOffset({ font_size, font_size / 2 })
	);
}

void PlayerRanderer::RenderSett(svg::Document& doc, svg::Point center) const {
	svg::Point scale_center{
		center.x * scale_ + offset_.x, center.y * scale_ + offset_.y
	};

	shapes::Hexagon(
		scale_center,
		scale_ / 3, degreeToRad(30))
		.SetFillColor(PlayerIdToColor(player_.getId()))
		.SetStrokeColor("black")
		.SetStrokeWidth(scale_ / 20)
		.Draw(doc);

	double font_size = scale_ / 1.3;

	doc.Add(
		svg::Text()
		.SetFontSize(font_size)
		.SetFillColor("black")
		.SetData(std::to_string(player_.getFreeSettlementCount()))
		.SetPosition(scale_center)
		.SetOffset({ font_size , font_size / 2 })
	);
}

void PlayerRanderer::RenderCastle(svg::Document& doc, svg::Point center) const {
	svg::Point scale_center{
		center.x * scale_ + offset_.x, center.y * scale_ + offset_.y
	};

	shapes::Hexagon(
		scale_center,
		scale_ / 2, degreeToRad(0))
		.SetFillColor(PlayerIdToColor(player_.getId()))
		.SetStrokeColor("black")
		.SetStrokeWidth(scale_ / 20)
		.Draw(doc);

	double font_size = scale_ / 1.3;

	doc.Add(
		svg::Text()
		.SetFontSize(font_size)
		.SetFillColor("black")
		.SetData(std::to_string(player_.getFreeCastleCount()))
		.SetPosition(scale_center)
		.SetOffset({ font_size , font_size / 2 })
	);
}

void PlayerRanderer::RenderResurse(svg::Document& doc, svg::Point center, Resurse resurse) const {
	size_t count = player_.getCountResurses(resurse);

	svg::Point scale_center{
		center.x * scale_ + offset_.x, center.y * scale_ + offset_.y
	};

	shapes::Road(scale_center, scale_ / 2, degreeToRad(90))
		.SetFillColor(GexTypeToColor(resurse))
		.SetStrokeColor("black")
		.SetStrokeWidth(scale_ / 20)
		.Draw(doc);

	double font_size = scale_ / 1.3;

	doc.Add(
		svg::Text()
		.SetFontSize(font_size)
		.SetFillColor("black")
		.SetData(std::to_string(count))
		.SetPosition(scale_center)
		.SetOffset({ font_size, font_size / 2 })
	);
}

static std::string CardToString(DevelopmentCard card) {
	switch (card) {
	case DevelopmentCard::Knights:
	return "K";
	case DevelopmentCard::RoadBuilding:
	return "R";
	case DevelopmentCard::YearOfPlenty:
	return "Y";
	case DevelopmentCard::Monopoly:
	return "M";
	case DevelopmentCard::University:
	case DevelopmentCard::Market:
	case DevelopmentCard::GreatHall:
	case DevelopmentCard::Chapel:
	case DevelopmentCard::Library:
	return "W";
	}
	return "X";
}

void PlayerRanderer::RenderDevCard(svg::Document& doc, svg::Point center, DevelopmentCard card) const {

	svg::Point scale_center{
		center.x * scale_ + offset_.x, center.y * scale_ + offset_.y
	};

	shapes::Road(scale_center, scale_ / 2, degreeToRad(90))
		.SetFillColor("gray")
		.SetStrokeColor("black")
		.SetStrokeWidth(scale_ / 20)
		.Draw(doc);

	double font_size = scale_ * 0.4;

	doc.Add(
		svg::Text()
		.SetFontSize(font_size)
		.SetFillColor("black")
		.SetData(CardToString(card))
		.SetPosition(scale_center)
		.SetOffset({ -font_size / 2, font_size / 2 })
	);

	font_size = scale_ / 1.3;

	doc.Add(
		svg::Text()
		.SetFontSize(font_size)
		.SetFillColor("black")
		.SetData(
			std::to_string(player_.GetReadyForUseCardCount(card)) + "/" +
			std::to_string(player_.GetPurchasedCardCount(card)) + "/" +
			std::to_string(player_.GetUsedCardCount(card))
		)
		.SetPosition(scale_center)
		.SetOffset({ font_size, font_size / 2 })
	);
}

void PlayerRanderer::Render(std::ostream& os) const {
	svg::Document doc;
	RenderRoad(doc, { 1.0, 1.0 });
	RenderSett(doc, { 1.0, 2.0 });
	RenderCastle(doc, { 1.0, 3.0 });

	RenderResurse(doc, { 3.0, 1.0 }, Resurse::Wood);
	RenderResurse(doc, { 3.0, 2.0 }, Resurse::Clay);
	RenderResurse(doc, { 3.0, 3.0 }, Resurse::Hay);
	RenderResurse(doc, { 3.0, 4.0 }, Resurse::Sheep);
	RenderResurse(doc, { 3.0, 5.0 }, Resurse::Stone);

	RenderDevCard(doc, { 6.0, 1.0 }, DevelopmentCard::Knights);
	RenderDevCard(doc, { 6.0, 2.0 }, DevelopmentCard::RoadBuilding);
	RenderDevCard(doc, { 6.0, 3.0 }, DevelopmentCard::YearOfPlenty);
	RenderDevCard(doc, { 6.0, 4.0 }, DevelopmentCard::Monopoly);

	RenderDevCard(doc, { 9.0, 1.0 }, DevelopmentCard::University);
	RenderDevCard(doc, { 9.0, 2.0 }, DevelopmentCard::Market);
	RenderDevCard(doc, { 9.0, 3.0 }, DevelopmentCard::GreatHall);
	RenderDevCard(doc, { 9.0, 4.0 }, DevelopmentCard::Chapel);
	RenderDevCard(doc, { 9.0, 5.0 }, DevelopmentCard::Library);
	doc.Render(os);

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