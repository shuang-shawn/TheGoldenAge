// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/TheGoldenAgeHealthComponent.h"
#include "GameFramework/Character.h"
#include "TheGoldenAgePlayerCharacter.generated.h"

class UAnimSequenceBase;
class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class UWidgetComponent;

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
 * Top-down player character: camera-relative WASD movement, movement-facing, battle in-place locomotion.
 * Clip choice lives in ABP_Hero and ABP_Explorer. This class reports the move direction, play rate, and bIsDead.
 * Health comes from the player row. At zero the character stops and the Anim Blueprint plays its die clip once.
 * One second later the server restores full health at the default spawn, and this pawn walks again.
 */
UCLASS()
class THEGOLDENAGE_API ATheGoldenAgePlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** Actor tag present on every player character. Enemies use it to find players. */
	static const FName PlayerActorTag;

	/** On BP_Hero. A Hero viewer can see monsters tagged Ghost. */
	static const FName HeroActorTag;

	/** On BP_Explorer. An Explorer viewer can see monsters tagged Trap. */
	static const FName ExplorerActorTag;

	ATheGoldenAgePlayerCharacter();

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void Move(const struct FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void Look(const struct FInputActionValue& Value);

	void OnCameraRotateStarted();
	void OnCameraRotateCompleted();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void Attack(const struct FInputActionValue& Value);

	/** Snaps the character to a world-horizontal facing direction (e.g. combat aim from the Player Controller). */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void RequestFaceDirection(const FVector& WorldDirection);

	/** Asks the server to damage this logged-in player. The row update applies the result. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Amount);

	void ApplyServerHealth(float NewHealth);

	/** Remote pawns: begin Attack 01 when the player row reports is_attacking. */
	void ApplyServerAttackState(bool bServerIsAttacking);

	/** Walking again after a server respawn. Call after the pawn is already at the spawn point. */
	void HandleRespawned();

	UFUNCTION(BlueprintPure, Category = "Health")
	UTheGoldenAgeHealthComponent* GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const;

	/** True while the Hero attack clip is playing (replicated via player.is_attacking). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAttackingForReplication() const { return bIsAttacking; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UTheGoldenAgeHealthComponent> Health;

	/** Screen-space bar. Pivot is the top edge, so the bar hangs below the feet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UWidgetComponent> HealthBar;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

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

	/** Clip used to time Hero attack end detection when the Anim Graph sequence player is unavailable. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> HeroAttackSequence;

	/** Health removed from each overlapped server monster during one Hero attack. Tune on BP_Hero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float HeroAttackDamage = 50.f;

	/** Forward reach of the attack overlap from the capsule center (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float HeroAttackReach = 140.f;

	/** Radius of the attack overlap sphere (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float HeroAttackRadius = 90.f;

	/** Normalized time in the attack clip when hit detection runs once (0–1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeroAttackHitTimeNormalized = 0.35f;

	/** How long a remote pawn keeps its last move clip after position updates stop. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RemoteIdleGraceSeconds = 0.15f;

	/** A position jump larger than this is a teleport, not locomotion. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "1.0"))
	float RemoteTeleportDistance = 800.f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	ETheGoldenAgeBattleMoveDirection CurrentBattleMoveDirection = ETheGoldenAgeBattleMoveDirection::None;

	void UpdateTopDownCameraOrientation(float DeltaSeconds);
	void UpdateMovementFacing(float DeltaSeconds);
	void UpdateRemotePresentationVelocity(float DeltaSeconds);
	void UpdateBattleLocomotionAnimation();
	void BeginHeroAttackPresentation();
	void NotifyLocalTransformPublish();
	void UpdateHeroAttackAnimation(float DeltaSeconds);
	bool HasCurrentAttackFinished(float PlaybackTime, float PlayLength) const;
	void TryHeroAttackHit(float AttackPlaybackTime, float AttackPlayLength);
	void PushHeroAttackAnimState(UAnimInstance* AnimInstance);
	void BindHealthBar();

	UFUNCTION()
	void HandleDied();

	ETheGoldenAgeBattleMoveDirection SelectBattleMoveDirection(const FVector& WorldVelocity) const;
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

	/** Drives ABP_Hero's attack blend while the local Hero plays Attack 01 once. */
	bool bIsAttacking = false;
	float AttackElapsedSeconds = 0.f;
	float AttackSampleTime = 0.f;
	bool bHasAttackSample = false;
	bool bHeroAttackHitApplied = false;
};
