//============================================================================
// Name        : catan.cpp
// Author      : Vadim Ivanov
// Version     :
// Copyright   : All rr
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <vector>
#include "catan.hpp"
#include "svg.h"
#include "map_rander.h"
#include "game_controller.hpp"
#include "console_interface.hpp"
using namespace std;

int main() {

	/* {
		svg::Document doc;
		shapes::Hexagon hexagon(::svg::Point{ 100.0, 100.0 }, 50.0, 0.0);
		
		hexagon.SetColor("red").Draw(doc);
		shapes::Hexagon(::svg::Point{ 200.0, 100 }, 50.0, 3.14 / 6).SetColor("blue").Draw(doc);
		shapes::Hexagon(::svg::Point{ 100.0, 50 }, 10.0, 3.14 / 6).SetColor("green").Draw(doc);
		doc.Render(cout);
	}*/


	/* {
		ivv::catan::Map map;
		ivv::catan::renderer::MapRenderer renderer{ map , {0.0, 0.0}, 10.0 };
		renderer.Render(std::cout);
		return 0;
	}*/


	//unsigned int player_count;

	//cout << "Enter count of players:";

	//cin >> player_count;
	std::vector<std::string> players{"player1", "player2", "player3", "player4" };
	/* {
		size_t i = 1;
		for (auto& p : players) {
			cout << "Set player " << i++ << " name: ";
			cin >> p;
		}
	}*/
		
	/*
	ivv::catan::GameController game{players};
	game.BuildSettlement(game.GetCurrentPlayer(), 1);
	game.BuildRoad(game.GetCurrentPlayer(), 1);
	game.BuildSettlement(game.GetCurrentPlayer(), 3);
	game.BuildRoad(game.GetCurrentPlayer(), 3);
	game.BuildSettlement(game.GetCurrentPlayer(), 5);
	game.BuildRoad(game.GetCurrentPlayer(), 5);
	game.BuildSettlement(game.GetCurrentPlayer(), 7);
	game.BuildRoad(game.GetCurrentPlayer(), 10);

	game.BuildSettlement(game.GetCurrentPlayer(), 9);
	game.BuildRoad(game.GetCurrentPlayer(), 12);
	game.BuildSettlement(game.GetCurrentPlayer(), 11);
	game.BuildRoad(game.GetCurrentPlayer(), 14);
	game.BuildSettlement(game.GetCurrentPlayer(), 13);
	game.BuildRoad(game.GetCurrentPlayer(), 16);
	game.BuildSettlement(game.GetCurrentPlayer(), 42);
	game.BuildRoad(game.GetCurrentPlayer(), 57);

	for (size_t i = 0; i < 1000; ++i) {
		game.Dice(game.GetCurrentPlayer());
		game.Pass(game.GetCurrentPlayer());
	}

	{
		ivv::catan::renderer::MapRenderer renderer{ game.GetMap() , {0.0, 0.0}, 10.0 };
		renderer.Render(std::cout);
		cout << "current " << game.GetCurrentPlayer() << endl;
	}

	game.Dice(game.GetCurrentPlayer());
	game.BuildRoad(game.GetCurrentPlayer(), 56);
	game.BuildSettlement(game.GetCurrentPlayer(), 40);
	game.BuildRoad(game.GetCurrentPlayer(), 55);
	game.BuildRoad(game.GetCurrentPlayer(), 54);
	game.BuildSettlement(game.GetCurrentPlayer(), 38);

	{
		ivv::catan::renderer::MapRenderer renderer{ game.GetMap() , {0.0, 0.0}, 10.0 };
		renderer.Render(std::cout);
	}


	cout << "!!!Hello World!!!" << endl; // prints !!!Hello World!!!
	*/

	{
		ivv::catan::console::Play play{ cout, cin };
	}
	cout << "End programm" << endl;

	return 0;
}
