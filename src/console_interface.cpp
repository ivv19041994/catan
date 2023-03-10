#include "console_interface.hpp"
#include "catan.hpp"
#include "map_rander.h"
#include "player_randerer.hpp"
#include "exception.hpp"

#include <string>
#include <string_view>
#include <charconv>
#include <fstream>

namespace ivv {
namespace catan {
namespace console {

template <typename StringImpl>
std::vector<StringImpl> SplitIntoWords(const StringImpl& text) {
	std::vector<StringImpl> words;
	size_t pos = 0;
	size_t size = text.size();
	size_t end;
	for (; pos != StringImpl::npos; pos = text.find_first_not_of(' ', end)) {
		end = text.find_first_of(' ', pos);
		if (end == StringImpl::npos) {
			end = size;
		}
		if (pos != end) {
			words.push_back(text.substr(pos, end - pos));
		}
	}

	return words;
}

static Resurse StringToResurse(std::string_view resurse) {
	using namespace std::string_view_literals;
	if (resurse == "w"sv) {
		return Resurse::Wood;
	}
	else if (resurse == "c"sv) {
		return Resurse::Clay;
	}
	else if (resurse == "h"sv) {
		return Resurse::Hay;
	}
	else if (resurse == "s"sv) {
		return Resurse::Sheep;
	}
	else if (resurse == "r"sv) {
		return Resurse::Stone;
	}
	else {
		throw logic_error("Error format");
	}
}

void Play::Command(std::string&& command) {
	using namespace std::string_view_literals;
	auto args = SplitIntoWords(std::string_view(command));

	if (args.size() < 1) {
		os_ << "Error format" << std::endl;
		return;
	}

	try {
		if (args[0] == "Sett"sv) {

			CommandSettlement(args);
		}
		else if (args[0] == "Dice"sv) {
			CommandDice(args);
		}
		else if (args[0] == "Road"sv) {
			CommandRoad(args);
		}
		else if (args[0] == "Player"sv) {
			CommandPlayer(args);
		}
		else if (args[0] == "Map"sv) {
			CommandMap(args);
		}
		else if (args[0] == "Pass"sv) {
			CommandPass(args);
		}
		else if (args[0] == "Step"sv) {
			CommandStep(args);
		}
		else if (args[0] == "LastDice"sv) {
			CommandLastDice(args);
		}
		else if (args[0] == "Drop"sv) {
			CommandDrop(args);
		}
		else if (args[0] == "Castle"sv) {
			CommandCastle(args);
		}
		else if (args[0] == "Market"sv) {
			CommandMarket(args);
		}
		else if (args[0] == "BanditMove"sv) {
			CommandBanditMove(args);
		}
		else if (args[0] == "BuyDevCard"sv) {
			CommandBuyDevCard(args);
		}
		else if (args[0] == "Knights"sv) {
			CommandKnights(args);
		}
		else if (args[0] == "RoadBuilding"sv) {
			CommandRoadBuilding(args);
		}
		else if (args[0] == "YearOfPlenty"sv) {
			CommandYearOfPlenty(args);
		}
		else if (args[0] == "Monopoly"sv) {
			CommandMonopoly(args);
		}
		else if (args[0] == "WinCard"sv) {
			CommandWinCard(args);
		}
		else if (args[0] == "Deal"sv) {
			CommandDeal(args);
		}
		


	}
	catch(logic_error& e) {
		os_ << e.what() << std::endl;
	}
}

std::optional<int> to_int(const std::string_view& input)
{
	int out;
	const std::from_chars_result result = std::from_chars(input.data(), input.data() + input.size(), out);
	if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
	{
		return std::nullopt;
	}
	return out;
}

void Play::CommandSettlement(const std::vector<std::string_view>& args) {
	if (args.size() != 3) {
		throw logic_error("Error format");
	}

	auto id = to_int(args[2]);
	if (!id) {
		throw logic_error("Error format");
	}

	game_controller_->BuildSettlement(args[1], *id);
}

void Play::CommandCastle(const std::vector<std::string_view>& args) {
	if (args.size() != 3) {
		throw logic_error("Error format");
	}

	auto id = to_int(args[2]);
	if (!id) {
		throw logic_error("Error format");
	}

	game_controller_->BuildCastle(args[1], *id);
}

void Play::CommandDice(const std::vector<std::string_view>& args) {
	if (args.size() != 2) {
		throw logic_error("Error format");
	}
	game_controller_->Dice(args[1]);
}
void Play::CommandRoad(const std::vector<std::string_view>& args) {
	if (args.size() != 3) {
		throw logic_error("Error format");
	}

	auto id = to_int(args[2]);
	if (!id) {
		throw logic_error("Error format");
	}

	game_controller_->BuildRoad(args[1], *id);
}
void Play::CommandPlayer(const std::vector<std::string_view>& args) {
	if (args.size() != 2) {
		throw logic_error("Error format");
	}
	game_controller_->PrintPlayer(os_, args[1]);
}
void Play::CommandMap(const std::vector<std::string_view>& args) {
	ivv::catan::renderer::MapRenderer renderer{ game_controller_->GetMap() , {1.0, 1.0}, 10.0 };
	renderer.Render(os_);
	os_ << std::endl;
}
void Play::CommandPass(const std::vector<std::string_view>& args) {
	if (args.size() != 2) {
		throw logic_error("Error format");
	}
	game_controller_->Pass(args[1]);
}

void Play::CommandStep(const std::vector<std::string_view>& args) {
	game_controller_->PrintStep(os_);
	os_ << std::endl;
}

void Play::CommandLastDice(const std::vector<std::string_view>& args) {
	auto dice = game_controller_->GetLastDice();
	os_ << "Last dice " << dice.first << " / " << dice.second << std::endl;
}

void Play::CommandBanditMove(const std::vector<std::string_view>& args) {
	if (args.size() == 3) {
		auto id = to_int(args[2]);
		if (id) {
			game_controller_->BanditMove(args[1], *id, "");
		}
	}
	else if (args.size() == 4) {
		auto id = to_int(args[2]);
		if (id) {
			game_controller_->BanditMove(args[1], *id, args[3]);
		}
	}
	else {
		throw logic_error("Error format");
	}
}

void Play::CommandBuyDevCard(const std::vector<std::string_view>& args) {
	if (args.size() != 2) {
		throw logic_error("Error format");
	}
	game_controller_->DevCard(args[1]);
}

void Play::CommandKnights(const std::vector<std::string_view>& args) {
	if (args.size() != 2) {
		throw logic_error("Error format");
	}
	game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::Knights, {});
}

void Play::CommandRoadBuilding(const std::vector<std::string_view>& args) {
	if (args.size() != 2) {
		throw logic_error("Error format");
	}
	game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::RoadBuilding, {});
}
void Play::CommandYearOfPlenty(const std::vector<std::string_view>& args) {
	if (args.size() != 4) {
		throw logic_error("Error format");
	}
	game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::YearOfPlenty, std::array<Resurse,2>{ StringToResurse (args[2]), StringToResurse(args[3]) });
}

void Play::CommandMonopoly(const std::vector<std::string_view>& args) {
	if (args.size() != 3) {
		throw logic_error("Error format");
	}
	game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::Monopoly,  StringToResurse(args[2]));
}
void Play::CommandWinCard(const std::vector<std::string_view>& args) {
	if (args.size() != 2) {
		throw logic_error("Error format");
	}

	try {game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::University, {});}
	catch (logic_error&) {}
	try { game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::Market, {}); }
	catch (logic_error&) {}
	try { game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::GreatHall, {}); }
	catch (logic_error&) {}
	try { game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::Chapel, {}); }
	catch (logic_error&) {}
	try { game_controller_->UseDevCard(args[1], ivv::catan::DevelopmentCard::Library, {}); }
	catch (logic_error&) {}
}

void Play::CommandDeal(const std::vector<std::string_view>& args) {
	using namespace std::string_view_literals;
	if (args.size() % 2 == 0) {
		throw logic_error("Error format");
	}
	std::map<Resurse, size_t> sell;
	std::map<Resurse, size_t> buy;
	size_t i = 2;
	for (; i < args.size() && args[i] != "/"sv; i += 2) {
		std::optional<int> count = to_int(args[i + 1]);
		if (!count) {
			throw logic_error("Error format");
		}
		sell[StringToResurse(args[i])] += *count;
	}
	++i;
	for (; i < args.size() && args[i] != "/"sv; i += 2) {
		std::optional<int> count = to_int(args[i + 1]);
		if (!count) {
			throw logic_error("Error format");
		}
		buy[StringToResurse(args[i])] += *count;
	}


	game_controller_->SetDeal(args[1], std::move(sell), std::move(buy));
}



void Play::CommandDrop(const std::vector<std::string_view>& args) {
	using namespace std::string_view_literals;
	if (args.size() % 2) {
		throw logic_error("Error format");
	}

	std::map<Resurse, size_t> resurses;

	for (size_t i = 2; i < args.size(); i += 2) {

		std::optional<int> count = to_int(args[i + 1]);

		if (!count) {
			throw logic_error("Error format");
		}

		resurses[StringToResurse(args[i])] += *count;
	}

	game_controller_->DropCards(args[1], resurses);
}

void Play::CommandMarket(const std::vector<std::string_view>& args) {
	using namespace std::string_view_literals;
	if (args.size() != 4) {
		throw logic_error("Error format");
	}
	game_controller_->Market(args[1], StringToResurse(args[2]), StringToResurse(args[3]));
}

Play::Play(std::ostream& os, std::istream& is) 
	: os_{ os }
	, is_{ is } {
	int players_count = 2;
	
	/*while (!(is_ >> players_count)) {
		std::cin.clear(std::istream::failbit);
	}
	std::string temp;
	std::getline(is_, temp);
	std::vector<std::string> players;
	for (int i = 0; i < players_count; i++) {
		std::getline(is_, temp);
		players.push_back(std::move(temp));
	}*/

	std::string temp;
	std::vector<std::string> players = { "1", "2" };

	game_controller_ = std::make_unique<GameController>(players);

	game_controller_->BuildSettlement(game_controller_->GetCurrentPlayer(), 9);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 19);

	game_controller_->BuildSettlement(game_controller_->GetCurrentPlayer(), 11);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 14);

	game_controller_->BuildSettlement(game_controller_->GetCurrentPlayer(), 13);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 16);

	game_controller_->BuildSettlement(game_controller_->GetCurrentPlayer(), 42);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 58);

	game_controller_->Dice(game_controller_->GetCurrentPlayer());
	//
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 26);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 27);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 28);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 36);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 44);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 43);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 45);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 46);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 29);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 42);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 30);
	game_controller_->BuildRoad(game_controller_->GetCurrentPlayer(), 37);
	game_controller_->PrintPlayer(os_, game_controller_->GetCurrentPlayer());

	while (!game_controller_->Finish()) {

		std::fstream file("D:/Users/Vadim/yandex_ws/catan/test.svg", std::fstream::trunc | std::fstream::out);
		ivv::catan::renderer::MapRenderer renderer{ game_controller_->GetMap() , {40.0, 40.0}, 100.0 };
		renderer.Render(file);
		file.flush();

		for (auto& p : players) {
			using namespace std::string_literals;
			std::fstream file("D:/Users/Vadim/yandex_ws/catan/"s + p +".svg"s, std::fstream::trunc | std::fstream::out);
			ivv::catan::renderer::PlayerRanderer player_renderer{ game_controller_->GetPlayer(p) , {0, 0}, 50 };
			player_renderer.Render(file);
			file.flush();
		}

		std::getline(is_, temp);
		Command(std::move(temp));
	}

	os_ << "Winner is " << *(game_controller_->GetWinner()) << std::endl;
}



}//namespace ivv::catan::console {
}//namespace ivv::catan {
}//namespace ivv 