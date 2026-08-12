// The standalone rules engine remains owned by the repository-level CMake
// target. UnrealBuildTool compiles the same implementation in this module so
// no platform-specific prebuilt libraries are required.
#include "catan.cpp"
#include "development_card_deck.cpp"
#include "dice.cpp"
#include "exception.cpp"
#include "facet.cpp"
#include "game_controller.cpp"
#include "gex.cpp"
#include "map.cpp"
#include "node.cpp"
#include "player.cpp"
