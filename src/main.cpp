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

	cout << "!!!Hello World!!!" << endl; // prints !!!Hello World!!!


	return 0;
}
