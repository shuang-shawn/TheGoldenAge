// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgeMonster.h"

#include "Character/TheGoldenAgePlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ATheGoldenAgeMonster::ATheGoldenAgeMonster()
{
	PrimaryActorTick.bCanEverTick = true;

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
}

void ATheGoldenAgeMonster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TArray<AActor*> NearbyPlayers;
	GatherPlayersInRange(NearbyPlayers);
	HandleNearbyPlayers(NearbyPlayers, DeltaSeconds);
}

void ATheGoldenAgeMonster::HandleNearbyPlayers_Implementation(const TArray<AActor*>& /*NearbyPlayers*/, float /*DeltaSeconds*/)
{
}

void ATheGoldenAgeMonster::GatherPlayersInRange(TArray<AActor*>& OutPlayers) const
{
	OutPlayers.Reset();

	UWorld* World = GetWorld();
	if (!World || DetectionRange <= 0.f)
	{
		return;
	}

	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsWithTag(World, ATheGoldenAgePlayerCharacter::PlayerActorTag, Players);

	const FVector Origin = GetActorLocation();
	const float RangeSquared = FMath::Square(DetectionRange);
	for (AActor* Player : Players)
	{
		if (!IsValid(Player) || Player == this)
		{
			continue;
		}

		if (FVector::DistSquared2D(Origin, Player->GetActorLocation()) <= RangeSquared)
		{
			OutPlayers.Add(Player);
		}
	}
}
