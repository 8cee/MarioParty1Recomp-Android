#include "MP1HUD.h"

#include "MP1GameMode.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"

void AMP1HUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;

    AMP1GameMode* GM = Cast<AMP1GameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM) return;

    const TArray<FMP1PlayerState>& Players = GM->GetPlayers();

    float X = 40.0f;
    float Y = 35.0f;
    DrawText(TEXT("MARIO PARTY 1 - UE5 REMAKE"), FLinearColor::White, X, Y, nullptr, 1.35f);
    Y += 42.0f;

    if (GM->IsInMinigame())
    {
        DrawText(
            FString::Printf(TEXT("BUMPER BALLS   |   %.0f seconds   |   WASD / Left Stick"),
                GM->GetMinigameSecondsRemaining()),
            FLinearColor::Yellow, X, Y, nullptr, 1.0f);
    }
    else
    {
        DrawText(
            FString::Printf(TEXT("Round %d / %d   |   Player %d turn   |   SPACE / ENTER / A = Roll Dice"),
                GM->GetRound(), GM->GetMaxRounds(), GM->GetCurrentPlayer() + 1),
            FLinearColor::Yellow, X, Y, nullptr, 1.0f);
    }
    Y += 38.0f;

    for (const FMP1PlayerState& P : Players)
    {
        const bool bCurrent = P.PlayerIndex == GM->GetCurrentPlayer();
        DrawText(
            FString::Printf(TEXT("P%d  Coins: %d  Stars: %d  Space: %d"),
                P.PlayerIndex + 1, P.Coins, P.Stars, P.SpaceIndex),
            bCurrent ? FLinearColor::Yellow : FLinearColor::White,
            X, Y, nullptr, 1.0f);
        Y += 28.0f;
    }

    Y += 15.0f;
    if (!GM->GetStatusText().IsEmpty())
    {
        DrawText(GM->GetStatusText(), FLinearColor::Green, X, Y, nullptr, 1.1f);
    }
}
