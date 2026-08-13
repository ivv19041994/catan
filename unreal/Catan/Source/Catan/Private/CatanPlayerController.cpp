#include "CatanPlayerController.h"
#include "CatanGameMode.h"
#include "CatanGameSubsystem.h"
#include "CatanNetworkSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCatanNetworkController, Log, All);

ACatanPlayerController::ACatanPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    bEnableTouchEvents = true;
    bEnableTouchOverEvents = true;
    DefaultMouseCursor = EMouseCursor::Hand;
}

void ACatanPlayerController::ServerSetLobbyReady_Implementation(bool bReady)
{
    if (ACatanGameMode* Mode = GetWorld()->GetAuthGameMode<ACatanGameMode>()) Mode->SetPlayerReady(this, bReady);
}

void ACatanPlayerController::ServerSetDisplayName_Implementation(const FString& PlayerName)
{
    if (ACatanGameMode* Mode = GetWorld()->GetAuthGameMode<ACatanGameMode>())
        Mode->SetPlayerDisplayName(this, PlayerName);
}

void ACatanPlayerController::ServerStartLobbyGame_Implementation()
{
    if (ACatanGameMode* Mode = GetWorld()->GetAuthGameMode<ACatanGameMode>()) Mode->StartLobbyGame(this);
}

void ACatanPlayerController::ServerExecuteCatanCommand_Implementation(ECatanServerCommand Command,
    int32 First, int32 Second, const FString& Text, const FCatanResourceView& FirstResources,
    const FCatanResourceView& SecondResources)
{
    UCatanGameSubsystem* Proxy = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    const FString AuthenticatedName = PlayerState ? PlayerState->GetPlayerName() : FString();
    const FCatanGameView View = Proxy->GetSnapshot();
    const bool bTradeResponse = Command == ECatanServerCommand::AcceptTrade || Command == ECatanServerCommand::CancelTrade;
    FString Error;
    bool bSuccess = false;
    if (!bTradeResponse && View.CurrentPlayer != AuthenticatedName)
        Error = TEXT("It is not your turn");
    else switch (Command)
    {
    case ECatanServerCommand::BuildSettlement: bSuccess = Proxy->TryBuildSettlement(First, Error); break;
    case ECatanServerCommand::BuildRoad: bSuccess = Proxy->TryBuildRoad(First, Error); break;
    case ECatanServerCommand::BuildCity: bSuccess = Proxy->TryBuildCity(First, Error); break;
    case ECatanServerCommand::MoveRobber: bSuccess = Proxy->TryMoveRobber(First, Error); break;
    case ECatanServerCommand::ChooseRobberVictim: bSuccess = Proxy->TryChooseRobberVictim(Text, Error); break;
    case ECatanServerCommand::DropResources: bSuccess = Proxy->TryDropResources(FirstResources, Error); break;
    case ECatanServerCommand::RollDice: bSuccess = Proxy->TryRollDice(Error); break;
    case ECatanServerCommand::BuyDevelopmentCard: bSuccess = Proxy->TryBuyDevelopmentCard(Error); break;
    case ECatanServerCommand::Pass: bSuccess = Proxy->TryPass(Error); break;
    case ECatanServerCommand::UseDevelopmentCard: bSuccess = Proxy->TryUseDevelopmentCard(
        static_cast<ECatanDevelopmentCard>(First), static_cast<ECatanResource>(Second),
        static_cast<ECatanResource>(FCString::Atoi(*Text)), Error); break;
    case ECatanServerCommand::BankTrade: bSuccess = Proxy->TryBankTrade(
        static_cast<ECatanResource>(First), static_cast<ECatanResource>(Second), Error); break;
    case ECatanServerCommand::OfferTrade: bSuccess = Proxy->TryOfferTrade(FirstResources, SecondResources, Error); break;
    case ECatanServerCommand::AcceptTrade: bSuccess = Proxy->TryAcceptTrade(AuthenticatedName, Error); break;
    case ECatanServerCommand::CancelTrade: bSuccess = Proxy->TryCancelTrade(AuthenticatedName, Error); break;
    case ECatanServerCommand::SelectBoardAction:
        Proxy->SelectBoardAction(static_cast<ECatanBoardAction>(First)); bSuccess = true; break;
    }
    ClientCatanCommandResult(bSuccess, bSuccess ? TEXT("Command accepted") : Error);
    UE_LOG(LogCatanNetworkController, Display, TEXT("Authenticated command player=%s command=%d success=%d"),
        *AuthenticatedName, static_cast<int32>(Command), bSuccess);
}

void ACatanPlayerController::ClientCatanCommandResult_Implementation(bool bSuccess, const FString& Message)
{
    UE_LOG(LogTemp, Log, TEXT("Catan command %s: %s"), bSuccess ? TEXT("accepted") : TEXT("rejected"), *Message);
}

void ACatanPlayerController::RunAutomatedSetupStep()
{
    if (!IsLocalController()) return;
    UCatanGameSubsystem* Proxy = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Proxy) return;
    const FCatanGameView View = Proxy->GetSnapshot();
    if (View.Phase == ECatanGamePhase::RollDice)
    {
        if (LastAutomatedSetupKey != TEXT("complete"))
        {
            LastAutomatedSetupKey = TEXT("complete");
            UE_LOG(LogCatanNetworkController, Display, TEXT("CATAN_SETUP_E2E complete player=%s"),
                PlayerState ? *PlayerState->GetPlayerName() : TEXT("unknown"));
        }
        return;
    }
    if (!Proxy->CanLocalPlayerAct(View))
    {
        LastAutomatedSetupKey.Reset();
        return;
    }
    if (View.Phase != ECatanGamePhase::SetupSettlement && View.Phase != ECatanGamePhase::SetupRoad) return;
    const FString Key = FString::Printf(TEXT("%s:%d"), *View.CurrentPlayer, static_cast<int32>(View.Phase));
    if (Key == LastAutomatedSetupKey) return;
    FString Error;
    bool bSent = false;
    int32 Target = INDEX_NONE;
    if (View.Phase == ECatanGamePhase::SetupSettlement && !View.ValidNodeTargets.IsEmpty())
    {
        Target = View.ValidNodeTargets[0];
        bSent = Proxy->TryBuildSettlement(Target, Error);
    }
    else if (View.Phase == ECatanGamePhase::SetupRoad && !View.ValidRoadTargets.IsEmpty())
    {
        Target = View.ValidRoadTargets[0];
        bSent = Proxy->TryBuildRoad(Target, Error);
    }
    if (bSent)
    {
        LastAutomatedSetupKey = Key;
        UE_LOG(LogCatanNetworkController, Display,
            TEXT("CATAN_SETUP_E2E sent player=%s phase=%d target=%d"),
            *View.CurrentPlayer, static_cast<int32>(View.Phase), Target);
    }
}

void ACatanPlayerController::RunMultiplayerE2EStep()
{
    if (!IsLocalController() || !GetWorld() || GetWorld()->GetNetMode() == NM_Standalone) return;
    UCatanGameSubsystem* Proxy = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Proxy) return;
    const FCatanGameView View = Proxy->GetSnapshot();
    if (!bReconnectSnapshotReported
        && FParse::Param(FCommandLine::Get(), TEXT("CatanExpectReconnect")))
    {
        const FCatanPlayerView* LocalPlayer = View.Players.FindByPredicate(
            [](const FCatanPlayerView& Item) { return Item.bIsLocalPlayer; });
        if (LocalPlayer && LocalPlayer->bResourcesVisible && !View.CurrentPlayer.IsEmpty())
        {
            bReconnectSnapshotReported = true;
            UE_LOG(LogCatanNetworkController, Display,
                TEXT("CATAN_MP_E2E reconnect snapshot player=%s current=%s phase=%d resources=%d"),
                *LocalPlayer->Name, *View.CurrentPlayer, static_cast<int32>(View.Phase),
                LocalPlayer->ResourceCards);
        }
    }
    if (!Proxy->CanLocalPlayerAct(View)) return;

    const FString LocalName = PlayerState ? PlayerState->GetPlayerName() : FString();
    const FString Key = FString::Printf(TEXT("%s:%d:%d:%d:%d:%d"), *View.CurrentPlayer,
        static_cast<int32>(View.Phase), View.FirstDie, View.SecondDie,
        View.PendingRobberHex, View.EventLog.Num());
    if (Key == LastAutomatedSetupKey) return;

    FString Error;
    bool bSent = false;
    FString Action;
    if (View.PendingRobberHex != INDEX_NONE && !View.RobberVictims.IsEmpty())
    {
        Action = TEXT("robber-victim");
        bSent = Proxy->TryChooseRobberVictim(View.RobberVictims[0], Error);
    }
    else switch (View.Phase)
    {
    case ECatanGamePhase::SetupSettlement:
        if (!View.ValidNodeTargets.IsEmpty())
        {
            Action = TEXT("setup-settlement");
            bSent = Proxy->TryBuildSettlement(View.ValidNodeTargets[0], Error);
        }
        break;
    case ECatanGamePhase::SetupRoad:
    case ECatanGamePhase::RoadBuilding:
        if (!View.ValidRoadTargets.IsEmpty())
        {
            Action = View.Phase == ECatanGamePhase::SetupRoad ? TEXT("setup-road") : TEXT("free-road");
            bSent = Proxy->TryBuildRoad(View.ValidRoadTargets[0], Error);
        }
        else if (View.Phase == ECatanGamePhase::RoadBuilding)
        {
            Action = TEXT("skip-free-road");
            bSent = Proxy->TryPass(Error);
        }
        break;
    case ECatanGamePhase::RollDice:
        Action = TEXT("roll");
        bSent = Proxy->TryRollDice(Error);
        break;
    case ECatanGamePhase::DropCards:
        if (const FCatanPlayerView* Player = View.Players.FindByPredicate(
            [&View](const FCatanPlayerView& Item) { return Item.Name == View.CurrentPlayer; }))
        {
            FCatanResourceView Drop;
            int32 Remaining = View.RequiredDiscardCount;
            const int32 Holdings[] = {Player->Resources.Wood, Player->Resources.Clay, Player->Resources.Hay,
                Player->Resources.Sheep, Player->Resources.Stone};
            int32* Counts[] = {&Drop.Wood, &Drop.Clay, &Drop.Hay, &Drop.Sheep, &Drop.Stone};
            for (int32 Resource = 0; Resource < 5 && Remaining > 0; ++Resource)
            {
                *Counts[Resource] = FMath::Min(Holdings[Resource], Remaining);
                Remaining -= *Counts[Resource];
            }
            if (Remaining == 0)
            {
                Action = TEXT("discard");
                bSent = Proxy->TryDropResources(Drop, Error);
            }
        }
        break;
    case ECatanGamePhase::MoveRobber:
        if (!View.ValidHexTargets.IsEmpty())
        {
            Action = TEXT("move-robber");
            bSent = Proxy->TryMoveRobber(View.ValidHexTargets[0], Error);
        }
        break;
    case ECatanGamePhase::CommonPlay:
        Action = TEXT("pass");
        bSent = Proxy->TryPass(Error);
        break;
    case ECatanGamePhase::Finished:
        break;
    }
    if (!bSent) return;
    LastAutomatedSetupKey = Key;
    UE_LOG(LogCatanNetworkController, Display,
        TEXT("CATAN_MP_E2E action player=%s action=%s phase=%d"),
        *LocalName, *Action, static_cast<int32>(View.Phase));
}

void ACatanPlayerController::BeginPlay()
{
    Super::BeginPlay();
    ActivateTouchInterface(nullptr);
    SetVirtualJoystickVisibility(false);
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    if (IsLocalController())
    {
        FTimerHandle IdentityHandle;
        GetWorldTimerManager().SetTimer(IdentityHandle, [this]
        {
            if (const UCatanNetworkSubsystem* Network = GetGameInstance()->GetSubsystem<UCatanNetworkSubsystem>())
                if (!Network->GetPendingPlayerName().IsEmpty())
                    ServerSetDisplayName(Network->GetPendingPlayerName());
        }, 1.0f, false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("CatanAutoReady")))
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, [this]
        {
            UE_LOG(LogCatanNetworkController, Display, TEXT("CATAN_SMOKE ready rpc: %s"),
                PlayerState ? *PlayerState->GetPlayerName() : TEXT("unknown"));
            ServerSetLobbyReady(true);
        }, 2.0f, false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("CatanAutoSetup")))
    {
        FTimerHandle SetupHandle;
        GetWorldTimerManager().SetTimer(SetupHandle, this,
            &ACatanPlayerController::RunAutomatedSetupStep, 0.25f, true, 2.5f);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("CatanMultiplayerE2E")))
    {
        FTimerHandle MultiplayerHandle;
        GetWorldTimerManager().SetTimer(MultiplayerHandle, this,
            &ACatanPlayerController::RunMultiplayerE2EStep, 0.35f, true, 2.5f);
    }
}
