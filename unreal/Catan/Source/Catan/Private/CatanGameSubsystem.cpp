#include "CatanGameSubsystem.h"

#include "game_controller.hpp"

#include <sstream>

namespace
{
ECatanResource ToViewResource(ivv::catan::Resurse Resource)
{
    using ivv::catan::Resurse;
    switch (Resource)
    {
    case Resurse::Wood: return ECatanResource::Wood;
    case Resurse::Clay: return ECatanResource::Clay;
    case Resurse::Hay: return ECatanResource::Hay;
    case Resurse::Sheep: return ECatanResource::Sheep;
    case Resurse::Stone: return ECatanResource::Stone;
    case Resurse::Not: return ECatanResource::Desert;
    }
    return ECatanResource::Desert;
}
}

UCatanGameSubsystem::~UCatanGameSubsystem() = default;

void UCatanGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StartLocalGame(TArray<FString>{TEXT("Player 1"), TEXT("Player 2")});
}

void UCatanGameSubsystem::Deinitialize()
{
    Game.reset();
    Super::Deinitialize();
}

void UCatanGameSubsystem::StartLocalGame(const TArray<FString>& PlayerNames)
{
    std::vector<std::string> Names;
    Names.reserve(PlayerNames.Num());
    for (const FString& Name : PlayerNames)
    {
        Names.emplace_back(TCHAR_TO_UTF8(*Name));
    }

    if (Names.size() < 2)
    {
        Names = {"Player 1", "Player 2"};
    }
    Game = std::make_unique<ivv::catan::GameController>(std::move(Names));
}

FCatanGameView UCatanGameSubsystem::GetSnapshot() const
{
    FCatanGameView View;
    if (!Game) return View;

    View.CurrentPlayer = UTF8_TO_TCHAR(Game->GetCurrentPlayer().c_str());
    std::ostringstream Step;
    Game->PrintStep(Step);
    View.Step = UTF8_TO_TCHAR(Step.str().c_str());

    const auto& Hexes = Game->GetMap().GetGexes();
    View.Hexes.Reserve(static_cast<int32>(Hexes.size()));
    for (int32 Index = 0; Index < static_cast<int32>(Hexes.size()); ++Index)
    {
        FCatanHexView& Hex = View.Hexes.Emplace_GetRef();
        Hex.Id = Index;
        Hex.Resource = ToViewResource(Hexes[Index].getType());
        Hex.Dice = Hexes[Index].getDice();
        Hex.bHasRobber = Hexes[Index].isBandit();
    }

    const auto Nodes = Game->GetMap().GetNodes();
    View.Nodes.Reserve(static_cast<int32>(Nodes.size()));
    for (int32 Index = 0; Index < static_cast<int32>(Nodes.size()); ++Index)
    {
        FCatanNodeView& Node = View.Nodes.Emplace_GetRef();
        Node.Id = Index;
        if (const ivv::catan::Building* Building = Nodes[Index].getBuilding())
        {
            Node.OwnerId = static_cast<int32>(Building->getPlayer()->getId());
            Node.bIsCity = !Building->canUpgrade();
        }
    }

    const auto Roads = Game->GetMap().GetFacets();
    View.Roads.Reserve(static_cast<int32>(Roads.size()));
    for (int32 Index = 0; Index < static_cast<int32>(Roads.size()); ++Index)
    {
        FCatanRoadView& Road = View.Roads.Emplace_GetRef();
        Road.Id = Index;
        if (const ivv::catan::Road* CoreRoad = Roads[Index].getRoad())
        {
            Road.OwnerId = static_cast<int32>(CoreRoad->getPlayer()->getId());
        }
    }
    return View;
}

bool UCatanGameSubsystem::TryBuildSettlement(int32 NodeId, FString& Error)
{
    if (!Game)
    {
        Error = TEXT("Game is not initialized");
        return false;
    }
    try
    {
        Game->BuildSettlement(Game->GetCurrentPlayer(), static_cast<size_t>(NodeId));
        Error.Reset();
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = UTF8_TO_TCHAR(Exception.what());
        return false;
    }
}

bool UCatanGameSubsystem::TryBuildRoad(int32 RoadId, FString& Error)
{
    if (!Game)
    {
        Error = TEXT("Game is not initialized");
        return false;
    }
    try
    {
        Game->BuildRoad(Game->GetCurrentPlayer(), static_cast<size_t>(RoadId));
        Error.Reset();
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = UTF8_TO_TCHAR(Exception.what());
        return false;
    }
}
