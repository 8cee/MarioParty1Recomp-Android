#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MP1BoardTypes.h"
#include "MP1GameMode.generated.h"

class AMP1BoardActor;
class AMP1PlayerPawn;
class ACameraActor;
class AMP1BumperBallsArena;

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

    UFUNCTION(BlueprintCallable, Category="Mario Party")
    void SelectBranchLeft();

    UFUNCTION(BlueprintCallable, Category="Mario Party")
    void SelectBranchRight();

    UFUNCTION(BlueprintCallable, Category="Mario Party")
    void ConfirmBranch();

    UFUNCTION(BlueprintPure, Category="Mario Party")
    int32 GetCurrentPlayer() const { return CurrentPlayer; }

    const TArray<FMP1PlayerState>& GetPlayers() const { return Players; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    int32 GetRound() const { return Round; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    int32 GetMaxRounds() const { return MaxRounds; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    FString GetStatusText() const { return StatusText; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    bool IsInMinigame() const { return bInMinigame; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    float GetMinigameSecondsRemaining() const;

    UFUNCTION(BlueprintPure, Category="Mario Party")
    int32 GetWinnerIndex() const { return WinnerIndex; }

private:
    UPROPERTY()
    TObjectPtr<AMP1BoardActor> Board;

    UPROPERTY()
    TArray<TObjectPtr<AMP1PlayerPawn>> Pawns;

    UPROPERTY()
    TObjectPtr<ACameraActor> BoardCamera;

    UPROPERTY()
    TArray<FMP1PlayerState> Players;

    UPROPERTY()
    TObjectPtr<AMP1BumperBallsArena> BumperBallsArena;

    int32 CurrentPlayer = 0;
    int32 Round = 1;
    int32 MaxRounds = 20;
    bool bMoving = false;
    bool bAwaitingBranch = false;
    bool bGameComplete = false;
    bool bInMinigame = false;
    int32 PendingSteps = 0;
    float MoveAccumulator = 0.0f;
    float MoveStepInterval = 0.22f;
    int32 BranchChoiceIndex = 0;
    int32 ForcedNextSpace = INDEX_NONE;
    TArray<int32> BranchOptions;
    FString StatusText;
    int32 WinnerIndex = INDEX_NONE;

    void SpawnBoardAndPlayers();
    void SpawnEnvironment();
    void BeginMoveCurrentPlayer(int32 Steps);
    void AdvanceMovementOneSpace();
    void FinishMovement();
    void ResolveLanding(FMP1PlayerState& Player);
    void NextTurn();
    void StartBumperBalls();
    void UpdateBumperBalls();
    void FinishBumperBalls();
    void FinishGame();
    void BindInput();
};
