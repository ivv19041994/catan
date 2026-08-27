#include "resource_bank.hpp"
#include "test.hpp"

using namespace ivv::catan;

int main() { return test::Run({
    {"physical bank starts with nineteen cards of every resource", [] {
        ResourceBank bank;
        for (Resurse resource : ResourceBank::Resources)
            test::Equal(bank.Count(resource), size_t{19}, "base supply is nineteen");
    }},
    {"normal production pays every claim and consumes physical cards", [] {
        ResourceBank bank;
        const std::array<size_t, 3> claims{1, 2, 0};
        const auto grants = bank.DistributeProduction(Resurse::Wood, claims);
        test::Equal(grants, std::vector<size_t>({1, 2, 0}), "all claims are fulfilled");
        test::Equal(bank.Count(Resurse::Wood), size_t{16}, "three cards leave the bank");
    }},
    {"short supply pays the sole recipient every remaining card", [] {
        ResourceBank bank;
        bank.Take(Resurse::Clay, 17);
        const std::array<size_t, 3> claims{0, 4, 0};
        const auto grants = bank.DistributeProduction(Resurse::Clay, claims);
        test::Equal(grants, std::vector<size_t>({0, 2, 0}), "sole recipient gets the remainder");
        test::Equal(bank.Count(Resurse::Clay), size_t{0}, "resource pile is exhausted");
    }},
    {"short supply pays nobody when several players are owed cards", [] {
        ResourceBank bank;
        bank.Take(Resurse::Stone, 17);
        const std::array<size_t, 3> claims{1, 2, 0};
        const auto grants = bank.DistributeProduction(Resurse::Stone, claims);
        test::Equal(grants, std::vector<size_t>({0, 0, 0}), "all competing claims are denied");
        test::Equal(bank.Count(Resurse::Stone), size_t{2}, "unpaid cards remain in bank");
    }},
    {"invalid resource and overdraw are rejected", [] {
        ResourceBank bank;
        test::Throws([&] { bank.Take(Resurse::Wood, 20); }, "cannot overdraw supply");
        test::Throws([&] { (void)bank.Count(Resurse::Not); }, "desert is not a bank pile");
    }},
}); }
