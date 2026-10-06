#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MP1BumperBallPawn.generated.h"

class UStaticMeshComponent;

UCLASS()
class MARIOPARTY1UE5_API AMP1BumperBallPawn : public APawn
{
    GENERATED_BODY()

public:
    AMP1BumperBallPawn();

    void SetPlayerIndex(int32 InIndex) { PlayerIndex = InIndex; }
    int32 GetPlayerIndex() const { return PlayerIndex; }

    void ApplyMoveInput(const FVector2D& Input, float Strength = 180000.0f);
    bool IsEliminated() const { return bEliminated; }
    void MarkEliminated();

private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> BallMesh;

    int32 PlayerIndex = 0;
    bool bEliminated = false;
};
