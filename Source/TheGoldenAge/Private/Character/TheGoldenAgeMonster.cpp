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

namespace
{
constexpr double RenderDelaySeconds = 0.1;
constexpr float RenderSnapDistance = 500.f;
constexpr int32 MaxSnapshots = 32;
}

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

void ATheGoldenAgeMonster::InitializeFromServer(uint64 InMonsterId)
{
	MonsterId = InMonsterId;
	SetReplicates(false);
	SetReplicateMovement(false);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;
	SetActorTickEnabled(true);

	// The server owns the transform. Local movement would fight the rendered samples.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
	}
}

void ATheGoldenAgeMonster::ApplyServerState(
	const FVector& Location,
	const FRotator& Rotation,
	bool /*bAttacking*/,
	bool bLoopAttack)
{
	bServerLoopAttack = bLoopAttack;
	PushSnapshot(Location, Rotation.Yaw);
}

void ATheGoldenAgeMonster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PresentSnapshot();
	UpdatePresentedAnim();
}

void ATheGoldenAgeMonster::UpdatePresentedAnim()
{
}

void ATheGoldenAgeMonster::PushSnapshot(const FVector& Location, float Yaw)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FMonsterSnapshot Snapshot;
	Snapshot.Location = Location;
	Snapshot.Yaw = Yaw;
	Snapshot.ReceivedTime = World->GetTimeSeconds();
	Snapshots.Add(Snapshot);

	if (Snapshots.Num() > MaxSnapshots)
	{
		Snapshots.RemoveAt(0, Snapshots.Num() - MaxSnapshots, EAllowShrinking::No);
	}
}

void ATheGoldenAgeMonster::PresentSnapshot()
{
	if (Snapshots.IsEmpty())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double RenderTime = World->GetTimeSeconds() - RenderDelaySeconds;
	while (Snapshots.Num() > 2 && Snapshots[1].ReceivedTime <= RenderTime)
	{
		Snapshots.RemoveAt(0, EAllowShrinking::No);
	}

	const FMonsterSnapshot& Newest = Snapshots.Last();
	if (Snapshots.Num() == 1 || RenderTime >= Newest.ReceivedTime)
	{
		SetActorLocation(Newest.Location, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(FRotator(0.f, Newest.Yaw, 0.f));
		return;
	}

	int32 NewerIndex = 0;
	while (NewerIndex < Snapshots.Num() && Snapshots[NewerIndex].ReceivedTime < RenderTime)
	{
		++NewerIndex;
	}
	if (NewerIndex <= 0)
	{
		const FMonsterSnapshot& Oldest = Snapshots[0];
		SetActorLocation(Oldest.Location, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(FRotator(0.f, Oldest.Yaw, 0.f));
		return;
	}

	const FMonsterSnapshot& Older = Snapshots[NewerIndex - 1];
	const FMonsterSnapshot& Newer = Snapshots[NewerIndex];
	if (FVector::DistSquared2D(Older.Location, Newer.Location) >= FMath::Square(RenderSnapDistance))
	{
		SetActorLocation(Newer.Location, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(FRotator(0.f, Newer.Yaw, 0.f));
		return;
	}

	const double Span = Newer.ReceivedTime - Older.ReceivedTime;
	const float Alpha = Span > KINDA_SMALL_NUMBER
		? FMath::Clamp(static_cast<float>((RenderTime - Older.ReceivedTime) / Span), 0.f, 1.f)
		: 1.f;
	const FVector Location = FMath::Lerp(Older.Location, Newer.Location, Alpha);
	const float Yaw = Older.Yaw + FMath::FindDeltaAngleDegrees(Older.Yaw, Newer.Yaw) * Alpha;
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator(0.f, Yaw, 0.f));
}
