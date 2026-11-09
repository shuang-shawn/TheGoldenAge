// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgeMonster.h"

#include "Character/TheGoldenAgePlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Spacetime/TheGoldenAgeMonsterSync.h"

const FName ATheGoldenAgeMonster::MonsterActorTag(TEXT("Monster"));
const FName ATheGoldenAgeMonster::TrapActorTag(TEXT("Trap"));
const FName ATheGoldenAgeMonster::GhostActorTag(TEXT("Ghost"));

namespace
{
constexpr float TransformReportInterval = 0.05f;
constexpr float ReportLocationTolerance = 10.f;
constexpr float ReportYawTolerance = 5.f;

UTheGoldenAgeMonsterSync* GetMonsterSync(const AActor* Actor)
{
	const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTheGoldenAgeMonsterSync>() : nullptr;
}
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

	// Facing is applied directly. Movement simulation would fight the snapped transform.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
	}
}

void ATheGoldenAgeMonster::ApplyServerState(
	const FString& OwnerName,
	const FVector& Location,
	const FRotator& Rotation,
	bool bAttacking,
	bool bLoopAttack,
	const FString& LocalUsername)
{
	const bool bLocalOwner = !LocalUsername.IsEmpty() && OwnerName == LocalUsername;
	if (bLocalOwner)
	{
		// An in-flight update can arrive after we asked to release. Wait for the cleared row.
		if (!bReleaseRequested)
		{
			if (Authority != EAuthority::Owner)
			{
				bHasReportedServerState = false;
				TimeSinceServerReport = 0.f;
			}
			Authority = EAuthority::Owner;
			ServerOwner = OwnerName;
		}
		return;
	}

	bReleaseRequested = false;
	ServerOwner = OwnerName;
	bServerAttacking = bAttacking;
	bServerLoopAttack = bLoopAttack;

	// Our claim has not landed yet. Keep waiting instead of claiming again.
	if (Authority == EAuthority::Claiming && OwnerName.IsEmpty())
	{
		return;
	}

	Authority = EAuthority::Passive;
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	ServerTargetYaw = Rotation.Yaw;
	bHasServerFacing = true;
	ApplyReplicatedAnim(bAttacking, bLoopAttack);
}

void ATheGoldenAgeMonster::NotifyClaimRejected()
{
	if (Authority == EAuthority::Claiming)
	{
		Authority = EAuthority::Passive;
	}
}

void ATheGoldenAgeMonster::NotifyReleaseRejected()
{
	if (!bReleaseRequested)
	{
		return;
	}

	bReleaseRequested = false;
	Authority = EAuthority::Owner;
	bHasReportedServerState = false;
	TimeSinceServerReport = 0.f;
}

void ATheGoldenAgeMonster::NotifyDisconnected()
{
	Authority = EAuthority::Passive;
	bReleaseRequested = false;
	ServerOwner.Reset();
}

void ATheGoldenAgeMonster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MonsterId == 0 || Authority == EAuthority::Owner)
	{
		TArray<AActor*> NearbyPlayers;
		GatherPlayersInRange(NearbyPlayers);
		HandleNearbyPlayers(NearbyPlayers, DeltaSeconds);
		if (MonsterId != 0)
		{
			ReportOwnedState(DeltaSeconds);
			TryReleaseOwnership(NearbyPlayers);
		}
		return;
	}

	// Unclaimed monsters stay idle until this client sees a player and takes the claim.
	if (Authority == EAuthority::Passive && ServerOwner.IsEmpty())
	{
		TArray<AActor*> NearbyPlayers;
		GatherPlayersInRange(NearbyPlayers);
		if (NearbyPlayers.Num() > 0)
		{
			if (UTheGoldenAgeMonsterSync* Sync = GetMonsterSync(this))
			{
				if (Sync->RequestClaim(MonsterId))
				{
					Authority = EAuthority::Claiming;
				}
			}
		}
	}

	TurnTowardServerYaw(DeltaSeconds);
	ApplyReplicatedAnim(bServerAttacking, bServerLoopAttack);
}

void ATheGoldenAgeMonster::HandleNearbyPlayers_Implementation(const TArray<AActor*>& /*NearbyPlayers*/, float /*DeltaSeconds*/)
{
}

void ATheGoldenAgeMonster::ApplyReplicatedAnim(bool /*bInAttacking*/, bool /*bInLoopAttack*/)
{
}

void ATheGoldenAgeMonster::GetServerAnimState(bool& bOutAttacking, bool& bOutLoopAttack) const
{
	bOutAttacking = false;
	bOutLoopAttack = false;
}

bool ATheGoldenAgeMonster::ShouldReleaseServerOwnership() const
{
	return true;
}

float ATheGoldenAgeMonster::GetFacingTurnRate() const
{
	return 480.f;
}

void ATheGoldenAgeMonster::TurnTowardServerYaw(float DeltaSeconds)
{
	if (!bHasServerFacing || GetFacingTurnRate() <= 0.f)
	{
		return;
	}

	const FRotator Desired(0.f, ServerTargetYaw, 0.f);
	const FRotator Updated = FMath::RInterpConstantTo(GetActorRotation(), Desired, DeltaSeconds, GetFacingTurnRate());
	SetActorRotation(FRotator(0.f, Updated.Yaw, 0.f));
}

void ATheGoldenAgeMonster::ReportOwnedState(float DeltaSeconds)
{
	if (bReleaseRequested)
	{
		return;
	}

	TimeSinceServerReport += DeltaSeconds;

	bool bAttacking = false;
	bool bLoopAttack = false;
	GetServerAnimState(bAttacking, bLoopAttack);

	const FVector Location = GetActorLocation();
	const FRotator Rotation = GetActorRotation();
	const float YawDelta = FMath::Abs(FRotator::NormalizeAxis(Rotation.Yaw - LastReportedRotation.Yaw));
	const bool bFirstReport = !bHasReportedServerState;
	const bool bAnimChanged = bFirstReport
		|| bAttacking != bLastReportedAttacking
		|| bLoopAttack != bLastReportedLoopAttack;
	const bool bTransformChanged = bFirstReport
		|| !Location.Equals(LastReportedLocation, ReportLocationTolerance)
		|| YawDelta >= ReportYawTolerance;
	const bool bTransformDue = TimeSinceServerReport >= TransformReportInterval;
	if (!bAnimChanged && !(bTransformDue && bTransformChanged))
	{
		return;
	}

	UTheGoldenAgeMonsterSync* Sync = GetMonsterSync(this);
	if (!Sync || !Sync->ReportState(MonsterId, Location, Rotation, bAttacking, bLoopAttack))
	{
		return;
	}

	if (bFirstReport || (bTransformDue && bTransformChanged))
	{
		TimeSinceServerReport = 0.f;
	}

	bHasReportedServerState = true;
	LastReportedLocation = Location;
	LastReportedRotation = Rotation;
	bLastReportedAttacking = bAttacking;
	bLastReportedLoopAttack = bLoopAttack;
}

void ATheGoldenAgeMonster::TryReleaseOwnership(const TArray<AActor*>& NearbyPlayers)
{
	if (bReleaseRequested || NearbyPlayers.Num() > 0 || !ShouldReleaseServerOwnership())
	{
		return;
	}

	UTheGoldenAgeMonsterSync* Sync = GetMonsterSync(this);
	if (!Sync || !Sync->Release(MonsterId))
	{
		return;
	}

	bReleaseRequested = true;
	Authority = EAuthority::Passive;
	ServerOwner.Reset();
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
