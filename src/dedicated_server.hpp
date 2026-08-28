#pragma once

#include "game_controller.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ivv::catan::dedicated {

struct Resources {
    int wood = 0;
    int clay = 0;
    int hay = 0;
    int sheep = 0;
    int stone = 0;

    bool operator==(const Resources&) const = default;
};

enum class Command : int {
    BuildSettlement,
    BuildRoad,
    BuildCity,
    MoveRobber,
    ChooseRobberVictim,
    DropResources,
    RollDice,
    BuyDevelopmentCard,
    Pass,
    UseDevelopmentCard,
    BankTrade,
    OfferTrade,
    AcceptTrade,
    CancelTrade,
    SelectBoardAction
};

struct CommandArgs {
    int first = 0;
    int second = 0;
    std::string text;
    Resources first_resources;
    Resources second_resources;
};

struct Result {
    bool ok = false;
    std::string message;
};

struct IdentityResult : Result {
    IdentityResult() = default;
    IdentityResult(bool succeeded, std::string detail)
    {
        ok = succeeded;
        message = std::move(detail);
    }

    std::string lobby_token;
    std::string player_token;
    std::string player_name;
};

struct LobbyPlayerSnapshot {
    int id = -1;
    std::string name;
    bool ready = false;
    bool host = false;
};

struct PlayerSnapshot {
    int id = -1;
    std::string name;
    bool current = false;
    bool local = false;
    bool resources_visible = false;
    int victory_points = 0;
    int victory_point_cards = 0;
    int resource_cards = 0;
    int development_cards = 0;
    int free_settlements = 0;
    int free_cities = 0;
    int free_roads = 0;
    Resources resources;
    Resources trade_rates;
    int knights = 0;
    int road_building = 0;
    int year_of_plenty = 0;
    int monopoly = 0;
    int pending_development = 0;
    bool largest_army = false;
    bool longest_road = false;
};

struct HexSnapshot {
    int id = -1;
    int resource = 5;
    int dice = 0;
    bool robber = false;
};

struct NodeSnapshot {
    int id = -1;
    int owner = -1;
    bool city = false;
};

struct RoadSnapshot {
    int id = -1;
    int owner = -1;
};

struct DealSnapshot {
    bool active = false;
    std::string offering_player;
    std::string target_player;
    Resources offered;
    Resources requested;
};

struct Snapshot {
    std::uint64_t revision = 0;
    bool playing = false;
    std::string lobby_name;
    std::string local_player;
    std::vector<LobbyPlayerSnapshot> lobby_players;

    std::string current_player;
    std::string step;
    int phase = 0;
    int board_action = 0;
    int first_die = 0;
    int second_die = 0;
    std::string winner;
    std::string status;
    int required_discard = 0;
    int pending_robber_hex = -1;
    std::vector<std::string> robber_victims;
    DealSnapshot deal;
    Resources bank_resources{19, 19, 19, 19, 19};
    std::vector<int> valid_nodes;
    std::vector<int> valid_roads;
    std::vector<int> valid_hexes;
    bool has_settlement_target = false;
    bool has_city_target = false;
    bool has_road_target = false;
    std::vector<std::string> events;
    std::vector<PlayerSnapshot> players;
    std::vector<HexSnapshot> hexes;
    std::vector<NodeSnapshot> nodes;
    std::vector<RoadSnapshot> roads;
};

class Service final {
public:
    using TokenFactory = std::function<std::string(std::size_t, bool)>;

    explicit Service(std::size_t max_lobbies = 128, TokenFactory token_factory = {});
    ~Service();

    IdentityResult CreateLobby(std::string player_name, std::string lobby_name);
    IdentityResult JoinLobby(std::string_view lobby_token, std::string player_name);
    Result LeaveLobby(std::string_view lobby_token, std::string_view player_token);
    Result SetReady(std::string_view lobby_token, std::string_view player_token, bool ready);
    Result StartGame(std::string_view lobby_token, std::string_view player_token);
    Result Execute(std::string_view lobby_token, std::string_view player_token,
        Command command, const CommandArgs& args = {});
    std::optional<Snapshot> GetSnapshot(std::string_view lobby_token,
        std::string_view player_token, std::string& error);
    std::size_t LobbyCount() const;

    // Server-owned persistence. The payload contains private authentication
    // tokens and must never be sent to a client or stored in a public place.
    std::string SerializeState() const;
    Result RestoreState(std::string_view state);

private:
    struct Lobby;
    std::size_t max_lobbies_;
    TokenFactory token_factory_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Lobby>> lobbies_;

    std::string NewToken(std::size_t length, bool lobby);
};

} // namespace ivv::catan::dedicated
