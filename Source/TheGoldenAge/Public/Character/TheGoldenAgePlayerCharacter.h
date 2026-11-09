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
	/** Actor tag present on every player character. Enemies use it to find players. */
	static const FName PlayerActorTag;

	ATheGoldenAgePlayerCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void Move(const struct FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void Look(const struct FInputActionValue& Value);

	void OnCameraRotateStarted();
	void OnCameraRotateCompleted();

	/** Future combat systems may request rotation toward a world direction without coupling to cursor polling. */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void RequestFaceDirection(const FVector& WorldDirection);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Boom pitch while orbiting. More negative looks steeper down at the character. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "-85", ClampMax = "-10"))
	float CameraPitchMin = -75.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "-85", ClampMax = "-10"))
	float CameraPitchMax = -20.f;

	/** Degrees applied per unit of mouse delta while right mouse is held. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.1"))
	float CameraYawRotateSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.1"))
	float CameraPitchRotateSpeed = 5.f;

	/** How quickly the boom catches the orbit target. Higher is snappier. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.1"))
	float CameraRotationInterpSpeed = 14.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Mouse delta. Chorded to CameraRotateAction so it only triggers while that action is held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	/** Right mouse button. Started and completed drive cursor capture around Look. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraRotateAction;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float MinMovingSpeed = 10.f;

	/** How quickly the mesh yaws toward the camera-relative move direction. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.1"))
	float FacingInterpSpeed = 24.f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float AnimPlayRateReferenceSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.25", ClampMax = "3.0"))
	float AnimPlayRateMin = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.25", ClampMax = "3.0"))
	float AnimPlayRateMax = 1.4f;

	/** How long a remote pawn keeps its last move clip after position updates stop. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RemoteIdleGraceSeconds = 0.15f;

	/** A position jump larger than this is a teleport, not locomotion. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "1.0"))
	float RemoteTeleportDistance = 800.f;

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

	void UpdateTopDownCameraOrientation(float DeltaSeconds);
	void UpdateMovementFacing(float DeltaSeconds);
	void UpdateRemotePresentationVelocity(float DeltaSeconds);
	void UpdateBattleLocomotionAnimation();

	ETheGoldenAgeBattleMoveDirection SelectBattleMoveDirection(const FVector& WorldVelocity) const;
	UAnimSequence* GetBattleAnimationForDirection(ETheGoldenAgeBattleMoveDirection Direction) const;
	FVector GetLocomotionVelocity() const;

	FVector LastMovementInputDirection = FVector::ZeroVector;
	FVector RemotePresentationVelocity = FVector::ZeroVector;
	FVector RemotePresentationLocation = FVector::ZeroVector;
	float RemotePresentationSampleAge = 0.f;
	float RemoteStillTime = 0.f;
	bool bHasRemotePresentationSample = false;

	float CameraPitch = -45.f;
	float CameraYaw = 45.f;
	float TargetCameraPitch = -45.f;
	float TargetCameraYaw = 45.f;
	bool bRotatingCamera = false;
	float SavedMouseX = 0.f;
	float SavedMouseY = 0.f;
};
