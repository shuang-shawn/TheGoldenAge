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

protected:
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

private:
	void GatherPlayersInRange(TArray<AActor*>& OutPlayers) const;
};
