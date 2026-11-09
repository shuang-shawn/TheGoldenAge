#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TheGoldenAgeLoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTheGoldenAgeLoginRequested, const FString&, Username, const FString&, Password);

UCLASS()
class THEGOLDENAGE_API UTheGoldenAgeLoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Spacetime")
	FOnTheGoldenAgeLoginRequested OnLoginRequested;

	void SetConnectionReady(bool bReady);
	void SetLoginPending(bool bPending);
	void SetErrorMessage(const FString& Message);
	void ClearErrorMessage();

	UTheGoldenAgeLoginWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildLayout();
	void RefreshEnabledState();

	UFUNCTION()
	void HandleLoginClicked();

	UPROPERTY()
	TObjectPtr<UEditableTextBox> UsernameBox;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> PasswordBox;

	UPROPERTY()
	TObjectPtr<UButton> LoginButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> ErrorText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	bool bConnectionReady = false;
	bool bLoginPending = false;
};
