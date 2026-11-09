#pragma once

#include "CoreMinimal.h"
#include "TheGoldenAgePlayerRole.generated.h"

/** Stored on the player row as a raw byte. 0 = Hero, 1 = Explorer. */
UENUM(BlueprintType)
enum class ETheGoldenAgePlayerRole : uint8
{
	Hero = 0 UMETA(DisplayName = "Hero"),
	Explorer = 1 UMETA(DisplayName = "Explorer")
};
