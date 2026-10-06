#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MP1BoardTypes.h"
#include "MP1GameMode.generated.h"

class AMP1BoardActor;
class AMP1PlayerPawn;
class ACameraActor;

UCLASS()
class MARIOPARTY1UE5_API AMP1GameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMP1GameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category="Mario Party")
    void RollDice();

    UFUNCTION(BlueprintPure, Category="Mario Party")
    int32 GetCurrentPlayer() const { return CurrentPlayer; }

    const TArray<FMP1PlayerState>& GetPlayers() const { return Players; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    int32 GetRound() const { return Round; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    int32 GetMaxRounds() const { return MaxRounds; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    FString GetStatusText() const { return StatusText; }

private:
    UPROPERTY()
    TObjectPtr<AMP1BoardActor> Board;

    UPROPERTY()
    TArray<TObjectPtr<AMP1PlayerPawn>> Pawns;

    UPROPERTY()
    TObjectPtr<ACameraActor> BoardCamera;

    UPROPERTY()
    TArray<FMP1PlayerState> Players;

    int32 CurrentPlayer = 0;
    int32 Round = 1;
    int32 MaxRounds = 20;
    bool bMoving = false;
    bool bGameComplete = false;
    int32 PendingSteps = 0;
    float MoveAccumulator = 0.0f;
    float MoveStepInterval = 0.22f;
    FString StatusText;

    void SpawnBoardAndPlayers();
    void SpawnEnvironment();
    void BeginMoveCurrentPlayer(int32 Steps);
    void AdvanceMovementOneSpace();
    void FinishMovement();
    void ResolveLanding(FMP1PlayerState& Player);
    void NextTurn();
    void BindInput();
};
