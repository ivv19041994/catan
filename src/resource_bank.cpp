#include "resource_bank.hpp"

#include "exception.hpp"

#include <algorithm>

namespace ivv::catan {

ResourceBank::ResourceBank()
{
	counts_.fill(CardsPerResource);
}

ResourceBank::ResourceBank(const std::map<Resurse, size_t>& counts)
{
	for (Resurse resource : Resources) {
		const auto found = counts.find(resource);
		counts_[Index(resource)] = found == counts.end() ? CardsPerResource : found->second;
		if (counts_[Index(resource)] > CardsPerResource)
			throw invalid_argument("Resource bank count exceeds physical supply");
	}
}

size_t ResourceBank::Index(Resurse resource)
{
	for (size_t index = 0; index < Resources.size(); ++index)
		if (Resources[index] == resource) return index;
	throw invalid_argument("Not is not a bank resource");
}

size_t ResourceBank::Count(Resurse resource) const
{
	return counts_[Index(resource)];
}

bool ResourceBank::CanTake(Resurse resource, size_t count) const
{
	return Count(resource) >= count;
}

void ResourceBank::Take(Resurse resource, size_t count)
{
	const size_t index = Index(resource);
	if (counts_[index] < count) throw logic_error("Resource bank does not contain enough cards");
	counts_[index] -= count;
}

void ResourceBank::Return(Resurse resource, size_t count)
{
	const size_t index = Index(resource);
	// Public Player mutation is retained for compatibility with the legacy
	// standalone API and tests. Capping here keeps the physical bank valid even
	// if such externally injected cards are later paid back.
	counts_[index] = count >= CardsPerResource - counts_[index]
		? CardsPerResource : counts_[index] + count;
}

std::map<Resurse, size_t> ResourceBank::Counts() const
{
	std::map<Resurse, size_t> result;
	for (Resurse resource : Resources) result[resource] = Count(resource);
	return result;
}

std::vector<size_t> ResourceBank::DistributeProduction(Resurse resource,
		std::span<const size_t> claims)
{
	std::vector<size_t> grants(claims.begin(), claims.end());
	size_t total = 0;
	size_t recipients = 0;
	for (size_t claim : claims) {
		total += claim;
		if (claim != 0) ++recipients;
	}
	if (total == 0) return grants;
	if (CanTake(resource, total)) {
		Take(resource, total);
		return grants;
	}
	std::fill(grants.begin(), grants.end(), 0);
	if (recipients == 1) {
		for (size_t index = 0; index < claims.size(); ++index) {
			if (claims[index] == 0) continue;
			grants[index] = Count(resource);
			Take(resource, grants[index]);
			break;
		}
	}
	return grants;
}

} // namespace ivv::catan
