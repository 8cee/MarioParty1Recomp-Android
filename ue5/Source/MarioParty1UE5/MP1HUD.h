#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MP1HUD.generated.h"

UCLASS()
class MARIOPARTY1UE5_API AMP1HUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
