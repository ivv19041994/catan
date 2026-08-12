#include "dice.hpp"

namespace ivv {
namespace game {

Dice::Dice()
	: Dice(1) {
}

Dice::Dice(size_t count)
	: count_{ count }
	, rd_{}
	, rng_{ rd_()} {
}

size_t Dice::Roll() {
	return distribution_(rng_);
}

Dice::DropResult Dice::Drop() const {
	DropResult res{};

	for (size_t i = 0; i < count_; ++i) {
		size_t drop = distribution_(rng_);
		res.each.push_back(drop);
		res.result += drop;
	}
	//return { 7, { 3,4 } };
	return res;
}

}//namespace ivv::game {
}//namespace ivv {
