#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MP1PlayerPawn.generated.h"

class UStaticMeshComponent;

UCLASS()
class MARIOPARTY1UE5_API AMP1PlayerPawn : public APawn
{
    GENERATED_BODY()

public:
    AMP1PlayerPawn();

    void SetBoardLocation(const FVector& Location);
    int32 GetPlayerIndex() const { return PlayerIndex; }
    void SetPlayerIndex(int32 InIndex) { PlayerIndex = InIndex; }

private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Mesh;

    int32 PlayerIndex = 0;
};
