#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Levels that should not connect to SpacetimeDB or run UTheGoldenAgePlayerSync /
 * UTheGoldenAgeMonsterSync must add this tag on World Settings (Tags).
 */
namespace TheGoldenAgeSpacetimeWorld
{
	extern const FName NoSpacetimeLevelTag;

	/** True for game worlds that are allowed to create Spacetime sync subsystems. */
	bool ShouldCreateSpacetimeSubsystems(const UWorld* World);
}