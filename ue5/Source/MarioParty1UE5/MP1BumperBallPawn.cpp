#include "MP1BumperBallPawn.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AMP1BumperBallPawn::AMP1BumperBallPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
    SetRootComponent(BallMesh);
    BallMesh->SetSimulatePhysics(true);
    BallMesh->SetEnableGravity(true);
    BallMesh->SetLinearDamping(0.7f);
    BallMesh->SetAngularDamping(0.4f);
    BallMesh->SetCollisionProfileName(TEXT("PhysicsActor"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded())
    {
        BallMesh->SetStaticMesh(Sphere.Object);
        BallMesh->SetWorldScale3D(FVector(0.7f));
    }
}

void AMP1BumperBallPawn::ApplyMoveInput(const FVector2D& Input, float Strength)
{
    if (bEliminated || !BallMesh) return;

    FVector Direction(Input.Y, Input.X, 0.0f);
    if (Direction.SizeSquared() > 1.0f)
    {
        Direction.Normalize();
    }

    BallMesh->AddForce(Direction * Strength);
}

void AMP1BumperBallPawn::MarkEliminated()
{
    bEliminated = true;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    if (BallMesh)
    {
        BallMesh->SetSimulatePhysics(false);
    }
}
