#include "dedicated_protocol.hpp"

#include <charconv>
#include <sstream>

namespace ivv::catan::dedicated::protocol {
namespace {

std::string ResourceString(const Resources& value)
{
    return std::to_string(value.wood) + ',' + std::to_string(value.clay) + ','
        + std::to_string(value.hay) + ',' + std::to_string(value.sheep) + ','
        + std::to_string(value.stone);
}

bool ParseInt(std::string_view value, int& output)
{
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    return result.ec == std::errc() && result.ptr == value.data() + value.size();
}

bool ParseUInt64(std::string_view value, std::uint64_t& output)
{
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    return result.ec == std::errc() && result.ptr == value.data() + value.size();
}

bool ParseResources(std::string_view value, Resources& output)
{
    const auto fields = Split(value, ',');
    return fields.size() == 5 && ParseInt(fields[0], output.wood)
        && ParseInt(fields[1], output.clay) && ParseInt(fields[2], output.hay)
        && ParseInt(fields[3], output.sheep) && ParseInt(fields[4], output.stone);
}

std::string IntList(const std::vector<int>& values)
{
    std::string result;
    for (int value : values) {
        if (!result.empty()) result.push_back(',');
        result += std::to_string(value);
    }
    return result;
}

bool ParseIntList(std::string_view value, std::vector<int>& output)
{
    output.clear();
    if (value.empty()) return true;
    for (const std::string& field : Split(value, ',')) {
        int parsed = 0;
        if (!ParseInt(field, parsed)) return false;
        output.push_back(parsed);
    }
    return true;
}

std::string Error(std::string_view message) { return "ERR\t" + HexEncode(message); }

} // namespace

std::string HexEncode(std::string_view value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (unsigned char byte : value) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 15]);
    }
    return result;
}

std::optional<std::string> HexDecode(std::string_view value)
{
    if (value.size() % 2 != 0) return std::nullopt;
    auto digit = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    std::string result;
    result.reserve(value.size() / 2);
    for (std::size_t index = 0; index < value.size(); index += 2) {
        const int high = digit(value[index]);
        const int low = digit(value[index + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        result.push_back(static_cast<char>((high << 4) | low));
    }
    return result;
}

std::vector<std::string> Split(std::string_view value, char delimiter)
{
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t end = value.find(delimiter, begin);
        result.emplace_back(value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin));
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return result;
}

std::string SerializeSnapshot(const Snapshot& snapshot)
{
    std::ostringstream output;
    output << "M\t" << snapshot.revision << '\t' << snapshot.playing << '\t'
           << HexEncode(snapshot.lobby_name) << '\t' << HexEncode(snapshot.local_player) << '\n';
    for (const auto& player : snapshot.lobby_players)
        output << "L\t" << player.id << '\t' << HexEncode(player.name) << '\t'
               << player.ready << '\t' << player.host << '\n';
    if (!snapshot.playing) return output.str();
    output << "G\t" << HexEncode(snapshot.current_player) << '\t' << HexEncode(snapshot.step)
           << '\t' << snapshot.phase << '\t' << snapshot.board_action << '\t'
           << snapshot.first_die << '\t' << snapshot.second_die << '\t'
           << HexEncode(snapshot.winner) << '\t' << HexEncode(snapshot.status) << '\t'
           << snapshot.required_discard << '\t' << snapshot.pending_robber_hex << '\t'
           << snapshot.has_settlement_target << '\t' << snapshot.has_city_target << '\t'
           << snapshot.has_road_target << '\n';
    output << "T\tN\t" << IntList(snapshot.valid_nodes) << '\n';
    output << "T\tR\t" << IntList(snapshot.valid_roads) << '\n';
    output << "T\tH\t" << IntList(snapshot.valid_hexes) << '\n';
    output << "B\t" << ResourceString(snapshot.bank_resources) << '\n';
    for (const auto& victim : snapshot.robber_victims) output << "V\t" << HexEncode(victim) << '\n';
    if (snapshot.deal.active)
        output << "D\t" << HexEncode(snapshot.deal.offering_player) << '\t'
               << HexEncode(snapshot.deal.target_player) << '\t'
               << ResourceString(snapshot.deal.offered) << '\t'
               << ResourceString(snapshot.deal.requested) << '\n';
    for (const auto& event : snapshot.events) output << "E\t" << HexEncode(event) << '\n';
    for (const auto& player : snapshot.players)
        output << "P\t" << player.id << '\t' << HexEncode(player.name) << '\t'
               << player.current << '\t' << player.local << '\t' << player.resources_visible << '\t'
               << player.victory_points << '\t' << player.resource_cards << '\t'
               << player.development_cards << '\t' << player.free_settlements << '\t'
               << player.free_cities << '\t' << player.free_roads << '\t'
               << ResourceString(player.resources) << '\t' << ResourceString(player.trade_rates) << '\t'
               << player.knights << '\t' << player.road_building << '\t'
               << player.year_of_plenty << '\t' << player.monopoly << '\t'
               << player.pending_development << '\t' << player.largest_army << '\t'
               << player.longest_road << '\t' << player.victory_point_cards << '\n';
    for (const auto& hex : snapshot.hexes)
        output << "H\t" << hex.id << '\t' << hex.resource << '\t' << hex.dice << '\t' << hex.robber << '\n';
    for (const auto& node : snapshot.nodes)
        output << "N\t" << node.id << '\t' << node.owner << '\t' << node.city << '\n';
    for (const auto& road : snapshot.roads)
        output << "R\t" << road.id << '\t' << road.owner << '\n';
    return output.str();
}

std::optional<Snapshot> DeserializeSnapshot(std::string_view payload, std::string& error)
{
    Snapshot snapshot;
    bool metadata = false;
    error = "Snapshot contains a malformed value";
    for (const std::string& line : Split(payload, '\n')) {
        if (line.empty()) continue;
        const auto fields = Split(line, '\t');
        auto decode = [&error](const std::string& value) -> std::optional<std::string> {
            auto decoded = HexDecode(value);
            if (!decoded) error = "Snapshot contains invalid text encoding";
            return decoded;
        };
        if (fields[0] == "M" && fields.size() == 5) {
            int playing = 0;
            auto lobby = decode(fields[3]); auto local = decode(fields[4]);
            if (!ParseUInt64(fields[1], snapshot.revision) || !ParseInt(fields[2], playing) || !lobby || !local) return std::nullopt;
            snapshot.playing = playing != 0; snapshot.lobby_name = *lobby; snapshot.local_player = *local; metadata = true;
        } else if (fields[0] == "L" && fields.size() == 5) {
            LobbyPlayerSnapshot item; int ready = 0, host = 0; auto name = decode(fields[2]);
            if (!ParseInt(fields[1], item.id) || !name || !ParseInt(fields[3], ready) || !ParseInt(fields[4], host)) return std::nullopt;
            item.name = *name; item.ready = ready != 0; item.host = host != 0; snapshot.lobby_players.push_back(std::move(item));
        } else if (fields[0] == "G" && fields.size() == 14) {
            int settlement = 0, city = 0, road = 0; auto current = decode(fields[1]); auto step = decode(fields[2]);
            auto winner = decode(fields[7]); auto status = decode(fields[8]);
            if (!current || !step || !winner || !status || !ParseInt(fields[3], snapshot.phase)
                || !ParseInt(fields[4], snapshot.board_action) || !ParseInt(fields[5], snapshot.first_die)
                || !ParseInt(fields[6], snapshot.second_die) || !ParseInt(fields[9], snapshot.required_discard)
                || !ParseInt(fields[10], snapshot.pending_robber_hex) || !ParseInt(fields[11], settlement)
                || !ParseInt(fields[12], city) || !ParseInt(fields[13], road)) return std::nullopt;
            snapshot.current_player = *current; snapshot.step = *step; snapshot.winner = *winner; snapshot.status = *status;
            snapshot.has_settlement_target = settlement != 0; snapshot.has_city_target = city != 0; snapshot.has_road_target = road != 0;
        } else if (fields[0] == "T" && fields.size() == 3) {
            std::vector<int>* target = nullptr;
            if (fields[1] == "N") target = &snapshot.valid_nodes;
            else if (fields[1] == "R") target = &snapshot.valid_roads;
            else if (fields[1] == "H") target = &snapshot.valid_hexes;
            else { error = "Snapshot contains an invalid target type"; return std::nullopt; }
            if (!ParseIntList(fields[2], *target)) return std::nullopt;
        } else if (fields[0] == "V" && fields.size() == 2) {
            auto value = decode(fields[1]); if (!value) return std::nullopt; snapshot.robber_victims.push_back(*value);
        } else if (fields[0] == "B" && fields.size() == 2) {
            if (!ParseResources(fields[1], snapshot.bank_resources)) return std::nullopt;
        } else if (fields[0] == "D" && fields.size() == 5) {
            auto offering = decode(fields[1]); auto target = decode(fields[2]);
            if (!offering || !target || !ParseResources(fields[3], snapshot.deal.offered) || !ParseResources(fields[4], snapshot.deal.requested)) return std::nullopt;
            snapshot.deal.active = true; snapshot.deal.offering_player = *offering; snapshot.deal.target_player = *target;
        } else if (fields[0] == "E" && fields.size() == 2) {
            auto value = decode(fields[1]); if (!value) return std::nullopt; snapshot.events.push_back(*value);
        } else if (fields[0] == "P" && fields.size() == 22) {
            PlayerSnapshot item; int current = 0, local = 0, visible = 0, army = 0, road = 0; auto name = decode(fields[2]);
            if (!name || !ParseInt(fields[1], item.id) || !ParseInt(fields[3], current) || !ParseInt(fields[4], local)
                || !ParseInt(fields[5], visible) || !ParseInt(fields[6], item.victory_points)
                || !ParseInt(fields[7], item.resource_cards) || !ParseInt(fields[8], item.development_cards)
                || !ParseInt(fields[9], item.free_settlements) || !ParseInt(fields[10], item.free_cities)
                || !ParseInt(fields[11], item.free_roads) || !ParseResources(fields[12], item.resources)
                || !ParseResources(fields[13], item.trade_rates) || !ParseInt(fields[14], item.knights)
                || !ParseInt(fields[15], item.road_building) || !ParseInt(fields[16], item.year_of_plenty)
                || !ParseInt(fields[17], item.monopoly) || !ParseInt(fields[18], item.pending_development)
                || !ParseInt(fields[19], army) || !ParseInt(fields[20], road)
                || !ParseInt(fields[21], item.victory_point_cards)) return std::nullopt;
            item.name = *name; item.current = current != 0; item.local = local != 0; item.resources_visible = visible != 0;
            item.largest_army = army != 0; item.longest_road = road != 0; snapshot.players.push_back(std::move(item));
        } else if (fields[0] == "H" && fields.size() == 5) {
            HexSnapshot item; int robber = 0;
            if (!ParseInt(fields[1], item.id) || !ParseInt(fields[2], item.resource) || !ParseInt(fields[3], item.dice) || !ParseInt(fields[4], robber)) return std::nullopt;
            item.robber = robber != 0; snapshot.hexes.push_back(item);
        } else if (fields[0] == "N" && fields.size() == 4) {
            NodeSnapshot item; int city = 0;
            if (!ParseInt(fields[1], item.id) || !ParseInt(fields[2], item.owner) || !ParseInt(fields[3], city)) return std::nullopt;
            item.city = city != 0; snapshot.nodes.push_back(item);
        } else if (fields[0] == "R" && fields.size() == 3) {
            RoadSnapshot item; if (!ParseInt(fields[1], item.id) || !ParseInt(fields[2], item.owner)) return std::nullopt; snapshot.roads.push_back(item);
        } else { error = "Snapshot contains an unknown or malformed record"; return std::nullopt; }
    }
    if (!metadata) { error = "Snapshot metadata is missing"; return std::nullopt; }
    error.clear();
    return snapshot;
}

std::string HandleRequest(Service& service, std::string_view request)
{
    while (!request.empty() && (request.back() == '\n' || request.back() == '\r')) request.remove_suffix(1);
    const auto fields = Split(request, '\t');
    if (fields.empty()) return Error("Empty request");
    if (fields[0] == "PING" && fields.size() == 1) return "OK\tPONG";
    if (fields[0] == "CREATE" && fields.size() == 3) {
        auto player = HexDecode(fields[1]); auto lobby = HexDecode(fields[2]);
        if (!player || !lobby) return Error("Invalid text encoding");
        const auto result = service.CreateLobby(*player, *lobby);
        if (!result.ok) return Error(result.message);
        return "OK\tCREATED\t" + result.lobby_token + '\t' + result.player_token + '\t' + HexEncode(result.player_name);
    }
    if (fields[0] == "CREATE2" && fields.size() == 4) {
        auto player = HexDecode(fields[2]); auto lobby = HexDecode(fields[3]);
        if (!player || !lobby) return Error("Invalid text encoding");
        const auto result = service.CreateLobby(*player, *lobby, fields[1]);
        if (!result.ok) return Error(result.message);
        return "OK\tCREATED\t" + result.lobby_token + '\t' + result.player_token + '\t' + HexEncode(result.player_name);
    }
    if (fields[0] == "JOIN" && fields.size() == 3) {
        auto player = HexDecode(fields[2]); if (!player) return Error("Invalid text encoding");
        const auto result = service.JoinLobby(fields[1], *player);
        if (!result.ok) return Error(result.message);
        return "OK\tJOINED\t" + result.lobby_token + '\t' + result.player_token + '\t' + HexEncode(result.player_name);
    }
    if (fields[0] == "JOIN2" && fields.size() == 4) {
        auto player = HexDecode(fields[3]); if (!player) return Error("Invalid text encoding");
        const auto result = service.JoinLobby(fields[2], *player, fields[1]);
        if (!result.ok) return Error(result.message);
        return "OK\tJOINED\t" + result.lobby_token + '\t' + result.player_token + '\t' + HexEncode(result.player_name);
    }
    if (fields[0] == "RESUME" && fields.size() == 3) {
        const auto result = service.ResumeLobby(fields[1], fields[2]);
        if (!result.ok) return Error(result.message);
        return "OK\tRESUMED\t" + result.lobby_token + '\t' + HexEncode(result.player_name);
    }
    if (fields[0] == "LEAVE2" && fields.size() == 4) {
        const auto result = service.LeaveLobby(fields[1], fields[2], fields[3]);
        return result.ok ? "OK\tLEFT\t" + HexEncode(result.message) : Error(result.message);
    }
    if (fields[0] == "READY2" && fields.size() == 5) {
        int ready = 0;
        if (!ParseInt(fields[4], ready) || (ready != 0 && ready != 1)) return Error("Invalid ready value");
        const auto result = service.SetReady(fields[1], fields[2], ready != 0, fields[3]);
        return result.ok ? "OK\tRESULT\t" + HexEncode(result.message) : Error(result.message);
    }
    if (fields[0] == "START2" && fields.size() == 4) {
        const auto result = service.StartGame(fields[1], fields[2], fields[3]);
        return result.ok ? "OK\tRESULT\t" + HexEncode(result.message) : Error(result.message);
    }
    if (fields[0] == "COMMAND2" && fields.size() == 10) {
        int command = 0; CommandArgs args;
        if (!ParseInt(fields[4], command) || command < 0 || command > static_cast<int>(Command::SelectBoardAction)
            || !ParseInt(fields[5], args.first) || !ParseInt(fields[6], args.second)) return Error("Invalid command arguments");
        auto text = HexDecode(fields[7]);
        if (!text || !ParseResources(fields[8], args.first_resources) || !ParseResources(fields[9], args.second_resources))
            return Error("Invalid command arguments");
        args.text = *text;
        const auto result = service.Execute(fields[1], fields[2], static_cast<Command>(command), args, fields[3]);
        return result.ok ? "OK\tRESULT\t" + HexEncode(result.message) : Error(result.message);
    }
    if (fields.size() >= 3 && (fields[0] == "LEAVE" || fields[0] == "READY"
        || fields[0] == "START" || fields[0] == "SNAPSHOT" || fields[0] == "COMMAND")) {
        if (fields[0] == "LEAVE" && fields.size() == 3) {
            const auto result = service.LeaveLobby(fields[1], fields[2]);
            return result.ok ? "OK\tLEFT\t" + HexEncode(result.message) : Error(result.message);
        }
        if (fields[0] == "READY" && fields.size() == 4) {
            int ready = 0; if (!ParseInt(fields[3], ready) || (ready != 0 && ready != 1)) return Error("Invalid ready value");
            const auto result = service.SetReady(fields[1], fields[2], ready != 0);
            return result.ok ? "OK\tRESULT\t" + HexEncode(result.message) : Error(result.message);
        }
        if (fields[0] == "START" && fields.size() == 3) {
            const auto result = service.StartGame(fields[1], fields[2]);
            return result.ok ? "OK\tRESULT\t" + HexEncode(result.message) : Error(result.message);
        }
        if (fields[0] == "SNAPSHOT" && fields.size() == 3) {
            std::string error; const auto snapshot = service.GetSnapshot(fields[1], fields[2], error);
            return snapshot ? "OK\tSNAPSHOT\t" + HexEncode(SerializeSnapshot(*snapshot)) : Error(error);
        }
        if (fields[0] == "COMMAND" && fields.size() == 9) {
            int command = 0; CommandArgs args;
            if (!ParseInt(fields[3], command) || command < 0 || command > static_cast<int>(Command::SelectBoardAction)
                || !ParseInt(fields[4], args.first) || !ParseInt(fields[5], args.second)) return Error("Invalid command arguments");
            auto text = HexDecode(fields[6]);
            if (!text || !ParseResources(fields[7], args.first_resources) || !ParseResources(fields[8], args.second_resources))
                return Error("Invalid command arguments");
            args.text = *text;
            const auto result = service.Execute(fields[1], fields[2], static_cast<Command>(command), args);
            return result.ok ? "OK\tRESULT\t" + HexEncode(result.message) : Error(result.message);
        }
    }
    return Error("Unknown or malformed request");
}

} // namespace ivv::catan::dedicated::protocol
