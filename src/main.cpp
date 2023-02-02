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


	unsigned int player_count;

	cout << "Enter count of players:";

	cin >> player_count;
	std::vector<std::string> players(player_count);
	{
		size_t i = 1;
		for (auto& p : players) {
			cout << "Set player " << i++ << " name: ";
			cin >> p;
		}
	}
		

	ivv::catan::Game game{players};

	cout << "!!!Hello World!!!" << endl; // prints !!!Hello World!!!


	return 0;
}
