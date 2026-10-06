#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "MP1RecompBridge.h"
#include "MP1RuntimeSubsystem.generated.h"

UCLASS()
class MARIOPARTY1UE5_API UMP1RuntimeSubsystem final
    : public UGameInstanceSubsystem
    , public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return true; }

    UFUNCTION(BlueprintCallable, Category="Mario Party")
    bool StartMarioParty(const FString& RomPath);

    UFUNCTION(BlueprintCallable, Category="Mario Party")
    void StopMarioParty();

    UFUNCTION(BlueprintPure, Category="Mario Party")
    FString GetLastError() const { return Bridge.GetLastError(); }

private:
    FMP1RecompBridge Bridge;
};
