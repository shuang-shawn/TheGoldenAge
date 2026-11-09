// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TheGoldenAgeMonster.generated.h"

/**
 * Shared monster base. Finds player-tagged actors inside DetectionRange and
 * leaves the response to child classes.
 */
UCLASS(Abstract)
class THEGOLDENAGE_API ATheGoldenAgeMonster : public ACharacter
{
	GENERATED_BODY()

public:
	ATheGoldenAgeMonster();

	virtual void Tick(float DeltaSeconds) override;

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
