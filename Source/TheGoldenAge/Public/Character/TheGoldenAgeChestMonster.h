// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/TheGoldenAgeMonster.h"
#include "TheGoldenAgeChestMonster.generated.h"

struct FAnimNode_SequencePlayer;

/**
 * Chest Monster presentation. The server turns it and sets loop_attack while a player
 * is inside detection range. This class plays that attack and lets the current cycle
 * finish after loop_attack clears. Clip choice lives in ABP_ChestMonster.
 */
UCLASS()
class THEGOLDENAGE_API ATheGoldenAgeChestMonster : public ATheGoldenAgeMonster
{
	GENERATED_BODY()

protected:
	/**
	 * True while Attack 01 should keep playing. Stays true after loop_attack clears
	 * until the cycle already in progress reaches its last frame.
	 * Copied each tick onto ABP_ChestMonster's bIsAttacking.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest Monster|Animation")
	bool bIsAttacking = false;

	/**
	 * Mirrors the server loop_attack flag, including the last cycle playing out.
	 * False makes the clip hold its last frame instead of wrapping.
	 * Copied each tick onto ABP_ChestMonster's bLoopAttack.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest Monster|Animation")
	bool bLoopAttack = false;

	/** Previous Attack 01 playback time, used to notice a cycle that already wrapped. */
	float AttackSampleTime = 0.f;

	/** True once AttackSampleTime has been taken while this attack is playing. */
	bool bHasAttackSample = false;

	virtual void UpdatePresentedAnim() override;

	bool HasCurrentAttackFinished(float PlaybackTime, float PlayLength) const;
	void PushAnimState();
};
