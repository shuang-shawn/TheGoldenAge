#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TheGoldenAgePlayMenuWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTheGoldenAgePlayRequested);

UCLASS()
class THEGOLDENAGE_API UTheGoldenAgePlayMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Spacetime")
	FOnTheGoldenAgePlayRequested OnPlayRequested;

	UTheGoldenAgePlayMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandlePlayClicked();

	UPROPERTY()
	TObjectPtr<UButton> PlayButton;
};
