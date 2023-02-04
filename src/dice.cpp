#include "dice.hpp"

namespace ivv {
namespace game {

Dice::Dice(size_t count)
	: count_{ count }
	, rd_{}
	, rng_{ rd_()} {
}

Dice::DropResult Dice::Drop() const {
	DropResult res{};

	for (size_t i = 0; i < count_; ++i) {
		size_t drop = (rng_() % 6) + 1;
		res.each.push_back(drop);
		res.result += drop;
	}
	//return { 7, { 3,4 } };
	return res;
}

}//namespace ivv::game {
}//namespace ivv {