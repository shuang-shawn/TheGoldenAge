// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgePlayerCharacter.h"

#include "Character/TheGoldenAgeMonster.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

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

	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleBattle(
		TEXT("/Game/RPGHeroSquad/Animation/TinyHero/Anim_Idle_Battle_TinyHero.Anim_Idle_Battle_TinyHero"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MoveFWD(
		TEXT("/Game/RPGHeroSquad/Animation/TinyHero/InPlace/Anim_MoveFWD_Battle_InPlace_TinyHero.Anim_MoveFWD_Battle_InPlace_TinyHero"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MoveBWD(
		TEXT("/Game/RPGHeroSquad/Animation/TinyHero/InPlace/Anim_MoveBWD_Battle_InPlace_TinyHero.Anim_MoveBWD_Battle_InPlace_TinyHero"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MoveLFT(
		TEXT("/Game/RPGHeroSquad/Animation/TinyHero/InPlace/Anim_MoveLFT_Battle_InPlace_TinyHero.Anim_MoveLFT_Battle_InPlace_TinyHero"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MoveRGT(
		TEXT("/Game/RPGHeroSquad/Animation/TinyHero/InPlace/Anim_MoveRGT_Battle_InPlace_TinyHero.Anim_MoveRGT_Battle_InPlace_TinyHero"));

	if (IdleBattle.Succeeded())
	{
		IdleBattleAnimation = IdleBattle.Object;
	}
	if (MoveFWD.Succeeded())
	{
		MoveForwardBattleAnimation = MoveFWD.Object;
	}
	if (MoveBWD.Succeeded())
	{
		MoveBackwardBattleAnimation = MoveBWD.Object;
	}
	if (MoveLFT.Succeeded())
	{
		MoveLeftBattleAnimation = MoveLFT.Object;
	}
	if (MoveRGT.Succeeded())
	{
		MoveRightBattleAnimation = MoveRGT.Object;
	}

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

	if (IsLocallyControlled())
	{
		ATheGoldenAgeMonster::ApplyLocalViewerVisibility(GetWorld(), this);
		UpdateTopDownCameraOrientation(DeltaSeconds);
		UpdateMovementFacing(DeltaSeconds);
	}
	else
	{
		// Other players are moved by transform snaps, so CharacterMovement velocity stays zero.
		UpdateRemotePresentationVelocity(DeltaSeconds);
	}

	UpdateBattleLocomotionAnimation();
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
	}
}

void ATheGoldenAgePlayerCharacter::Move(const FInputActionValue& Value)
{
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

UAnimSequence* ATheGoldenAgePlayerCharacter::GetBattleAnimationForDirection(ETheGoldenAgeBattleMoveDirection Direction) const
{
	switch (Direction)
	{
	case ETheGoldenAgeBattleMoveDirection::Forward:
		return MoveForwardBattleAnimation;
	case ETheGoldenAgeBattleMoveDirection::Backward:
		return MoveBackwardBattleAnimation;
	case ETheGoldenAgeBattleMoveDirection::Left:
		return MoveLeftBattleAnimation;
	case ETheGoldenAgeBattleMoveDirection::Right:
		return MoveRightBattleAnimation;
	default:
		return IdleBattleAnimation;
	}
}

void ATheGoldenAgePlayerCharacter::UpdateBattleLocomotionAnimation()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	if (MeshComp->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
	{
		MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}

	const FVector LocomotionVelocity = GetLocomotionVelocity();
	const float Speed2D = LocomotionVelocity.Size2D();
	const bool bIsMoving = Speed2D >= MinMovingSpeed;

	CurrentBattleMoveDirection = bIsMoving
		? SelectBattleMoveDirection(LocomotionVelocity)
		: ETheGoldenAgeBattleMoveDirection::None;

	UAnimSequence* DesiredAnimation = bIsMoving
		? GetBattleAnimationForDirection(CurrentBattleMoveDirection)
		: IdleBattleAnimation.Get();

	if (!DesiredAnimation)
	{
		return;
	}

	UAnimSingleNodeInstance* SingleNode = MeshComp->GetSingleNodeInstance();
	if (!SingleNode)
	{
		return;
	}

	if (SingleNode->GetCurrentAsset() != DesiredAnimation)
	{
		SingleNode->SetAnimationAsset(DesiredAnimation, true);
	}

	SingleNode->SetPlaying(true);
	SingleNode->SetLooping(true);

	const float PlayRate = bIsMoving
		? FMath::Clamp(Speed2D / AnimPlayRateReferenceSpeed, AnimPlayRateMin, AnimPlayRateMax)
		: 1.f;
	SingleNode->SetPlayRate(PlayRate);
}
