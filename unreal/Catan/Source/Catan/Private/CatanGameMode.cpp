#include "CatanGameMode.h"

#include "CatanBoardActor.h"
#include "CatanCameraPawn.h"
#include "CatanHUD.h"
#include "CatanMenuBackdropActor.h"
#include "CatanGameState.h"
#include "CatanPlayerState.h"
#include "CatanGameSubsystem.h"
#include "CatanNetworkSubsystem.h"
#include "CatanPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameSession.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogCatanNetworkMode, Log, All);

ACatanGameMode::ACatanGameMode()
{
    DefaultPawnClass = ACatanCameraPawn::StaticClass();
    PlayerControllerClass = ACatanPlayerController::StaticClass();
    PlayerStateClass = ACatanPlayerState::StaticClass();
    GameStateClass = ACatanGameState::StaticClass();
    HUDClass = ACatanHUD::StaticClass();
    GameSessionClass = AGameSession::StaticClass();
}

const FString* ACatanGameMode::FindExpectedPlayerName(const FString& Name) const
{
    return ExpectedPlayerNames.FindByPredicate([&Name](const FString& Expected)
    {
        return Expected.Equals(Name.TrimStartAndEnd(), ESearchCase::IgnoreCase);
    });
}

bool ACatanGameMode::AreAllExpectedPlayersConnected() const
{
    if (!bRestoredLobby || ExpectedPlayerNames.IsEmpty() || !GameState) return false;
    if (GameState->PlayerArray.Num() != ExpectedPlayerNames.Num()) return false;
    for (const FString& Expected : ExpectedPlayerNames)
        if (!GameState->PlayerArray.ContainsByPredicate([&Expected](const APlayerState* State)
            { return State && State->GetPlayerName().Equals(Expected, ESearchCase::IgnoreCase); }))
            return false;
    return true;
}

void ACatanGameMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty() || !bRestoredLobby) return;
    FString Requested = UGameplayStatics::ParseOption(Options, TEXT("CatanName")).TrimStartAndEnd();
    if (Requested.IsEmpty()) Requested = UGameplayStatics::ParseOption(Options, TEXT("Name")).TrimStartAndEnd();
    const FString* Expected = FindExpectedPlayerName(Requested);
    if (!Expected)
    {
        ErrorMessage = TEXT("This player name is not part of the saved game");
        return;
    }
    if (GameState && GameState->PlayerArray.ContainsByPredicate([Expected](const APlayerState* State)
        { return State && State->GetPlayerName().Equals(*Expected, ESearchCase::IgnoreCase); }))
        ErrorMessage = TEXT("This saved player is already connected");
}

FString ACatanGameMode::InitNewPlayer(APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
    if (!Error.IsEmpty()) return Error;
    FString Requested = UGameplayStatics::ParseOption(Options, TEXT("CatanName")).TrimStartAndEnd();
    if (Requested.IsEmpty())
        Requested = UGameplayStatics::ParseOption(Options, TEXT("Name")).TrimStartAndEnd();
    if (Requested.IsEmpty()) Requested = FString::Printf(TEXT("Player %d"), GameState->PlayerArray.Num());
    Requested = Requested.Left(24);
    if (bRestoredLobby)
    {
        const FString* Expected = FindExpectedPlayerName(Requested);
        if (!Expected) return TEXT("This player name is not part of the saved game");
        const bool bAlreadyConnected = GameState->PlayerArray.ContainsByPredicate(
            [NewPlayerController, Expected](const APlayerState* State)
            {
                return State != NewPlayerController->PlayerState
                    && State->GetPlayerName().Equals(*Expected, ESearchCase::IgnoreCase);
            });
        if (bAlreadyConnected) return TEXT("This saved player is already connected");
        Requested = *Expected;
    }
    FString Unique = Requested;
    int32 Suffix = 2;
    auto Exists = [this, NewPlayerController](const FString& Candidate)
    {
        for (APlayerState* State : GameState->PlayerArray)
            if (State != NewPlayerController->PlayerState && State->GetPlayerName().Equals(Candidate, ESearchCase::IgnoreCase))
                return true;
        return false;
    };
    while (Exists(Unique)) Unique = FString::Printf(TEXT("%s %d"), *Requested, Suffix++);
    NewPlayerController->PlayerState->SetPlayerName(Unique);
    if (ACatanPlayerState* State = NewPlayerController->GetPlayerState<ACatanPlayerState>())
        State->DisplayName = Unique;
    if (ACatanPlayerController* CatanController = Cast<ACatanPlayerController>(NewPlayerController))
        CatanController->RequestedPlayerName = Unique;
    return FString();
}

void ACatanGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (GetNetMode() == NM_Standalone) return;
    if (ACatanPlayerController* CatanController = Cast<ACatanPlayerController>(NewPlayer))
        if (!CatanController->RequestedPlayerName.IsEmpty() && NewPlayer->PlayerState)
        {
            NewPlayer->PlayerState->SetPlayerName(CatanController->RequestedPlayerName);
            if (ACatanPlayerState* State = NewPlayer->GetPlayerState<ACatanPlayerState>())
                State->DisplayName = CatanController->RequestedPlayerName;
        }
    UE_LOG(LogCatanNetworkMode, Display, TEXT("CATAN_E2E post login player=%s total=%d"),
        NewPlayer && NewPlayer->PlayerState ? *NewPlayer->PlayerState->GetPlayerName() : TEXT("unknown"),
        GameState ? GameState->PlayerArray.Num() : 0);
    if (ACatanPlayerState* State = NewPlayer->GetPlayerState<ACatanPlayerState>())
    {
        State->bLobbyHost = GameState->PlayerArray.Num() == 1;
        State->bLobbyReady = false;
    }
    PublishLobby();
    if (bLobbyGameStarted)
        GetGameInstance()->GetSubsystem<UCatanGameSubsystem>()->PublishAuthoritativeState();
}

void ACatanGameMode::Logout(AController* Exiting)
{
    const bool bWasHost = Exiting && Exiting->PlayerState
        && Cast<ACatanPlayerState>(Exiting->PlayerState)->bLobbyHost;
    Super::Logout(Exiting);
    if (GetNetMode() == NM_Standalone) return;
    if (bWasHost && !GameState->PlayerArray.IsEmpty())
        if (ACatanPlayerState* State = Cast<ACatanPlayerState>(GameState->PlayerArray[0])) State->bLobbyHost = true;
    PublishLobby();
    UE_LOG(LogCatanNetworkMode, Display, TEXT("CATAN_HUD_GRAPH logout total=%d"),
        GameState ? GameState->PlayerArray.Num() : 0);
}

void ACatanGameMode::SetPlayerReady(APlayerController* Player, bool bReady)
{
    if (bLobbyGameStarted || !Player) return;
    if (ACatanPlayerState* State = Player->GetPlayerState<ACatanPlayerState>()) State->bLobbyReady = bReady;
    PublishLobby();
}

void ACatanGameMode::SetPlayerDisplayName(APlayerController* Player, const FString& RequestedName)
{
    if (!Player) return;
    FString Base = RequestedName.TrimStartAndEnd().Left(24);
    Base.ReplaceInline(TEXT("\t"), TEXT(" "));
    Base.ReplaceInline(TEXT("\n"), TEXT(" "));
    if (Base.IsEmpty()) Base = TEXT("Player");
    if (bRestoredLobby && !bLobbyGameStarted)
    {
        const FString* Expected = FindExpectedPlayerName(Base);
        if (!Expected) return;
        const bool bAlreadyConnected = GameState->PlayerArray.ContainsByPredicate(
            [Player, Expected](const APlayerState* State)
            {
                return State != Player->PlayerState
                    && State->GetPlayerName().Equals(*Expected, ESearchCase::IgnoreCase);
            });
        if (bAlreadyConnected) return;
        Base = *Expected;
    }
    if (bLobbyGameStarted)
    {
        const FCatanGameView View = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>()->GetSnapshot();
        const bool bKnownPlayer = View.Players.ContainsByPredicate(
            [&Base](const FCatanPlayerView& Item) { return Item.Name.Equals(Base, ESearchCase::IgnoreCase); });
        const bool bAlreadyConnected = GameState->PlayerArray.ContainsByPredicate(
            [Player, &Base](const APlayerState* State)
            {
                return State != Player->PlayerState
                    && State->GetPlayerName().Equals(Base, ESearchCase::IgnoreCase);
            });
        if (!bKnownPlayer || bAlreadyConnected) return;
    }
    FString Unique = Base;
    int32 Suffix = 2;
    auto Exists = [this, Player](const FString& Candidate)
    {
        for (APlayerState* State : GameState->PlayerArray)
            if (State != Player->PlayerState && State->GetPlayerName().Equals(Candidate, ESearchCase::IgnoreCase))
                return true;
        return false;
    };
    while (Exists(Unique)) Unique = FString::Printf(TEXT("%s %d"), *Base, Suffix++);
    if (ACatanPlayerState* State = Player->GetPlayerState<ACatanPlayerState>())
    {
        State->DisplayName = Unique;
        State->ForceNetUpdate();
    }
    UE_LOG(LogCatanNetworkMode, Display, TEXT("CATAN_E2E identity player=%s"), *Unique);
    PublishLobby();
    if (bLobbyGameStarted)
    {
        GetGameInstance()->GetSubsystem<UCatanGameSubsystem>()->PublishAuthoritativeState();
        UE_LOG(LogCatanNetworkMode, Display, TEXT("CATAN_MP_E2E reconnect restored player=%s"), *Unique);
    }
}

void ACatanGameMode::StartLobbyGame(APlayerController* Requester)
{
    if (bLobbyGameStarted || !Requester) return;
    const ACatanPlayerState* RequesterState = Requester->GetPlayerState<ACatanPlayerState>();
    if (!RequesterState || !RequesterState->bLobbyHost) return;
    if (GameState->PlayerArray.Num() < 2 || GameState->PlayerArray.Num() > 4) return;
    if (bRestoredLobby && !AreAllExpectedPlayersConnected()) return;
    TArray<FString> Names;
    for (APlayerState* State : GameState->PlayerArray)
    {
        const ACatanPlayerState* CatanState = Cast<ACatanPlayerState>(State);
        if (!CatanState || !CatanState->bLobbyReady) return;
        Names.Add(State->GetPlayerName());
    }
    UCatanGameSubsystem* Games = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (bRestoredLobby)
    {
        FString Error;
        if (!Games->LoadLanSavedGame(Error))
        {
            UE_LOG(LogCatanNetworkMode, Error, TEXT("CATAN_SAVE lobby restore failed: %s"), *Error);
            return;
        }
    }
    else Games->StartLocalGame(Names);
    bLobbyGameStarted = true;
    ShowGameBoard();
    if (ACatanGameState* State = GetGameState<ACatanGameState>()) State->NetworkMode = ECatanNetworkMode::Playing;
    PublishLobby();
    Games->PublishAuthoritativeState();
    UE_LOG(LogCatanNetworkMode, Display, TEXT("CATAN_SMOKE match started players=%d restored=%d"),
        Names.Num(), bRestoredLobby);
}

void ACatanGameMode::StartSinglePlayerGame(const FString& HumanName, int32 BotCount)
{
    if (GetNetMode() != NM_Standalone) return;
    ShowGameBoard();
    GetGameInstance()->GetSubsystem<UCatanGameSubsystem>()->StartBotGame(HumanName, BotCount);
    if (ACatanGameState* State = GetGameState<ACatanGameState>())
        State->NetworkMode = ECatanNetworkMode::Playing;
}

void ACatanGameMode::ShowDedicatedGameBoard()
{
    if (GetNetMode() != NM_Standalone) return;
    ShowGameBoard();
    if (ACatanGameState* State = GetGameState<ACatanGameState>())
        State->NetworkMode = ECatanNetworkMode::Playing;
}

void ACatanGameMode::ShowGameBoard()
{
    if (MenuBackdrop)
    {
        MenuBackdrop->Destroy();
        MenuBackdrop = nullptr;
    }
    if (!UGameplayStatics::GetActorOfClass(GetWorld(), ACatanBoardActor::StaticClass()))
        GetWorld()->SpawnActor<ACatanBoardActor>(ACatanBoardActor::StaticClass(),
            FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator);
}

void ACatanGameMode::PublishLobby()
{
    ACatanGameState* State = GetGameState<ACatanGameState>();
    if (!State) return;
    if (!bLobbyGameStarted) State->NetworkMode = ECatanNetworkMode::Lobby;
    State->ExpectedPlayerNames = ExpectedPlayerNames;
    State->LobbyPlayers.Reset();
    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        const ACatanPlayerState* CatanState = Cast<ACatanPlayerState>(PlayerState);
        FCatanLobbyPlayerView& View = State->LobbyPlayers.Emplace_GetRef();
        View.Name = PlayerState->GetPlayerName();
        View.PlayerId = PlayerState->GetPlayerId();
        View.bReady = CatanState && CatanState->bLobbyReady;
        View.bHost = CatanState && CatanState->bLobbyHost;
    }
    State->NotifyLocalProxy();
    State->ForceNetUpdate();
    if (bRestoredLobby && !bLobbyGameStarted)
    {
        TArray<FString> Waiting;
        for (const FString& Expected : ExpectedPlayerNames)
            if (!GameState->PlayerArray.ContainsByPredicate([&Expected](const APlayerState* PlayerState)
                { return PlayerState && PlayerState->GetPlayerName().Equals(Expected, ESearchCase::IgnoreCase); }))
                Waiting.Add(Expected);
        UE_LOG(LogCatanNetworkMode, Display,
            TEXT("CATAN_SAVE lobby expected=%d connected=%d waiting=%s"),
            ExpectedPlayerNames.Num(), State->LobbyPlayers.Num(), *FString::Join(Waiting, TEXT(",")));
    }
    int32 ExpectedPlayers = 0;
    if (!bLobbyGameStarted && FParse::Value(FCommandLine::Get(), TEXT("CatanAutoStart="), ExpectedPlayers)
        && ExpectedPlayers == State->LobbyPlayers.Num())
    {
        APlayerController* HostController = nullptr;
        bool bAllReady = true;
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* Controller = It->Get();
            const ACatanPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ACatanPlayerState>() : nullptr;
            bAllReady = bAllReady && PlayerState && PlayerState->bLobbyReady;
            if (PlayerState && PlayerState->bLobbyHost) HostController = Controller;
        }
        if (bAllReady && HostController) StartLobbyGame(HostController);
    }
}

void ACatanGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (UCatanNetworkSubsystem* Network = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCatanNetworkSubsystem>() : nullptr)
    {
        bRestoredLobby = GetNetMode() == NM_ListenServer && Network->IsHostingSavedLobby();
        if (bRestoredLobby) ExpectedPlayerNames = Network->GetSavedExpectedPlayerNames();
    }
    if (ACatanGameState* State = GetGameState<ACatanGameState>())
    {
        State->NetworkMode = GetNetMode() == NM_Standalone ? ECatanNetworkMode::MainMenu : ECatanNetworkMode::Lobby;
        State->ExpectedPlayerNames = ExpectedPlayerNames;
    }
    MenuBackdrop = GetWorld()->SpawnActor<ACatanMenuBackdropActor>(
        ACatanMenuBackdropActor::StaticClass(), FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    if (GetNetMode() == NM_Standalone)
    {
        UCatanGameSubsystem* Proxy = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
        int32 AutoBots = 0;
        if (FParse::Value(FCommandLine::Get(), TEXT("CatanAutoBots="), AutoBots) && AutoBots > 0)
        {
            FString PlayerName = TEXT("Player");
            FParse::Value(FCommandLine::Get(), TEXT("CatanPlayerName="), PlayerName);
            StartSinglePlayerGame(PlayerName, AutoBots);
            UE_LOG(LogCatanNetworkMode, Display, TEXT("CATAN_BOT_E2E started human=%s bots=%d"),
                *PlayerName, FMath::Clamp(AutoBots, 1, 3));
        }
        else
        {
            Proxy->StartLocalGame(TArray<FString>{TEXT("Player 1"), TEXT("Player 2")});
        }
    }
}
