#include "MP1RomBoardLoader.h"

#include "Misc/FileHelper.h"

namespace
{
constexpr int32 MainFsStart = 0x31C7E0;
constexpr int32 MainFsEnd = 0xFCB860;

uint16 ReadBE16(const TArray<uint8>& D, int32 O)
{
    return (uint16(D[O]) << 8) | uint16(D[O + 1]);
}

int16 ReadBES16(const TArray<uint8>& D, int32 O)
{
    return static_cast<int16>(ReadBE16(D, O));
}

uint32 ReadBE32(const TArray<uint8>& D, int32 O)
{
    return (uint32(D[O]) << 24) | (uint32(D[O + 1]) << 16) | (uint32(D[O + 2]) << 8) | uint32(D[O + 3]);
}

float ReadBEFloat(const TArray<uint8>& D, int32 O)
{
    const uint32 Bits = ReadBE32(D, O);
    float Value = 0.0f;
    static_assert(sizeof(float) == sizeof(uint32));
    FMemory::Memcpy(&Value, &Bits, sizeof(float));
#if PLATFORM_LITTLE_ENDIAN
    // Bits is already reconstructed into host numeric order by ReadBE32.
#endif
    return Value;
}

bool InRange(const TArray<uint8>& D, int32 O, int32 N)
{
    return O >= 0 && N >= 0 && O <= D.Num() && N <= D.Num() - O;
}

EMP1SpaceType MapSpaceType(uint16 Raw)
{
    switch (Raw & 0xFF)
    {
        case 1: return EMP1SpaceType::Blue;
        case 2: return EMP1SpaceType::Red;
        case 3: return EMP1SpaceType::Minigame;
        case 4: return EMP1SpaceType::Happening;
        case 5: return EMP1SpaceType::Star;
        case 6: return EMP1SpaceType::Chance;
        case 8: return EMP1SpaceType::Mushroom;
        case 9: return EMP1SpaceType::Bowser;
        default: return EMP1SpaceType::Neutral;
    }
}
}

bool FMP1RomBoardLoader::DecodeType1(const uint8* Data, int32 Size, int32 ExpectedSize, TArray<uint8>& Out, FString& OutError)
{
    Out.Reset();
    Out.Reserve(ExpectedSize);

    TArray<uint8> Ring;
    Ring.SetNumZeroed(0x400);

    int32 RingPos = 0;
    int32 Pos = 0;

    while (Out.Num() < ExpectedSize)
    {
        if (Pos >= Size)
        {
            OutError = TEXT("Truncated MP1 type-1 control stream.");
            return false;
        }

        uint8 Control = Data[Pos++];
        for (int32 Bit = 0; Bit < 8 && Out.Num() < ExpectedSize; ++Bit)
        {
            if (Control & 1)
            {
                if (Pos >= Size)
                {
                    OutError = TEXT("Truncated MP1 type-1 literal.");
                    return false;
                }

                const uint8 Value = Data[Pos++];
                Out.Add(Value);
                Ring[RingPos] = Value;
                RingPos = (RingPos + 1) & 0x3FF;
            }
            else
            {
                if (Pos + 2 > Size)
                {
                    OutError = TEXT("Truncated MP1 type-1 backreference.");
                    return false;
                }

                const uint8 A = Data[Pos++];
                const uint8 B = Data[Pos++];
                const int32 ReadPos = (((B & 0xC0) << 2) | A) & 0x3FF;
                const int32 Length = (B & 0x3F) + 3;

                for (int32 N = 0; N < Length && Out.Num() < ExpectedSize; ++N)
                {
                    const uint8 Value = Ring[(ReadPos + N + 66) & 0x3FF];
                    Out.Add(Value);
                    Ring[RingPos] = Value;
                    RingPos = (RingPos + 1) & 0x3FF;
                }
            }

            Control >>= 1;
        }
    }

    return true;
}

bool FMP1RomBoardLoader::ExtractMainFsFile(const TArray<uint8>& Rom, int32 Directory, int32 FileIndex, TArray<uint8>& Out, FString& OutError)
{
    if (!InRange(Rom, MainFsStart, 8) || Rom.Num() < MainFsEnd)
    {
        OutError = TEXT("ROM is too small for the MP1 NTSC-U main filesystem.");
        return false;
    }

    const uint32 DirCount = ReadBE32(Rom, MainFsStart);
    if (Directory < 0 || Directory >= static_cast<int32>(DirCount))
    {
        OutError = TEXT("MP1 MainFS directory index is out of range.");
        return false;
    }

    const int32 DirEntry = MainFsStart + 4 + Directory * 4;
    if (!InRange(Rom, DirEntry, 4))
    {
        OutError = TEXT("MP1 MainFS directory table is truncated.");
        return false;
    }

    const int32 DirBase = MainFsStart + static_cast<int32>(ReadBE32(Rom, DirEntry));
    if (!InRange(Rom, DirBase, 4))
    {
        OutError = TEXT("MP1 MainFS directory points outside the ROM.");
        return false;
    }

    const uint32 FileCount = ReadBE32(Rom, DirBase);
    if (FileIndex < 0 || FileIndex >= static_cast<int32>(FileCount))
    {
        OutError = TEXT("MP1 board file index is out of range.");
        return false;
    }

    const int32 FileEntry = DirBase + 4 + FileIndex * 4;
    if (!InRange(Rom, FileEntry, 4))
    {
        OutError = TEXT("MP1 MainFS file table is truncated.");
        return false;
    }

    const int32 Header = DirBase + static_cast<int32>(ReadBE32(Rom, FileEntry));
    if (!InRange(Rom, Header, 8))
    {
        OutError = TEXT("MP1 MainFS file header is invalid.");
        return false;
    }

    const int32 DecodedSize = static_cast<int32>(ReadBE32(Rom, Header));
    const uint32 Compression = ReadBE32(Rom, Header + 4);
    const int32 Payload = Header + 8;

    if (DecodedSize < 0 || DecodedSize > 32 * 1024 * 1024)
    {
        OutError = TEXT("MP1 board file decoded size is invalid.");
        return false;
    }

    if (Compression == 0)
    {
        if (!InRange(Rom, Payload, DecodedSize))
        {
            OutError = TEXT("Raw MP1 board file is truncated.");
            return false;
        }
        Out = TArray<uint8>(Rom.GetData() + Payload, DecodedSize);
        return true;
    }

    if (Compression == 1)
    {
        return DecodeType1(Rom.GetData() + Payload, MainFsEnd - Payload, DecodedSize, Out, OutError);
    }

    OutError = FString::Printf(TEXT("Unsupported MP1 compression type %u."), Compression);
    return false;
}

bool FMP1RomBoardLoader::LoadBoard(const FString& RomPath, int32 BoardFile, TArray<FMP1BoardSpaceData>& OutSpaces, FString& OutError)
{
    TArray<uint8> Rom;
    if (!FFileHelper::LoadFileToArray(Rom, *RomPath))
    {
        OutError = FString::Printf(TEXT("Unable to read ROM: %s"), *RomPath);
        return false;
    }

    TArray<uint8> Board;
    if (!ExtractMainFsFile(Rom, 0x0A, BoardFile, Board, OutError))
    {
        return false;
    }

    if (!InRange(Board, 0, 12))
    {
        OutError = TEXT("MP1 board file header is truncated.");
        return false;
    }

    const int32 SpaceCount = ReadBE16(Board, 0);
    const int32 ChainCountB = ReadBE16(Board, 4);
    const int32 SpacesOffset = ReadBE16(Board, 6);
    const int32 ChainsBOffset = ReadBE16(Board, 10);

    if (SpaceCount <= 0 || SpaceCount > 1024)
    {
        OutError = TEXT("MP1 board space count is invalid.");
        return false;
    }

    TArray<FMP1BoardSpaceData> Parsed;
    Parsed.Reserve(SpaceCount);

    int32 Pos = SpacesOffset;
    for (int32 I = 0; I < SpaceCount; ++I)
    {
        if (!InRange(Board, Pos, 16))
        {
            OutError = TEXT("MP1 board space table is truncated.");
            return false;
        }

        FMP1BoardSpaceData S;
        S.Index = I;
        S.Type = MapSpaceType(ReadBE16(Board, Pos + 2));

        const float X = ReadBEFloat(Board, Pos + 4) * 5.0f;
        const float Y = ReadBEFloat(Board, Pos + 8) * 5.0f;
        const float Z = ReadBEFloat(Board, Pos + 12) * 5.0f;

        // N64 board data is Y-up in practice for the original camera/model
        // conventions. UE is Z-up, so remap into X,Z,Y.
        S.Location = FVector(X, Z, Y);
        Parsed.Add(S);
        Pos += 16;
    }

    if (!InRange(Board, ChainsBOffset, ChainCountB * 2))
    {
        OutError = TEXT("MP1 board chain table is truncated.");
        return false;
    }

    for (int32 ChainIndex = 0; ChainIndex < ChainCountB; ++ChainIndex)
    {
        const int32 Relative = ReadBE16(Board, ChainsBOffset + ChainIndex * 2);
        int32 ChainPos = ChainsBOffset + Relative;
        if (!InRange(Board, ChainPos, 2))
        {
            OutError = TEXT("MP1 board chain points outside the board file.");
            return false;
        }

        const int32 Length = ReadBE16(Board, ChainPos);
        ChainPos += 2;
        if (!InRange(Board, ChainPos, Length * 2))
        {
            OutError = TEXT("MP1 board chain is truncated.");
            return false;
        }

        int32 Previous = INDEX_NONE;
        for (int32 I = 0; I < Length; ++I)
        {
            const int32 Current = ReadBES16(Board, ChainPos + I * 2);
            if (!Parsed.IsValidIndex(Current))
            {
                OutError = TEXT("MP1 board chain references an invalid space.");
                return false;
            }

            if (Previous != INDEX_NONE && Parsed.IsValidIndex(Previous))
            {
                Parsed[Previous].Next.AddUnique(Current);
            }
            Previous = Current;
        }
    }

    for (int32 I = 0; I < Parsed.Num(); ++I)
    {
        if (Parsed[I].Next.IsEmpty() && Parsed.Num() > 1)
        {
            Parsed[I].Next.Add((I + 1) % Parsed.Num());
        }
    }

    OutSpaces = MoveTemp(Parsed);
    OutError.Reset();
    return true;
}
