#include "dedicated_server.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace ivv::catan::dedicated {
namespace {

struct AuthPlayer {
    int id = -1;
    std::string name;
    std::string token;
    bool ready = false;
    bool host = false;
};

std::string TrimAndSanitize(std::string value, std::string_view fallback, std::size_t limit)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [](unsigned char c) { return !std::isspace(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [](unsigned char c) { return !std::isspace(c); }).base(), value.end());
    for (char& c : value)
        if (static_cast<unsigned char>(c) < 32 || c == '\t' || c == '\n' || c == '\r') c = ' ';
    if (value.empty()) value = std::string(fallback);
    if (value.size() > limit) value.resize(limit);
    return value;
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

int ToPhase(GameController::GameStep step)
{
    using Step = GameController::GameStep;
    switch (step) {
    case Step::ForwardBuildingSettlement:
    case Step::BackwardBuildingSettlement: return 0;
    case Step::ForwardBuildingRoad:
    case Step::BackwardBuildingRoad: return 1;
    case Step::DiceDrop: return 2;
    case Step::CommonPlay: return 3;
    case Step::DropCards: return 4;
    case Step::BanditMove: return 5;
    case Step::RoadBuilding: return 6;
    case Step::Finish: return 7;
    }
    return 7;
}

int ToResource(Resurse resource)
{
    switch (resource) {
    case Resurse::Wood: return 0;
    case Resurse::Clay: return 1;
    case Resurse::Hay: return 2;
    case Resurse::Sheep: return 3;
    case Resurse::Stone: return 4;
    case Resurse::Not: return 5;
    }
    return 5;
}

Resurse CoreResource(int resource)
{
    switch (resource) {
    case 0: return Resurse::Wood;
    case 1: return Resurse::Clay;
    case 2: return Resurse::Hay;
    case 3: return Resurse::Sheep;
    case 4: return Resurse::Stone;
    default: return Resurse::Not;
    }
}

Resources ResourceView(const Player& player)
{
    return {
        static_cast<int>(player.getCountResurses(Resurse::Wood)),
        static_cast<int>(player.getCountResurses(Resurse::Clay)),
        static_cast<int>(player.getCountResurses(Resurse::Hay)),
        static_cast<int>(player.getCountResurses(Resurse::Sheep)),
        static_cast<int>(player.getCountResurses(Resurse::Stone))
    };
}

Resources ResourceView(const std::map<Resurse, std::size_t>& resources)
{
    auto count = [&resources](Resurse resource) {
        const auto found = resources.find(resource);
        return found == resources.end() ? 0 : static_cast<int>(found->second);
    };
    return {count(Resurse::Wood), count(Resurse::Clay), count(Resurse::Hay),
        count(Resurse::Sheep), count(Resurse::Stone)};
}

std::map<Resurse, std::size_t> ResourceMap(const Resources& resources)
{
    std::map<Resurse, std::size_t> result;
    auto add = [&result](Resurse resource, int count) {
        if (count > 0) result[resource] = static_cast<std::size_t>(count);
    };
    add(Resurse::Wood, resources.wood);
    add(Resurse::Clay, resources.clay);
    add(Resurse::Hay, resources.hay);
    add(Resurse::Sheep, resources.sheep);
    add(Resurse::Stone, resources.stone);
    return result;
}

int DevelopmentCount(const Player& player)
{
    constexpr DevelopmentCard cards[] = {
        DevelopmentCard::Knights, DevelopmentCard::RoadBuilding,
        DevelopmentCard::YearOfPlenty, DevelopmentCard::Monopoly,
        DevelopmentCard::University, DevelopmentCard::Market,
        DevelopmentCard::GreatHall, DevelopmentCard::Chapel, DevelopmentCard::Library
    };
    std::size_t count = 0;
    for (DevelopmentCard card : cards) {
        count += player.GetReadyForUseCardCount(card);
        count += player.GetPurchasedCardCount(card);
    }
    return static_cast<int>(count);
}

int PendingDevelopmentCount(const Player& player)
{
    constexpr DevelopmentCard cards[] = {DevelopmentCard::Knights, DevelopmentCard::RoadBuilding,
        DevelopmentCard::YearOfPlenty, DevelopmentCard::Monopoly};
    std::size_t count = 0;
    for (DevelopmentCard card : cards) count += player.GetPurchasedCardCount(card);
    return static_cast<int>(count);
}

bool ValidResource(int value) { return value >= 0 && value < 5; }

constexpr std::string_view StateMagic{"CATAN_DEDICATED_STATE"};
constexpr std::uint32_t StateVersion = 2;
constexpr std::uint32_t MaximumStoredString = 16 * 1024 * 1024;
constexpr std::size_t MaximumReplayEntries = 2048;

bool ValidRequestId(std::string_view value)
{
    return value.empty() || (value.size() >= 8 && value.size() <= 128
        && std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isalnum(character) || character == '-' || character == '_';
        }));
}

bool ValidTokenInput(std::string_view value)
{
    return !value.empty() && value.size() <= 128
        && std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isalnum(character) || character == '-' || character == '_';
        });
}

void AppendFingerprintField(std::string& target, std::string_view value)
{
    target += std::to_string(value.size());
    target.push_back(':');
    target.append(value);
    target.push_back('|');
}

std::string Fingerprint(std::string_view operation,
    std::initializer_list<std::string_view> fields)
{
    std::string result;
    AppendFingerprintField(result, operation);
    for (std::string_view field : fields) AppendFingerprintField(result, field);
    return result;
}

std::string ResourceFingerprint(const Resources& value)
{
    return std::to_string(value.wood) + ',' + std::to_string(value.clay) + ','
        + std::to_string(value.hay) + ',' + std::to_string(value.sheep) + ','
        + std::to_string(value.stone);
}

class StateWriter {
public:
    void Raw(std::string_view value) { data_.append(value); }
    void U8(std::uint8_t value) { data_.push_back(static_cast<char>(value)); }
    void Bool(bool value) { U8(value ? 1 : 0); }
    void U32(std::uint32_t value)
    {
        for (unsigned shift = 0; shift < 32; shift += 8)
            U8(static_cast<std::uint8_t>((value >> shift) & 0xff));
    }
    void U64(std::uint64_t value)
    {
        for (unsigned shift = 0; shift < 64; shift += 8)
            U8(static_cast<std::uint8_t>((value >> shift) & 0xff));
    }
    void I32(std::int32_t value) { U32(static_cast<std::uint32_t>(value)); }
    void String(std::string_view value)
    {
        if (value.size() > MaximumStoredString)
            throw std::length_error("Dedicated server state field is too large");
        U32(static_cast<std::uint32_t>(value.size()));
        Raw(value);
    }
    std::string Finish() && { return std::move(data_); }

private:
    std::string data_;
};

class StateReader {
public:
    explicit StateReader(std::string_view data) : data_(data) {}

    std::string_view Raw(std::size_t count)
    {
        if (count > data_.size() - offset_)
            throw std::invalid_argument("Dedicated server state is truncated");
        const std::string_view result = data_.substr(offset_, count);
        offset_ += count;
        return result;
    }
    std::uint8_t U8() { return static_cast<std::uint8_t>(Raw(1)[0]); }
    bool Bool()
    {
        const std::uint8_t value = U8();
        if (value > 1) throw std::invalid_argument("Dedicated server state has an invalid boolean");
        return value != 0;
    }
    std::uint32_t U32()
    {
        std::uint32_t result = 0;
        for (unsigned shift = 0; shift < 32; shift += 8)
            result |= static_cast<std::uint32_t>(U8()) << shift;
        return result;
    }
    std::uint64_t U64()
    {
        std::uint64_t result = 0;
        for (unsigned shift = 0; shift < 64; shift += 8)
            result |= static_cast<std::uint64_t>(U8()) << shift;
        return result;
    }
    std::int32_t I32() { return static_cast<std::int32_t>(U32()); }
    std::string String()
    {
        const std::uint32_t size = U32();
        if (size > MaximumStoredString)
            throw std::invalid_argument("Dedicated server state field is too large");
        return std::string(Raw(size));
    }
    void RequireFinished() const
    {
        if (offset_ != data_.size())
            throw std::invalid_argument("Dedicated server state has trailing data");
    }

private:
    std::string_view data_;
    std::size_t offset_ = 0;
};

} // namespace

struct Service::Lobby {
    std::string token;
    std::string name;
    std::vector<AuthPlayer> players;
    std::unique_ptr<GameController> game;
    std::uint64_t revision = 1;
    int board_action = 0;
    int pending_robber_hex = -1;
    std::vector<std::string> robber_victims;
    std::string active_trade_target;
    std::string status = "Waiting for players";
    std::vector<std::string> events;

    AuthPlayer* Authenticate(std::string_view player_token)
    {
        const auto found = std::find_if(players.begin(), players.end(),
            [player_token](const AuthPlayer& player) { return player.token == player_token; });
        return found == players.end() ? nullptr : &*found;
    }

    const AuthPlayer* Authenticate(std::string_view player_token) const
    {
        const auto found = std::find_if(players.begin(), players.end(),
            [player_token](const AuthPlayer& player) { return player.token == player_token; });
        return found == players.end() ? nullptr : &*found;
    }

    std::string UniqueName(std::string requested) const
    {
        const std::string base = TrimAndSanitize(std::move(requested), "Player", 24);
        std::string candidate = base;
        for (int suffix = 2;; ++suffix) {
            const std::string lower = Lower(candidate);
            const bool exists = std::any_of(players.begin(), players.end(),
                [&lower](const AuthPlayer& player) { return Lower(player.name) == lower; });
            if (!exists) return candidate;
            candidate = base + " " + std::to_string(suffix);
            if (candidate.size() > 24) candidate.resize(24);
        }
    }
};

struct Service::ReplayEntry {
    std::string request_id;
    std::string fingerprint;
    bool identity = false;
    IdentityResult identity_result;
    Result result;
};

Service::Service(std::size_t max_lobbies, TokenFactory token_factory)
    : max_lobbies_(std::max<std::size_t>(1, max_lobbies)), token_factory_(std::move(token_factory))
{
    if (!token_factory_) {
        token_factory_ = [](std::size_t length, bool lobby) {
            static constexpr char lobby_alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
            static constexpr char private_alphabet[] = "0123456789abcdef";
            thread_local std::mt19937_64 random(std::random_device{}());
            const std::string_view alphabet = lobby ? std::string_view(lobby_alphabet)
                                                    : std::string_view(private_alphabet);
            std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
            std::string token;
            token.reserve(length + (lobby && length > 4 ? 1 : 0));
            for (std::size_t index = 0; index < length; ++index) {
                if (lobby && index == 4) token.push_back('-');
                token.push_back(alphabet[pick(random)]);
            }
            return token;
        };
    }
}

Service::~Service() = default;

bool Service::TryReplayIdentity(std::string_view request_id, std::string_view fingerprint,
    IdentityResult& output) const
{
    if (request_id.empty()) return false;
    const auto found = std::find_if(replay_entries_.begin(), replay_entries_.end(),
        [request_id](const ReplayEntry& entry) { return entry.request_id == request_id; });
    if (found == replay_entries_.end()) return false;
    if (!found->identity || found->fingerprint != fingerprint) {
        output = {false, "Request id was already used for a different operation"};
        return true;
    }
    output = found->identity_result;
    return true;
}

bool Service::TryReplayResult(std::string_view request_id, std::string_view fingerprint,
    Result& output) const
{
    if (request_id.empty()) return false;
    const auto found = std::find_if(replay_entries_.begin(), replay_entries_.end(),
        [request_id](const ReplayEntry& entry) { return entry.request_id == request_id; });
    if (found == replay_entries_.end()) return false;
    if (found->identity || found->fingerprint != fingerprint) {
        output = {false, "Request id was already used for a different operation"};
        return true;
    }
    output = found->result;
    return true;
}

void Service::RememberIdentity(std::string_view request_id, std::string fingerprint,
    const IdentityResult& result)
{
    if (request_id.empty() || !result.ok) return;
    if (replay_entries_.size() >= MaximumReplayEntries) replay_entries_.erase(replay_entries_.begin());
    ReplayEntry& entry = replay_entries_.emplace_back();
    entry.request_id = request_id;
    entry.fingerprint = std::move(fingerprint);
    entry.identity = true;
    entry.identity_result = result;
}

void Service::RememberResult(std::string_view request_id, std::string fingerprint,
    const Result& result)
{
    if (request_id.empty() || !result.ok) return;
    if (replay_entries_.size() >= MaximumReplayEntries) replay_entries_.erase(replay_entries_.begin());
    ReplayEntry& entry = replay_entries_.emplace_back();
    entry.request_id = request_id;
    entry.fingerprint = std::move(fingerprint);
    entry.result = result;
}

std::string Service::NewToken(std::size_t length, bool lobby)
{
    for (int attempt = 0; attempt < 1000; ++attempt) {
        std::string token = token_factory_(length, lobby);
        if (token.empty()) continue;
        bool exists = lobbies_.contains(token);
        if (!lobby) {
            for (const auto& [unused, room] : lobbies_) {
                exists = exists || std::any_of(room->players.begin(), room->players.end(),
                    [&token](const AuthPlayer& player) { return player.token == token; });
                if (exists) break;
            }
        }
        if (!exists) return token;
    }
    throw std::runtime_error("Could not generate a unique security token");
}

IdentityResult Service::CreateLobby(std::string player_name, std::string lobby_name,
    std::string_view request_id)
{
    std::lock_guard lock(mutex_);
    if (!ValidRequestId(request_id)) return {false, "Request id is invalid"};
    if (player_name.size() > 256 || lobby_name.size() > 256)
        return {false, "Lobby identity is too large"};
    const std::string fingerprint = Fingerprint("CREATE", {player_name, lobby_name});
    IdentityResult replay;
    if (TryReplayIdentity(request_id, fingerprint, replay)) return replay;
    if (lobbies_.size() >= max_lobbies_)
        return {false, "Dedicated server reached its lobby limit"};
    auto lobby = std::make_unique<Lobby>();
    lobby->token = NewToken(8, true);
    lobby->name = TrimAndSanitize(std::move(lobby_name), "Catan lobby", 40);
    AuthPlayer host;
    host.id = 0;
    host.name = lobby->UniqueName(std::move(player_name));
    host.token = NewToken(32, false);
    host.host = true;
    IdentityResult result;
    result.ok = true;
    result.message = "Lobby created";
    result.lobby_token = lobby->token;
    result.player_token = host.token;
    result.player_name = host.name;
    lobby->players.push_back(std::move(host));
    lobbies_.emplace(lobby->token, std::move(lobby));
    RememberIdentity(request_id, fingerprint, result);
    return result;
}

IdentityResult Service::JoinLobby(std::string_view lobby_token, std::string player_name,
    std::string_view request_id)
{
    std::lock_guard lock(mutex_);
    if (!ValidRequestId(request_id)) return {false, "Request id is invalid"};
    if (!ValidTokenInput(lobby_token)) return {false, "Lobby token is invalid"};
    if (player_name.size() > 256) return {false, "Player name is too large"};
    const std::string fingerprint = Fingerprint("JOIN", {lobby_token, player_name});
    IdentityResult replay;
    if (TryReplayIdentity(request_id, fingerprint, replay)) return replay;
    const auto found = lobbies_.find(std::string(lobby_token));
    if (found == lobbies_.end()) return {false, "Lobby token is invalid"};
    Lobby& lobby = *found->second;
    if (lobby.game) return {false, "This game has already started"};
    if (lobby.players.size() >= 4) return {false, "This lobby is full"};
    AuthPlayer player;
    player.id = static_cast<int>(lobby.players.size());
    player.name = lobby.UniqueName(std::move(player_name));
    player.token = NewToken(32, false);
    IdentityResult result;
    result.ok = true;
    result.message = "Joined lobby";
    result.lobby_token = lobby.token;
    result.player_token = player.token;
    result.player_name = player.name;
    lobby.players.push_back(std::move(player));
    ++lobby.revision;
    RememberIdentity(request_id, fingerprint, result);
    return result;
}

IdentityResult Service::ResumeLobby(std::string_view lobby_token, std::string_view player_token)
{
    std::lock_guard lock(mutex_);
    if (!ValidTokenInput(lobby_token)) return {false, "Lobby token is invalid"};
    if (!ValidTokenInput(player_token)) return {false, "Player token is invalid"};
    const auto found = lobbies_.find(std::string(lobby_token));
    if (found == lobbies_.end()) return {false, "Lobby token is invalid"};
    const AuthPlayer* player = found->second->Authenticate(player_token);
    if (!player) return {false, "Player token is invalid"};
    IdentityResult result{true, "Dedicated session resumed"};
    result.lobby_token = found->second->token;
    result.player_token = player->token;
    result.player_name = player->name;
    return result;
}

Result Service::LeaveLobby(std::string_view lobby_token, std::string_view player_token,
    std::string_view request_id)
{
    std::lock_guard lock(mutex_);
    if (!ValidRequestId(request_id)) return {false, "Request id is invalid"};
    if (!ValidTokenInput(lobby_token)) return {false, "Lobby token is invalid"};
    if (!ValidTokenInput(player_token)) return {false, "Player token is invalid"};
    const std::string fingerprint = Fingerprint("LEAVE", {lobby_token, player_token});
    Result replay;
    if (TryReplayResult(request_id, fingerprint, replay)) return replay;
    const auto found = lobbies_.find(std::string(lobby_token));
    if (found == lobbies_.end()) return {false, "Lobby token is invalid"};
    Lobby& lobby = *found->second;
    const AuthPlayer* authenticated = lobby.Authenticate(player_token);
    if (!authenticated) return {false, "Player token is invalid"};
    if (lobby.game) return {false, "The game has already started"};
    if (authenticated->host) {
        lobbies_.erase(found);
        const Result result{true, "Lobby closed"};
        RememberResult(request_id, fingerprint, result);
        return result;
    }
    const std::string player_name = authenticated->name;
    lobby.players.erase(std::remove_if(lobby.players.begin(), lobby.players.end(),
        [player_token](const AuthPlayer& player) { return player.token == player_token; }), lobby.players.end());
    for (std::size_t index = 0; index < lobby.players.size(); ++index)
        lobby.players[index].id = static_cast<int>(index);
    lobby.status = player_name + " left the lobby";
    lobby.events.push_back(lobby.status);
    ++lobby.revision;
    const Result result{true, lobby.status};
    RememberResult(request_id, fingerprint, result);
    return result;
}

Result Service::SetReady(std::string_view lobby_token, std::string_view player_token, bool ready,
    std::string_view request_id)
{
    std::lock_guard lock(mutex_);
    if (!ValidRequestId(request_id)) return {false, "Request id is invalid"};
    if (!ValidTokenInput(lobby_token)) return {false, "Lobby token is invalid"};
    if (!ValidTokenInput(player_token)) return {false, "Player token is invalid"};
    const std::string fingerprint = Fingerprint("READY",
        {lobby_token, player_token, ready ? std::string_view("1") : std::string_view("0")});
    Result replay;
    if (TryReplayResult(request_id, fingerprint, replay)) return replay;
    const auto found = lobbies_.find(std::string(lobby_token));
    if (found == lobbies_.end()) return {false, "Lobby token is invalid"};
    Lobby& lobby = *found->second;
    AuthPlayer* player = lobby.Authenticate(player_token);
    if (!player) return {false, "Player token is invalid"};
    if (lobby.game) return {false, "The game has already started"};
    player->ready = ready;
    lobby.status = ready ? player->name + " is ready" : player->name + " is not ready";
    ++lobby.revision;
    const Result result{true, lobby.status};
    RememberResult(request_id, fingerprint, result);
    return result;
}

Result Service::StartGame(std::string_view lobby_token, std::string_view player_token,
    std::string_view request_id)
{
    std::lock_guard lock(mutex_);
    if (!ValidRequestId(request_id)) return {false, "Request id is invalid"};
    if (!ValidTokenInput(lobby_token)) return {false, "Lobby token is invalid"};
    if (!ValidTokenInput(player_token)) return {false, "Player token is invalid"};
    const std::string fingerprint = Fingerprint("START", {lobby_token, player_token});
    Result replay;
    if (TryReplayResult(request_id, fingerprint, replay)) return replay;
    const auto found = lobbies_.find(std::string(lobby_token));
    if (found == lobbies_.end()) return {false, "Lobby token is invalid"};
    Lobby& lobby = *found->second;
    AuthPlayer* player = lobby.Authenticate(player_token);
    if (!player) return {false, "Player token is invalid"};
    if (!player->host) return {false, "Only the lobby creator can start the game"};
    if (lobby.game) return {false, "The game has already started"};
    if (lobby.players.size() < 2 || lobby.players.size() > 4)
        return {false, "A game requires 2 to 4 players"};
    if (!std::all_of(lobby.players.begin(), lobby.players.end(),
        [](const AuthPlayer& item) { return item.ready; }))
        return {false, "Every player must be ready"};
    std::vector<std::string> names;
    names.reserve(lobby.players.size());
    for (const AuthPlayer& item : lobby.players) names.push_back(item.name);
    lobby.game = std::make_unique<GameController>(std::move(names));
    lobby.status = "Game started";
    lobby.events.push_back(lobby.status);
    ++lobby.revision;
    const Result result{true, lobby.status};
    RememberResult(request_id, fingerprint, result);
    return result;
}

Result Service::Execute(std::string_view lobby_token, std::string_view player_token,
    Command command, const CommandArgs& args, std::string_view request_id)
{
    std::lock_guard lock(mutex_);
    if (!ValidRequestId(request_id)) return {false, "Request id is invalid"};
    if (!ValidTokenInput(lobby_token)) return {false, "Lobby token is invalid"};
    if (!ValidTokenInput(player_token)) return {false, "Player token is invalid"};
    if (args.text.size() > 256) return {false, "Command text is too large"};
    const std::string command_text = std::to_string(static_cast<int>(command));
    const std::string first = std::to_string(args.first);
    const std::string second = std::to_string(args.second);
    const std::string first_resources = ResourceFingerprint(args.first_resources);
    const std::string second_resources = ResourceFingerprint(args.second_resources);
    const std::string fingerprint = Fingerprint("COMMAND", {lobby_token, player_token,
        command_text, first, second, args.text, first_resources, second_resources});
    Result replay;
    if (TryReplayResult(request_id, fingerprint, replay)) return replay;
    const auto found = lobbies_.find(std::string(lobby_token));
    if (found == lobbies_.end()) return {false, "Lobby token is invalid"};
    Lobby& lobby = *found->second;
    AuthPlayer* player = lobby.Authenticate(player_token);
    if (!player) return {false, "Player token is invalid"};
    if (!lobby.game) return {false, "The game has not started"};
    GameController& game = *lobby.game;
    const bool trade_response = command == Command::AcceptTrade || command == Command::CancelTrade;
    if (!trade_response && game.GetCurrentPlayer() != player->name)
        return {false, "It is not your turn"};

    auto complete = [this, &lobby, request_id, &fingerprint](std::string message) {
        lobby.board_action = 0;
        lobby.status = std::move(message);
        lobby.events.push_back(lobby.status);
        if (lobby.events.size() > 40) lobby.events.erase(lobby.events.begin());
        ++lobby.revision;
        const Result result{true, lobby.status};
        RememberResult(request_id, fingerprint, result);
        return result;
    };

    try {
        switch (command) {
        case Command::BuildSettlement:
            game.BuildSettlement(player->name, static_cast<std::size_t>(args.first));
            return complete("Settlement built");
        case Command::BuildRoad:
            game.BuildRoad(player->name, static_cast<std::size_t>(args.first));
            return complete("Road built");
        case Command::BuildCity:
            game.BuildCastle(player->name, static_cast<std::size_t>(args.first));
            return complete("City built");
        case Command::MoveRobber: {
            if (args.first < 0 || args.first >= static_cast<int>(game.GetMap().GetGexes().size()))
                return {false, "Invalid robber hex"};
            if (!game.CanMoveBandit(static_cast<std::size_t>(args.first)))
                return {false, "The robber cannot move to this hex"};
            std::set<std::string> victims;
            for (const Node* node : game.GetMap().GetGexes()[args.first].GetNodes())
                if (const Building* building = node->getBuilding())
                    if (building->getPlayer()->getName() != player->name)
                        victims.insert(building->getPlayer()->getName());
            if (!victims.empty()) {
                lobby.pending_robber_hex = args.first;
                lobby.robber_victims.assign(victims.begin(), victims.end());
                lobby.status = "Choose a player to steal from";
                ++lobby.revision;
                const Result result{true, lobby.status};
                RememberResult(request_id, fingerprint, result);
                return result;
            }
            game.BanditMove(player->name, static_cast<std::size_t>(args.first));
            return complete("Robber moved");
        }
        case Command::ChooseRobberVictim:
            if (lobby.pending_robber_hex < 0) return {false, "Choose a robber hex first"};
            if (std::find(lobby.robber_victims.begin(), lobby.robber_victims.end(), args.text)
                == lobby.robber_victims.end())
                return {false, "This player is not a valid robber victim"};
            game.BanditMove(player->name, static_cast<std::size_t>(lobby.pending_robber_hex), args.text);
            lobby.pending_robber_hex = -1;
            lobby.robber_victims.clear();
            return complete("Robber moved and a resource was stolen");
        case Command::DropResources:
            game.DropCards(player->name, ResourceMap(args.first_resources));
            return complete("Resources discarded");
        case Command::RollDice:
            game.Dice(player->name);
            return complete("Dice rolled");
        case Command::BuyDevelopmentCard:
            game.DevCard(player->name);
            return complete("Development card bought");
        case Command::Pass:
            game.Pass(player->name);
            lobby.active_trade_target.clear();
            return complete("Turn passed");
        case Command::UseDevelopmentCard: {
            if (args.first < 0 || args.first > 3) return {false, "Invalid development card"};
            DevelopmentCard card = DevelopmentCard::Knights;
            GameController::UseDevCardParam param;
            if (args.first == 1) card = DevelopmentCard::RoadBuilding;
            else if (args.first == 2) {
                if (!ValidResource(args.second) || args.text.empty()) return {false, "Invalid Year of Plenty resources"};
                const int second_resource = std::stoi(args.text);
                if (!ValidResource(second_resource)) return {false, "Invalid Year of Plenty resources"};
                card = DevelopmentCard::YearOfPlenty;
                param = std::array<Resurse, 2>{CoreResource(args.second), CoreResource(second_resource)};
            } else if (args.first == 3) {
                if (!ValidResource(args.second)) return {false, "Invalid Monopoly resource"};
                card = DevelopmentCard::Monopoly;
                param = CoreResource(args.second);
            }
            game.UseDevCard(player->name, card, param);
            return complete("Development card played");
        }
        case Command::BankTrade:
            if (!ValidResource(args.first) || !ValidResource(args.second) || args.first == args.second)
                return {false, "Choose two different resources"};
            game.Market(player->name, CoreResource(args.first), CoreResource(args.second));
            return complete("Bank trade completed");
        case Command::OfferTrade:
            if (args.text.empty() || args.text == player->name
                || std::none_of(lobby.players.begin(), lobby.players.end(),
                    [&args](const AuthPlayer& item) { return item.name == args.text; }))
                return {false, "Choose another player to receive the offer"};
            game.SetDeal(player->name, ResourceMap(args.first_resources), ResourceMap(args.second_resources));
            lobby.active_trade_target = args.text;
            return complete("Trade offered to " + args.text);
        case Command::AcceptTrade: {
            const auto& deal = game.GetActivDeal();
            if (!deal) return {false, "There is no active trade"};
            if (lobby.active_trade_target != player->name)
                return {false, "Only the selected recipient can accept this trade"};
            game.SetDeal(player->name, deal->buy, deal->sell);
            lobby.active_trade_target.clear();
            return complete(player->name + " accepted the trade");
        }
        case Command::CancelTrade:
            if (!game.GetActivDeal()) return {false, "There is no active trade"};
            if (game.GetCurrentPlayer() != player->name && lobby.active_trade_target != player->name)
                return {false, "Only the offerer or recipient can cancel this trade"};
            game.CancelDeal(player->name);
            lobby.active_trade_target.clear();
            return complete("Trade cancelled");
        case Command::SelectBoardAction:
            if (args.first < 0 || args.first > 4) return {false, "Invalid board action"};
            lobby.board_action = args.first;
            lobby.status = "Select a target on the board";
            ++lobby.revision;
            const Result result{true, lobby.status};
            RememberResult(request_id, fingerprint, result);
            return result;
        }
    } catch (const std::exception& exception) {
        return {false, exception.what()};
    }
    return {false, "Unknown command"};
}

std::optional<Snapshot> Service::GetSnapshot(std::string_view lobby_token,
    std::string_view player_token, std::string& error)
{
    std::lock_guard lock(mutex_);
    if (!ValidTokenInput(lobby_token)) { error = "Lobby token is invalid"; return std::nullopt; }
    if (!ValidTokenInput(player_token)) { error = "Player token is invalid"; return std::nullopt; }
    const auto found = lobbies_.find(std::string(lobby_token));
    if (found == lobbies_.end()) { error = "Lobby token is invalid"; return std::nullopt; }
    Lobby& lobby = *found->second;
    const AuthPlayer* authenticated = lobby.Authenticate(player_token);
    if (!authenticated) { error = "Player token is invalid"; return std::nullopt; }

    Snapshot view;
    view.revision = lobby.revision;
    view.playing = lobby.game != nullptr;
    view.lobby_name = lobby.name;
    view.local_player = authenticated->name;
    for (const AuthPlayer& player : lobby.players)
        view.lobby_players.push_back({player.id, player.name, player.ready, player.host});
    if (!lobby.game) return view;

    GameController& game = *lobby.game;
    view.current_player = game.GetCurrentPlayer();
    view.phase = ToPhase(game.GetStep());
    view.board_action = lobby.board_action;
    view.status = lobby.status;
    view.pending_robber_hex = lobby.pending_robber_hex;
    view.robber_victims = lobby.robber_victims;
    view.events = lobby.events;
    const auto dice = game.GetLastDice();
    view.first_die = static_cast<int>(dice.first);
    view.second_die = static_cast<int>(dice.second);
    if (const auto winner = game.GetWinner()) view.winner = *winner;
    std::ostringstream step;
    game.PrintStep(step);
    view.step = step.str();
    if (const auto& deal = game.GetActivDeal()) {
        view.deal.active = true;
        view.deal.offering_player = game.GetCurrentPlayer();
        view.deal.target_player = lobby.active_trade_target;
        view.deal.offered = ResourceView(deal->sell);
        view.deal.requested = ResourceView(deal->buy);
    }
    const ResourceBank& bank = game.GetResourceBank();
    view.bank_resources = {
        static_cast<int>(bank.Count(Resurse::Wood)),
        static_cast<int>(bank.Count(Resurse::Clay)),
        static_cast<int>(bank.Count(Resurse::Hay)),
        static_cast<int>(bank.Count(Resurse::Sheep)),
        static_cast<int>(bank.Count(Resurse::Stone))};

    for (const AuthPlayer& auth : lobby.players) {
        const Player& player = game.GetPlayer(auth.name);
        PlayerSnapshot snapshot;
        snapshot.id = static_cast<int>(player.getId());
        snapshot.name = auth.name;
        snapshot.current = auth.name == view.current_player;
        snapshot.local = auth.token == player_token;
        snapshot.resources_visible = snapshot.local;
        const bool reveal_victory_cards = snapshot.local
            || game.GetStep() == GameController::GameStep::Finish;
        snapshot.victory_points = static_cast<int>(reveal_victory_cards
            ? player.GetWinPoints() : player.GetPublicWinPoints());
        snapshot.victory_point_cards = reveal_victory_cards
            ? static_cast<int>(player.GetVictoryPointCardCount()) : 0;
        snapshot.resource_cards = static_cast<int>(player.getCountResurses());
        snapshot.development_cards = DevelopmentCount(player);
        snapshot.free_settlements = static_cast<int>(player.getFreeSettlementCount());
        snapshot.free_cities = static_cast<int>(player.getFreeCastleCount());
        snapshot.free_roads = static_cast<int>(player.getFreeRoadCount());
        snapshot.trade_rates = {
            static_cast<int>(player.GetMarketPrice(Resurse::Wood)),
            static_cast<int>(player.GetMarketPrice(Resurse::Clay)),
            static_cast<int>(player.GetMarketPrice(Resurse::Hay)),
            static_cast<int>(player.GetMarketPrice(Resurse::Sheep)),
            static_cast<int>(player.GetMarketPrice(Resurse::Stone))
        };
        snapshot.largest_army = player.HasLargestArmy();
        snapshot.longest_road = player.HasLongestRoad();
        if (snapshot.local) {
            snapshot.resources = ResourceView(player);
            snapshot.knights = static_cast<int>(player.GetReadyForUseCardCount(DevelopmentCard::Knights));
            snapshot.road_building = static_cast<int>(player.GetReadyForUseCardCount(DevelopmentCard::RoadBuilding));
            snapshot.year_of_plenty = static_cast<int>(player.GetReadyForUseCardCount(DevelopmentCard::YearOfPlenty));
            snapshot.monopoly = static_cast<int>(player.GetReadyForUseCardCount(DevelopmentCard::Monopoly));
            snapshot.pending_development = PendingDevelopmentCount(player);
        }
        if (snapshot.current && view.phase == 4)
            view.required_discard = static_cast<int>(player.getCountResurses() / 2);
        view.players.push_back(std::move(snapshot));
    }

    const auto& hexes = game.GetMap().GetGexes();
    for (int index = 0; index < static_cast<int>(hexes.size()); ++index)
        view.hexes.push_back({index, ToResource(hexes[index].getType()),
            hexes[index].getDice(), hexes[index].isBandit()});
    const auto nodes = game.GetMap().GetNodes();
    for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
        NodeSnapshot node{index};
        if (const Building* building = nodes[index].getBuilding()) {
            node.owner = static_cast<int>(building->getPlayer()->getId());
            node.city = !building->canUpgrade();
        }
        view.nodes.push_back(node);
    }
    const auto roads = game.GetMap().GetFacets();
    for (int index = 0; index < static_cast<int>(roads.size()); ++index) {
        RoadSnapshot road{index};
        if (const Road* core_road = roads[index].getRoad())
            road.owner = static_cast<int>(core_road->getPlayer()->getId());
        view.roads.push_back(road);
    }

    const bool settlement_targets = view.phase == 0 || (view.phase == 3 && view.board_action == 1);
    const bool city_targets = view.phase == 3 && view.board_action == 3;
    if (settlement_targets || city_targets)
        for (int index = 0; index < static_cast<int>(view.nodes.size()); ++index)
            if ((city_targets && game.CanBuildCastle(index))
                || (settlement_targets && game.CanBuildSettlement(index)))
                view.valid_nodes.push_back(index);
    if (view.phase == 3) {
        for (int index = 0; index < static_cast<int>(view.nodes.size()); ++index) {
            view.has_settlement_target = view.has_settlement_target || game.CanBuildSettlement(index);
            view.has_city_target = view.has_city_target || game.CanBuildCastle(index);
        }
    } else if (view.phase == 0) view.has_settlement_target = !view.valid_nodes.empty();

    const bool road_targets = view.phase == 1 || view.phase == 6 || (view.phase == 3 && view.board_action == 2);
    if (road_targets)
        for (int index = 0; index < static_cast<int>(view.roads.size()); ++index)
            if (game.CanBuildRoad(index)) view.valid_roads.push_back(index);
    if (view.phase == 3) {
        for (int index = 0; index < static_cast<int>(view.roads.size()); ++index)
            if (game.CanBuildRoad(index)) { view.has_road_target = true; break; }
    } else if (view.phase == 1 || view.phase == 6) view.has_road_target = !view.valid_roads.empty();

    if (view.phase == 5 && view.pending_robber_hex < 0)
        for (int index = 0; index < static_cast<int>(view.hexes.size()); ++index)
            if (game.CanMoveBandit(index)) view.valid_hexes.push_back(index);
    return view;
}

std::size_t Service::LobbyCount() const
{
    std::lock_guard lock(mutex_);
    return lobbies_.size();
}

std::string Service::SerializeState() const
{
    std::lock_guard lock(mutex_);
    StateWriter writer;
    writer.Raw(StateMagic);
    writer.U32(StateVersion);
    writer.U32(static_cast<std::uint32_t>(lobbies_.size()));

    std::vector<const Lobby*> ordered;
    ordered.reserve(lobbies_.size());
    for (const auto& [unused, lobby] : lobbies_) ordered.push_back(lobby.get());
    std::sort(ordered.begin(), ordered.end(), [](const Lobby* left, const Lobby* right) {
        return left->token < right->token;
    });

    for (const Lobby* lobby : ordered) {
        writer.String(lobby->token);
        writer.String(lobby->name);
        writer.U64(lobby->revision);
        writer.I32(lobby->board_action);
        writer.I32(lobby->pending_robber_hex);
        writer.String(lobby->active_trade_target);
        writer.String(lobby->status);

        writer.U32(static_cast<std::uint32_t>(lobby->robber_victims.size()));
        for (const std::string& victim : lobby->robber_victims) writer.String(victim);
        writer.U32(static_cast<std::uint32_t>(lobby->events.size()));
        for (const std::string& event : lobby->events) writer.String(event);

        writer.U32(static_cast<std::uint32_t>(lobby->players.size()));
        for (const AuthPlayer& player : lobby->players) {
            writer.I32(player.id);
            writer.String(player.name);
            writer.String(player.token);
            writer.Bool(player.ready);
            writer.Bool(player.host);
        }
        writer.Bool(lobby->game != nullptr);
        if (lobby->game) writer.String(lobby->game->SerializeState());
    }
    writer.U32(static_cast<std::uint32_t>(replay_entries_.size()));
    for (const ReplayEntry& entry : replay_entries_) {
        writer.String(entry.request_id);
        writer.String(entry.fingerprint);
        writer.Bool(entry.identity);
        if (entry.identity) {
            writer.String(entry.identity_result.message);
            writer.String(entry.identity_result.lobby_token);
            writer.String(entry.identity_result.player_token);
            writer.String(entry.identity_result.player_name);
        } else writer.String(entry.result.message);
    }
    return std::move(writer).Finish();
}

Result Service::RestoreState(std::string_view state)
{
    std::lock_guard lock(mutex_);
    try {
        StateReader reader(state);
        if (reader.Raw(StateMagic.size()) != StateMagic)
            return {false, "Dedicated server state has an invalid signature"};
        const std::uint32_t version = reader.U32();
        if (version < 1 || version > StateVersion)
            return {false, "Dedicated server state version is not supported"};
        const std::uint32_t lobby_count = reader.U32();
        if (lobby_count > max_lobbies_)
            return {false, "Dedicated server state exceeds the configured lobby limit"};

        std::unordered_map<std::string, std::unique_ptr<Lobby>> restored;
        std::unordered_set<std::string> private_tokens;
        for (std::uint32_t lobby_index = 0; lobby_index < lobby_count; ++lobby_index) {
            auto lobby = std::make_unique<Lobby>();
            lobby->token = reader.String();
            lobby->name = reader.String();
            lobby->revision = reader.U64();
            lobby->board_action = reader.I32();
            lobby->pending_robber_hex = reader.I32();
            lobby->active_trade_target = reader.String();
            lobby->status = reader.String();
            if (lobby->token.empty() || lobby->token.size() > 128
                || lobby->name.empty() || lobby->name.size() > 40 || lobby->revision == 0
                || lobby->board_action < 0 || lobby->board_action > 4
                || lobby->pending_robber_hex < -1 || lobby->pending_robber_hex > 18
                || lobby->status.size() > 1024)
                throw std::invalid_argument("Dedicated server state has invalid lobby metadata");

            const std::uint32_t victim_count = reader.U32();
            if (victim_count > 3)
                throw std::invalid_argument("Dedicated server state has too many robber victims");
            for (std::uint32_t index = 0; index < victim_count; ++index)
                lobby->robber_victims.push_back(reader.String());
            const std::uint32_t event_count = reader.U32();
            if (event_count > 40)
                throw std::invalid_argument("Dedicated server state has too many events");
            for (std::uint32_t index = 0; index < event_count; ++index) {
                std::string event = reader.String();
                if (event.size() > 1024)
                    throw std::invalid_argument("Dedicated server state event is too large");
                lobby->events.push_back(std::move(event));
            }

            const std::uint32_t player_count = reader.U32();
            if (player_count < 1 || player_count > 4)
                throw std::invalid_argument("Dedicated server state has an invalid player count");
            int host_count = 0;
            std::unordered_set<std::string> player_names;
            for (std::uint32_t index = 0; index < player_count; ++index) {
                AuthPlayer player;
                player.id = reader.I32();
                player.name = reader.String();
                player.token = reader.String();
                player.ready = reader.Bool();
                player.host = reader.Bool();
                if (player.id != static_cast<int>(index) || player.name.empty()
                    || player.name.size() > 24 || player.token.empty() || player.token.size() > 128
                    || !player_names.insert(Lower(player.name)).second
                    || !private_tokens.insert(player.token).second)
                    throw std::invalid_argument("Dedicated server state has invalid player credentials");
                if (player.host) ++host_count;
                lobby->players.push_back(std::move(player));
            }
            if (host_count != 1 || !lobby->players.front().host)
                throw std::invalid_argument("Dedicated server state has an invalid lobby host");
            auto is_player = [&lobby](const std::string& name) {
                return std::any_of(lobby->players.begin(), lobby->players.end(),
                    [&name](const AuthPlayer& player) { return player.name == name; });
            };
            if ((!lobby->active_trade_target.empty() && !is_player(lobby->active_trade_target))
                || std::any_of(lobby->robber_victims.begin(), lobby->robber_victims.end(),
                    [&is_player](const std::string& name) { return !is_player(name); }))
                throw std::invalid_argument("Dedicated server state references an unknown player");

            if (reader.Bool()) {
                lobby->game = GameController::DeserializeState(reader.String());
                const std::vector<std::string> game_names = lobby->game->GetPlayerNames();
                if (game_names.size() != lobby->players.size())
                    throw std::invalid_argument("Dedicated game players do not match lobby credentials");
                for (const AuthPlayer& player : lobby->players)
                    if (std::find(game_names.begin(), game_names.end(), player.name) == game_names.end())
                        throw std::invalid_argument("Dedicated game players do not match lobby credentials");
            } else if (lobby->pending_robber_hex != -1 || !lobby->robber_victims.empty()
                || !lobby->active_trade_target.empty() || lobby->board_action != 0) {
                throw std::invalid_argument("Waiting lobby contains active game state");
            }
            if (!restored.emplace(lobby->token, std::move(lobby)).second)
                throw std::invalid_argument("Dedicated server state contains a duplicate lobby token");
        }
        std::vector<ReplayEntry> restored_replays;
        if (version >= 2) {
            const std::uint32_t replay_count = reader.U32();
            if (replay_count > MaximumReplayEntries)
                throw std::invalid_argument("Dedicated server state has too many replay records");
            std::unordered_set<std::string> request_ids;
            restored_replays.reserve(replay_count);
            for (std::uint32_t index = 0; index < replay_count; ++index) {
                ReplayEntry entry;
                entry.request_id = reader.String();
                entry.fingerprint = reader.String();
                entry.identity = reader.Bool();
                if (!ValidRequestId(entry.request_id) || entry.request_id.empty()
                    || entry.fingerprint.empty() || entry.fingerprint.size() > 4096
                    || !request_ids.insert(entry.request_id).second)
                    throw std::invalid_argument("Dedicated server state has an invalid replay record");
                if (entry.identity) {
                    entry.identity_result.ok = true;
                    entry.identity_result.message = reader.String();
                    entry.identity_result.lobby_token = reader.String();
                    entry.identity_result.player_token = reader.String();
                    entry.identity_result.player_name = reader.String();
                    if (entry.identity_result.lobby_token.empty()
                        || entry.identity_result.lobby_token.size() > 128
                        || entry.identity_result.player_token.empty()
                        || entry.identity_result.player_token.size() > 128
                        || entry.identity_result.player_name.empty()
                        || entry.identity_result.player_name.size() > 24)
                        throw std::invalid_argument("Dedicated server state has an invalid identity replay");
                } else {
                    entry.result.ok = true;
                    entry.result.message = reader.String();
                }
                const std::string& message = entry.identity
                    ? entry.identity_result.message : entry.result.message;
                if (message.size() > 1024)
                    throw std::invalid_argument("Dedicated server replay message is too large");
                restored_replays.push_back(std::move(entry));
            }
        }
        reader.RequireFinished();
        lobbies_.swap(restored);
        replay_entries_.swap(restored_replays);
        return {true, "Dedicated server state restored"};
    } catch (const std::exception& exception) {
        return {false, exception.what()};
    }
}

} // namespace ivv::catan::dedicated
