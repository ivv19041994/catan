#pragma once

#include <random>
#include <vector>

namespace ivv {
namespace game {

class IDice {
public:
	virtual ~IDice() = default;
	virtual size_t Roll() = 0;
};

class Dice final : public IDice {
public:
	Dice();
	explicit Dice(size_t count);
	struct DropResult{
		size_t result{};
		std::vector<size_t> each{};
	};

	size_t Roll() override;
	DropResult Drop() const;
private:
	const size_t count_;
	std::random_device rd_;
	mutable std::default_random_engine rng_;
	mutable std::uniform_int_distribution<size_t> distribution_{1, 6};
};
	

}//namespace ivv::game {
}//namespace ivv {
