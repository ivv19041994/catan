#include "development_card_deck.hpp"

#include "exception.hpp"

#include <algorithm>
#include <random>

namespace ivv {
namespace catan {

DevelopmentCardDeck::DevelopmentCardDeck() {
	cards_.insert(cards_.end(), 14, DevelopmentCard::Knights);
	cards_.insert(cards_.end(), 2, DevelopmentCard::RoadBuilding);
	cards_.insert(cards_.end(), 2, DevelopmentCard::YearOfPlenty);
	cards_.insert(cards_.end(), 2, DevelopmentCard::Monopoly);
	cards_.insert(cards_.end(), 1, DevelopmentCard::University);
	cards_.insert(cards_.end(), 1, DevelopmentCard::Market);
	cards_.insert(cards_.end(), 1, DevelopmentCard::GreatHall);
	cards_.insert(cards_.end(), 1, DevelopmentCard::Chapel);
	cards_.insert(cards_.end(), 1, DevelopmentCard::Library);

	std::random_device random_device;
	std::mt19937 generator(random_device());
	std::shuffle(cards_.begin(), cards_.end(), generator);
}

bool DevelopmentCardDeck::Empty() const {
	return cards_.empty();
}

DevelopmentCard DevelopmentCardDeck::Draw() {
	if (Empty()) {
		throw logic_error("Development card deck is empty!");
	}

	const DevelopmentCard card = cards_.front();
	cards_.pop_front();
	return card;
}

}//namespace ivv::catan
}//namespace ivv
