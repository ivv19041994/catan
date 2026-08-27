#pragma once

#include "catan.hpp"

#include <array>
#include <cstddef>
#include <map>
#include <span>
#include <vector>

namespace ivv::catan {

// Core-owned physical resource-card supply. This type deliberately contains
// no rendering, transport, or Unreal concepts so the rules module remains
// independently buildable and testable.
class ResourceBank final {
public:
	static constexpr size_t CardsPerResource = 19;
	static constexpr std::array<Resurse, 5> Resources{
		Resurse::Wood, Resurse::Clay, Resurse::Hay, Resurse::Sheep, Resurse::Stone};

	ResourceBank();
	explicit ResourceBank(const std::map<Resurse, size_t>& counts);

	size_t Count(Resurse resource) const;
	bool CanTake(Resurse resource, size_t count = 1) const;
	void Take(Resurse resource, size_t count = 1);
	void Return(Resurse resource, size_t count = 1);
	std::map<Resurse, size_t> Counts() const;
	std::vector<size_t> DistributeProduction(Resurse resource,
		std::span<const size_t> claims);

private:
	std::array<size_t, Resources.size()> counts_{};
	static size_t Index(Resurse resource);
};

} // namespace ivv::catan
