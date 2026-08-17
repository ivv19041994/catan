#include "dedicated_protocol.hpp"
#include "test.hpp"

#include <algorithm>
#include <memory>
#include <unordered_map>

using namespace ivv::catan::dedicated;

namespace {

struct Room {
    IdentityResult host;
    IdentityResult guest;
};

Service DeterministicService(std::size_t limit = 128)
{
    auto counter = std::make_shared<int>(0);
    return Service(limit, [counter](std::size_t, bool lobby) {
        ++*counter;
        return std::string(lobby ? "ROOM-" : "player-token-") + std::to_string(*counter);
    });
}

Room CreateStartedRoom(Service& service, std::string suffix = {})
{
    Room room{service.CreateLobby("Alice" + suffix, "Room" + suffix), {}};
    room.guest = service.JoinLobby(room.host.lobby_token, "Bob" + suffix);
    test::Check(room.host.ok && room.guest.ok, "room creation must succeed");
    test::Check(service.SetReady(room.host.lobby_token, room.host.player_token, true).ok, "host ready");
    test::Check(service.SetReady(room.host.lobby_token, room.guest.player_token, true).ok, "guest ready");
    test::Check(service.StartGame(room.host.lobby_token, room.host.player_token).ok, "host starts room");
    return room;
}

const IdentityResult& IdentityFor(const Room& room, std::string_view name)
{
    return room.host.player_name == name ? room.host : room.guest;
}

void CompleteSetup(Service& service, const Room& room)
{
    for (int action = 0; action < 4; ++action) {
        std::string error;
        auto view = service.GetSnapshot(room.host.lobby_token, room.host.player_token, error);
        test::Check(view.has_value(), "snapshot during setup");
        const auto& identity = IdentityFor(room, view->current_player);
        test::Check(!view->valid_nodes.empty(), "settlement target exists");
        CommandArgs settlement; settlement.first = view->valid_nodes.front();
        test::Check(service.Execute(room.host.lobby_token, identity.player_token,
            Command::BuildSettlement, settlement).ok, "setup settlement");
        view = service.GetSnapshot(room.host.lobby_token, identity.player_token, error);
        test::Check(view && !view->valid_roads.empty(), "road target exists");
        CommandArgs road; road.first = view->valid_roads.front();
        test::Check(service.Execute(room.host.lobby_token, identity.player_token,
            Command::BuildRoad, road).ok, "setup road");
    }
}

} // namespace

int main() { return test::Run({
    {"create issues separate lobby and private player tokens", [] {
        auto service = DeterministicService();
        const auto created = service.CreateLobby(" Alice ", " Family room ");
        test::Check(created.ok, "create succeeds");
        test::Check(!created.lobby_token.empty() && !created.player_token.empty(), "both tokens issued");
        test::Check(created.lobby_token != created.player_token, "tokens serve different purposes");
        test::Equal(created.player_name, std::string("Alice"), "name is sanitized");
    }},
    {"authentication rejects wrong lobby and player tokens", [] {
        auto service = DeterministicService();
        const auto host = service.CreateLobby("Alice", "Room");
        test::Check(!service.JoinLobby("wrong-room", "Bob").ok, "unknown room denied");
        test::Check(!service.SetReady(host.lobby_token, "wrong-player", true).ok, "wrong identity denied");
        std::string error;
        test::Check(!service.GetSnapshot(host.lobby_token, "wrong-player", error), "private snapshot denied");
    }},
    {"guest leave removes only that player and invalidates the token", [] {
        auto service = DeterministicService();
        const auto host = service.CreateLobby("Alice", "Room");
        const auto guest = service.JoinLobby(host.lobby_token, "Bob");
        test::Check(service.LeaveLobby(host.lobby_token, guest.player_token).ok, "guest can leave lobby");
        test::Equal(service.LobbyCount(), std::size_t{1}, "guest leave keeps lobby alive");
        std::string error;
        const auto host_view = service.GetSnapshot(host.lobby_token, host.player_token, error);
        test::Check(host_view && host_view->lobby_players.size() == 1, "host sees guest removed");
        test::Check(!service.GetSnapshot(host.lobby_token, guest.player_token, error), "departed token is invalid");
        test::Check(!service.LeaveLobby(host.lobby_token, guest.player_token).ok, "repeated leave is rejected");
    }},
    {"host leave closes lobby for every client", [] {
        auto service = DeterministicService();
        const auto host = service.CreateLobby("Alice", "Room");
        const auto guest = service.JoinLobby(host.lobby_token, "Bob");
        test::Check(service.LeaveLobby(host.lobby_token, host.player_token).ok, "host can close lobby");
        test::Equal(service.LobbyCount(), std::size_t{0}, "host leave deletes lobby");
        std::string error;
        test::Check(!service.GetSnapshot(host.lobby_token, guest.player_token, error),
            "guest cannot access closed lobby");
    }},
    {"leave is authenticated and limited to pre-game lobby", [] {
        auto service = DeterministicService();
        const auto room = CreateStartedRoom(service);
        test::Check(!service.LeaveLobby(room.host.lobby_token, "forged").ok, "forged leave denied");
        test::Check(!service.LeaveLobby(room.host.lobby_token, room.guest.player_token).ok,
            "playing client cannot use lobby leave");
        test::Equal(service.LobbyCount(), std::size_t{1}, "started game remains alive");
    }},
    {"duplicate names are made unique and lobby has four-player limit", [] {
        auto service = DeterministicService();
        const auto host = service.CreateLobby("Player", "Room");
        const auto second = service.JoinLobby(host.lobby_token, "player");
        const auto third = service.JoinLobby(host.lobby_token, "Player");
        const auto fourth = service.JoinLobby(host.lobby_token, "Player");
        test::Equal(second.player_name, std::string("player 2"), "case-insensitive duplicate gets suffix");
        test::Check(third.ok && fourth.ok, "four players admitted");
        test::Check(!service.JoinLobby(host.lobby_token, "Fifth").ok, "fifth player denied");
    }},
    {"only ready lobby creator starts and started room is closed", [] {
        auto service = DeterministicService();
        const auto host = service.CreateLobby("Alice", "Room");
        const auto guest = service.JoinLobby(host.lobby_token, "Bob");
        test::Check(!service.StartGame(host.lobby_token, guest.player_token).ok, "guest cannot start");
        test::Check(!service.StartGame(host.lobby_token, host.player_token).ok, "unready room cannot start");
        service.SetReady(host.lobby_token, host.player_token, true);
        service.SetReady(host.lobby_token, guest.player_token, true);
        test::Check(service.StartGame(host.lobby_token, host.player_token).ok, "ready host starts");
        test::Check(!service.JoinLobby(host.lobby_token, "Late").ok, "started room rejects joins");
    }},
    {"multiple lobbies own independent games and credentials", [] {
        auto service = DeterministicService();
        const auto first = CreateStartedRoom(service, " One");
        const auto second = CreateStartedRoom(service, " Two");
        test::Equal(service.LobbyCount(), std::size_t{2}, "both rooms retained");
        test::Check(first.host.lobby_token != second.host.lobby_token, "rooms have unique handles");
        std::string error;
        test::Check(!service.GetSnapshot(first.host.lobby_token, second.host.player_token, error),
            "credentials cannot cross lobby boundary");
    }},
    {"commands require authenticated current player", [] {
        auto service = DeterministicService();
        const auto room = CreateStartedRoom(service);
        std::string error;
        const auto view = service.GetSnapshot(room.host.lobby_token, room.host.player_token, error);
        const auto& current = IdentityFor(room, view->current_player);
        const auto& other = current.player_token == room.host.player_token ? room.guest : room.host;
        CommandArgs args; args.first = view->valid_nodes.front();
        test::Check(!service.Execute(room.host.lobby_token, other.player_token, Command::BuildSettlement, args).ok,
            "out-of-turn player denied");
        test::Check(!service.Execute(room.host.lobby_token, "forged", Command::BuildSettlement, args).ok,
            "forged token denied");
        test::Check(service.Execute(room.host.lobby_token, current.player_token, Command::BuildSettlement, args).ok,
            "current authenticated player accepted");
    }},
    {"personalized snapshots never disclose opponent private inventory", [] {
        auto service = DeterministicService();
        const auto room = CreateStartedRoom(service);
        CompleteSetup(service, room);
        std::string error;
        const auto alice = service.GetSnapshot(room.host.lobby_token, room.host.player_token, error);
        const auto bob = service.GetSnapshot(room.host.lobby_token, room.guest.player_token, error);
        test::Check(alice && bob, "both players can fetch view");
        for (const auto& player : alice->players) {
            if (player.local) {
                const int total = player.resources.wood + player.resources.clay + player.resources.hay
                    + player.resources.sheep + player.resources.stone;
                test::Equal(total, player.resource_cards, "local exact resource total is consistent");
                test::Check(player.resources_visible, "local inventory visible");
            } else {
                test::Check(!player.resources_visible && player.resources == Resources{}, "opponent inventory redacted");
            }
        }
        const std::string payload = protocol::SerializeSnapshot(*alice);
        test::Check(payload.find(room.guest.player_token) == std::string::npos, "private token absent from snapshot wire data");
        test::Check(payload.find(room.host.player_token) == std::string::npos, "own token also stays outside game state");
        std::string parse_error;
        const auto round_trip = protocol::DeserializeSnapshot(payload, parse_error);
        test::Check(round_trip && round_trip->playing, "full playing snapshot round-trips");
        test::Equal(round_trip->hexes.size(), std::size_t{19}, "all hexes survive transport");
        test::Equal(round_trip->nodes.size(), std::size_t{54}, "all nodes survive transport");
        test::Equal(round_trip->roads.size(), std::size_t{72}, "all roads survive transport");
    }},
    {"snapshot and request protocol round-trip unicode and delimiters", [] {
        auto service = DeterministicService();
        const std::string create = "CREATE\t" + protocol::HexEncode("Игрок\tA") + "\t" + protocol::HexEncode("Комната\nA");
        const std::string response = protocol::HandleRequest(service, create);
        const auto fields = protocol::Split(response, '\t');
        test::Check(fields.size() == 5 && fields[0] == "OK" && fields[1] == "CREATED", "create response shape");
        const std::string snapshot_response = protocol::HandleRequest(service, "SNAPSHOT\t" + fields[2] + "\t" + fields[3]);
        const auto snapshot_fields = protocol::Split(snapshot_response, '\t');
        test::Check(snapshot_fields.size() == 3 && snapshot_fields[1] == "SNAPSHOT", "snapshot response shape");
        const auto payload = protocol::HexDecode(snapshot_fields[2]);
        std::string error; const auto decoded = payload ? protocol::DeserializeSnapshot(*payload, error) : std::nullopt;
        test::Check(decoded.has_value(), "snapshot wire format parses");
        test::Equal(decoded->local_player, std::string("Игрок A"), "control characters sanitized after unicode transport");
        test::Check(response.find(fields[3]) != std::string::npos, "creator receives its own token once");
        test::Check(snapshot_response.find(fields[3]) == std::string::npos, "later response does not leak token in payload");
    }},
    {"leave request removes guest and host closes lobby", [] {
        auto service = DeterministicService();
        const auto host = service.CreateLobby("Alice", "Room");
        const auto guest = service.JoinLobby(host.lobby_token, "Bob");
        const auto guest_leave = protocol::Split(protocol::HandleRequest(service,
            "LEAVE\t" + host.lobby_token + "\t" + guest.player_token), '\t');
        test::Check(guest_leave.size() == 3 && guest_leave[0] == "OK" && guest_leave[1] == "LEFT",
            "guest leave protocol succeeds");
        const auto forged_leave = protocol::Split(protocol::HandleRequest(service,
            "LEAVE\t" + host.lobby_token + "\tforged"), '\t');
        test::Check(forged_leave.size() >= 2 && forged_leave[0] == "ERR", "forged leave protocol denied");
        const auto host_leave = protocol::Split(protocol::HandleRequest(service,
            "LEAVE\t" + host.lobby_token + "\t" + host.player_token), '\t');
        test::Check(host_leave.size() == 3 && host_leave[0] == "OK", "host close protocol succeeds");
        test::Equal(service.LobbyCount(), std::size_t{0}, "protocol host leave closes lobby");
    }},
}); }
