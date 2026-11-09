// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TheGoldenAgeMonster.generated.h"

/**
 * Shared monster base. Finds actors tagged Player inside DetectionRange and
 * leaves the response to child classes. Role tags do not affect detection.
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
	 * A Hero sees Ghost. An Explorer sees Trap. Detection is unchanged.
	 * Pass the possessed pawn: Possess assigns the controller pawn only after PossessedBy returns.
	 */
	static void ApplyLocalViewerVisibility(UWorld* World, const APawn* Viewer = nullptr);

	/** 0 means this actor is local-only and is not stored in the monster table. */
	uint64 GetMonsterId() const { return MonsterId; }

	/**
	 * Server monsters are driven by one claiming client. Everyone else turns toward the latest yaw.
	 * Local-only actors (MonsterId 0) ignore this.
	 */
	void InitializeFromServer(uint64 InMonsterId);
	void ApplyServerState(
		const FString& OwnerName,
		const FVector& Location,
		const FRotator& Rotation,
		bool bAttacking,
		bool bLoopAttack,
		const FString& LocalUsername);
	void NotifyClaimRejected();
	void NotifyReleaseRejected();
	void NotifyDisconnected();

protected:
	enum class EAuthority : uint8
	{
		Passive,
		Claiming,
		Owner
	};
	/** Horizontal distance from this actor to a player that counts as nearby. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster", meta = (ClampMin = "0.0"))
	float DetectionRange = 500.f;

	/**
	 * Nearby player-tagged actors this frame. Empty when nobody is inside DetectionRange.
	 * Child classes decide what that means.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Monster")
	void HandleNearbyPlayers(const TArray<AActor*>& NearbyPlayers, float DeltaSeconds);
	virtual void HandleNearbyPlayers_Implementation(const TArray<AActor*>& NearbyPlayers, float DeltaSeconds);

	/** Child classes copy these onto the Anim Blueprint. Passive monsters use the server values. */
	virtual void ApplyReplicatedAnim(bool bInAttacking, bool bInLoopAttack);

	/** Attack and loop flags the owner publishes. Defaults are idle. */
	virtual void GetServerAnimState(bool& bOutAttacking, bool& bOutLoopAttack) const;

	/** Owner drops the claim once nobody is nearby and the attack cycle has finished. */
	virtual bool ShouldReleaseServerOwnership() const;

	/** Degrees per second used when a remote client turns toward the latest server yaw. */
	virtual float GetFacingTurnRate() const;

private:
	void GatherPlayersInRange(TArray<AActor*>& OutPlayers) const;
	void ReportOwnedState(float DeltaSeconds);
	void TryReleaseOwnership(const TArray<AActor*>& NearbyPlayers);
	void TurnTowardServerYaw(float DeltaSeconds);

	uint64 MonsterId = 0;
	EAuthority Authority = EAuthority::Passive;
	FString ServerOwner;
	bool bServerAttacking = false;
	bool bServerLoopAttack = false;
	bool bReleaseRequested = false;
	bool bHasReportedServerState = false;
	float TimeSinceServerReport = 0.f;
	FVector LastReportedLocation = FVector::ZeroVector;
	FRotator LastReportedRotation = FRotator::ZeroRotator;
	bool bLastReportedAttacking = false;
	bool bLastReportedLoopAttack = false;
	bool bHasServerFacing = false;
	float ServerTargetYaw = 0.f;
};
