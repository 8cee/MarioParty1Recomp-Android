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

    UFUNCTION(BlueprintCallable, Category="Mario Party")
    void RollDice();

    UFUNCTION(BlueprintPure, Category="Mario Party")
    int32 GetCurrentPlayer() const { return CurrentPlayer; }

    UFUNCTION(BlueprintPure, Category="Mario Party")
    const TArray<FMP1PlayerState>& GetPlayers() const { return Players; }

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
    bool bMoving = false;

    void SpawnBoardAndPlayers();
    void MoveCurrentPlayer(int32 Steps);
    void ResolveLanding(FMP1PlayerState& Player);
    void NextTurn();
    void BindInput();
};
