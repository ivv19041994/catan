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
    {"v2 identity requests are idempotent and reject request-id reuse", [] {
        auto service = DeterministicService();
        const std::string create_request = "CREATE2\tcreate-request-0001\t416c696365\t526f6f6d";
        const std::string created = protocol::HandleRequest(service, create_request);
        test::Equal(protocol::HandleRequest(service, create_request), created,
            "lost create response can be replayed exactly");
        test::Equal(service.LobbyCount(), std::size_t{1}, "create replay owns one lobby");
        const auto create_fields = protocol::Split(created, '\t');
        const std::string conflict = protocol::HandleRequest(service,
            "CREATE2\tcreate-request-0001\t4d616c6c6f7279\t4f74686572");
        test::Check(conflict.starts_with("ERR\t"), "same id with different create payload is denied");

        const std::string join_request = "JOIN2\tjoin-request-000001\t" + create_fields[2] + "\t426f62";
        const std::string joined = protocol::HandleRequest(service, join_request);
        test::Equal(protocol::HandleRequest(service, join_request), joined,
            "lost join response returns the original player token");
        std::string error;
        const auto join_fields = protocol::Split(joined, '\t');
        const auto snapshot = service.GetSnapshot(create_fields[2], join_fields[3], error);
        test::Check(snapshot && snapshot->lobby_players.size() == 2,
            "join replay creates exactly one guest");

        auto restored = DeterministicService();
        test::Check(restored.RestoreState(service.SerializeState()).ok, "replay cache persists");
        test::Equal(protocol::HandleRequest(restored, create_request), created,
            "create response remains idempotent after server restart");
        test::Equal(protocol::HandleRequest(restored, join_request), joined,
            "join response remains idempotent after server restart");
    }},
    {"authenticated v2 mutations execute once even after state advances", [] {
        auto service = DeterministicService();
        const auto host = service.CreateLobby("Alice", "Room");
        const auto guest = service.JoinLobby(host.lobby_token, "Bob");
        const auto ready = service.SetReady(host.lobby_token, host.player_token, true,
            "ready-request-0001");
        std::string error;
        const auto ready_view = service.GetSnapshot(host.lobby_token, host.player_token, error);
        test::Check(ready.ok && ready_view, "first ready mutation succeeds");
        test::Check(service.SetReady(host.lobby_token, host.player_token, true,
            "ready-request-0001").ok, "ready replay succeeds");
        const auto replay_view = service.GetSnapshot(host.lobby_token, host.player_token, error);
        test::Equal(replay_view->revision, ready_view->revision, "ready replay does not increment revision");
        test::Check(!service.SetReady(host.lobby_token, host.player_token, false,
            "ready-request-0001").ok, "same id cannot change ready payload");

        service.SetReady(host.lobby_token, guest.player_token, true, "ready-request-0002");
        test::Check(service.StartGame(host.lobby_token, host.player_token,
            "start-request-0001").ok, "first start succeeds");
        test::Check(service.StartGame(host.lobby_token, host.player_token,
            "start-request-0001").ok, "start replay succeeds after game exists");

        const auto setup = service.GetSnapshot(host.lobby_token, host.player_token, error);
        const IdentityResult& current = setup->current_player == host.player_name ? host : guest;
        CommandArgs settlement; settlement.first = setup->valid_nodes.front();
        const auto built = service.Execute(host.lobby_token, current.player_token,
            Command::BuildSettlement, settlement, "command-request-01");
        const auto built_view = service.GetSnapshot(host.lobby_token, current.player_token, error);
        test::Check(built.ok && built_view, "first command succeeds");
        test::Check(service.Execute(host.lobby_token, current.player_token,
            Command::BuildSettlement, settlement, "command-request-01").ok,
            "command replay returns original success");
        const auto replayed = service.GetSnapshot(host.lobby_token, current.player_token, error);
        test::Equal(replayed->revision, built_view->revision, "command replay does not mutate game twice");
        auto restored = DeterministicService();
        test::Check(restored.RestoreState(service.SerializeState()).ok,
            "command replay cache survives server restart");
        test::Check(restored.Execute(host.lobby_token, current.player_token,
            Command::BuildSettlement, settlement, "command-request-01").ok,
            "lost command response replays after restart");
        const auto restored_view = restored.GetSnapshot(host.lobby_token, current.player_token, error);
        test::Equal(restored_view->revision, built_view->revision,
            "post-restart replay does not mutate game twice");
        ++settlement.first;
        test::Check(!service.Execute(host.lobby_token, current.player_token,
            Command::BuildSettlement, settlement, "command-request-01").ok,
            "same command id cannot target another node");
    }},
    {"idempotent leave can be retried after it removes its player or lobby", [] {
        auto service = DeterministicService();
        const auto host = service.CreateLobby("Alice", "Room");
        const auto guest = service.JoinLobby(host.lobby_token, "Bob");
        const auto guest_leave = service.LeaveLobby(host.lobby_token, guest.player_token,
            "leave-guest-request-01");
        test::Check(guest_leave.ok, "guest leaves once");
        test::Equal(service.LeaveLobby(host.lobby_token, guest.player_token,
            "leave-guest-request-01").message, guest_leave.message,
            "guest retry succeeds after credential removal");
        const auto host_leave = service.LeaveLobby(host.lobby_token, host.player_token,
            "leave-host-request-001");
        test::Check(host_leave.ok && service.LobbyCount() == 0, "host closes lobby once");
        test::Equal(service.LeaveLobby(host.lobby_token, host.player_token,
            "leave-host-request-001").message, host_leave.message,
            "host retry succeeds after lobby removal");
    }},
    {"oversized v2 identity token and command fields are rejected before persistence", [] {
        auto service = DeterministicService();
        test::Check(!service.CreateLobby(std::string(257, 'A'), "Room",
            "oversized-create-0001").ok, "oversized identity is rejected");
        test::Equal(service.LobbyCount(), std::size_t{0}, "oversized create leaves no lobby");
        const auto room = CreateStartedRoom(service);
        test::Check(!service.ResumeLobby(std::string(129, 'A'), room.host.player_token).ok,
            "oversized lobby token is rejected");
        std::string error;
        const auto view = service.GetSnapshot(room.host.lobby_token, room.host.player_token, error);
        const IdentityResult& current = IdentityFor(room, view->current_player);
        CommandArgs args;
        args.text.assign(257, 'X');
        test::Check(!service.Execute(room.host.lobby_token, current.player_token,
            Command::Pass, args, "oversized-command-001").ok,
            "ignored oversized command text cannot poison replay state");
        auto restored = DeterministicService();
        test::Check(restored.RestoreState(service.SerializeState()).ok,
            "state remains self-consistent after rejected oversized requests");
    }},
    {"resume authenticates an existing waiting or active player without joining twice", [] {
        auto service = DeterministicService();
        const auto first = CreateStartedRoom(service, " Resume");
        const auto second = service.CreateLobby("Other", "Other room");
        const auto resumed = service.ResumeLobby(first.host.lobby_token, first.guest.player_token);
        test::Check(resumed.ok && resumed.player_name == first.guest.player_name,
            "active guest resumes with original identity");
        test::Equal(service.LobbyCount(), std::size_t{2}, "resume creates no lobby");
        test::Check(!service.ResumeLobby(first.host.lobby_token, second.player_token).ok,
            "token from another lobby cannot resume");
        const std::string wire = protocol::HandleRequest(service,
            "RESUME\t" + first.host.lobby_token + "\t" + first.host.player_token);
        const auto fields = protocol::Split(wire, '\t');
        test::Check(fields.size() == 4 && fields[0] == "OK" && fields[1] == "RESUMED",
            "resume is exposed by the dedicated protocol");
        test::Check(wire.find(first.host.player_token) == std::string::npos,
            "resume response does not echo private player token");
    }},
    {"dedicated state restores multiple lobbies games and original credentials", [] {
        auto service = DeterministicService();
        const auto waiting_host = service.CreateLobby("Waiting host", "Waiting room");
        const auto waiting_guest = service.JoinLobby(waiting_host.lobby_token, "Waiting guest");
        test::Check(service.SetReady(waiting_host.lobby_token, waiting_host.player_token, true).ok,
            "waiting room mutation succeeds before save");
        const auto playing = CreateStartedRoom(service, " Persistent");
        std::string error;
        const auto before = service.GetSnapshot(playing.host.lobby_token, playing.guest.player_token, error);
        test::Check(before && before->playing, "started room exists before save");

        const std::string state = service.SerializeState();
        auto restored = DeterministicService();
        const auto result = restored.RestoreState(state);
        test::Check(result.ok, "complete multi-lobby state restores: " + result.message);
        test::Equal(restored.LobbyCount(), std::size_t{2}, "both lobbies survive restart");
        test::Equal(restored.SerializeState(), state, "full dedicated state round-trips byte for byte");

        const auto waiting = restored.GetSnapshot(waiting_host.lobby_token,
            waiting_guest.player_token, error);
        test::Check(waiting && !waiting->playing && waiting->lobby_players.size() == 2,
            "waiting lobby and guest token survive restart");
        test::Check(waiting->lobby_players.front().ready, "ready state survives restart");
        const auto after = restored.GetSnapshot(playing.host.lobby_token,
            playing.guest.player_token, error);
        test::Check(after && after->playing, "active game and player token survive restart");
        test::Equal(after->revision, before->revision, "game revision survives restart");
        test::Equal(after->current_player, before->current_player, "current player survives restart");
        test::Equal(after->hexes.size(), before->hexes.size(), "board survives restart");
        test::Equal(after->valid_nodes, before->valid_nodes, "legal setup targets survive restart");
        test::Check(!restored.GetSnapshot(playing.host.lobby_token,
            waiting_guest.player_token, error), "restored credentials remain isolated by lobby");
    }},
    {"restored active game continues from its exact setup state", [] {
        auto service = DeterministicService();
        const auto room = CreateStartedRoom(service);
        std::string error;
        const auto before = service.GetSnapshot(room.host.lobby_token, room.host.player_token, error);
        const auto& current = IdentityFor(room, before->current_player);
        CommandArgs settlement; settlement.first = before->valid_nodes.front();
        test::Check(service.Execute(room.host.lobby_token, current.player_token,
            Command::BuildSettlement, settlement).ok, "settlement is placed before restart");

        auto restored = DeterministicService();
        const auto restore = restored.RestoreState(service.SerializeState());
        test::Check(restore.ok, "mid-turn game restores: " + restore.message);
        const auto resumed = restored.GetSnapshot(room.host.lobby_token, current.player_token, error);
        test::Check(resumed && !resumed->valid_roads.empty(), "restored setup requires adjacent road");
        CommandArgs road; road.first = resumed->valid_roads.front();
        test::Check(restored.Execute(room.host.lobby_token, current.player_token,
            Command::BuildRoad, road).ok, "same authenticated player continues after restart");
    }},
    {"invalid dedicated state is rejected atomically", [] {
        auto source = DeterministicService();
        source.CreateLobby("Alice", "Source");
        const std::string valid = source.SerializeState();

        auto target = DeterministicService();
        const auto existing = target.CreateLobby("Existing", "Must remain");
        test::Check(!target.RestoreState(valid.substr(0, valid.size() / 2)).ok,
            "truncated state is rejected");
        test::Equal(target.LobbyCount(), std::size_t{1}, "failed restore keeps live state intact");
        std::string error;
        test::Check(target.GetSnapshot(existing.lobby_token, existing.player_token, error).has_value(),
            "existing credentials remain valid after failed restore");

        std::string trailing = valid + "junk";
        test::Check(!target.RestoreState(trailing).ok, "trailing bytes are rejected");
        std::string wrong_signature = valid;
        wrong_signature.front() = 'X';
        test::Check(!target.RestoreState(wrong_signature).ok, "wrong signature is rejected");
        std::string unsupported_version = valid;
        unsupported_version[std::string_view("CATAN_DEDICATED_STATE").size()] = 99;
        test::Check(!target.RestoreState(unsupported_version).ok, "unsupported version is rejected");
    }},
    {"restore enforces configured multi-lobby limit", [] {
        auto source = DeterministicService();
        source.CreateLobby("One", "One");
        source.CreateLobby("Two", "Two");
        auto limited = DeterministicService(1);
        const auto result = limited.RestoreState(source.SerializeState());
        test::Check(!result.ok, "oversized persisted service is rejected");
        test::Equal(limited.LobbyCount(), std::size_t{0}, "limit failure imports no partial lobby");
    }},
    {"dedicated state version one remains readable", [] {
        auto source = DeterministicService();
        const auto room = source.CreateLobby("Alice", "Legacy room");
        std::string version_one = source.SerializeState();
        test::Check(version_one.size() > 4, "version two state has replay-count trailer");
        version_one.resize(version_one.size() - 4);
        constexpr std::size_t version_offset = std::string_view("CATAN_DEDICATED_STATE").size();
        version_one[version_offset] = 1;
        version_one[version_offset + 1] = 0;
        version_one[version_offset + 2] = 0;
        version_one[version_offset + 3] = 0;
        auto restored = DeterministicService();
        test::Check(restored.RestoreState(version_one).ok, "version one snapshot restores");
        std::string error;
        test::Check(restored.GetSnapshot(room.lobby_token, room.player_token, error).has_value(),
            "version one credentials remain valid");
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
        test::Equal(round_trip->bank_resources, alice->bank_resources,
            "public finite bank survives dedicated transport");
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
