// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TheGoldenAgePlayerCharacter.generated.h"

class UAnimSequence;
class UCameraComponent;
class UInputAction;
class USpringArmComponent;

UENUM(BlueprintType)
enum class ETheGoldenAgeBattleMoveDirection : uint8
{
	None UMETA(DisplayName = "None"),
	Forward UMETA(DisplayName = "Forward"),
	Backward UMETA(DisplayName = "Backward"),
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right")
};

/**
 * Top-down player character: camera-relative WASD movement, movement-facing, battle in-place locomotion anims.
 */
UCLASS()
class THEGOLDENAGE_API ATheGoldenAgePlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATheGoldenAgePlayerCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void Move(const struct FInputActionValue& Value);

	/** Future combat systems may request rotation toward a world direction without coupling to cursor polling. */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void RequestFaceDirection(const FVector& WorldDirection);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float MinMovingSpeed = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float AnimPlayRateReferenceSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.25", ClampMax = "3.0"))
	float AnimPlayRateMin = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.25", ClampMax = "3.0"))
	float AnimPlayRateMax = 1.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> IdleBattleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> MoveForwardBattleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> MoveBackwardBattleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> MoveLeftBattleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> MoveRightBattleAnimation;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	ETheGoldenAgeBattleMoveDirection CurrentBattleMoveDirection = ETheGoldenAgeBattleMoveDirection::None;

	void UpdateTopDownCameraOrientation();
	void UpdateMovementFacing(float DeltaSeconds);
	void UpdateBattleLocomotionAnimation();

	ETheGoldenAgeBattleMoveDirection SelectBattleMoveDirection(const FVector& WorldVelocity) const;
	UAnimSequence* GetBattleAnimationForDirection(ETheGoldenAgeBattleMoveDirection Direction) const;

	FVector LastMovementInputDirection = FVector::ZeroVector;
};
