#include "console_interface.hpp"
#include "catan.hpp"
#include "map_rander.h"
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

void Play::Command(std::string&& command) {
	using namespace std::string_view_literals;
	auto args = SplitIntoWords(std::string_view(command));

	if (args.size() < 1) {
		os_ << "Error format" << std::endl;
		return;
	}

	try {
		if (args[0] == "Settlement"sv) {

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

		std::fstream file("C:/Users/user04134/Desktop/catan/test.svg", std::fstream::trunc | std::fstream::out);
		ivv::catan::renderer::MapRenderer renderer{ game_controller_->GetMap() , {40.0, 40.0}, 100.0 };
		renderer.Render(file);
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

	throw logic_error("Error format");
	
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

void Play::CommandDrop(const std::vector<std::string_view>& args) {
	using namespace std::string_view_literals;
	if (args.size() % 2) {
		throw logic_error("Error format");
	}

	std::map<Resurse, unsigned int> resurses;

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
	int players_count;
	
	while (!(is_ >> players_count)) {
		std::cin.clear(std::istream::failbit);
	}
	std::string temp;
	std::getline(is_, temp);
	std::vector<std::string> players;
	for (int i = 0; i < players_count; i++) {
		std::getline(is_, temp);
		players.push_back(std::move(temp));
	}

	game_controller_ = std::make_unique<GameController>(std::move(players));

	while (!game_controller_->Finish()) {
		std::getline(is_, temp);
		Command(std::move(temp));
	}
}



}//namespace ivv::catan::console {
}//namespace ivv::catan {
}//namespace ivv 