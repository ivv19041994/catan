#pragma once

#include "catan.hpp"

#include <deque>
#include <vector>

namespace ivv {
namespace catan {

class IDevelopmentCardDeck {
public:
	virtual ~IDevelopmentCardDeck() = default;
	virtual bool Empty() const = 0;
	virtual DevelopmentCard Draw() = 0;
	virtual const std::deque<DevelopmentCard>* PersistenceCards() const { return nullptr; }
};

class DevelopmentCardDeck final : public IDevelopmentCardDeck {
public:
	DevelopmentCardDeck();
	explicit DevelopmentCardDeck(std::deque<DevelopmentCard> cards);

	bool Empty() const override;
	DevelopmentCard Draw() override;
	const std::deque<DevelopmentCard>& RemainingCards() const;
	const std::deque<DevelopmentCard>* PersistenceCards() const override { return &cards_; }

private:
	std::deque<DevelopmentCard> cards_;
};

}//namespace ivv::catan
}//namespace ivv
