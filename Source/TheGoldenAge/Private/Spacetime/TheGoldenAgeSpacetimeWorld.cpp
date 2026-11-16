#include "Spacetime/TheGoldenAgeSpacetimeWorld.h"

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace TheGoldenAgeSpacetimeWorld
{
const FName NoSpacetimeLevelTag(TEXT("NoSpacetime"));

bool ShouldCreateSpacetimeSubsystems(const UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	const AWorldSettings* Settings = World->GetWorldSettings();
	if (Settings && Settings->ActorHasTag(NoSpacetimeLevelTag))
	{
		return false;
	}

	return true;
}
} // namespace TheGoldenAgeSpacetimeWorld