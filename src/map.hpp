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

		void initGexs();
		bool badGexConfiguration();
		void initRandomTypeForGexs();
		void initPorts();


	public:

		Map();

		void diceEvent(std::pair<unsigned int, unsigned int> d);
		void diceEvent(size_t val);

		bool canPlaceStartBuilding(size_t nodeId);
		void placeStartBuilding(size_t nodeId, Player* p);
		void placeSettlement(size_t nodeId, Player* p);

		bool canPlaceCastle(size_t nodeId, const Player& p) const;
		void placeCastle(size_t nodeId, Player& p);

		bool canPlaceBuilding(size_t nodeId, const Player& player);
		bool canPlaceRoad(size_t facetId, Player* p) const;
		void placeRoad(size_t id, Player* p);

		std::set<Gex*> getGexsByNodeId(size_t nodeId);
		std::set<Facet*> getFacetsByNodeId(size_t nodeId);
		bool isNodeAndFacetNeighbor(size_t nodeId, size_t facetId);

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