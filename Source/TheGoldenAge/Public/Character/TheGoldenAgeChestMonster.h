// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/TheGoldenAgeMonster.h"
#include "TheGoldenAgeChestMonster.generated.h"

/**
 * Chest Monster: idle until a player is inside the shared detection range,
 * then turn to face the closest one and loop the attack animation.
 * When the last player leaves, the attack cycle already playing finishes before idle resumes.
 * Clip choice lives in ABP_ChestMonster. This class reports whether it is attacking
 * and whether that attack should keep looping.
 */
UCLASS()
class THEGOLDENAGE_API ATheGoldenAgeChestMonster : public ATheGoldenAgeMonster
{
	GENERATED_BODY()

protected:
	/** How fast the actor yaws toward the closest player, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest Monster", meta = (ClampMin = "0.0"))
	float FacingTurnRate = 480.f;

	/**
	 * True while Attack 01 should keep playing. Stays true after the last player
	 * leaves until the cycle already in progress reaches its last frame.
	 * Copied each tick onto ABP_ChestMonster's bIsAttacking, which selects the attack clip.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest Monster|Animation")
	bool bIsAttacking = false;

	/**
	 * True only while a player is in range and Attack 01 should loop.
	 * False when nobody is nearby, including idle and the last cycle playing out,
	 * so the clip holds its last frame instead of wrapping.
	 * Copied each tick onto ABP_ChestMonster's bLoopAttack and published as loop_attack.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest Monster|Animation")
	bool bLoopAttack = false;

	/** Previous Attack 01 playback time, used to notice a cycle that already wrapped. */
	float AttackSampleTime = 0.f;

	/** True once AttackSampleTime has been taken while this attack is playing. */
	bool bHasAttackSample = false;

	virtual void HandleNearbyPlayers_Implementation(const TArray<AActor*>& NearbyPlayers, float DeltaSeconds) override;
	virtual void ApplyReplicatedAnim(bool bInAttacking, bool bInLoopAttack) override;
	virtual void GetServerAnimState(bool& bOutAttacking, bool& bOutLoopAttack) const override;
	virtual bool ShouldReleaseServerOwnership() const override;
	virtual float GetFacingTurnRate() const override;

	AActor* FindClosestPlayer(const TArray<AActor*>& NearbyPlayers) const;
	void FacePlayer(const AActor* Player, float DeltaSeconds);
	bool HasCurrentAttackFinished(float PlaybackTime, float PlayLength) const;
	void PushAnimState();
};
