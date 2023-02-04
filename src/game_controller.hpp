#pragma once

#include "catan.hpp"
#include "dice.hpp"

#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <optional>
#include <deque>
#include <variant>

namespace ivv {
namespace catan {



class GameController
{
public:

	enum class GameStep
	{
		ForwardBuildingSettlement,
		ForwardBuildingRoad,
		BackwardBuildingSettlement,
		BackwardBuildingRoad,
		DiceDrop,
		CommonPlay,
		DropCards,
		BanditMove,
		RoadBuilding, 
		Finish
	};

	struct Deal {
		std::map<Resurse, size_t> sell;
		std::map<Resurse, size_t> buy;
	};

	GameController(std::initializer_list<std::string> players);
	GameController(std::vector<std::string> players);

	void BuildSettlement(std::string_view player, size_t settlement_id);
	void BuildRoad(std::string_view player, size_t road_id);
	void BuildCastle(std::string_view player, size_t settlement_id);

	std::string GetCurrentPlayer() const;

	void Dice(std::string_view player);
	std::pair<size_t, size_t> GetLastDice() const;

	void Pass(std::string_view player);

	const Map& GetMap() const;

	bool Finish();

	std::optional<std::string> GetWinner();

	void PrintPlayer(std::ostream& os, std::string_view player);
	void PrintStep(std::ostream& os);

	void DropCards(std::string_view player, const std::map<Resurse, size_t>& resurses);
	void CheckNextDropCard();

	void Market(std::string_view player, Resurse from, Resurse to);
	
	void BanditMove(std::string_view player, size_t gex_id, std::string_view other_payer = "");

	void DevCard(std::string_view player);

	using UseDevCardParam = std::optional<std::variant<
			std::array<Resurse, 2>, //YearOfPlenty: resurses[2]
			Resurse //Monopoly: 
		>>;

	void UseDevCard(std::string_view player, DevelopmentCard card, UseDevCardParam param);
	void SetDeal(std::string_view player, std::map<Resurse, size_t> sell, std::map<Resurse, size_t> buy);

	const std::optional<Deal>& GetActivDeal() const;
	const Player& GetPlayer(std::string_view player) const;

private:
	std::vector <Player> players_;
	std::unordered_map<std::string_view, Player*> player_by_name_;
	unsigned int current_player_;
	unsigned int current_drop_cards_player_;
	Map map;

	Bandit bandit_;

	const game::Dice dice_{ 2 };
	std::pair<size_t, size_t> last_dice_;

	std::deque<DevelopmentCard> cards_deque_;
	Player* player_knights_{};//владелец карты рыцарей
	Player* player_roads_{};//владелец карты длинная дорога

	GameStep step_ = GameStep::ForwardBuildingSettlement;
	GameStep step_after_bandit_ = GameStep::CommonPlay;

	size_t road_building_count_;

	std::optional<std::string> winner_;

	std::optional<Deal> activ_deal_;

	//void startPlace();

	void MixPlayers();

	void BindPlayers();

	void InitCardsDeque();

	void BuildSettlement(Player& player, size_t settlement_id);
	void BuildRoad(Player& player, size_t road_id);
	void BuildCastle(Player& player, size_t settlement_id);

	Player& CheckCurrentPlayer(std::string_view player);
	Player& CheckAnyPlayer(std::string_view player);

	void DropCards(Player& player, const std::map<Resurse, size_t>& resurses);

	void BanditMove(Player& player, Gex& gex, Player* other_payer);

	void CheckKnightsCard();

	void CheckWinner();
};

std::ostream& operator<<(std::ostream& os, GameController::GameStep step);
}//namespace ivv::catan {
}//namespace ivv {