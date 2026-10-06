#include "MP1RuntimeSubsystem.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UMP1RuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FString RomPath;
    if (FParse::Value(FCommandLine::Get(), TEXT("rom="), RomPath))
    {
        StartMarioParty(RomPath);
    }
}

void UMP1RuntimeSubsystem::Deinitialize()
{
    StopMarioParty();
    Super::Deinitialize();
}

void UMP1RuntimeSubsystem::Tick(float DeltaTime)
{
    Bridge.Tick(DeltaTime);
}

TStatId UMP1RuntimeSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UMP1RuntimeSubsystem, STATGROUP_Tickables);
}

bool UMP1RuntimeSubsystem::StartMarioParty(const FString& RomPath)
{
    return Bridge.Initialize(RomPath);
}

void UMP1RuntimeSubsystem::StopMarioParty()
{
    Bridge.Shutdown();
}
