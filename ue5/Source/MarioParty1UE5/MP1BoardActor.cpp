#include "MP1BoardActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"

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
        BuildPrototypeBoard();
    }
    RebuildInstances();
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
