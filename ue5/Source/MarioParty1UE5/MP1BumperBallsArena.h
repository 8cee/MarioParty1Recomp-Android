#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP1BumperBallsArena.generated.h"

class AMP1BumperBallPawn;
class AStaticMeshActor;

UCLASS()
class MARIOPARTY1UE5_API AMP1BumperBallsArena : public AActor
{
    GENERATED_BODY()

public:
    AMP1BumperBallsArena();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void SetHumanInput(const FVector2D& Input) { HumanInput = Input; }

    bool IsFinished() const { return bFinished; }
    int32 GetWinnerIndex() const { return WinnerIndex; }
    float GetSecondsRemaining() const { return FMath::Max(0.0f, Duration - Elapsed); }

private:
    UPROPERTY()
    TObjectPtr<AStaticMeshActor> Platform;

    UPROPERTY()
    TArray<TObjectPtr<AMP1BumperBallPawn>> Balls;

    FVector2D HumanInput = FVector2D::ZeroVector;
    float Elapsed = 0.0f;
    float Duration = 60.0f;
    bool bFinished = false;
    int32 WinnerIndex = INDEX_NONE;

    void SpawnArena();
    void UpdateAI(float DeltaSeconds);
    void CheckEliminations();
    void FinishIfNeeded();
};
