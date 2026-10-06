#pragma once

#include "CoreMinimal.h"

enum class EMP1RuntimeState : uint8
{
    Stopped,
    Initializing,
    Running,
    Failed
};

class MARIOPARTY1UE5_API FMP1RecompBridge
{
public:
    bool Initialize(const FString& RomPath);
    void Tick(double DeltaSeconds);
    void Shutdown();

    EMP1RuntimeState GetState() const { return State; }
    const FString& GetLastError() const { return LastError; }

private:
    EMP1RuntimeState State = EMP1RuntimeState::Stopped;
    FString LastError;
    FString ActiveRomPath;
};
