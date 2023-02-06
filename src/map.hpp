#pragma once

#include <array>
#include <vector>

#include "facet.hpp"
#include "node.hpp"
#include "gex.hpp"
#include "catan.hpp"

namespace ivv {
namespace catan {

	class Map
	{
	public:

		static constexpr unsigned int gexs_count = 19;
	private:
		std::random_device rd;
		std::default_random_engine rng;

		std::array<Gex, gexs_count> gexs;
		std::array<Facet, 72> facets;
		std::array<Node, 54> nodes;

		std::map<size_t, std::set<Gex*>> dices;

		void initNode(int node, std::vector<int> nodes, std::vector<int> roads, std::vector<int> gexs);
		void initNodes();
		void initRandomTypeForGexs();
		void initPorts();


	public:

		Map();

		void diceEvent(std::pair<unsigned int, unsigned int> d);
		void diceEvent(size_t val);

		bool canPlaceStartBuilding(unsigned int nodeId);
		void placeStartBuilding(unsigned int nodeId, Player* p);
		void placeSettlement(unsigned int nodeId, Player* p);

		bool canPlaceCastle(unsigned int nodeId, const Player& p) const;
		void placeCastle(unsigned int nodeId, Player& p);

		bool canPlaceBuilding(unsigned int nodeId, const Player& player);
		bool canPlaceRoad(unsigned int facetId, Player* p) const;
		void placeRoad(unsigned int id, Player* p);

		std::set<Gex*> getGexsByNodeId(unsigned int nodeId);
		std::set<Facet*> getFacetsByNodeId(unsigned int nodeId);
		bool isNodeAndFacetNeighbor(unsigned int nodeId, unsigned int facetId);

		std::array<Gex, gexs_count>& GetGexes();
		const std::array<Gex, gexs_count>& GetGexes() const;
		const std::array<Node, 54> GetNodes() const;
		const std::array<Facet, 72> GetFacets() const;

		//std::set<const Facet*> GetLongWay(
		//	const Facet* from, 
		//	const Player* player, 
		//	std::set<const Node*> ban_node = std::set<const Node*>{}, 
		//	std::set<const Facet*> already = std::set<const Facet*>{}, 
		//	size_t deep = 0)  const;
		//size_t GetRoadSize(const Player* player) const;
	};

}//namespace ivv::catan {
}//namespace ivv {