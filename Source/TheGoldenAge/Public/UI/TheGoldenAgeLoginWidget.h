#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TheGoldenAgeLoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTheGoldenAgeLoginRequested, const FString&, Username, const FString&, Password);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTheGoldenAgeRoleChosen, uint8, Role);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTheGoldenAgeRoleChoiceCancelled);

UCLASS()
class THEGOLDENAGE_API UTheGoldenAgeLoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Spacetime")
	FOnTheGoldenAgeLoginRequested OnLoginRequested;

	UPROPERTY(BlueprintAssignable, Category = "Spacetime")
	FOnTheGoldenAgeRoleChosen OnRoleChosen;

	UPROPERTY(BlueprintAssignable, Category = "Spacetime")
	FOnTheGoldenAgeRoleChoiceCancelled OnRoleChoiceCancelled;

	void SetConnectionReady(bool bReady);
	void SetLoginPending(bool bPending);
	void SetErrorMessage(const FString& Message);
	void ClearErrorMessage();
	void ShowRoleChoice();
	void HideRoleChoice();

	UTheGoldenAgeLoginWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildLayout();
	void RefreshEnabledState();

	UFUNCTION()
	void HandleLoginClicked();

	UFUNCTION()
	void HandleHeroClicked();

	UFUNCTION()
	void HandleExplorerClicked();

	UFUNCTION()
	void HandleRoleBackClicked();

	UPROPERTY()
	TObjectPtr<UEditableTextBox> UsernameBox;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> PasswordBox;

	UPROPERTY()
	TObjectPtr<UButton> LoginButton;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> RoleBox;

	UPROPERTY()
	TObjectPtr<UButton> HeroButton;

	UPROPERTY()
	TObjectPtr<UButton> ExplorerButton;

	UPROPERTY()
	TObjectPtr<UButton> RoleBackButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> ErrorText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	bool bConnectionReady = false;
	bool bLoginPending = false;
	bool bChoosingRole = false;
};
