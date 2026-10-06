#include "MP1BumperBallsArena.h"

#include "MP1BumperBallPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

AMP1BumperBallsArena::AMP1BumperBallsArena()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AMP1BumperBallsArena::BeginPlay()
{
    Super::BeginPlay();
    SpawnArena();
}

void AMP1BumperBallsArena::SpawnArena()
{
    Platform = GetWorld()->SpawnActor<AStaticMeshActor>();
    if (Platform && Platform->GetStaticMeshComponent())
    {
        UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        if (Cylinder)
        {
            Platform->GetStaticMeshComponent()->SetStaticMesh(Cylinder);
            Platform->SetActorScale3D(FVector(8.0f, 8.0f, 0.35f));
            Platform->SetActorLocation(GetActorLocation());
            Platform->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
            Platform->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        }
    }

    static const FVector Starts[4] = {
        FVector(550, 0, 220),
        FVector(-550, 0, 220),
        FVector(0, 550, 220),
        FVector(0, -550, 220)
    };

    Balls.SetNum(4);
    for (int32 I = 0; I < 4; ++I)
    {
        Balls[I] = GetWorld()->SpawnActor<AMP1BumperBallPawn>(
            AMP1BumperBallPawn::StaticClass(),
            GetActorLocation() + Starts[I],
            FRotator::ZeroRotator
        );
        if (Balls[I])
        {
            Balls[I]->SetPlayerIndex(I);
        }
    }
}

void AMP1BumperBallsArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (AMP1BumperBallPawn* Ball : Balls)
    {
        if (IsValid(Ball))
        {
            Ball->Destroy();
        }
    }
    Balls.Reset();

    if (IsValid(Platform))
    {
        Platform->Destroy();
        Platform = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void AMP1BumperBallsArena::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished) return;

    Elapsed += DeltaSeconds;

    if (Balls.IsValidIndex(0) && Balls[0] && !Balls[0]->IsEliminated())
    {
        Balls[0]->ApplyMoveInput(HumanInput);
    }

    UpdateAI(DeltaSeconds);
    CheckEliminations();
    FinishIfNeeded();
}

void AMP1BumperBallsArena::UpdateAI(float)
{
    AMP1BumperBallPawn* Human = Balls.IsValidIndex(0) ? Balls[0].Get() : nullptr;
    const FVector HumanPos = Human ? Human->GetActorLocation() : GetActorLocation();

    for (int32 I = 1; I < Balls.Num(); ++I)
    {
        AMP1BumperBallPawn* Ball = Balls[I];
        if (!Ball || Ball->IsEliminated()) continue;

        const FVector Delta = HumanPos - Ball->GetActorLocation();
        FVector2D Input(Delta.Y, Delta.X);
        if (Input.SizeSquared() > KINDA_SMALL_NUMBER)
        {
            Input.Normalize();
        }

        // Small deterministic orbit component prevents all CPU players from
        // converging along exactly the same line.
        Input += FVector2D(FMath::Sin(Elapsed + I), FMath::Cos(Elapsed * 0.7f + I)) * 0.25f;
        Ball->ApplyMoveInput(Input, 155000.0f);
    }
}

void AMP1BumperBallsArena::CheckEliminations()
{
    for (AMP1BumperBallPawn* Ball : Balls)
    {
        if (!Ball || Ball->IsEliminated()) continue;

        const FVector Relative = Ball->GetActorLocation() - GetActorLocation();
        const float Radius = FVector2D(Relative.X, Relative.Y).Size();
        if (Relative.Z < -350.0f || Radius > 1150.0f)
        {
            Ball->MarkEliminated();
        }
    }
}

void AMP1BumperBallsArena::FinishIfNeeded()
{
    int32 AliveCount = 0;
    int32 LastAlive = INDEX_NONE;

    for (AMP1BumperBallPawn* Ball : Balls)
    {
        if (Ball && !Ball->IsEliminated())
        {
            AliveCount++;
            LastAlive = Ball->GetPlayerIndex();
        }
    }

    if (AliveCount <= 1)
    {
        WinnerIndex = LastAlive;
        bFinished = true;
        return;
    }

    if (Elapsed >= Duration)
    {
        // MP1 allows draws. For this vertical slice, choose the surviving ball
        // closest to the platform center as the tiebreaker.
        float BestDistanceSq = TNumericLimits<float>::Max();
        for (AMP1BumperBallPawn* Ball : Balls)
        {
            if (!Ball || Ball->IsEliminated()) continue;
            const FVector P = Ball->GetActorLocation() - GetActorLocation();
            const float D = FVector2D(P.X, P.Y).SizeSquared();
            if (D < BestDistanceSq)
            {
                BestDistanceSq = D;
                WinnerIndex = Ball->GetPlayerIndex();
            }
        }
        bFinished = true;
    }
}
