#pragma once

#include "catan.hpp"
#include "dice.hpp"

#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <optional>

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
		CommonPlay
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

	void PrintPlayer(std::ostream& os, std::string_view player);
	void PrintStep(std::ostream& os);
private:
	std::vector <Player> players_;
	std::unordered_map<std::string_view, Player*> player_by_name_;
	unsigned int current_player_;
	Map map;

	Bandit bandit_;

	const game::Dice dice_{ 2 };
	std::pair<size_t, size_t> last_dice_;

	GameStep step_ = GameStep::ForwardBuildingSettlement;

	std::optional<std::string> winner_;

	void startPlace();

	void MixPlayers();

	void BindPlayers();

	void BuildSettlement(Player& player, size_t settlement_id);
	void BuildRoad(Player& player, size_t road_id);

	Player& CheckCurrentPlayer(std::string_view player);


};

std::ostream& operator<<(std::ostream& os, GameController::GameStep step);
}//namespace ivv::catan {
}//namespace ivv {