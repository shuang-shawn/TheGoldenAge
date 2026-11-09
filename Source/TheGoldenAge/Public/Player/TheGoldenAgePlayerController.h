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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 DefaultMappingContextPriority = 0;

	void AddDefaultMappingContext();
};
