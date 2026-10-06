#pragma once

#include "CoreMinimal.h"
#include "MP1BoardTypes.generated.h"

UENUM(BlueprintType)
enum class EMP1SpaceType : uint8
{
    Blue,
    Red,
    Happening,
    Chance,
    Minigame,
    Mushroom,
    Bowser,
    Star,
    Neutral
};

USTRUCT(BlueprintType)
struct FMP1BoardSpaceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Index = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMP1SpaceType Type = EMP1SpaceType::Blue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> Next;
};

USTRUCT(BlueprintType)
struct FMP1PlayerState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 PlayerIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 SpaceIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Coins = 10;

    UPROPERTY(BlueprintReadOnly)
    int32 Stars = 0;
};
