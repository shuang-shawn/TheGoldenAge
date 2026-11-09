// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TheGoldenAgePlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class THEGOLDENAGE_API ATheGoldenAgePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATheGoldenAgePlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	/** Console: DamageLocalPlayer 25. Asks the server to damage the possessed player. */
	UFUNCTION(Exec)
	void DamageLocalPlayer(float Amount);

	/** Console: DamageMonster 1 25. Asks the server to damage the monster row id. */
	UFUNCTION(Exec)
	void DamageMonster(uint64 MonsterId, float Amount);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 DefaultMappingContextPriority = 0;

	void AddDefaultMappingContext();
};
