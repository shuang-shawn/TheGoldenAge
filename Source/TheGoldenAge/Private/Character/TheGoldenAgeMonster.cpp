// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgeMonster.h"

#include "Character/TheGoldenAgeHealthComponent.h"
#include "Character/TheGoldenAgePlayerCharacter.h"
#include "Spacetime/TheGoldenAgeMonsterSync.h"
#include "Components/WidgetComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/TheGoldenAgeHealthBarWidget.h"
#include "UObject/UnrealType.h"

const FName ATheGoldenAgeMonster::MonsterActorTag(TEXT("Monster"));
const FName ATheGoldenAgeMonster::TrapActorTag(TEXT("Trap"));
const FName ATheGoldenAgeMonster::GhostActorTag(TEXT("Ghost"));

namespace
{
constexpr double RenderDelaySeconds = 0.1;
constexpr float RenderSnapDistance = 500.f;
constexpr int32 MaxSnapshots = 32;
constexpr float AttackEndToleranceSeconds = 0.001f;
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

	Health = CreateDefaultSubobject<UTheGoldenAgeHealthComponent>(TEXT("Health"));

	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetWidgetClass(UTheGoldenAgeHealthBarWidget::StaticClass());
	HealthBar->SetDrawSize(FVector2D(160.f, 14.f));
	HealthBar->SetPivot(FVector2D(0.5f, 0.f));
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, -110.f));
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBar->SetGenerateOverlapEvents(false);
	HealthBar->SetCanEverAffectNavigation(false);
}

void ATheGoldenAgeMonster::BeginPlay()
{
	Super::BeginPlay();
	if (Health)
	{
		Health->OnDied.AddDynamic(this, &ATheGoldenAgeMonster::HandleDied);
	}
	BindHealthBar();
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
	PushHealthAnimState();
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

void ATheGoldenAgeMonster::ApplyServerHealth(float NewHealth)
{
	if (Health)
	{
		Health->ApplyServerHealth(NewHealth);
	}
	BindHealthBar();
}

bool ATheGoldenAgeMonster::IsDead() const
{
	return Health && Health->IsDead();
}

void ATheGoldenAgeMonster::BindHealthBar()
{
	if (!HealthBar || !Health)
	{
		return;
	}

	if (!HealthBar->GetWidget())
	{
		HealthBar->InitWidget();
	}

	if (UTheGoldenAgeHealthBarWidget* Bar = Cast<UTheGoldenAgeHealthBarWidget>(HealthBar->GetWidget()))
	{
		Bar->SetHealthComponent(Health);
	}
}

void ATheGoldenAgeMonster::PushHealthAnimState()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	UClass* AnimClass = AnimInstance->GetClass();
	if (FBoolProperty* DeadProperty = FindFProperty<FBoolProperty>(AnimClass, TEXT("bIsDead")))
	{
		DeadProperty->SetPropertyValue_InContainer(AnimInstance, IsDead());
	}
}

void ATheGoldenAgeMonster::HandleDied()
{
	// Attack presentation stops while dead; the server also clears attack flags.
}

void ATheGoldenAgeMonster::HandleRespawned()
{
	Snapshots.Empty();
	bMonsterAttackHitApplied = false;
	bHasMonsterAttackHitSample = false;
	MonsterAttackHitSampleTime = 0.f;
}

void ATheGoldenAgeMonster::TryMonsterAttackHit(float AttackPlaybackTime, float AttackPlayLength)
{
	if (MonsterId == 0 || IsDead() || MonsterAttackRadius <= 0.f)
	{
		return;
	}

	if (bHasMonsterAttackHitSample && AttackPlaybackTime + AttackEndToleranceSeconds < MonsterAttackHitSampleTime)
	{
		bMonsterAttackHitApplied = false;
	}
	MonsterAttackHitSampleTime = AttackPlaybackTime;
	bHasMonsterAttackHitSample = true;

	if (bMonsterAttackHitApplied)
	{
		return;
	}

	if (AttackPlayLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float HitTime = AttackPlayLength * MonsterAttackHitTimeNormalized;
	if (AttackPlaybackTime + AttackEndToleranceSeconds < HitTime)
	{
		return;
	}

	bMonsterAttackHitApplied = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UTheGoldenAgeMonsterSync* MonsterSync = World->GetSubsystem<UTheGoldenAgeMonsterSync>();
	if (!MonsterSync)
	{
		return;
	}

	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		return;
	}

	const FVector Origin = GetActorLocation() + Forward * MonsterAttackReach;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterAttackOverlap), false, this);
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	if (!World->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(MonsterAttackRadius),
		QueryParams))
	{
		return;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ATheGoldenAgePlayerCharacter* Player = Cast<ATheGoldenAgePlayerCharacter>(Overlap.GetActor());
		if (!Player || !Player->IsLocallyControlled() || Player->IsDead())
		{
			continue;
		}

		MonsterSync->RequestMonsterAttackPlayer(MonsterId);
		break;
	}
}

void ATheGoldenAgeMonster::FlushPresentedTransform()
{
	PresentSnapshot();
}
