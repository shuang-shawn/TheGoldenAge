// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TheGoldenAgeMonster.generated.h"

class UTheGoldenAgeHealthComponent;
class UWidgetComponent;

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

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyServerHealth(float NewHealth);

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	UTheGoldenAgeHealthComponent* GetHealth() const { return Health; }

	/** Clears stale death-position samples before the respawn row is applied. */
	void HandleRespawned();

	/** Renders the latest queued sample immediately (used right after respawn). */
	void FlushPresentedTransform();

	/**
	 * Once per attack cycle, overlaps local player pawns at MonsterAttackHitTimeNormalized
	 * and asks the server to apply that monster row's attack_damage.
	 */
	void TryMonsterAttackHit(float AttackPlaybackTime, float AttackPlayLength);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UTheGoldenAgeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UWidgetComponent> HealthBar;

	void PushHealthAnimState();
	void BindHealthBar();

	UFUNCTION()
	void HandleDied();
	/** True while the latest server row says the attack should keep looping. */
	bool WantsLoopingAttack() const { return bServerLoopAttack; }

	/**
	 * Copies the attack flags onto the Anim Blueprint.
	 * A falling loop_attack finishes the cycle already playing before idle resumes.
	 */
	virtual void UpdatePresentedAnim();

	/** Forward reach of the attack overlap from the capsule center (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float MonsterAttackReach = 110.f;

	/** Radius of the attack overlap sphere (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float MonsterAttackRadius = 95.f;

	/** Normalized time in the attack clip when hit detection runs once per cycle (0–1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MonsterAttackHitTimeNormalized = 0.35f;

	float MonsterAttackHitSampleTime = 0.f;
	bool bHasMonsterAttackHitSample = false;
	bool bMonsterAttackHitApplied = false;

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
