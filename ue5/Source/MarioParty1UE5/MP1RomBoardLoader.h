#pragma once

#include "CoreMinimal.h"
#include "MP1BoardTypes.h"

class FMP1RomBoardLoader
{
public:
    static bool LoadBoard(const FString& RomPath, int32 BoardFile, TArray<FMP1BoardSpaceData>& OutSpaces, FString& OutError);

private:
    static bool ExtractMainFsFile(const TArray<uint8>& Rom, int32 Directory, int32 FileIndex, TArray<uint8>& Out, FString& OutError);
    static bool DecodeType1(const uint8* Data, int32 Size, int32 ExpectedSize, TArray<uint8>& Out, FString& OutError);
};
