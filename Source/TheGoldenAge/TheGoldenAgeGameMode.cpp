// Copyright Epic Games, Inc. All Rights Reserved.

#include "TheGoldenAgeGameMode.h"

#include "TheGoldenAgePlayerController.h"

#include "GameFramework/Controller.h"
#include "GameFramework/DefaultPawn.h"
#include "UObject/ConstructorHelpers.h"

namespace TheGoldenAgeGameModePrivate
{
	static const TCHAR* PlayerPawnClassPath = TEXT("/Game/RPGHeroSquad/CharacterBP/BP_TinyHeroPolyart.BP_TinyHeroPolyart_C");
}

ATheGoldenAgeGameMode::ATheGoldenAgeGameMode()
{
	PlayerControllerClass = ATheGoldenAgePlayerController::StaticClass();

	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(
		TEXT("/Game/RPGHeroSquad/CharacterBP/BP_TinyHeroPolyart"));
	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

UClass* ATheGoldenAgeGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// AGameModeBase sets DefaultPawnClass to ADefaultPawn. Do not treat that as the project pawn.
	if (DefaultPawnClass && DefaultPawnClass != ADefaultPawn::StaticClass())
	{
		return DefaultPawnClass;
	}

	if (UClass* PlayerPawnClass = LoadClass<APawn>(nullptr, TheGoldenAgeGameModePrivate::PlayerPawnClassPath))
	{
		DefaultPawnClass = PlayerPawnClass;
		return PlayerPawnClass;
	}

	UE_LOG(LogGameMode, Error, TEXT("Failed to load player pawn class: %s"), TheGoldenAgeGameModePrivate::PlayerPawnClassPath);
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}
