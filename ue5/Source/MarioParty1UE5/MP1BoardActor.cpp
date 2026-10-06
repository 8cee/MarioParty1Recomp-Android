#include "MP1BoardActor.h"
#include "MP1RomBoardLoader.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

AMP1BoardActor::AMP1BoardActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    SpaceMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Spaces"));
    SpaceMeshes->SetupAttachment(Root);
    SpaceMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SpaceMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (SpaceMesh.Succeeded())
    {
        SpaceMeshes->SetStaticMesh(SpaceMesh.Object);
    }
}

void AMP1BoardActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (Spaces.IsEmpty())
    {
        const FString DefaultBoard = FPaths::ProjectContentDir() / TEXT("MP1/Boards/DK.json");
        if (!LoadBoardJson(DefaultBoard))
        {
            BuildPrototypeBoard();
        }
    }
    RebuildInstances();
}



bool AMP1BoardActor::LoadBoardFromRom(const FString& RomPath, int32 BoardFile)
{
    TArray<FMP1BoardSpaceData> Parsed;
    FString Error;
    if (!FMP1RomBoardLoader::LoadBoard(RomPath, BoardFile, Parsed, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("MP1 ROM board import failed: %s"), *Error);
        return false;
    }

    Spaces = MoveTemp(Parsed);
    RebuildInstances();
    UE_LOG(LogTemp, Display, TEXT("Loaded %d original MP1 board spaces from %s"), Spaces.Num(), *RomPath);
    return !Spaces.IsEmpty();
}

bool AMP1BoardActor::LoadBoardJson(const FString& JsonPath)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *JsonPath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonSpaces = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("spaces"), JsonSpaces) || JsonSpaces == nullptr)
    {
        return false;
    }

    TArray<FMP1BoardSpaceData> Parsed;
    Parsed.Reserve(JsonSpaces->Num());

    for (const TSharedPtr<FJsonValue>& Value : *JsonSpaces)
    {
        const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Obj.IsValid())
        {
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
        if (!Obj->TryGetArrayField(TEXT("position_ue"), Position) || Position == nullptr || Position->Num() != 3)
        {
            return false;
        }

        FMP1BoardSpaceData S;
        S.Index = Obj->GetIntegerField(TEXT("index"));
        const int32 RawType = Obj->GetIntegerField(TEXT("type"));
        S.Type = static_cast<EMP1SpaceType>(FMath::Clamp(RawType, 0, static_cast<int32>(EMP1SpaceType::Neutral)));
        S.Location = FVector(
            static_cast<float>((*Position)[0]->AsNumber()),
            static_cast<float>((*Position)[1]->AsNumber()),
            static_cast<float>((*Position)[2]->AsNumber())
        );
        Parsed.Add(S);
    }

    const TArray<TSharedPtr<FJsonValue>>* Chains = nullptr;
    if (RootObject->TryGetArrayField(TEXT("chains_b"), Chains) && Chains != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& ChainValue : *Chains)
        {
            const TArray<TSharedPtr<FJsonValue>>& Chain = ChainValue->AsArray();
            for (int32 i = 0; i + 1 < Chain.Num(); ++i)
            {
                const int32 From = static_cast<int32>(Chain[i]->AsNumber());
                const int32 To = static_cast<int32>(Chain[i + 1]->AsNumber());
                if (Parsed.IsValidIndex(From) && Parsed.IsValidIndex(To))
                {
                    Parsed[From].Next.AddUnique(To);
                }
            }
        }
    }

    // Preserve a playable route even if a board chain endpoint has no outgoing
    // edge in the exported table. Real branch selection is migrated per board.
    for (int32 i = 0; i < Parsed.Num(); ++i)
    {
        if (Parsed[i].Next.IsEmpty() && Parsed.Num() > 1)
        {
            Parsed[i].Next.Add((i + 1) % Parsed.Num());
        }
    }

    Spaces = MoveTemp(Parsed);
    RebuildInstances();
    return !Spaces.IsEmpty();
}

void AMP1BoardActor::BuildPrototypeBoard()
{
    Spaces.Reset();

    // Functional vertical slice. The exact board topology is replaced by ROM-
    // extracted MP1 chain data as each original board is migrated.
    constexpr int32 Count = 40;
    constexpr float RadiusX = 1800.0f;
    constexpr float RadiusY = 1100.0f;

    for (int32 i = 0; i < Count; ++i)
    {
        const float A = (2.0f * PI * i) / Count;
        FMP1BoardSpaceData S;
        S.Index = i;
        S.Location = FVector(FMath::Cos(A) * RadiusX, FMath::Sin(A) * RadiusY, 80.0f);
        S.Next.Add((i + 1) % Count);

        if (i == 10 || i == 30) S.Type = EMP1SpaceType::Red;
        else if (i == 15) S.Type = EMP1SpaceType::Chance;
        else if (i == 22) S.Type = EMP1SpaceType::Happening;
        else if (i == 35) S.Type = EMP1SpaceType::Star;
        else S.Type = EMP1SpaceType::Blue;

        Spaces.Add(S);
    }

    RebuildInstances();
}

void AMP1BoardActor::RebuildInstances()
{
    if (!SpaceMeshes) return;
    SpaceMeshes->ClearInstances();

    for (const FMP1BoardSpaceData& S : Spaces)
    {
        FTransform T;
        T.SetLocation(S.Location);
        T.SetScale3D(FVector(0.8f, 0.8f, 0.12f));
        SpaceMeshes->AddInstance(T);
    }
}

bool AMP1BoardActor::GetSpace(int32 Index, FMP1BoardSpaceData& OutSpace) const
{
    if (!Spaces.IsValidIndex(Index)) return false;
    OutSpace = Spaces[Index];
    return true;
}

int32 AMP1BoardActor::GetNextSpace(int32 Index) const
{
    if (!Spaces.IsValidIndex(Index) || Spaces[Index].Next.IsEmpty()) return Index;
    return Spaces[Index].Next[0];
}

FVector AMP1BoardActor::GetSpaceLocation(int32 Index) const
{
    return Spaces.IsValidIndex(Index) ? Spaces[Index].Location : GetActorLocation();
}
