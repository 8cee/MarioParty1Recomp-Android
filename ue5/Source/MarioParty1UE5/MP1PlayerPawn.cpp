#include "MP1PlayerPawn.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AMP1PlayerPawn::AMP1PlayerPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Piece"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PieceMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (PieceMesh.Succeeded())
    {
        Mesh->SetStaticMesh(PieceMesh.Object);
        Mesh->SetWorldScale3D(FVector(0.45f));
    }
}

void AMP1PlayerPawn::SetBoardLocation(const FVector& Location)
{
    SetActorLocation(Location + FVector(0, 0, 130.0f));
}
