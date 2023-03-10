#pragma once
#include <iostream>
#include <optional>
#include <memory>
#include "game_controller.hpp"
namespace ivv {
namespace catan {
namespace console {

class Play {
public:
	Play(std::ostream& os, std::istream& is);
private:
	std::ostream& os_;
	std::istream& is_;
	std::unique_ptr<GameController> game_controller_;

	void Command(std::string&& command);
	void CommandSettlement(const std::vector<std::string_view>& args);
	void CommandCastle(const std::vector<std::string_view>& args);
	void CommandRoad(const std::vector<std::string_view>& args);

	void CommandDice(const std::vector<std::string_view>& args);
	
	void CommandPlayer(const std::vector<std::string_view>& args);
	void CommandMap(const std::vector<std::string_view>& args);
	void CommandPass(const std::vector<std::string_view>& args);
	void CommandStep(const std::vector<std::string_view>& args);
	void CommandLastDice(const std::vector<std::string_view>& args);
	void CommandDrop(const std::vector<std::string_view>& args);
	void CommandMarket(const std::vector<std::string_view>& args);
	void CommandBanditMove(const std::vector<std::string_view>& args);

	void CommandBuyDevCard(const std::vector<std::string_view>& args);
	void CommandKnights(const std::vector<std::string_view>& args);
	void CommandRoadBuilding(const std::vector<std::string_view>& args);
	void CommandYearOfPlenty(const std::vector<std::string_view>& args);
	void CommandMonopoly(const std::vector<std::string_view>& args);
	void CommandWinCard(const std::vector<std::string_view>& args);

	void CommandDeal(const std::vector<std::string_view>& args);
};

}//namespace ivv::catan::console {
}//namespace ivv::catan {
}//namespace ivv 