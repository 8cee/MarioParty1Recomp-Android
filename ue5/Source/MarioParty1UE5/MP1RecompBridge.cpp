#include "MP1RecompBridge.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

bool FMP1RecompBridge::Initialize(const FString& RomPath)
{
    Shutdown();
    State = EMP1RuntimeState::Initializing;

    ActiveRomPath = FPaths::ConvertRelativePathToFull(RomPath);
    if (!IFileManager::Get().FileExists(*ActiveRomPath))
    {
        LastError = FString::Printf(TEXT("Mario Party ROM not found: %s"), *ActiveRomPath);
        State = EMP1RuntimeState::Failed;
        return false;
    }

    // Phase 1 intentionally stops before calling the recomp runtime. The next
    // integration step will attach N64ModernRuntime CPU/RSP execution here
    // while UE5 owns the window, input, audio, and rendering lifecycle.
    LastError.Reset();
    State = EMP1RuntimeState::Running;
    return true;
}

void FMP1RecompBridge::Tick(double DeltaSeconds)
{
    if (State != EMP1RuntimeState::Running)
    {
        return;
    }

    // Runtime stepping will be connected here. Keeping this boundary explicit
    // prevents RT64/SDL from creating a second Android window behind UE5.
    (void)DeltaSeconds;
}

void FMP1RecompBridge::Shutdown()
{
    ActiveRomPath.Reset();
    LastError.Reset();
    State = EMP1RuntimeState::Stopped;
}
