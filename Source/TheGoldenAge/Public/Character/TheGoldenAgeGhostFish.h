// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/TheGoldenAgeMonster.h"
#include "TheGoldenAgeGhostFish.generated.h"

/**
 * Ghost Fish: idle until a player is inside the shared detection range,
 * then turn to face the closest one and loop the attack animation.
 * When the last player leaves, the attack cycle already playing finishes before idle resumes.
 * Behaviour currently matches the Chest Monster. Clip choice lives in ABP_GhostFish.
 * This class reports whether it is attacking and whether that attack should keep looping.
 */
UCLASS()
class THEGOLDENAGE_API ATheGoldenAgeGhostFish : public ATheGoldenAgeMonster
{
	GENERATED_BODY()

protected:
	/** How fast the actor yaws toward the closest player, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost Fish", meta = (ClampMin = "0.0"))
	float FacingTurnRate = 480.f;

	/**
	 * True while the attack should keep playing. Stays true after the last player
	 * leaves until the cycle already in progress reaches its last frame.
	 * Copied each tick onto ABP_GhostFish's bIsAttacking, which selects the attack clip.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Fish|Animation")
	bool bIsAttacking = false;

	/**
	 * True while the attack should loop. False only while the current cycle plays out
	 * after the last player leaves, so the clip holds its last frame instead of wrapping.
	 * Copied each tick onto ABP_GhostFish's bLoopAttack.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Fish|Animation")
	bool bLoopAttack = true;

	/** Previous attack playback time, used to notice a cycle that already wrapped. */
	float AttackSampleTime = 0.f;

	/** True once AttackSampleTime has been taken while this attack is playing. */
	bool bHasAttackSample = false;

	virtual void HandleNearbyPlayers_Implementation(const TArray<AActor*>& NearbyPlayers, float DeltaSeconds) override;

	AActor* FindClosestPlayer(const TArray<AActor*>& NearbyPlayers) const;
	void FacePlayer(const AActor* Player, float DeltaSeconds);
	bool HasCurrentAttackFinished(float PlaybackTime, float PlayLength) const;
	void PushAnimState();
};
