// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/TheGoldenAgeMonster.h"
#include "TheGoldenAgeGhostFish.generated.h"

struct FAnimNode_SequencePlayer;

/**
 * Ghost Fish presentation. The server chases in a straight line and sets loop_attack
 * inside attack range. This class plays that attack and lets the current cycle finish
 * after loop_attack clears. Clip choice lives in ABP_GhostFish.
 */
UCLASS()
class THEGOLDENAGE_API ATheGoldenAgeGhostFish : public ATheGoldenAgeMonster
{
	GENERATED_BODY()

protected:
	/**
	 * True while the attack should keep playing. Stays true after loop_attack clears
	 * until the cycle already in progress reaches its last frame.
	 * Copied each tick onto ABP_GhostFish's bIsAttacking.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Fish|Animation")
	bool bIsAttacking = false;

	/**
	 * Mirrors the server loop_attack flag, including the last cycle playing out.
	 * False makes the clip hold its last frame instead of wrapping.
	 * Copied each tick onto ABP_GhostFish's bLoopAttack.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Fish|Animation")
	bool bLoopAttack = false;

	/** Previous attack playback time, used to notice a cycle that already wrapped. */
	float AttackSampleTime = 0.f;

	/** True once AttackSampleTime has been taken while this attack is playing. */
	bool bHasAttackSample = false;

	virtual void UpdatePresentedAnim() override;

	bool HasCurrentAttackFinished(float PlaybackTime, float PlayLength) const;
	/** Starts a looping attack, or lets the current cycle finish when bShouldLoop is false. */
	void ApplyAttackState(bool bShouldLoop, const FAnimNode_SequencePlayer* AttackPlayer);
	void PushAnimState();
};
