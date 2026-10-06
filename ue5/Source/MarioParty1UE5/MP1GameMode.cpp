#include "MP1GameMode.h"

#include "MP1BoardActor.h"
#include "MP1PlayerPawn.h"
#include "MP1HUD.h"
#include "MP1BumperBallsArena.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/UObjectGlobals.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

AMP1GameMode::AMP1GameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = nullptr;
    HUDClass = AMP1HUD::StaticClass();
}

void AMP1GameMode::BeginPlay()
{
    Super::BeginPlay();
    SpawnEnvironment();
    SpawnBoardAndPlayers();
    BindInput();
    StatusText = TEXT("Player 1: roll the dice.");
}

void AMP1GameMode::SpawnEnvironment()
{
    AStaticMeshActor* Floor = GetWorld()->SpawnActor<AStaticMeshActor>();
    if (Floor && Floor->GetStaticMeshComponent())
    {
        UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
        if (PlaneMesh)
        {
            Floor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
            Floor->SetActorScale3D(FVector(50.0f, 50.0f, 1.0f));
            Floor->SetActorLocation(FVector(0, 0, -10.0f));
        }
    }

    ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>();
    if (Sun)
    {
        Sun->SetActorRotation(FRotator(-55.0f, -35.0f, 0.0f));
        Sun->GetLightComponent()->SetIntensity(8.0f);
    }

    APointLight* Fill = GetWorld()->SpawnActor<APointLight>();
    if (Fill)
    {
        Fill->SetActorLocation(FVector(0, 0, 2500.0f));
        Fill->GetLightComponent()->SetIntensity(25000.0f);
    }
}

void AMP1GameMode::SpawnBoardAndPlayers()
{
    Board = GetWorld()->SpawnActor<AMP1BoardActor>(AMP1BoardActor::StaticClass(), FTransform::Identity);

    FString RomPath;
    if (!FParse::Value(FCommandLine::Get(), TEXT("rom="), RomPath))
    {
        const FString BesideExe = FPaths::Combine(FPaths::LaunchDir(), TEXT("marioparty.us.z64"));
        const FString SavedRom = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MP1/marioparty.us.z64"));
        if (IFileManager::Get().FileExists(*BesideExe))
        {
            RomPath = BesideExe;
        }
        else if (IFileManager::Get().FileExists(*SavedRom))
        {
            RomPath = SavedRom;
        }
    }

    if (!RomPath.IsEmpty() && Board)
    {
        if (Board->LoadBoardFromRom(RomPath, 0x45))
        {
            StatusText = TEXT("Loaded original DK's Jungle Adventure board from ROM.");
        }
    }

    Players.SetNum(4);
    Pawns.SetNum(4);
    for (int32 i = 0; i < 4; ++i)
    {
        Players[i].PlayerIndex = i;
        Players[i].Coins = 10;
        Players[i].Stars = 0;
        Players[i].SpaceIndex = 0;

        Pawns[i] = GetWorld()->SpawnActor<AMP1PlayerPawn>();
        Pawns[i]->SetPlayerIndex(i);
        Pawns[i]->SetBoardLocation(Board->GetSpaceLocation(0) + FVector(i * 45.0f, 0, 0));
    }

    BoardCamera = GetWorld()->SpawnActor<ACameraActor>();
    BoardCamera->SetActorLocation(FVector(0, -3500, 3600));
    BoardCamera->SetActorRotation(FRotator(-42.0f, 90.0f, 0.0f));
    BoardCamera->GetCameraComponent()->SetFieldOfView(55.0f);

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetViewTarget(BoardCamera);
        PC->bShowMouseCursor = false;
    }
}

void AMP1GameMode::BindInput()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    if (!PC->InputComponent) return;

    PC->InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AMP1GameMode::RollDice);
    PC->InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AMP1GameMode::RollDice);
    PC->InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AMP1GameMode::RollDice);
    PC->InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AMP1GameMode::SelectBranchLeft);
    PC->InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AMP1GameMode::SelectBranchRight);
    PC->InputComponent->BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, &AMP1GameMode::SelectBranchLeft);
    PC->InputComponent->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, &AMP1GameMode::SelectBranchRight);
}

void AMP1GameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bGameComplete) return;

    if (bInMinigame)
    {
        UpdateBumperBalls();
        return;
    }

    if (!bMoving || bAwaitingBranch) return;

    MoveAccumulator += DeltaSeconds;
    while (bMoving && MoveAccumulator >= MoveStepInterval)
    {
        MoveAccumulator -= MoveStepInterval;
        AdvanceMovementOneSpace();
    }
}

void AMP1GameMode::RollDice()
{
    if (bInMinigame) return;

    if (bAwaitingBranch)
    {
        ConfirmBranch();
        return;
    }

    if (bMoving || bGameComplete || !Players.IsValidIndex(CurrentPlayer)) return;

    // MP1's normal die is 1-10.
    const int32 Roll = FMath::RandRange(1, 10);
    StatusText = FString::Printf(TEXT("Player %d rolled %d"), CurrentPlayer + 1, Roll);
    BeginMoveCurrentPlayer(Roll);
}

void AMP1GameMode::BeginMoveCurrentPlayer(int32 Steps)
{
    bMoving = true;
    PendingSteps = Steps;
    MoveAccumulator = MoveStepInterval;
}

void AMP1GameMode::AdvanceMovementOneSpace()
{
    if (!Players.IsValidIndex(CurrentPlayer) || !Board)
    {
        FinishMovement();
        return;
    }

    FMP1PlayerState& P = Players[CurrentPlayer];

    if (ForcedNextSpace == INDEX_NONE)
    {
        BranchOptions = Board->GetNextSpaces(P.SpaceIndex);
        if (BranchOptions.Num() > 1)
        {
            bAwaitingBranch = true;
            BranchChoiceIndex = 0;
            StatusText = FString::Printf(TEXT("Player %d: choose route %d/%d with Left/Right, confirm with A/Space"),
                CurrentPlayer + 1, BranchChoiceIndex + 1, BranchOptions.Num());
            return;
        }
    }

    if (ForcedNextSpace != INDEX_NONE)
    {
        P.SpaceIndex = ForcedNextSpace;
        ForcedNextSpace = INDEX_NONE;
        BranchOptions.Reset();
    }
    else
    {
        P.SpaceIndex = Board->GetNextSpace(P.SpaceIndex);
    }

    if (Pawns.IsValidIndex(CurrentPlayer) && Pawns[CurrentPlayer])
    {
        Pawns[CurrentPlayer]->SetBoardLocation(Board->GetSpaceLocation(P.SpaceIndex));
    }

    PendingSteps--;
    if (PendingSteps <= 0)
    {
        FinishMovement();
    }
}

void AMP1GameMode::FinishMovement()
{
    if (Players.IsValidIndex(CurrentPlayer))
    {
        ResolveLanding(Players[CurrentPlayer]);
        NextTurn();
    }

    PendingSteps = 0;
    MoveAccumulator = 0.0f;
    bMoving = false;
}


void AMP1GameMode::SelectBranchLeft()
{
    if (!bAwaitingBranch || BranchOptions.IsEmpty()) return;
    BranchChoiceIndex = (BranchChoiceIndex - 1 + BranchOptions.Num()) % BranchOptions.Num();
    StatusText = FString::Printf(TEXT("Player %d: route %d/%d selected"),
        CurrentPlayer + 1, BranchChoiceIndex + 1, BranchOptions.Num());
}

void AMP1GameMode::SelectBranchRight()
{
    if (!bAwaitingBranch || BranchOptions.IsEmpty()) return;
    BranchChoiceIndex = (BranchChoiceIndex + 1) % BranchOptions.Num();
    StatusText = FString::Printf(TEXT("Player %d: route %d/%d selected"),
        CurrentPlayer + 1, BranchChoiceIndex + 1, BranchOptions.Num());
}

void AMP1GameMode::ConfirmBranch()
{
    if (!bAwaitingBranch || !BranchOptions.IsValidIndex(BranchChoiceIndex)) return;

    ForcedNextSpace = BranchOptions[BranchChoiceIndex];
    bAwaitingBranch = false;
    MoveAccumulator = 0.0f;
    AdvanceMovementOneSpace();
}

void AMP1GameMode::ResolveLanding(FMP1PlayerState& Player)
{
    FMP1BoardSpaceData Space;
    if (!Board->GetSpace(Player.SpaceIndex, Space)) return;

    switch (Space.Type)
    {
        case EMP1SpaceType::Blue:
            Player.Coins += 3;
            StatusText += TEXT("  - Blue space: +3 coins");
            break;
        case EMP1SpaceType::Red:
            Player.Coins = FMath::Max(0, Player.Coins - 3);
            StatusText += TEXT("  - Red space: -3 coins");
            break;
        case EMP1SpaceType::Star:
            if (Player.Coins >= 20)
            {
                Player.Coins -= 20;
                Player.Stars += 1;
                StatusText += TEXT("  - Star purchased for 20 coins");
            }
            else
            {
                StatusText += TEXT("  - Need 20 coins for the Star");
            }
            break;
        default:
            break;
    }
}

void AMP1GameMode::NextTurn()
{
    CurrentPlayer++;
    if (CurrentPlayer >= Players.Num())
    {
        CurrentPlayer = 0;
        StartBumperBalls();
    }
}

void AMP1GameMode::StartBumperBalls()
{
    bInMinigame = true;
    bMoving = false;
    bAwaitingBranch = false;
    StatusText = TEXT("BUMPER BALLS - WASD / Left Stick - knock everyone off!");

    for (AMP1PlayerPawn* Pawn : Pawns)
    {
        if (Pawn) Pawn->SetActorHiddenInGame(true);
    }
    if (Board) Board->SetActorHiddenInGame(true);

    const FVector ArenaLocation(0.0f, 0.0f, 4200.0f);
    BumperBallsArena = GetWorld()->SpawnActor<AMP1BumperBallsArena>(
        AMP1BumperBallsArena::StaticClass(), ArenaLocation, FRotator::ZeroRotator);

    if (BoardCamera)
    {
        BoardCamera->SetActorLocation(FVector(0, -3100, 5600));
        BoardCamera->SetActorRotation(FRotator(-27.0f, 90.0f, 0.0f));
    }
}

void AMP1GameMode::UpdateBumperBalls()
{
    if (!BumperBallsArena) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    FVector2D Input = FVector2D::ZeroVector;
    if (PC)
    {
        Input.X = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
        Input.Y = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);

        if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left)) Input.X -= 1.0f;
        if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) Input.X += 1.0f;
        if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down)) Input.Y -= 1.0f;
        if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up)) Input.Y += 1.0f;
    }

    BumperBallsArena->SetHumanInput(Input.GetClampedToMaxSize(1.0f));

    if (BumperBallsArena->IsFinished())
    {
        FinishBumperBalls();
    }
}

void AMP1GameMode::FinishBumperBalls()
{
    if (!BumperBallsArena) return;

    const int32 Winner = BumperBallsArena->GetWinnerIndex();
    if (Players.IsValidIndex(Winner))
    {
        Players[Winner].Coins += 10;
        StatusText = FString::Printf(TEXT("Player %d wins Bumper Balls! +10 coins"), Winner + 1);
    }
    else
    {
        StatusText = TEXT("Bumper Balls ended in a draw.");
    }

    BumperBallsArena->Destroy();
    BumperBallsArena = nullptr;
    bInMinigame = false;

    if (Board) Board->SetActorHiddenInGame(false);
    for (AMP1PlayerPawn* Pawn : Pawns)
    {
        if (Pawn) Pawn->SetActorHiddenInGame(false);
    }

    if (BoardCamera)
    {
        BoardCamera->SetActorLocation(FVector(0, -3500, 3600));
        BoardCamera->SetActorRotation(FRotator(-42.0f, 90.0f, 0.0f));
    }

    Round++;
    if (Round > MaxRounds)
    {
        Round = MaxRounds;
        bGameComplete = true;
        StatusText += TEXT(" - Game complete");
        return;
    }

    StatusText += FString::Printf(TEXT(" - Round %d begins"), Round);
}
