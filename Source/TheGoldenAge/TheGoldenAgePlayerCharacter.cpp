// Copyright Epic Games, Inc. All Rights Reserved.

#include "TheGoldenAgePlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

ATheGoldenAgePlayerCharacter::ATheGoldenAgePlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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
	CameraBoom->SetRelativeRotation(FRotator(-55.f, 0.f, 0.f));

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
}

void ATheGoldenAgePlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!MoveAction)
	{
		MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Main/Input/IA_Move.IA_Move"));
	}
}

void ATheGoldenAgePlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateTopDownCameraOrientation();
	UpdateMovementFacing(DeltaSeconds);
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

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATheGoldenAgePlayerCharacter::Move);
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ATheGoldenAgePlayerCharacter::Move);
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

	const FRotator CameraYawRot(0.f, CameraBoom->GetComponentRotation().Yaw, 0.f);
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

void ATheGoldenAgePlayerCharacter::UpdateTopDownCameraOrientation()
{
	if (!CameraBoom)
	{
		return;
	}

	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetWorldRotation(FRotator(-55.f, 45.f, 0.f));
}

void ATheGoldenAgePlayerCharacter::UpdateMovementFacing(float DeltaSeconds)
{
	const float Speed2D = GetVelocity().Size2D();
	FVector FaceDirection = FVector::ZeroVector;

	if (Speed2D >= MinMovingSpeed)
	{
		FaceDirection = GetVelocity().GetSafeNormal2D();
	}
	else if (!LastMovementInputDirection.IsNearlyZero())
	{
		return;
	}

	if (!FaceDirection.IsNearlyZero())
	{
		const FRotator TargetRotation = FRotationMatrix::MakeFromX(FaceDirection).Rotator();
		const FRotator NewRotation = FMath::RInterpTo(
			GetActorRotation(),
			TargetRotation,
			DeltaSeconds,
			GetCharacterMovement()->RotationRate.Yaw / 90.f);
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

	const float Speed2D = GetVelocity().Size2D();
	const bool bIsMoving = Speed2D >= MinMovingSpeed;

	CurrentBattleMoveDirection = bIsMoving
		? SelectBattleMoveDirection(GetVelocity())
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
