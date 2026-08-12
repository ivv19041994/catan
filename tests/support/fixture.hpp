#pragma once

#include "fakes.hpp"
#include "test.hpp"

#include <array>
#include <sstream>

namespace test {

inline std::string Step(ivv::catan::GameController& game) {
    std::ostringstream out;
    game.PrintStep(out);
    return out.str();
}

inline std::vector<std::string> CompleteSetup(ivv::catan::GameController& game, size_t player_count) {
    constexpr std::array<size_t, 8> nodes{0, 3, 6, 16, 26, 38, 46, 53};
    constexpr std::array<size_t, 8> roads{0, 2, 5, 23, 38, 54, 61, 71};
    std::vector<std::string> order;
    for (size_t i = 0; i < player_count * 2; ++i) {
        order.push_back(game.GetCurrentPlayer());
        game.BuildSettlement(order.back(), nodes[i]);
        game.BuildRoad(order.back(), roads[i]);
    }
    return order;
}

inline void AddRoll(ControlledGame& controlled, size_t first, size_t second) {
    controlled.first_die->rolls.push_back(first);
    controlled.second_die->rolls.push_back(second);
}

inline ivv::catan::Player& MutablePlayer(ivv::catan::GameController& game, std::string_view name) {
    return const_cast<ivv::catan::Player&>(game.GetPlayer(name));
}

inline constexpr std::array<ivv::catan::Resurse, 5> Resources{
    ivv::catan::Resurse::Wood, ivv::catan::Resurse::Clay,
    ivv::catan::Resurse::Hay, ivv::catan::Resurse::Sheep,
    ivv::catan::Resurse::Stone
};

inline void SeedResources(ivv::catan::Player& player,
                          const std::map<ivv::catan::Resurse, size_t>& resources) {
    std::map<ivv::catan::Resurse, size_t> existing;
    for (auto resource : Resources) {
        const size_t count = player.getCountResurses(resource);
        if (count) existing[resource] = count;
    }
    if (!existing.empty()) player.Drop(existing);
    player.addResurse(resources);
}

inline void SeedResources(ivv::catan::GameController& game, std::string_view player,
                          const std::map<ivv::catan::Resurse, size_t>& resources) {
    SeedResources(MutablePlayer(game, player), resources);
}

inline std::array<size_t, 5> ResourceCounts(const ivv::catan::Player& player) {
    std::array<size_t, 5> result{};
    for (size_t i = 0; i < Resources.size(); ++i)
        result[i] = player.getCountResurses(Resources[i]);
    return result;
}

inline void EnterCommonPlay(ControlledGame& controlled, size_t player_count = 2,
                            size_t first = 1, size_t second = 1) {
    CompleteSetup(*controlled.game, player_count);
    AddRoll(controlled, first, second);
    controlled.game->Dice(controlled.game->GetCurrentPlayer());
}

} // namespace test
