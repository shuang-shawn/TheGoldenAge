// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgePlayerCharacter.h"

#include "Character/TheGoldenAgeHealthComponent.h"
#include "Character/TheGoldenAgeMonster.h"
#include "Player/TheGoldenAgePlayerController.h"
#include "Spacetime/TheGoldenAgeMonsterSync.h"
#include "Spacetime/TheGoldenAgePlayerSync.h"
#include "UI/TheGoldenAgeHealthBarWidget.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequenceBase.h"
#include "EnhancedInputComponent.h"
#include "Engine/OverlapResult.h"
#include "InputAction.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr float AttackEndToleranceSeconds = 0.001f;

FAnimNode_SequencePlayer* FindHeroAttackSequencePlayer(UAnimInstance* AnimInstance, const UAnimSequenceBase* ExpectedAttackSequence)
{
	const IAnimClassInterface* AnimClass = AnimInstance
		? IAnimClassInterface::GetFromClass(AnimInstance->GetClass())
		: nullptr;
	if (!AnimClass)
	{
		return nullptr;
	}

	for (const FStructProperty* NodeProperty : AnimClass->GetAnimNodeProperties())
	{
		if (!NodeProperty || NodeProperty->Struct != FAnimNode_SequencePlayer::StaticStruct())
		{
			continue;
		}

		FAnimNode_SequencePlayer* Player = NodeProperty->ContainerPtrToValuePtr<FAnimNode_SequencePlayer>(AnimInstance);
		const UAnimSequenceBase* Sequence = Player ? Player->GetSequence() : nullptr;
		if (!Sequence)
		{
			continue;
		}

		if (ExpectedAttackSequence && Sequence == ExpectedAttackSequence)
		{
			return Player;
		}

		if (Sequence->GetName().Contains(TEXT("Attack01")))
		{
			return Player;
		}
	}

	return nullptr;
}

void ApplyLocomotionPlayRates(UAnimInstance* AnimInstance, float MovePlayRate)
{
	const IAnimClassInterface* AnimClass = AnimInstance
		? IAnimClassInterface::GetFromClass(AnimInstance->GetClass())
		: nullptr;
	if (!AnimClass)
	{
		return;
	}

	for (const FStructProperty* NodeProperty : AnimClass->GetAnimNodeProperties())
	{
		if (!NodeProperty || NodeProperty->Struct != FAnimNode_SequencePlayer::StaticStruct())
		{
			continue;
		}

		FAnimNode_SequencePlayer* Player = NodeProperty->ContainerPtrToValuePtr<FAnimNode_SequencePlayer>(AnimInstance);
		const UAnimSequenceBase* Sequence = Player ? Player->GetSequence() : nullptr;
		if (!Sequence)
		{
			continue;
		}

		// Idle clips stay at 1. Move clips, including the Explorer sprint used for every direction, scale with speed.
		const FString SequenceName = Sequence->GetName();
		const float PlayRate = SequenceName.Contains(TEXT("Idle"))
			|| SequenceName.Contains(TEXT("Die"))
			|| SequenceName.Contains(TEXT("Attack"))
			? 1.f
			: MovePlayRate;
		Player->SetPlayRate(PlayRate);
	}
}
}

const FName ATheGoldenAgePlayerCharacter::PlayerActorTag(TEXT("Player"));
const FName ATheGoldenAgePlayerCharacter::HeroActorTag(TEXT("Hero"));
const FName ATheGoldenAgePlayerCharacter::ExplorerActorTag(TEXT("Explorer"));

ATheGoldenAgePlayerCharacter::ATheGoldenAgePlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.AddUnique(PlayerActorTag);

	// Presence is owned by SpacetimeDB. Pawns are not replicated or auto-possessed.
	bReplicates = false;
	SetReplicates(false);
	SetReplicateMovement(false);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
	Movement->MaxWalkSpeed = 300.f;
	Movement->JumpZVelocity = 0.f;
	Movement->AirControl = 0.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 900.f;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->SetRelativeRotation(FRotator(-45.f, 0.f, 0.f));

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

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

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionAsset(
		TEXT("/Game/_Main/Input/IA_Move.IA_Move"));
	if (MoveActionAsset.Succeeded())
	{
		MoveAction = MoveActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionAsset(
		TEXT("/Game/_Main/Input/IA_Look.IA_Look"));
	if (LookActionAsset.Succeeded())
	{
		LookAction = LookActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> CameraRotateActionAsset(
		TEXT("/Game/_Main/Input/IA_CameraRotate.IA_CameraRotate"));
	if (CameraRotateActionAsset.Succeeded())
	{
		CameraRotateAction = CameraRotateActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AttackActionAsset(
		TEXT("/Game/_Main/Input/IA_Attack.IA_Attack"));
	if (AttackActionAsset.Succeeded())
	{
		AttackAction = AttackActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> HeroAttackSequenceAsset(
		TEXT("/Game/RPGHeroSquad/Animation/TinyHero/Anim_Attack01_TinyHero.Anim_Attack01_TinyHero"));
	if (HeroAttackSequenceAsset.Succeeded())
	{
		HeroAttackSequence = HeroAttackSequenceAsset.Object;
	}
}

void ATheGoldenAgePlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// A saved Blueprint tag list replaces the constructor value, so put the tag back on every instance.
	Tags.AddUnique(PlayerActorTag);
	if (GetClass()->GetName().Contains(TEXT("BP_Explorer")))
	{
		Tags.AddUnique(ExplorerActorTag);
	}
	else if (GetClass()->GetName().Contains(TEXT("BP_Hero")))
	{
		Tags.AddUnique(HeroActorTag);
	}

	if (!MoveAction)
	{
		MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_Move.IA_Move"));
	}
	if (!LookAction)
	{
		LookAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_Look.IA_Look"));
	}
	if (!CameraRotateAction)
	{
		CameraRotateAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_CameraRotate.IA_CameraRotate"));
	}
	if (!AttackAction)
	{
		AttackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_Attack.IA_Attack"));
	}

	if (Health)
	{
		Health->OnDied.AddDynamic(this, &ATheGoldenAgePlayerCharacter::HandleDied);
	}
	BindHealthBar();

	if (IsLocallyControlled())
	{
		ATheGoldenAgeMonster::ApplyLocalViewerVisibility(GetWorld(), this);
	}
}

void ATheGoldenAgePlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// Pass this pawn. The controller does not point at it until Possess returns.
	if (IsLocallyControlled())
	{
		ATheGoldenAgeMonster::ApplyLocalViewerVisibility(GetWorld(), this);
	}
}

void ATheGoldenAgePlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bDead = IsDead();
	if (IsLocallyControlled())
	{
		ATheGoldenAgeMonster::ApplyLocalViewerVisibility(GetWorld(), this);
		UpdateTopDownCameraOrientation(DeltaSeconds);
		if (!bDead && !bIsAttacking)
		{
			UpdateMovementFacing(DeltaSeconds);
		}
	}
	else if (!bDead)
	{
		// Other players are moved by transform snaps, so CharacterMovement velocity stays zero.
		UpdateRemotePresentationVelocity(DeltaSeconds);
	}

	UpdateBattleLocomotionAnimation();
	UpdateHeroAttackAnimation(DeltaSeconds);
}

void ATheGoldenAgePlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Input is set up on possess, which can happen before BeginPlay, so resolve the asset here too.
	if (!MoveAction)
	{
		MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_Move.IA_Move"));
	}
	if (!LookAction)
	{
		LookAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_Look.IA_Look"));
	}
	if (!CameraRotateAction)
	{
		CameraRotateAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_CameraRotate.IA_CameraRotate"));
	}
	if (!AttackAction)
	{
		AttackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_Attack.IA_Attack"));
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATheGoldenAgePlayerCharacter::Move);
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ATheGoldenAgePlayerCharacter::Move);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATheGoldenAgePlayerCharacter::Look);
		}
		if (CameraRotateAction)
		{
			EnhancedInput->BindAction(CameraRotateAction, ETriggerEvent::Started, this, &ATheGoldenAgePlayerCharacter::OnCameraRotateStarted);
			EnhancedInput->BindAction(CameraRotateAction, ETriggerEvent::Completed, this, &ATheGoldenAgePlayerCharacter::OnCameraRotateCompleted);
			EnhancedInput->BindAction(CameraRotateAction, ETriggerEvent::Canceled, this, &ATheGoldenAgePlayerCharacter::OnCameraRotateCompleted);
		}
		if (AttackAction)
		{
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &ATheGoldenAgePlayerCharacter::Attack);
		}
	}
}

void ATheGoldenAgePlayerCharacter::Move(const FInputActionValue& Value)
{
	if (IsDead())
	{
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	if (MovementVector.IsNearlyZero())
	{
		LastMovementInputDirection = FVector::ZeroVector;
		return;
	}

	const FRotator CameraYawRot(0.f, CameraYaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(CameraYawRot).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(CameraYawRot).GetUnitAxis(EAxis::Y);

	const FVector WorldMoveDirection = (ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X).GetSafeNormal();
	LastMovementInputDirection = WorldMoveDirection;

	// Single call with a clamped scale so diagonals are never faster than cardinal movement.
	AddMovementInput(WorldMoveDirection, FMath::Min(MovementVector.Size(), 1.f));
}

void ATheGoldenAgePlayerCharacter::RequestFaceDirection(const FVector& WorldDirection)
{
	const FVector FlatDirection = WorldDirection.GetSafeNormal2D();
	if (FlatDirection.IsNearlyZero())
	{
		return;
	}

	SetActorRotation(FRotationMatrix::MakeFromX(FlatDirection).Rotator());
}

void ATheGoldenAgePlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	TargetCameraYaw += LookVector.X * CameraYawRotateSpeed;

	const float PitchMin = FMath::Min(CameraPitchMin, CameraPitchMax);
	const float PitchMax = FMath::Max(CameraPitchMin, CameraPitchMax);
	TargetCameraPitch = FMath::Clamp(TargetCameraPitch + LookVector.Y * CameraPitchRotateSpeed, PitchMin, PitchMax);
}

void ATheGoldenAgePlayerCharacter::OnCameraRotateStarted()
{
	bRotatingCamera = true;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->GetMousePosition(SavedMouseX, SavedMouseY);
		PC->CurrentMouseCursor = EMouseCursor::None;
		PC->SetShowMouseCursor(false);
	}
}

void ATheGoldenAgePlayerCharacter::Attack(const FInputActionValue& /*Value*/)
{
	if (!IsLocallyControlled() || IsDead() || bIsAttacking)
	{
		return;
	}

	if (!ActorHasTag(HeroActorTag))
	{
		return;
	}

	if (ATheGoldenAgePlayerController* GoldenAgePC = Cast<ATheGoldenAgePlayerController>(GetController()))
	{
		FVector GroundTarget = FVector::ZeroVector;
		if (GoldenAgePC->GetMouseCursorGroundLocationForPawn(GroundTarget))
		{
			RequestFaceDirection(GroundTarget - GetActorLocation());
		}
	}

	BeginHeroAttackPresentation();
}

void ATheGoldenAgePlayerCharacter::BeginHeroAttackPresentation()
{
	bIsAttacking = true;
	bHasAttackSample = false;
	bHeroAttackHitApplied = false;
	AttackElapsedSeconds = 0.f;

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		if (FAnimNode_SequencePlayer* AttackPlayer = FindHeroAttackSequencePlayer(AnimInstance, HeroAttackSequence))
		{
			AttackPlayer->SetAccumulatedTime(0.f);
		}
		PushHeroAttackAnimState(AnimInstance);
	}

	if (IsLocallyControlled())
	{
		NotifyLocalTransformPublish();
	}
}

void ATheGoldenAgePlayerCharacter::NotifyLocalTransformPublish()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UTheGoldenAgePlayerSync* Sync = World->GetSubsystem<UTheGoldenAgePlayerSync>())
		{
			Sync->RequestLocalTransformPublish();
		}
	}
}

void ATheGoldenAgePlayerCharacter::ApplyServerAttackState(bool bServerIsAttacking)
{
	if (IsLocallyControlled() || !ActorHasTag(HeroActorTag) || IsDead())
	{
		return;
	}

	if (bServerIsAttacking && !bIsAttacking)
	{
		BeginHeroAttackPresentation();
	}
}

void ATheGoldenAgePlayerCharacter::OnCameraRotateCompleted()
{
	if (!bRotatingCamera)
	{
		return;
	}

	bRotatingCamera = false;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->CurrentMouseCursor = EMouseCursor::Default;
		PC->SetShowMouseCursor(true);
		PC->SetMouseLocation(FMath::RoundToInt(SavedMouseX), FMath::RoundToInt(SavedMouseY));
	}
}

void ATheGoldenAgePlayerCharacter::UpdateTopDownCameraOrientation(float DeltaSeconds)
{
	if (!CameraBoom)
	{
		return;
	}

	const FRotator SmoothedRotation = FMath::RInterpTo(
		FRotator(CameraPitch, CameraYaw, 0.f),
		FRotator(TargetCameraPitch, TargetCameraYaw, 0.f),
		DeltaSeconds,
		CameraRotationInterpSpeed);
	CameraPitch = SmoothedRotation.Pitch;
	CameraYaw = SmoothedRotation.Yaw;

	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetWorldRotation(SmoothedRotation);
}

void ATheGoldenAgePlayerCharacter::UpdateRemotePresentationVelocity(float DeltaSeconds)
{
	const FVector CurrentLocation = GetActorLocation();
	if (!bHasRemotePresentationSample)
	{
		RemotePresentationLocation = CurrentLocation;
		bHasRemotePresentationSample = true;
		RemotePresentationVelocity = FVector::ZeroVector;
		RemotePresentationSampleAge = 0.f;
		RemoteStillTime = 0.f;
		return;
	}

	RemotePresentationSampleAge += DeltaSeconds;
	const FVector Delta = CurrentLocation - RemotePresentationLocation;
	const float Distance2D = Delta.Size2D();

	if (Distance2D > RemoteTeleportDistance)
	{
		RemotePresentationLocation = CurrentLocation;
		RemotePresentationVelocity = FVector::ZeroVector;
		RemotePresentationSampleAge = 0.f;
		RemoteStillTime = 0.f;
		return;
	}

	if (Distance2D >= 1.f)
	{
		const float SampleDt = FMath::Max(RemotePresentationSampleAge, KINDA_SMALL_NUMBER);
		RemotePresentationVelocity = Delta / SampleDt;
		RemotePresentationLocation = CurrentLocation;
		RemotePresentationSampleAge = 0.f;
		RemoteStillTime = 0.f;
		return;
	}

	RemoteStillTime += DeltaSeconds;
	if (RemoteStillTime >= RemoteIdleGraceSeconds)
	{
		RemotePresentationVelocity = FVector::ZeroVector;
		// Drop the idle gap so the next step is not averaged down into a standstill.
		RemotePresentationSampleAge = 0.f;
	}
}

FVector ATheGoldenAgePlayerCharacter::GetLocomotionVelocity() const
{
	return IsLocallyControlled() ? GetVelocity() : RemotePresentationVelocity;
}

void ATheGoldenAgePlayerCharacter::UpdateMovementFacing(float DeltaSeconds)
{
	const float Speed2D = GetVelocity().Size2D();
	FVector FaceDirection = FVector::ZeroVector;

	if (!LastMovementInputDirection.IsNearlyZero())
	{
		FaceDirection = LastMovementInputDirection;
	}
	else if (Speed2D >= MinMovingSpeed)
	{
		FaceDirection = GetVelocity().GetSafeNormal2D();
	}

	if (!FaceDirection.IsNearlyZero())
	{
		const FRotator TargetRotation = FRotationMatrix::MakeFromX(FaceDirection).Rotator();
		const FRotator NewRotation = FMath::RInterpTo(
			GetActorRotation(),
			TargetRotation,
			DeltaSeconds,
			FacingInterpSpeed);
		SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));
	}
}

ETheGoldenAgeBattleMoveDirection ATheGoldenAgePlayerCharacter::SelectBattleMoveDirection(const FVector& WorldVelocity) const
{
	const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(WorldVelocity);
	const FVector FlatLocal(LocalVelocity.X, LocalVelocity.Y, 0.f);

	if (FlatLocal.IsNearlyZero())
	{
		return ETheGoldenAgeBattleMoveDirection::None;
	}

	if (FMath::Abs(FlatLocal.X) >= FMath::Abs(FlatLocal.Y))
	{
		return FlatLocal.X >= 0.f ? ETheGoldenAgeBattleMoveDirection::Right : ETheGoldenAgeBattleMoveDirection::Left;
	}

	return FlatLocal.Y >= 0.f ? ETheGoldenAgeBattleMoveDirection::Forward : ETheGoldenAgeBattleMoveDirection::Backward;
}

void ATheGoldenAgePlayerCharacter::UpdateBattleLocomotionAnimation()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	const FVector LocomotionVelocity = GetLocomotionVelocity();
	const float Speed2D = LocomotionVelocity.Size2D();
	const bool bIsMoving = Speed2D >= MinMovingSpeed;

	CurrentBattleMoveDirection = bIsMoving
		? SelectBattleMoveDirection(LocomotionVelocity)
		: ETheGoldenAgeBattleMoveDirection::None;

	const float PlayRate = bIsMoving
		? FMath::Clamp(Speed2D / AnimPlayRateReferenceSpeed, AnimPlayRateMin, AnimPlayRateMax)
		: 1.f;

	// Both Anim Blueprints treat 0 as idle. ABP_Hero plays the forward clip for every other value.
	// ABP_Explorer still maps 1 forward, 2 backward, 3 left, and any other value to its last move pose.
	// Those numbers match ETheGoldenAgeBattleMoveDirection. Do not insert an enum value ahead of Right.
	UClass* AnimClass = AnimInstance->GetClass();
	if (FByteProperty* DirectionProperty = FindFProperty<FByteProperty>(AnimClass, TEXT("BattleMoveDirection")))
	{
		DirectionProperty->SetPropertyValue_InContainer(AnimInstance, static_cast<uint8>(CurrentBattleMoveDirection));
	}
	if (FDoubleProperty* PlayRateProperty = FindFProperty<FDoubleProperty>(AnimClass, TEXT("LocomotionPlayRate")))
	{
		PlayRateProperty->SetPropertyValue_InContainer(AnimInstance, static_cast<double>(PlayRate));
	}
	if (FBoolProperty* DeadProperty = FindFProperty<FBoolProperty>(AnimClass, TEXT("bIsDead")))
	{
		DeadProperty->SetPropertyValue_InContainer(AnimInstance, IsDead());
	}

	ApplyLocomotionPlayRates(AnimInstance, PlayRate);
}

void ATheGoldenAgePlayerCharacter::UpdateHeroAttackAnimation(float DeltaSeconds)
{
	if (!ActorHasTag(HeroActorTag))
	{
		return;
	}

	const bool bLocal = IsLocallyControlled();
	if (!bLocal && !bIsAttacking)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	if (IsDead())
	{
		bIsAttacking = false;
		bHasAttackSample = false;
		bHeroAttackHitApplied = false;
		AttackElapsedSeconds = 0.f;
		PushHeroAttackAnimState(AnimInstance);
		return;
	}

	if (bIsAttacking)
	{
		AttackElapsedSeconds += DeltaSeconds;

		float ExpectedDuration = HeroAttackSequence ? HeroAttackSequence->GetPlayLength() : 0.f;
		FAnimNode_SequencePlayer* AttackPlayer = FindHeroAttackSequencePlayer(AnimInstance, HeroAttackSequence);

		bool bFinished = ExpectedDuration > KINDA_SMALL_NUMBER
			&& AttackElapsedSeconds >= ExpectedDuration - AttackEndToleranceSeconds;

		if (AttackPlayer)
		{
			const float PlayLength = AttackPlayer->GetCurrentAssetLength();
			if (PlayLength > KINDA_SMALL_NUMBER)
			{
				ExpectedDuration = FMath::Max(ExpectedDuration, PlayLength);
				const float PlaybackTime = AttackPlayer->GetCurrentAssetTime();
				if (HasCurrentAttackFinished(PlaybackTime, PlayLength))
				{
					bFinished = true;
				}
				else
				{
					AttackSampleTime = PlaybackTime;
					bHasAttackSample = true;
				}
			}
			else
			{
				bHasAttackSample = false;
			}

			if (!bFinished && ExpectedDuration > KINDA_SMALL_NUMBER)
			{
				bFinished = AttackElapsedSeconds >= ExpectedDuration - AttackEndToleranceSeconds;
			}
		}
		else
		{
			bHasAttackSample = false;
		}

		float HitPlaybackTime = AttackElapsedSeconds;
		float HitPlayLength = ExpectedDuration;
		if (AttackPlayer && AttackPlayer->GetCurrentAssetLength() > KINDA_SMALL_NUMBER)
		{
			HitPlaybackTime = AttackPlayer->GetCurrentAssetTime();
			HitPlayLength = AttackPlayer->GetCurrentAssetLength();
		}
		else if (HeroAttackSequence)
		{
			HitPlayLength = FMath::Max(HitPlayLength, HeroAttackSequence->GetPlayLength());
		}

		if (bLocal)
		{
			TryHeroAttackHit(HitPlaybackTime, HitPlayLength);
		}

		if (bFinished)
		{
			const bool bWasAttacking = bIsAttacking;
			bIsAttacking = false;
			bHasAttackSample = false;
			bHeroAttackHitApplied = false;
			AttackElapsedSeconds = 0.f;
			if (bLocal && bWasAttacking)
			{
				NotifyLocalTransformPublish();
			}
		}
	}

	PushHeroAttackAnimState(AnimInstance);
}

void ATheGoldenAgePlayerCharacter::TryHeroAttackHit(float AttackPlaybackTime, float AttackPlayLength)
{
	if (bHeroAttackHitApplied || HeroAttackDamage <= 0.f || HeroAttackRadius <= 0.f)
	{
		return;
	}

	const float EffectiveLength = AttackPlayLength > KINDA_SMALL_NUMBER
		? AttackPlayLength
		: (HeroAttackSequence ? HeroAttackSequence->GetPlayLength() : 0.f);
	if (EffectiveLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float HitTime = EffectiveLength * HeroAttackHitTimeNormalized;
	if (AttackPlaybackTime + AttackEndToleranceSeconds < HitTime)
	{
		return;
	}

	bHeroAttackHitApplied = true;

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

	const FVector Origin = GetActorLocation() + Forward * HeroAttackReach;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeroAttackOverlap), false, this);
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	if (!World->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(HeroAttackRadius),
		QueryParams))
	{
		return;
	}

	TSet<uint64> DamagedIds;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ATheGoldenAgeMonster* Monster = Cast<ATheGoldenAgeMonster>(Overlap.GetActor());
		if (!Monster)
		{
			continue;
		}

		const uint64 MonsterId = Monster->GetMonsterId();
		if (MonsterId == 0 || Monster->IsDead() || DamagedIds.Contains(MonsterId))
		{
			continue;
		}

		DamagedIds.Add(MonsterId);
		MonsterSync->RequestMonsterDamage(MonsterId, HeroAttackDamage);
	}
}

bool ATheGoldenAgePlayerCharacter::HasCurrentAttackFinished(float PlaybackTime, float PlayLength) const
{
	if (PlayLength <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const bool bReachedEnd = PlaybackTime >= PlayLength - AttackEndToleranceSeconds;
	const bool bWrapped = bHasAttackSample && PlaybackTime + AttackEndToleranceSeconds < AttackSampleTime;
	return bReachedEnd || bWrapped;
}

void ATheGoldenAgePlayerCharacter::PushHeroAttackAnimState(UAnimInstance* AnimInstance)
{
	UClass* AnimClass = AnimInstance->GetClass();
	if (FBoolProperty* IsAttackingProperty = FindFProperty<FBoolProperty>(AnimClass, TEXT("bIsAttacking")))
	{
		IsAttackingProperty->SetPropertyValue_InContainer(AnimInstance, bIsAttacking);
	}
}

void ATheGoldenAgePlayerCharacter::ApplyDamage(float Amount)
{
	if (!IsLocallyControlled() || !FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UTheGoldenAgePlayerSync* Sync = World->GetSubsystem<UTheGoldenAgePlayerSync>())
		{
			Sync->RequestLocalDamage(Amount);
		}
	}
}

void ATheGoldenAgePlayerCharacter::SetSpacetimePlayerName(const FString& Name)
{
	SpacetimePlayerName = Name;
}

void ATheGoldenAgePlayerCharacter::ApplyServerHealth(float NewHealth)
{
	if (Health)
	{
		Health->ApplyServerHealth(NewHealth);
	}
	BindHealthBar();
}

bool ATheGoldenAgePlayerCharacter::IsDead() const
{
	return Health && Health->IsDead();
}

void ATheGoldenAgePlayerCharacter::BindHealthBar()
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

void ATheGoldenAgePlayerCharacter::HandleDied()
{
	bIsAttacking = false;
	bHasAttackSample = false;
	bHeroAttackHitApplied = false;
	AttackElapsedSeconds = 0.f;
	LastMovementInputDirection = FVector::ZeroVector;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
}

void ATheGoldenAgePlayerCharacter::HandleRespawned()
{
	LastMovementInputDirection = FVector::ZeroVector;
	RemotePresentationVelocity = FVector::ZeroVector;
	RemotePresentationLocation = GetActorLocation();
	bHasRemotePresentationSample = true;
	RemotePresentationSampleAge = 0.f;
	RemoteStillTime = 0.f;

	if (!IsLocallyControlled())
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}
}
