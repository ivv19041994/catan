#pragma once

#include "catan.hpp"

#include <deque>

namespace ivv {
namespace catan {

class IDevelopmentCardDeck {
public:
	virtual ~IDevelopmentCardDeck() = default;
	virtual bool Empty() const = 0;
	virtual DevelopmentCard Draw() = 0;
};

class DevelopmentCardDeck final : public IDevelopmentCardDeck {
public:
	DevelopmentCardDeck();

	bool Empty() const override;
	DevelopmentCard Draw() override;

private:
	std::deque<DevelopmentCard> cards_;
};

}//namespace ivv::catan
}//namespace ivv
