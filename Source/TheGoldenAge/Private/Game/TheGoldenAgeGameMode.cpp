// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/TheGoldenAgeGameMode.h"

#include "Player/TheGoldenAgePlayerController.h"

#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"

ATheGoldenAgeGameMode::ATheGoldenAgeGameMode()
{
	PlayerControllerClass = ATheGoldenAgePlayerController::StaticClass();
	DefaultPawnClass = nullptr;

	// ConstructorHelpers records a hard cook dependency. A string passed to LoadClass does not.
	static ConstructorHelpers::FClassFinder<APawn> HeroPawn(TEXT("/Game/_Main/Blueprint/BP_Hero"));
	if (HeroPawn.Succeeded())
	{
		HeroPawnClass = HeroPawn.Class;
	}

	static ConstructorHelpers::FClassFinder<APawn> ExplorerPawn(TEXT("/Game/_Main/Blueprint/BP_Explorer"));
	if (ExplorerPawn.Succeeded())
	{
		ExplorerPawnClass = ExplorerPawn.Class;
	}

	static ConstructorHelpers::FClassFinder<ATheGoldenAgeMonster> ChestMonster(TEXT("/Game/_Main/Blueprint/BP_ChestMonster"));
	if (ChestMonster.Succeeded())
	{
		ChestMonsterClass = ChestMonster.Class;
	}

	static ConstructorHelpers::FClassFinder<ATheGoldenAgeMonster> GhostFish(TEXT("/Game/_Main/Blueprint/BP_GhostFish"));
	if (GhostFish.Succeeded())
	{
		GhostFishClass = GhostFish.Class;
	}
}

void ATheGoldenAgeGameMode::RestartPlayer(AController* NewPlayer)
{
	// UTheGoldenAgePlayerSync possesses the account pawn after login.
	(void)NewPlayer;
}
