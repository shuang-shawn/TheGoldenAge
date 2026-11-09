// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/TheGoldenAgeMonster.h"
#include "GameFramework/GameModeBase.h"
#include "TheGoldenAgeGameMode.generated.h"

UCLASS()
class THEGOLDENAGE_API ATheGoldenAgeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATheGoldenAgeGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;

	/** Spawned for player role Hero. Hard reference so a packaged build cooks the Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "Spacetime")
	TSubclassOf<APawn> HeroPawnClass;

	/** Spawned for player role Explorer. */
	UPROPERTY(EditDefaultsOnly, Category = "Spacetime")
	TSubclassOf<APawn> ExplorerPawnClass;

	/** Spawned for monster kind 0. */
	UPROPERTY(EditDefaultsOnly, Category = "Spacetime")
	TSubclassOf<ATheGoldenAgeMonster> ChestMonsterClass;

	/** Spawned for monster kind 1. */
	UPROPERTY(EditDefaultsOnly, Category = "Spacetime")
	TSubclassOf<ATheGoldenAgeMonster> GhostFishClass;
};
