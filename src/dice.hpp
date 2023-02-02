#pragma once

#include <random>

namespace ivv {
namespace game {

class Dice {
public:
	Dice(size_t count);
	struct DropResult{
		size_t result{};
		std::vector<size_t> each{};
	};

	DropResult Drop() const;
private:
	const size_t count_;
	std::random_device rd_;
	mutable std::default_random_engine rng_;
};
	

}//namespace ivv::game {
}//namespace ivv {