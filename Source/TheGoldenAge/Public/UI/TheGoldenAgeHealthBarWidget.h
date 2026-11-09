// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TheGoldenAgeHealthBarWidget.generated.h"

class UProgressBar;
class UTheGoldenAgeHealthComponent;

/** Horizontal bar bound to a player health component. */
UCLASS()
class THEGOLDENAGE_API UTheGoldenAgeHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTheGoldenAgeHealthBarWidget(const FObjectInitializer& ObjectInitializer);

	void SetHealthComponent(UTheGoldenAgeHealthComponent* InHealth);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleHealthChanged(float InHealth, float InMaxHealth);

	UPROPERTY()
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY()
	TObjectPtr<UTheGoldenAgeHealthComponent> HealthComponent;
};
