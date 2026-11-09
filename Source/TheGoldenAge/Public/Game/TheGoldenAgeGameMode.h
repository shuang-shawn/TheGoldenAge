// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TheGoldenAgeGameMode.generated.h"

UCLASS()
class THEGOLDENAGE_API ATheGoldenAgeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATheGoldenAgeGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;
};
