// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgeMonster.h"

#include "Character/TheGoldenAgePlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

const FName ATheGoldenAgeMonster::MonsterActorTag(TEXT("Monster"));
const FName ATheGoldenAgeMonster::TrapActorTag(TEXT("Trap"));
const FName ATheGoldenAgeMonster::GhostActorTag(TEXT("Ghost"));

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

void ATheGoldenAgeMonster::BeginPlay()
{
	Super::BeginPlay();
	ApplyLocalViewerVisibility(GetWorld());
}

void ATheGoldenAgeMonster::ApplyLocalViewerVisibility(UWorld* World, const APawn* Viewer)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Possess calls PossessedBy before SetPawn, so the controller pawn can still be the previous one.
	if (!Viewer)
	{
		const APlayerController* Controller = World->GetFirstPlayerController();
		Viewer = Controller ? Controller->GetPawn() : nullptr;
	}
	if (!Viewer || !Viewer->IsLocallyControlled())
	{
		return;
	}

	const bool bHero = Viewer->ActorHasTag(ATheGoldenAgePlayerCharacter::HeroActorTag);
	const bool bExplorer = Viewer->ActorHasTag(ATheGoldenAgePlayerCharacter::ExplorerActorTag);
	if (!bHero && !bExplorer)
	{
		return;
	}

	TArray<AActor*> Monsters;
	UGameplayStatics::GetAllActorsWithTag(World, MonsterActorTag, Monsters);
	for (AActor* Monster : Monsters)
	{
		if (!IsValid(Monster))
		{
			continue;
		}

		const bool bVisibleToHero = Monster->ActorHasTag(GhostActorTag);
		const bool bVisibleToExplorer = Monster->ActorHasTag(TrapActorTag);
		const bool bVisible = (bHero && bVisibleToHero) || (bExplorer && bVisibleToExplorer);
		Monster->SetActorHiddenInGame(!bVisible);
	}
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

	// Role tags (Hero, Explorer) are for who can see this monster. Detection uses Player only.
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
