#include "catan.hpp"

#include "map_rander.h"

#include <deque>
#include <cassert>
#include <utility>
#include <iostream>
#include <algorithm>


namespace ivv{
	namespace catan{

		void Map::initRandomTypeForGexs()
		{

			std::deque<Resurse> types{
				Resurse::Wood,
				Resurse::Wood,
				Resurse::Wood,
				Resurse::Wood,
				Resurse::Sheep,
				Resurse::Sheep,
				Resurse::Sheep,
				Resurse::Sheep,
				Resurse::Hay,
				Resurse::Hay,
				Resurse::Hay,
				Resurse::Hay,
				Resurse::Clay,
				Resurse::Clay,
				Resurse::Clay,
				Resurse::Stone,
				Resurse::Stone,
				Resurse::Stone
			};

			assert(types.size() == (gexs_count - 1));
			gexs[9].setType(Resurse::Not);
			for(unsigned int i = 0; i < gexs_count; ++i)
			{
				if(i == 9)
					continue;

				int idx = rng() % types.size();
				gexs[i].setType(types[idx]);
				types.erase(types.begin() + idx);
			}

			const std::array<unsigned int, 12> bigCircle = {0, 1, 2, 6, 11, 15, 18, 17, 16, 12, 7, 3};
			const std::array<unsigned int, 6> smallCircle = {4, 5, 10, 14, 13, 8};

			unsigned int randDig = rng() % 6;
			std::pair<unsigned int, unsigned int> randPair = {randDig * 2, randDig};

			const std::array<unsigned int, 18> dice_ind = {
					5, 2, 6, 3, 8,
					10, 9, 12, 11, 4,
					8, 10, 9, 4, 5,
					6, 3, 11};
			for(int i = 0; i < 12;
					i += 2,
					randPair.first = (randPair.first + 2) % 12,
					randPair.second = (randPair.second + 1) % 6)
			{
				dices[dice_ind[i]].insert(&gexs[bigCircle[randPair.first]]);
				gexs[bigCircle[randPair.first]].setDice(dice_ind[i]);

				dices[dice_ind[i + 1]].insert(&gexs[bigCircle[randPair.first + 1]]);
				gexs[bigCircle[randPair.first + 1]].setDice(dice_ind[i + 1]);

				auto gex_index = smallCircle[randPair.second];
				dices[dice_ind[i / 2 + 12]].insert(&gexs[gex_index]);
				gexs[gex_index].setDice(dice_ind[i / 2 + 12]);
			}
		}

		void Map::initNodes()
		{
			int i = 0;
			for(auto& node:nodes)
				node.index = i++;
			initNode(0,
					{1, 8},//nodes
					{0, 6},//facets
					{0});  //gexs

			initNode(1,
					{0, 2},//nodes
					{0, 1},//facets
					{0});  //gexs

			initNode(2,
					{1, 3, 10},//nodes
					{1, 2, 7},//facets
					{0, 1});  //gexs

			initNode(3,
					{2, 4},//nodes
					{2, 3},//facets
					{1});  //gexs

			initNode(4,
					{3, 5, 12},//nodes
					{3, 4, 8},//facets
					{1, 2});  //gexs

			initNode(5,
					{4, 6},//nodes
					{4, 5},//facets
					{2});  //gexs

			initNode(6,
					{5, 14},//nodes
					{5, 9},//facets
					{2});  //gexs

			initNode(7,
					{8, 17},//nodes
					{10, 18},//facets
					{3});  //gexs

			initNode(8,
					{0, 7, 9},//nodes
					{6, 10, 11},//facets
					{0, 3});  //gexs

			initNode(9,
					{8, 10, 19},//nodes
					{11, 12, 19},//facets
					{0, 3, 4});  //gexs

			initNode(10,
								{2, 9, 11},//nodes
								{7, 12, 13},//facets
								{0, 1, 4});  //gexs

			initNode(11,
								{10, 12, 21},//nodes
								{13, 14, 20},//facets
								{1, 4, 5});  //gexs

			initNode(12,
								{4, 11, 13},//nodes
								{8, 14, 15},//facets
								{1, 2, 5});  //gexs

			initNode(13,
								{12, 14, 23},//nodes
								{15, 16, 21},//facets
								{2, 5, 6});  //gexs

			initNode(14,
								{6, 13, 15},//nodes
								{9, 16, 17},//facets
								{2, 6});  //gexs

			initNode(15,
								{14, 25},//nodes
								{17, 22},//facets
								{6});  //gexs

			initNode(16,
								{17, 27},//nodes
								{23, 33},//facets
								{7});  //gexs

			initNode(17,
								{7, 16, 18},//nodes
								{18, 23, 24},//facets
								{3, 7});  //gexs

			initNode(18,
								{17, 19, 29},//nodes
								{24, 25, 34},//facets
								{3, 7, 8});  //gexs

			initNode(19,
								{9, 18, 20},//nodes
								{19, 25, 26},//facets
								{3, 4, 8});  //gexs

			initNode(20,
								{19, 21, 31},//nodes
								{26, 27, 35},//facets
								{4, 8, 9});  //gexs

			initNode(21,
								{11, 20, 22},//nodes
								{20, 27, 28},//facets
								{4, 5, 9});  //gexs

			initNode(22,
								{21, 23, 33},//nodes
								{28, 29, 36},//facets
								{5, 9, 10});  //gexs

			initNode(23,
								{13, 22, 24},//nodes
								{21, 29, 30},//facets
								{5, 6, 10});  //gexs

			initNode(24,
								{23, 25, 35},//nodes
								{30, 31, 37},//facets
								{6, 10, 11});  //gexs

			initNode(25,
								{15, 24, 26},//nodes
								{22, 31, 32},//facets
								{6, 11});  //gexs

			initNode(26,
								{25, 37},//nodes
								{32, 38},//facets
								{11});  //gexs

			initNode(27,
								{16, 28},//nodes
								{33, 39},//facets
								{7});  //gexs

			initNode(28,
								{27, 29, 38},//nodes
								{39, 40, 49},//facets
								{7, 12});  //gexs

			initNode(29,
								{18, 28, 30},//nodes
								{34, 40, 41},//facets
								{7, 8, 12});  //gexs

			initNode(30,
								{29, 31, 40},//nodes
								{41, 42, 50},//facets
								{8, 12, 13});  //gexs

			initNode(31,
								{20, 30, 32},//nodes
								{35, 42, 43},//facets
								{8, 9, 13});  //gexs

			initNode(32,
								{31, 33, 42},//nodes
								{43, 44, 51},//facets
								{9, 13, 14});  //gexs

			initNode(33,
								{22, 32, 34},//nodes
								{36, 44, 45},//facets
								{9, 10, 14});  //gexs

			initNode(34,
								{33, 35, 44},//nodes
								{45, 46, 52},//facets
								{10, 14, 15});  //gexs

			initNode(35,
								{24, 34, 36},//nodes
								{37, 46, 47},//facets
								{10, 11, 15});  //gexs

			initNode(36,
								{35, 37, 46},//nodes
								{47, 48, 53},//facets
								{11, 15});  //gexs

			initNode(37,
								{26, 36},//nodes
								{38, 48},//facets
								{11});  //gexs

			initNode(38,
								{28, 39},//nodes
								{49, 54},//facets
								{12});  //gexs

			initNode(39,
								{38, 40, 47},//nodes
								{54, 55, 62},//facets
								{12, 16});  //gexs

			initNode(40,
								{30, 39, 41},//nodes
								{50, 55, 56},//facets
								{12, 13, 16});  //gexs

			initNode(41,
								{40, 42, 49},//nodes
								{56, 57, 63},//facets
								{13, 16, 17});  //gexs

			initNode(42,
								{32, 41, 43},//nodes
								{51, 57, 58},//facets
								{13, 14, 17});  //gexs

			initNode(43,
								{42, 44, 51},//nodes
								{58, 59, 64},//facets
								{14, 17, 18});  //gexs

			initNode(44,
								{34, 43, 45},//nodes
								{52, 59, 60},//facets
								{14, 15, 18});  //gexs

			initNode(45,
								{44, 46, 53},//nodes
								{60, 61, 65},//facets
								{15, 18});  //gexs

			initNode(46,
								{36, 45},//nodes
								{53, 61},//facets
								{15});  //gexs

			initNode(47,
								{39, 48},//nodes
								{62, 66},//facets
								{16});  //gexs

			initNode(48,
								{47, 49},//nodes
								{66, 67},//facets
								{16});  //gexs

			initNode(49,
								{41, 48, 50},//nodes
								{63, 67, 68},//facets
								{16, 17});  //gexs

			initNode(50,
								{49, 51},//nodes
								{68, 69},//facets
								{17});  //gexs

			initNode(51,
								{43, 50, 52},//nodes
								{64, 69, 70},//facets
								{17, 18});  //gexs

			initNode(52,
								{51, 53},//nodes
								{70, 71},//facets
								{18});  //gexs

			initNode(53,
								{45, 52},//nodes
								{65, 71},//facets
								{18});  //gexs
		}

		void Map::initPorts() {
			using namespace std::placeholders;
			auto specialized_port = [](Resurse resurse, Player* player) {
				player->DownPriceOnMarket(resurse, 2);
			};
			auto common_port = [](Player* player) {
				player->DownPriceOnMarket(Resurse::Wood, 3);
				player->DownPriceOnMarket(Resurse::Clay, 3);
				player->DownPriceOnMarket(Resurse::Sheep, 3);
				player->DownPriceOnMarket(Resurse::Hay, 3);
				player->DownPriceOnMarket(Resurse::Stone, 3);
			};

			nodes[0].SetBuilderChanger(common_port);
			nodes[1].SetBuilderChanger(common_port);

			nodes[3].SetBuilderChanger(std::bind(specialized_port, Resurse::Sheep, _1));
			nodes[4].SetBuilderChanger(std::bind(specialized_port, Resurse::Sheep, _1));

			nodes[14].SetBuilderChanger(common_port);
			nodes[15].SetBuilderChanger(common_port);

			nodes[26].SetBuilderChanger(common_port);
			nodes[37].SetBuilderChanger(common_port);

			nodes[45].SetBuilderChanger(std::bind(specialized_port, Resurse::Clay, _1));
			nodes[46].SetBuilderChanger(std::bind(specialized_port, Resurse::Clay, _1));

			nodes[47].SetBuilderChanger(common_port);
			nodes[48].SetBuilderChanger(common_port);

			nodes[50].SetBuilderChanger(std::bind(specialized_port, Resurse::Wood, _1));
			nodes[51].SetBuilderChanger(std::bind(specialized_port, Resurse::Wood, _1));

			nodes[28].SetBuilderChanger(std::bind(specialized_port, Resurse::Hay, _1));
			nodes[38].SetBuilderChanger(std::bind(specialized_port, Resurse::Hay, _1));

			nodes[7].SetBuilderChanger(std::bind(specialized_port, Resurse::Stone, _1));
			nodes[17].SetBuilderChanger(std::bind(specialized_port, Resurse::Stone, _1));
		}

		Map::Map() :rng(rd())
		{
			initNodes();
			initNodes();//нужно вызвать дважды для образования всех связей между Facets

			initRandomTypeForGexs();

			initPorts();
		}

		void Map::diceEvent(std::pair<unsigned int, unsigned int> d)
		{
			diceEvent(d.first + d.second);
		}

		void Map::diceEvent(size_t val)
		{
			if (val == 7) {
				return;
			}
			for (const auto gex : dices.at(val))
			{
				gex->diceEvent();
			}
		}

		bool Map::canPlaceStartBuilding(unsigned int nodeId)
		{
			Node &n = nodes[nodeId];
			if(!n.isFree())
				return false;
			return n.neighborsNodeIsFree();
		}

		bool Map::canPlaceBuilding(unsigned int nodeId, const Player& player)
		{
			if (!canPlaceStartBuilding(nodeId)) {
				return false;
			}
			Node& n = nodes[nodeId];

			for(auto& facet: n.getNeighborFacets()) {
				if (facet->getRoad() && facet->getRoad()->getPlayer()->getId() == player.getId()) {
					return true;
				}
			}
			return false;
		}

		bool Map::canPlaceCastle(unsigned int nodeId, const Player& p) const {
			const Node& n = nodes[nodeId];
			const Building* building = n.getBuilding();
			if (building == nullptr || building->getPlayer() != &p) {
				return false;
			}
			const Settlement* settlement = dynamic_cast<const Settlement*>(building);
			return (settlement) ? true : false;
		}

		void Map::placeCastle(unsigned int nodeId, Player& p) {
			using namespace std::string_literals;
			if (!canPlaceCastle(nodeId, p)) {
				throw invalid_argument{ "Node must be settlement of "s + p.getName()  + "!"s};
			}
			Castle* castle = p.getFreeCastle();
			if (!castle) {
				throw invalid_argument{ p.getName() + " haven't free castles!"};
			}

			nodes[nodeId].setBuilding(castle);
		}
		

		void Map::placeStartBuilding(unsigned int nodeId, Player* p)
		{
			Node &n = nodes[nodeId];
			if(!n.neighborsNodeIsFree())
				throw invalid_argument{"Node is busy"};
			n.setBuilding(p->getFreeSettlement());
		}

		void Map::placeSettlement(unsigned int nodeId, Player* p)
		{
			using namespace std::string_literals;
			Node& n = nodes[nodeId];
			if (!n.neighborsNodeIsFree())
				throw invalid_argument{ "Node is busy" };
			Building* b = p->getFreeSettlement();
			if (!b) {
				throw logic_error("Player "s + p->getName() + " haven't free settlement!");
			}
			n.setBuilding(p->getFreeSettlement());
		}

		bool Map::canPlaceRoad(unsigned int facetId, Player* p) const
		{
			if (facetId >= facets.size()) {
				return false;
			}
			const Facet &f = facets[facetId];

			if(!f.isFree())
			{
				return false;
			}

			if(!f.haveNeighborFacetWith(p) && !f.haveNeighborNodeWith(p))
			{
				return false;
			}

			if(p->getFreeRoad() == nullptr)
			{
				return false;
			}
			return true;
		}

		void Map::placeRoad(unsigned int facetId, Player* p)
		{
			Facet &f = facets[facetId];
			if(!canPlaceRoad(facetId, p))
				throw invalid_argument("Invalid place road");

			Road *r = p->getFreeRoad();

			f.setRoad(r);
		}

		std::set<Gex*> Map::getGexsByNodeId(unsigned int nodeId)
		{
			Node &n = nodes[nodeId];
			return n.getNeighborGexs();
		}

		std::set<Facet*> Map::getFacetsByNodeId(unsigned int nodeId)
		{
			Node &n = nodes[nodeId];
			return n.getNeighborFacets();
		}

		bool Map::isNodeAndFacetNeighbor(unsigned int nodeId, unsigned int facetId)
		{
			if(nodes[nodeId].getNeighborFacets().count(&facets[facetId]))
				return true;
			return false;
		}

		void Map::initNode(int node, std::vector<int> ns, std::vector<int> fs, std::vector<int> gs)
		{
			for(int n: ns)
			{
				nodes[node].addNeighbor(&nodes[n]);
				nodes[n].addNeighbor(&nodes[node]);
			}

			for(int f: fs)
			{
				nodes[node].addNeighbor(&facets[f]);
				facets[f].addNeighbor(&nodes[node]);
				for(auto fn: nodes[node].getNeighborFacets())
					facets[f].addNeighbor(fn);
			}

			for(int g: gs)
			{
				nodes[node].addNeighbor(&gexs[g]);
				gexs[g].addNeighbor(&nodes[node]);
			}
		}

		std::array<Gex, Map::gexs_count>& Map::GetGexes() {
			return gexs;
		}

		const std::array<Gex, Map::gexs_count>& Map::GetGexes() const {
			return gexs;
		}

		const std::array<Node, 54> Map::GetNodes() const {
			return nodes;
		}

		const std::array<Facet, 72> Map::GetFacets() const {
			return facets;
		}

		size_t Map::GetRoadSize(const Player* player) const {
			size_t res = 0;
			for (const Facet& facet : facets) {
				if (facet.getRoad() && facet.getRoad()->getPlayer() == player) {
					size_t current = GetLongWay(&facet, player).size();
					std::cout << "current = " << current << std::endl;
					res = std::max(res, current);
				}
			}
			return res;
		}
		/*
		std::set<const Facet*> Map::GetLongWay(const Facet* from, const Player* player, std::set<const Facet*> already) const {

			std::set< const Facet*> neighbors;
			const std::set<Node*>& nodes = from->GetNeighborNodes();
			std::cout << "Current facet = " << std::distance(facets.data(), from) << std::endl;
			
			for (const auto& node : nodes) {
				if (node->getBuilding() == nullptr || node->getBuilding()->getPlayer() == player) {
					auto& node_facets = node->getNeighborFacets();
					for (const Facet* f : node_facets) {
						if (f != from && already.count(f) == 0 && f->getRoad() && f->getRoad()->getPlayer() == player) {
							neighbors.insert(f);
						}
					}
				}
			}

			already.insert(from);

			if (neighbors.size() == 0) {
				return std::move(already);
			}

			if (neighbors.size() == 1) {
				return GetLongWay(*neighbors.begin(), player, std::move(already));
			}

			std::vector<std::set< const Facet*>> result_by_neighbors;
			for (auto& neighbor : neighbors) {
				result_by_neighbors.push_back(GetLongWay(neighbor, player, { from }));
				result_by_neighbors.back().erase(from);
			}

			std::sort(result_by_neighbors.begin(), result_by_neighbors.end(), [](const auto& lhs, const auto& rhs) {
				return lhs.size() > rhs.size();
				}
			);
			size_t counter = 0;

			for (size_t i = 0; i < result_by_neighbors.size() && counter < 2; ++i) {
				std::set< const Facet*>& result_by_neighbor = result_by_neighbors[i];
				bool not_inter = std::none_of(already.begin(), already.end(),
					[&result_by_neighbor](const Facet* facet) {
						return result_by_neighbor.count(facet) ? true : false;
					});
				if (not_inter) {
					already.insert(result_by_neighbor.begin(), result_by_neighbor.end());
					++counter;
				}
				
			}

			return std::move(already);
		}*/

		std::string SetFacetToString(const Facet* first, const std::set<const Facet*>& facets) {
			std::string ret;
			for (auto f : facets) {
				ret += std::to_string(std::distance(first, f)) + ", ";
			}
			return ret;
		}

		std::set<const Facet*> Map::GetLongWay(const Facet* from, const Player* player, std::set<const Node*> ban_node, std::set<const Facet*> already, size_t deep)  const {

			std::vector<const Node*> nodes{};
			std::vector<std::vector<const Facet*>> neighbors{};
			size_t neighbors_count = 0;
			
			std::cout << std::string(deep, '\t') << "Current facet = " << std::distance(facets.data(), from) << std::endl;

			for (const Node* node: from->GetNeighborNodes()) {
				if (ban_node.count(node) == 0 &&
					(node->getBuilding() == nullptr || node->getBuilding()->getPlayer() == player)) {
					nodes.push_back(node);
					neighbors.push_back({});
					auto& node_facets = node->getNeighborFacets();
					for (const Facet* f : node_facets) {
						if (f != from && already.count(f) == 0 && f->getRoad() && f->getRoad()->getPlayer() == player) {
							neighbors.back().push_back(f);
							++neighbors_count;
						}
					}
				}
			}

			already.insert(from);

			if (neighbors_count == 0) {
				std::cout << std::string(deep, '\t') << "return " << already.size() << " " << SetFacetToString(facets.data(), already) << std::endl;
				return std::move(already);
			}

			if (neighbors_count == 1) {
				for (size_t i = 0; i < neighbors.size(); ++i) {
					if (neighbors[i].size()) {
						ban_node.insert(nodes[i]);
						auto ret = GetLongWay(neighbors[i][0], player, std::move(ban_node), std::move(already), deep + 1);
						std::cout << std::string(deep, '\t') << "return " << ret.size() << " " << SetFacetToString(facets.data(), ret) << std::endl;
						return ret;
					}
				}
				assert(false);
			}

			std::vector< std::vector<std::set< const Facet*>>> result_by_neighbors(neighbors.size());
			for (size_t i = 0; i < neighbors.size(); ++i) {
				for (const Facet* facet : neighbors[i]) {
					auto ban_node_copy = ban_node;
					ban_node_copy.insert(nodes[i]);
					result_by_neighbors[i].push_back(GetLongWay(facet, player, std::move(ban_node_copy), { from }, deep + 1));
					result_by_neighbors[i].back().erase(from);
				}
			}

			//теперь надо найти лучшую комбинацию
			assert(result_by_neighbors.size() <= 2 && result_by_neighbors.size() != 0);

			if (result_by_neighbors.size() == 1) {
				//тут все просто, кто длиннее тот и лучше
				std::sort(result_by_neighbors[0].begin(), result_by_neighbors[0].end(), [](const auto& lhs, const auto& rhs) {
					return lhs.size() > rhs.size();
					}
				);

				already.insert(result_by_neighbors[0][0].begin(), result_by_neighbors[0][0].end());
				std::cout << std::string(deep, '\t') << "return(1) " << already.size() << " " << SetFacetToString(facets.data(), already) << std::endl;
				return std::move(already);
			}
			//result_by_neighbors.size() == 2
			//тут надо перебрать пары и найти лучшую не пересекающуюся сумму
			std::set<const Facet*> condidate;

			for (size_t i = 0; i < result_by_neighbors[0].size(); ++i) {
				for (size_t j = 0; j < result_by_neighbors[1].size(); ++j) {
					std::set<const Facet*> current;

					bool not_inter = std::none_of(result_by_neighbors[0][i].begin(), result_by_neighbors[0][i].end(),
						[&result_by_neighbors, j](const Facet* facet) {
							return result_by_neighbors[1][j].count(facet) ? true : false;
						});
					if (not_inter) {
						current.insert(result_by_neighbors[0][i].begin(), result_by_neighbors[0][i].end());
						current.insert(result_by_neighbors[1][j].begin(), result_by_neighbors[1][j].end());
					}
					else {
						if (result_by_neighbors[0][i].size() >= result_by_neighbors[1][j].size()) {
							current = result_by_neighbors[0][i];
						}
						else {
							current = result_by_neighbors[1][j];
						}
					}

					if (current.size() > condidate.size()) {
						std::swap(current, condidate);
					}
				}
			}

			already.insert(condidate.begin(), condidate.end());

			std::cout << std::string(deep, '\t') << "return(2) " << already.size() << " " << SetFacetToString(facets.data(), already) << std::endl;
			return std::move(already);
		}


		const std::set<Node*>& Facet::GetNeighborNodes() const {
			return node_neighbor;
		}



		Gex::Gex()
		{

		}

		Resurse Gex::getType() const
		{
			return resurse;
		}

		void Gex::setType(Resurse r)
		{
			resurse = r;
		}

		void Gex::addNeighbor(Node* n)
		{
			node_neighbor.insert(n);;
		}

		const std::set<Node*>& Gex::GetNodes() const {
			return node_neighbor;
		}

		void Gex::diceEvent()
		{
			if (bandit_) {
				return;
			}
			for(auto& node: node_neighbor)
			{
				node->diceEvent(resurse);
			}
		}

		void Gex::setDice(int dice) {
			dice_ = dice;
		}

		int Gex::getDice() const {
			return dice_;
		}

		void Gex::setBandit(Bandit& b) {
			if (&b == bandit_) {
				return;
			}
			Gex* gex = b.getGex();
			if (gex) {
				gex->bandit_ = nullptr;
			}
			b.setGex(this);
			bandit_ = &b;
		}

		bool Gex::isBandit() const {
			return bandit_ != nullptr;
		}

		void Node::diceEvent(Resurse r)
		{
			if(building == nullptr)
				return;
			building->diceEvent(r);
		}

		bool Node::neighborsNodeIsFree() const
		{
			for(auto nn: node_neighbor)
				if(!nn->isFree())
					return false;
			return true;
		}

		bool Node::isFree() const
		{
			return building == nullptr;
		}

		bool Node::isBusyBy(Player *p) const
		{
			if(isFree())
			{
				return false;
			}
			return building->getPlayer() == p;
		}

		void Node::setBuilding(Building* b)
		{
			if(building != nullptr)
				building->setFree();
			b->setBusy();
			building = b;
			if (change_builder_func_) {
				change_builder_func_(b->getPlayer());
			}
		}

		const Building* Node::getBuilding() const {
			return building;
		}

		std::set<Gex*> Node::getNeighborGexs()
		{
			return gex_neighbor;
		}

		const std::set<Facet*>& Node::getNeighborFacets() const
		{
			return facet_neighbor;
		}

		void Node::addNeighbor(Gex* gex)
		{
			gex_neighbor.insert(gex);
		}
		void Node::addNeighbor(Facet* facet)
		{
			facet_neighbor.insert(facet);
		}
		void Node::addNeighbor(Node* node)
		{
			node_neighbor.insert(node);
		}

		void Settlement::diceEvent(Resurse r)
		{
			player->addResurse(r);
		}

		void Castle::diceEvent(Resurse r)
		{
			player->addResurse(r, 2);
		}







		Player::Player(std::string n, size_t id): name{n}, id_{id}
		{
			resurses_[Resurse::Wood] = 100;
			resurses_[Resurse::Clay] = 100;
			resurses_[Resurse::Hay] = 100;
			resurses_[Resurse::Sheep] = 100;
			resurses_[Resurse::Stone] = 100;

			cards_[DevelopmentCard::Knights] = 0;
			cards_[DevelopmentCard::RoadBuilding] = 0;
			cards_[DevelopmentCard::YearOfPlenty] = 0;
			cards_[DevelopmentCard::Monopoly] = 0;
			cards_[DevelopmentCard::University] = 0;
			cards_[DevelopmentCard::Market] = 0;
			cards_[DevelopmentCard::GreatHall] = 0;
			cards_[DevelopmentCard::Chapel] = 0;
			cards_[DevelopmentCard::Library] = 0;
			cards_[DevelopmentCard::Knights] = 0;

			cards_buy_on_this_turn_[DevelopmentCard::Knights] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::RoadBuilding] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::YearOfPlenty] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::Monopoly] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::University] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::Market] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::GreatHall] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::Chapel] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::Library] = 0;
			cards_buy_on_this_turn_[DevelopmentCard::Knights] = 0;

			cards_used_[DevelopmentCard::Knights] = 0;
			cards_used_[DevelopmentCard::RoadBuilding] = 0;
			cards_used_[DevelopmentCard::YearOfPlenty] = 0;
			cards_used_[DevelopmentCard::Monopoly] = 0;
			cards_used_[DevelopmentCard::University] = 0;
			cards_used_[DevelopmentCard::Market] = 0;
			cards_used_[DevelopmentCard::GreatHall] = 0;
			cards_used_[DevelopmentCard::Chapel] = 0;
			cards_used_[DevelopmentCard::Library] = 0;
			cards_used_[DevelopmentCard::Knights] = 0;
		}

		void Player::addResurse(Resurse resurse, size_t count)
		{
			if(Resurse::Not == resurse)
				return;
			resurses_[resurse] += count;
			//std::cout << "resurses["<< static_cast<int>(resurse) << "] = " << resurses[resurse] << std::endl;
		}

		void Player::addResurse(const std::map<Resurse, size_t>& resurses) {
			for (auto& [name, count] : resurses) {
				addResurse(name, count);
			}
		}

		bool Player::HaveSettlemenResurses() const {
			return
				resurses_.at(Resurse::Wood) >= 1 &&
				resurses_.at(Resurse::Clay) >= 1 &&
				resurses_.at(Resurse::Hay) >= 1 &&
				resurses_.at(Resurse::Sheep) >= 1;
		}
		void Player::FreeSettlemenResurses() {
			--resurses_.at(Resurse::Wood);
			--resurses_.at(Resurse::Clay);
			--resurses_.at(Resurse::Hay);
			--resurses_.at(Resurse::Sheep);
		}

		bool Player::HaveRoadResurses() const {
			return
				resurses_.at(Resurse::Wood) >= 1 &&
				resurses_.at(Resurse::Clay) >= 1;
		}
		void Player::FreeRoadResurses() {
			--resurses_.at(Resurse::Wood);
			--resurses_.at(Resurse::Clay);
		}

		bool Player::HaveDevCardResurses() const {
			return
				resurses_.at(Resurse::Hay) >= 1 &&
				resurses_.at(Resurse::Sheep) >= 1 &&
				resurses_.at(Resurse::Stone) >= 1;
		}
		void Player::FreeDevCardResurses() {
			--resurses_.at(Resurse::Hay);
			--resurses_.at(Resurse::Sheep);
			--resurses_.at(Resurse::Stone);
		}

		bool Player::HaveCastleResurses() const {
			return
				resurses_.at(Resurse::Hay) >= 2 &&
				resurses_.at(Resurse::Stone) >= 3;
		}
		void Player::FreeCastleResurses() {
			resurses_.at(Resurse::Hay) -= 2;
			resurses_.at(Resurse::Stone) -= 3;
		}

		size_t Player::getCountResurses() const {
			size_t ret = 0;
			for (const auto& [name, count] : resurses_) {
				ret += count;
			}
			return ret;
		}

		size_t Player::getCountResurses(Resurse reusrse) const {
			auto find = resurses_.find(reusrse);
			if (find == resurses_.end()) {
				return 0;
			}
			return find->second;
		}

		bool Player::Have(const std::map<Resurse, size_t>& resurses) const {
			for (auto& [name, count] : resurses) {
				auto resurse = resurses_.find(name);
				if (resurse == resurses_.end()) {
					return false;
				}
				if (resurse->second < count) {
					return false;
				}
			}
			return true;
		}

		void Player::Drop(const std::map<Resurse, size_t>& resurses) {
			using namespace std::string_literals;

			if (!Have(resurses)) {
				throw logic_error("Player "s + this->name + " haven't resurses for drop!"s);
			}

			for (auto& [name, count] : resurses) {
				resurses_[name] -= count;
			}
		}

		void Player::Market(Resurse from, Resurse to) {
			using namespace std::string_literals;
			auto price = resurses_market_price_.at(from);

			if (resurses_.at(from) < price) {
				throw logic_error("Player "s + this->name + " haven't resurses for market!"s);
			}
			++resurses_.at(to);
			resurses_.at(from) -= price;
		}

		const std::string& Player::getName() const {
			return name;
		}

		Settlement *Player::getFreeSettlement()
		{
			for(auto& settlement: settlements)
				if(settlement.isFree())
				{
					settlement.setPlayer(this);
					return &settlement;
				}
			return nullptr;
		}
		Castle *Player::getFreeCastle()
		{
			for(auto& castle: castles)
				if(castle.isFree())
				{
					castle.setPlayer(this);
					return &castle;
				}
			return nullptr;
		}
		Road *Player::getFreeRoad()
		{
			for(auto& road: roads)
				if(road.isFree())
				{
					road.setPlayer(this);
					return &road;
				}
			return nullptr;
		}

		size_t Player::getId() const {
			return id_;
		}

		size_t Player::getFreeSettlementCount() const {
			size_t ret = 0;
			for (const auto& element : settlements) {
				if (element.isFree()) {
					++ret;
				}
			}
			return ret;
		}
		size_t Player::getFreeCastleCount() const {
			size_t ret = 0;
			for (const auto& element : castles) {
				if (element.isFree()) {
					++ret;
				}
			}
			return ret;
		}
		size_t Player::getFreeRoadCount() const {
			size_t ret = 0;
			for (const auto& element : roads) {
				if (element.isFree()) {
					++ret;
				}
			}
			return ret;
		}

		void Player::Print(std::ostream&os ) const {
			os << "Player " << name << std::endl
				<< "Id: " << id_ << std::endl
				<< "Free settlements: " << getFreeSettlementCount() << std::endl
				<< "Free castles: " << getFreeCastleCount() << std::endl
				<< "Free roads: " << getFreeRoadCount() << std::endl
				<< "Wood: " << (resurses_.count(Resurse::Wood) ? resurses_.at(Resurse::Wood) : 0) << std::endl
				<< "Clay: " << (resurses_.count(Resurse::Clay) ? resurses_.at(Resurse::Clay) : 0) << std::endl
				<< "Hay: " << (resurses_.count(Resurse::Hay) ? resurses_.at(Resurse::Hay) : 0) << std::endl
				<< "Sheep: " << (resurses_.count(Resurse::Sheep) ? resurses_.at(Resurse::Sheep) : 0) << std::endl
				<< "Stone: " << (resurses_.count(Resurse::Stone) ? resurses_.at(Resurse::Stone) : 0) << std::endl
				<< "Price Wood: " << (resurses_market_price_.count(Resurse::Wood) ? resurses_market_price_.at(Resurse::Wood) : 0) << std::endl
				<< "Price Clay: " << (resurses_market_price_.count(Resurse::Clay) ? resurses_market_price_.at(Resurse::Clay) : 0) << std::endl
				<< "Price Hay: " << (resurses_market_price_.count(Resurse::Hay) ? resurses_market_price_.at(Resurse::Hay) : 0) << std::endl
				<< "Price Sheep: " << (resurses_market_price_.count(Resurse::Sheep) ? resurses_market_price_.at(Resurse::Sheep) : 0) << std::endl
				<< "Price Stone: " << (resurses_market_price_.count(Resurse::Stone) ? resurses_market_price_.at(Resurse::Stone) : 0) << std::endl
				<< "Win points: " << GetWinPoints() << std::endl
				<< "Knights: " << cards_.at(DevelopmentCard::Knights) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Knights) << "/" << cards_used_.at(DevelopmentCard::Knights) << std::endl
				<< "RoadBuilding: " << cards_.at(DevelopmentCard::RoadBuilding) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::RoadBuilding) << "/" << cards_used_.at(DevelopmentCard::RoadBuilding) << std::endl
				<< "YearOfPlenty: " << cards_.at(DevelopmentCard::YearOfPlenty) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::YearOfPlenty) << "/" << cards_used_.at(DevelopmentCard::YearOfPlenty) << std::endl
				<< "Monopoly: " << cards_.at(DevelopmentCard::Monopoly) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Monopoly) << "/" << cards_used_.at(DevelopmentCard::Monopoly) << std::endl
				<< "University: " << cards_.at(DevelopmentCard::University) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::University) << "/" << cards_used_.at(DevelopmentCard::University) << std::endl
				<< "Market: " << cards_.at(DevelopmentCard::Market) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Market) << "/" << cards_used_.at(DevelopmentCard::Market) << std::endl
				<< "GreatHall: " << cards_.at(DevelopmentCard::GreatHall) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::GreatHall) << "/" << cards_used_.at(DevelopmentCard::GreatHall) << std::endl
				<< "Chapel: " << cards_.at(DevelopmentCard::Chapel) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Chapel) << "/" << cards_used_.at(DevelopmentCard::Chapel) << std::endl
				<< "Library: " << cards_.at(DevelopmentCard::Library) << "/" << cards_buy_on_this_turn_.at(DevelopmentCard::Library) << "/" << cards_used_.at(DevelopmentCard::Library) << std::endl;

		}

		void Player::DownPriceOnMarket(Resurse resurse, size_t price) {
			size_t& price_ = resurses_market_price_[resurse];
			if (price_ > price) {
				price_ = price;
			}
		}

		std::optional<Resurse> Player::Still() {
			size_t total = getCountResurses();
			if (!total) {
				return {};
			}

			std::random_device rd{};
			std::default_random_engine rng{ rd() };

			size_t r = rng() % total;

			for (auto& [name, count] : resurses_) {
				if (r < count) {
					--count;
					return name;
				}
				r -= count;
			}
			assert(false);
			return {};
		}

		static bool IsWinnerCard(DevelopmentCard card) {
			switch (card) {
			case DevelopmentCard::Knights:
			case DevelopmentCard::RoadBuilding:
			case DevelopmentCard::YearOfPlenty:
			case DevelopmentCard::Monopoly:
				return false;
			case DevelopmentCard::University:
			case DevelopmentCard::Market:
			case DevelopmentCard::GreatHall:
			case DevelopmentCard::Chapel:
			case DevelopmentCard::Library:
				return true;
			}
			return false;
		}

		void Player::PutCard(DevelopmentCard card) {
			if (IsWinnerCard(card)) {
				++cards_[card];
			}
			else {
				++cards_buy_on_this_turn_[card];
			}
			

			
		}

		void Player::OnEndTurn() {
			already_use_dev_card_on_this_turn_ = false;

			for (auto& [name, count] : cards_buy_on_this_turn_) {
				cards_[name] += count;
				cards_count_ += count;
				count = 0;
			}
		}

		size_t Player::StillAll(Resurse resurse) {
			size_t& count = resurses_[resurse];
			size_t ret = count;
			count = 0;
			return ret;
		}

		size_t Player::GetCardCount(DevelopmentCard card, const std::map<DevelopmentCard, size_t>& card_deque) const {
			auto find = card_deque.find(card);
			if (find == card_deque.end()) {
				return 0;
			}
			return find->second;
		}

		size_t Player::GetReadyForUseCardCount(DevelopmentCard card) const {
			return GetCardCount(card, cards_);
		}
		size_t Player::GetPurchasedCardCount(DevelopmentCard card) const {
			return GetCardCount(card, cards_buy_on_this_turn_);
		}

		size_t Player::GetUsedCardCount(DevelopmentCard card) const {
			return GetCardCount(card, cards_used_);
		}

		void Player::SetKnightCard() {
			knight_card_ = true;
		}
		void Player::ResetKnightCard() {
			knight_card_ = false;
		}

		size_t Player::GetWinPoints() const {
			size_t res = 0;
			res += 5 - getFreeSettlementCount();
			res += (4 - getFreeCastleCount()) * 2;
			res += win_cards_used_;
			if (knight_card_) {
				res += 2;
			}
			if (road_card_) {
				res += 2;
			}
			return res;
		}

		void Player::Use(DevelopmentCard card) {
			using namespace std::string_literals;
			auto& count = cards_[card];
			if (count == 0) {
				throw logic_error("Player "s + this->name + " haven't this card!"s);
			}

			switch (card) {
			case DevelopmentCard::Knights:
			case DevelopmentCard::RoadBuilding:
			case DevelopmentCard::YearOfPlenty:
			case DevelopmentCard::Monopoly:
				if (already_use_dev_card_on_this_turn_) {
					throw logic_error("Player "s + this->name + " already use card in this turn!"s);
				}
				already_use_dev_card_on_this_turn_ = true;
				break;
			case DevelopmentCard::University:
			case DevelopmentCard::Market:
			case DevelopmentCard::GreatHall:
			case DevelopmentCard::Chapel:
			case DevelopmentCard::Library:
					++win_cards_used_;
					break;
			}
			++cards_used_[card];
			--count;
			--cards_count_;
		}

		std::ostream& operator<<(std::ostream& os, const Player& player) {
			player.Print(os);
			return os;
		}

		void Construction::setBusy()
		{
			_free = false;
		}

		Player* Construction::getPlayer() const
		{
			return player;
		}

		void Construction::setPlayer(Player *p)
		{
			player = p;
		}

		bool Construction::isFree() const
		{
			return _free;
		}


		void Building::setFree()
		{
			_free = true;
		}


		out_of_range::out_of_range(const std::string& what): std::out_of_range{what}
		{

		}

		invalid_argument::invalid_argument(const std::string& what): std::invalid_argument{what}
		{

		}

		bool Facet::isFree() const
		{
			return road == nullptr;
		}

		bool Facet::isBusyBy(Player *p) const
		{
			if(isFree())
				return false;
			return road->getPlayer() == p;
		}

		bool Facet::haveNeighborFacetWith(Player *p) const 
		{
			for(auto& facet: facet_neighbor)
				if(facet->isBusyBy(p))
					return true;
			return false;
		}

		bool Facet::haveNeighborNodeWith(Player *p) const
		{
			for(auto& node: node_neighbor)
			{
				if(node->isBusyBy(p))
					return true;
			}
			return false;
		}

		void Facet::setRoad(Road *r)
		{
			if(road != nullptr)
				throw invalid_argument("Facet already busy");
			road = r;
			r->setBusy();
		}

		const Road* Facet::getRoad() const {
			return road;
		}

		void Facet::addNeighbor(Facet* f)
		{
			facet_neighbor.insert(f);
		}
		void Facet::addNeighbor(Node* n)
		{
			node_neighbor.insert(n);
		}

		void Bandit::setGex(Gex* gex) {
			gex_ = gex;
		}
		Gex* Bandit::getGex() {
			return gex_;
		}

	}
}
