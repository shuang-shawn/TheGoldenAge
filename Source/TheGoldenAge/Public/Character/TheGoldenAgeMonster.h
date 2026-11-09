// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TheGoldenAgeMonster.generated.h"

/**
 * Shared monster base. The server decides position, facing, and attack flags.
 * This actor only renders the subscribed row. Role tags still decide who can see it.
 */
UCLASS(Abstract)
class THEGOLDENAGE_API ATheGoldenAgeMonster : public ACharacter
{
	GENERATED_BODY()

public:
	/** Present on every monster. Viewer rules use this together with Trap or Ghost. */
	static const FName MonsterActorTag;

	/** Chest-style monsters. Explorer viewers can see this tag. */
	static const FName TrapActorTag;

	/** Ghost-style monsters. Hero viewers can see this tag. */
	static const FName GhostActorTag;

	ATheGoldenAgeMonster();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Shows or hides monster-tagged actors for the local viewer.
	 * A Hero sees Ghost. An Explorer sees Trap.
	 * Pass the possessed pawn: Possess assigns the controller pawn only after PossessedBy returns.
	 */
	static void ApplyLocalViewerVisibility(UWorld* World, const APawn* Viewer = nullptr);

	/** 0 means this actor is not stored in the monster table. It does not run AI. */
	uint64 GetMonsterId() const { return MonsterId; }

	/** A spawned server monster. Movement stays off; Tick only plays the row. */
	void InitializeFromServer(uint64 InMonsterId);

	/** Queues one server sample. Tick renders about 100 ms behind the latest sample. */
	void ApplyServerState(const FVector& Location, const FRotator& Rotation, bool bAttacking, bool bLoopAttack);

protected:
	/** True while the latest server row says the attack should keep looping. */
	bool WantsLoopingAttack() const { return bServerLoopAttack; }

	/**
	 * Copies the attack flags onto the Anim Blueprint.
	 * A falling loop_attack finishes the cycle already playing before idle resumes.
	 */
	virtual void UpdatePresentedAnim();

private:
	struct FMonsterSnapshot
	{
		FVector Location = FVector::ZeroVector;
		float Yaw = 0.f;
		double ReceivedTime = 0.0;
	};

	void PresentSnapshot();
	void PushSnapshot(const FVector& Location, float Yaw);

	uint64 MonsterId = 0;
	bool bServerLoopAttack = false;
	TArray<FMonsterSnapshot> Snapshots;
};
