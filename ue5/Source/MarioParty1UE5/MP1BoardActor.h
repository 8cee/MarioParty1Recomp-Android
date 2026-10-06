#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP1BoardTypes.h"
#include "MP1BoardActor.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;

UCLASS()
class MARIOPARTY1UE5_API AMP1BoardActor : public AActor
{
    GENERATED_BODY()

public:
    AMP1BoardActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable)
    void BuildPrototypeBoard();

    UFUNCTION(BlueprintCallable)
    bool LoadBoardJson(const FString& JsonPath);

    UFUNCTION(BlueprintCallable)
    bool LoadBoardFromRom(const FString& RomPath, int32 BoardFile = 0x45);

    UFUNCTION(BlueprintPure)
    bool GetSpace(int32 Index, FMP1BoardSpaceData& OutSpace) const;

    UFUNCTION(BlueprintPure)
    int32 GetNextSpace(int32 Index) const;

    UFUNCTION(BlueprintPure)
    FVector GetSpaceLocation(int32 Index) const;

    const TArray<FMP1BoardSpaceData>& GetSpaces() const { return Spaces; }

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> SpaceMeshes;

    UPROPERTY(EditAnywhere, Category="Mario Party|Board")
    TArray<FMP1BoardSpaceData> Spaces;

    void RebuildInstances();
};
