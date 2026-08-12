#include "CatanPlayerController.h"
#include "CatanGameMode.h"
#include "CatanGameSubsystem.h"
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
    DefaultMouseCursor = EMouseCursor::Hand;
}

void ACatanPlayerController::ServerSetLobbyReady_Implementation(bool bReady)
{
    if (ACatanGameMode* Mode = GetWorld()->GetAuthGameMode<ACatanGameMode>()) Mode->SetPlayerReady(this, bReady);
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

void ACatanPlayerController::BeginPlay()
{
    Super::BeginPlay();
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
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
}
