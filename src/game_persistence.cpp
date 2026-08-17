#include "game_controller.hpp"

#include "exception.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ivv {
namespace catan {
namespace {

constexpr std::string_view SaveMagic{"CATAN_CORE_STATE"};
constexpr uint32_t SaveVersion = 1;
constexpr uint32_t NoPlayer = std::numeric_limits<uint32_t>::max();
constexpr uint64_t MaxStoredCount = 1'000'000;

constexpr std::array<Resurse, 5> Resources{
	Resurse::Wood, Resurse::Clay, Resurse::Hay, Resurse::Sheep, Resurse::Stone};
constexpr std::array<DevelopmentCard, 9> Cards{
	DevelopmentCard::Knights, DevelopmentCard::RoadBuilding,
	DevelopmentCard::YearOfPlenty, DevelopmentCard::Monopoly,
	DevelopmentCard::University, DevelopmentCard::Market,
	DevelopmentCard::GreatHall, DevelopmentCard::Chapel,
	DevelopmentCard::Library};

class Writer {
public:
	void Raw(std::string_view value) { data_.append(value); }
	void U8(uint8_t value) { data_.push_back(static_cast<char>(value)); }
	void Bool(bool value) { U8(value ? 1 : 0); }
	void U32(uint32_t value) {
		for (unsigned shift = 0; shift < 32; shift += 8)
			U8(static_cast<uint8_t>((value >> shift) & 0xff));
	}
	void U64(uint64_t value) {
		for (unsigned shift = 0; shift < 64; shift += 8)
			U8(static_cast<uint8_t>((value >> shift) & 0xff));
	}
	void String(std::string_view value) {
		if (value.size() > std::numeric_limits<uint32_t>::max())
			throw logic_error("String is too large to save");
		U32(static_cast<uint32_t>(value.size()));
		Raw(value);
	}
	std::string Finish() && { return std::move(data_); }

private:
	std::string data_;
};

class Reader {
public:
	explicit Reader(std::string_view data) : data_(data) {}

	std::string_view Raw(size_t count) {
		if (count > data_.size() - offset_)
			throw invalid_argument("Saved game is truncated");
		auto result = data_.substr(offset_, count);
		offset_ += count;
		return result;
	}
	uint8_t U8() { return static_cast<uint8_t>(Raw(1)[0]); }
	bool Bool() {
		const uint8_t value = U8();
		if (value > 1) throw invalid_argument("Invalid boolean in saved game");
		return value != 0;
	}
	uint32_t U32() {
		uint32_t result = 0;
		for (unsigned shift = 0; shift < 32; shift += 8)
			result |= static_cast<uint32_t>(U8()) << shift;
		return result;
	}
	uint64_t U64() {
		uint64_t result = 0;
		for (unsigned shift = 0; shift < 64; shift += 8)
			result |= static_cast<uint64_t>(U8()) << shift;
		return result;
	}
	std::string String() {
		const uint32_t size = U32();
		if (size > 1024 * 1024) throw invalid_argument("Saved string is too large");
		return std::string(Raw(size));
	}
	void RequireFinished() const {
		if (offset_ != data_.size())
			throw invalid_argument("Saved game has trailing data");
	}

private:
	std::string_view data_;
	size_t offset_{};
};

size_t Count(Reader& reader) {
	const uint64_t value = reader.U64();
	if (value > MaxStoredCount || value > std::numeric_limits<size_t>::max())
		throw invalid_argument("Invalid count in saved game");
	return static_cast<size_t>(value);
}

Resurse ReadResource(Reader& reader, bool allow_not) {
	const auto value = reader.U8();
	const auto maximum = static_cast<uint8_t>(allow_not ? Resurse::Not : Resurse::Stone);
	if (value > maximum) throw invalid_argument("Invalid resource in saved game");
	return static_cast<Resurse>(value);
}

DevelopmentCard ReadCard(Reader& reader) {
	const auto value = reader.U8();
	if (value > static_cast<uint8_t>(DevelopmentCard::Library))
		throw invalid_argument("Invalid development card in saved game");
	return static_cast<DevelopmentCard>(value);
}

GameController::GameStep ReadStep(Reader& reader) {
	const auto value = reader.U8();
	if (value > static_cast<uint8_t>(GameController::GameStep::Finish))
		throw invalid_argument("Invalid game step in saved game");
	return static_cast<GameController::GameStep>(value);
}

struct PlayerState {
	std::string name;
	size_t id{};
	std::array<size_t, Resources.size()> resources{};
	std::array<size_t, Resources.size()> prices{};
	std::array<size_t, Cards.size()> ready{};
	std::array<size_t, Cards.size()> purchased{};
	std::array<size_t, Cards.size()> used{};
	size_t card_count{};
	bool already_used{};
	bool largest_army{};
	bool longest_road{};
};

struct BuildingState { uint32_t owner{NoPlayer}; bool city{}; };

struct State {
	std::vector<PlayerState> players;
	uint32_t current{};
	uint32_t drop_current{};
	GameController::GameStep step{};
	GameController::GameStep after_bandit{};
	size_t die_a{};
	size_t die_b{};
	size_t road_building_count{};
	std::optional<std::string> winner;
	std::optional<size_t> setup_settlement;
	std::optional<GameController::Deal> deal;
	uint32_t army_holder{NoPlayer};
	uint32_t road_holder{NoPlayer};
	std::array<std::pair<Resurse, int>, Map::gexs_count> hexes{};
	uint32_t robber_hex{};
	std::array<BuildingState, Map::nodes_count> buildings{};
	std::array<uint32_t, Map::facets_count> roads{};
	std::deque<DevelopmentCard> deck;
};

uint32_t PlayerIndex(const std::vector<Player>& players, const Player* player) {
	if (!player) return NoPlayer;
	for (size_t i = 0; i < players.size(); ++i) {
		if (&players[i] == player) return static_cast<uint32_t>(i);
	}
	throw logic_error("Saved game references an unknown player");
}

void WriteResourceMap(Writer& writer, const std::map<Resurse, size_t>& values) {
	for (Resurse resource : Resources) {
		auto found = values.find(resource);
		writer.U64(found == values.end() ? 0 : found->second);
	}
}

void WriteCardMap(Writer& writer, const std::map<DevelopmentCard, size_t>& values) {
	for (DevelopmentCard card : Cards) {
		auto found = values.find(card);
		writer.U64(found == values.end() ? 0 : found->second);
	}
}

std::map<Resurse, size_t> ReadResourceMap(Reader& reader) {
	std::map<Resurse, size_t> result;
	for (Resurse resource : Resources) result[resource] = Count(reader);
	return result;
}

State ReadState(std::string_view data) {
	Reader reader(data);
	if (reader.Raw(SaveMagic.size()) != SaveMagic)
		throw invalid_argument("Not a Catan saved game");
	if (reader.U32() != SaveVersion)
		throw invalid_argument("Unsupported saved game version");

	State state;
	const uint32_t player_count = reader.U32();
	if (player_count < 2 || player_count > 4)
		throw invalid_argument("Saved game must contain 2 to 4 players");
	state.players.resize(player_count);
	std::set<std::string> names;
	std::set<size_t> ids;
	for (PlayerState& player : state.players) {
		player.name = reader.String();
		player.id = Count(reader);
		if (player.name.empty() || !names.insert(player.name).second || !ids.insert(player.id).second)
			throw invalid_argument("Saved game has invalid player identities");
		for (size_t& value : player.resources) value = Count(reader);
		for (size_t& value : player.prices) {
			value = Count(reader);
			if (value == 0) throw invalid_argument("Saved market price must be positive");
		}
		for (size_t& value : player.ready) value = Count(reader);
		for (size_t& value : player.purchased) value = Count(reader);
		for (size_t& value : player.used) value = Count(reader);
		player.card_count = Count(reader);
		player.already_used = reader.Bool();
		player.largest_army = reader.Bool();
		player.longest_road = reader.Bool();
	}

	state.current = reader.U32();
	state.drop_current = reader.U32();
	state.step = ReadStep(reader);
	if (state.current >= player_count || state.drop_current > player_count
		|| (state.step == GameController::GameStep::DropCards && state.drop_current >= player_count))
		throw invalid_argument("Saved current player is invalid");
	state.after_bandit = ReadStep(reader);
	state.die_a = Count(reader);
	state.die_b = Count(reader);
	if (state.die_a > 6 || state.die_b > 6)
		throw invalid_argument("Saved dice are invalid");
	state.road_building_count = Count(reader);
	if (state.road_building_count > 2)
		throw invalid_argument("Saved road-building count is invalid");
	if (reader.Bool()) state.winner = reader.String();
	if (reader.Bool()) {
		state.setup_settlement = Count(reader);
		if (*state.setup_settlement >= Map::nodes_count)
			throw invalid_argument("Saved setup settlement is invalid");
	}
	if (reader.Bool()) {
		state.deal = GameController::Deal{ReadResourceMap(reader), ReadResourceMap(reader)};
	}
	state.army_holder = reader.U32();
	state.road_holder = reader.U32();
	if ((state.army_holder != NoPlayer && state.army_holder >= player_count) ||
		(state.road_holder != NoPlayer && state.road_holder >= player_count))
		throw invalid_argument("Saved award owner is invalid");

	unsigned robber_count = 0;
	for (auto& hex : state.hexes) {
		hex.first = ReadResource(reader, true);
		hex.second = static_cast<int>(reader.U8());
		if (hex.second > 12 || hex.second == 1 || hex.second == 7)
			throw invalid_argument("Saved hex dice number is invalid");
	}
	state.robber_hex = reader.U32();
	if (state.robber_hex >= Map::gexs_count) throw invalid_argument("Saved robber hex is invalid");
	(void)robber_count;

	for (BuildingState& building : state.buildings) {
		building.owner = reader.U32();
		if (building.owner != NoPlayer && building.owner >= player_count)
			throw invalid_argument("Saved building owner is invalid");
		building.city = reader.Bool();
		if (building.owner == NoPlayer && building.city)
			throw invalid_argument("Empty saved node cannot be a city");
	}
	for (uint32_t& owner : state.roads) {
		owner = reader.U32();
		if (owner != NoPlayer && owner >= player_count)
			throw invalid_argument("Saved road owner is invalid");
	}
	const uint32_t deck_size = reader.U32();
	if (deck_size > 25) throw invalid_argument("Saved development deck is invalid");
	for (uint32_t i = 0; i < deck_size; ++i) state.deck.push_back(ReadCard(reader));
	reader.RequireFinished();
	return state;
}

} // namespace

std::string GameController::SerializeState() const {
	const std::deque<DevelopmentCard>* deck = development_cards_->PersistenceCards();
	if (!deck) throw logic_error("Development card deck does not support persistence");

	Writer writer;
	writer.Raw(SaveMagic);
	writer.U32(SaveVersion);
	writer.U32(static_cast<uint32_t>(players_.size()));
	for (const Player& player : players_) {
		writer.String(player.name);
		writer.U64(player.id_);
		WriteResourceMap(writer, player.resurses_);
		WriteResourceMap(writer, player.resurses_market_price_);
		WriteCardMap(writer, player.cards_);
		WriteCardMap(writer, player.cards_buy_on_this_turn_);
		WriteCardMap(writer, player.cards_used_);
		writer.U64(player.cards_count_);
		writer.Bool(player.already_use_dev_card_on_this_turn_);
		writer.Bool(player.knight_card_);
		writer.Bool(player.road_card_);
	}

	writer.U32(current_player_);
	writer.U32(current_drop_cards_player_);
	writer.U8(static_cast<uint8_t>(step_));
	writer.U8(static_cast<uint8_t>(step_after_bandit_));
	writer.U64(last_dice_.first);
	writer.U64(last_dice_.second);
	writer.U64(road_building_count_);
	writer.Bool(winner_.has_value());
	if (winner_) writer.String(*winner_);
	writer.Bool(setup_settlement_id_.has_value());
	if (setup_settlement_id_) writer.U64(*setup_settlement_id_);
	writer.Bool(activ_deal_.has_value());
	if (activ_deal_) {
		WriteResourceMap(writer, activ_deal_->sell);
		WriteResourceMap(writer, activ_deal_->buy);
	}
	writer.U32(PlayerIndex(players_, player_knights_));
	writer.U32(PlayerIndex(players_, player_roads_));

	uint32_t robber_hex = NoPlayer;
	const auto& hexes = map.GetGexes();
	for (size_t i = 0; i < hexes.size(); ++i) {
		writer.U8(static_cast<uint8_t>(hexes[i].getType()));
		writer.U8(static_cast<uint8_t>(hexes[i].getDice()));
		if (hexes[i].isBandit()) robber_hex = static_cast<uint32_t>(i);
	}
	if (robber_hex == NoPlayer) throw logic_error("Robber is not placed on the map");
	writer.U32(robber_hex);

	for (const Node& node : map.GetNodes()) {
		const Building* building = node.getBuilding();
		writer.U32(PlayerIndex(players_, building ? building->getPlayer() : nullptr));
		writer.Bool(building && building->isCity());
	}
	for (const Facet& facet : map.GetFacets()) {
		const Road* road = facet.getRoad();
		writer.U32(PlayerIndex(players_, road ? road->getPlayer() : nullptr));
	}

	writer.U32(static_cast<uint32_t>(deck->size()));
	for (DevelopmentCard card : *deck)
		writer.U8(static_cast<uint8_t>(card));
	return std::move(writer).Finish();
}

std::unique_ptr<GameController> GameController::DeserializeState(std::string_view data) {
	State state = ReadState(data);
	std::vector<std::string> names;
	for (const PlayerState& player : state.players) names.push_back(player.name);

	Dependencies dependencies;
	dependencies.dice[0] = std::make_unique<game::Dice>();
	dependencies.dice[1] = std::make_unique<game::Dice>();
	dependencies.development_cards = std::make_unique<DevelopmentCardDeck>(state.deck);
	auto game = std::make_unique<GameController>(names, std::move(dependencies));

	game->player_by_name_.clear();
	game->players_.clear();
	game->players_.reserve(state.players.size());
	for (const PlayerState& saved : state.players) {
		game->players_.emplace_back(saved.name, saved.id);
		Player& player = game->players_.back();
		for (size_t i = 0; i < Resources.size(); ++i) {
			player.resurses_[Resources[i]] = saved.resources[i];
			player.resurses_market_price_[Resources[i]] = saved.prices[i];
		}
		for (size_t i = 0; i < Cards.size(); ++i) {
			player.cards_[Cards[i]] = saved.ready[i];
			player.cards_buy_on_this_turn_[Cards[i]] = saved.purchased[i];
			player.cards_used_[Cards[i]] = saved.used[i];
		}
		player.cards_count_ = saved.card_count;
		player.already_use_dev_card_on_this_turn_ = saved.already_used;
		player.knight_card_ = saved.largest_army;
		player.road_card_ = saved.longest_road;
	}
	game->BindPlayers();

	game->current_player_ = state.current;
	game->current_drop_cards_player_ = state.drop_current;
	game->step_ = state.step;
	game->step_after_bandit_ = state.after_bandit;
	game->last_dice_ = {state.die_a, state.die_b};
	game->road_building_count_ = state.road_building_count;
	game->winner_ = std::move(state.winner);
	game->setup_settlement_id_ = state.setup_settlement;
	game->activ_deal_ = std::move(state.deal);
	game->player_knights_ = state.army_holder == NoPlayer ? nullptr : &game->players_[state.army_holder];
	game->player_roads_ = state.road_holder == NoPlayer ? nullptr : &game->players_[state.road_holder];

	game->map.RestoreHexConfiguration(state.hexes);
	for (size_t i = 0; i < state.buildings.size(); ++i) {
		if (state.buildings[i].owner != NoPlayer)
			game->map.RestoreBuilding(i, game->players_[state.buildings[i].owner], state.buildings[i].city);
	}
	for (size_t i = 0; i < state.roads.size(); ++i) {
		if (state.roads[i] != NoPlayer)
			game->map.RestoreRoad(i, game->players_[state.roads[i]]);
	}
	// Port callbacks run while buildings are attached. The persisted prices are
	// authoritative and must be applied after those topology callbacks.
	for (size_t player_index = 0; player_index < state.players.size(); ++player_index) {
		for (size_t resource_index = 0; resource_index < Resources.size(); ++resource_index) {
			game->players_[player_index].resurses_market_price_[Resources[resource_index]] =
				state.players[player_index].prices[resource_index];
		}
	}
	game->map.GetGexes()[state.robber_hex].setBandit(game->bandit_);

	return game;
}

} // namespace catan
} // namespace ivv
