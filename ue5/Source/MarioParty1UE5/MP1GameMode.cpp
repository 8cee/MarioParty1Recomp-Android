#include "MP1GameMode.h"

#include "MP1BoardActor.h"
#include "MP1PlayerPawn.h"
#include "MP1HUD.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AMP1GameMode::AMP1GameMode()
{
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
        static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
        if (PlaneMesh.Succeeded())
        {
            Floor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh.Object);
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
        Fill->GetLightComponent()->SetAttenuationRadius(7000.0f);
    }
}

void AMP1GameMode::SpawnBoardAndPlayers()
{
    Board = GetWorld()->SpawnActor<AMP1BoardActor>(AMP1BoardActor::StaticClass(), FTransform::Identity);

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

    EnableInput(PC);
    if (!InputComponent) return;

    InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AMP1GameMode::RollDice);
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AMP1GameMode::RollDice);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AMP1GameMode::RollDice);
}

void AMP1GameMode::RollDice()
{
    if (bMoving || !Players.IsValidIndex(CurrentPlayer)) return;

    // MP1's normal die is 1-10.
    const int32 Roll = FMath::RandRange(1, 10);
    StatusText = FString::Printf(TEXT("Player %d rolled %d"), CurrentPlayer + 1, Roll);
    MoveCurrentPlayer(Roll);
}

void AMP1GameMode::MoveCurrentPlayer(int32 Steps)
{
    bMoving = true;

    FMP1PlayerState& P = Players[CurrentPlayer];
    for (int32 i = 0; i < Steps; ++i)
    {
        P.SpaceIndex = Board->GetNextSpace(P.SpaceIndex);
    }

    if (Pawns.IsValidIndex(CurrentPlayer) && Pawns[CurrentPlayer])
    {
        Pawns[CurrentPlayer]->SetBoardLocation(Board->GetSpaceLocation(P.SpaceIndex));
    }

    ResolveLanding(P);
    NextTurn();
    bMoving = false;
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
        Round++;
        if (Round > MaxRounds)
        {
            Round = MaxRounds;
            StatusText += TEXT("  - Game complete");
            return;
        }
        StatusText += FString::Printf(TEXT("  - Round %d begins"), Round);
    }
}
