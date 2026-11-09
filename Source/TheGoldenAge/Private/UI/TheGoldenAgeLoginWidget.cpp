#include "UI/TheGoldenAgeLoginWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Player/TheGoldenAgePlayerRole.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UTheGoldenAgeLoginWidget::UTheGoldenAgeLoginWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UTheGoldenAgeLoginWidget::RebuildWidget()
{
	BuildLayout();
	return Super::RebuildWidget();
}

void UTheGoldenAgeLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetConnectionReady(false);
}

void UTheGoldenAgeLoginWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* PanelBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBackground"));
	PanelBackground->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.94f));
	PanelBackground->SetPadding(FMargin(24.f));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelBackground))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LoginPanel"));
	PanelBackground->SetContent(Panel);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Player Login")));
	if (UVerticalBoxSlot* TitleSlot = Panel->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("Connecting to server...")));
	if (UVerticalBoxSlot* StatusSlot = Panel->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	UsernameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("UsernameBox"));
	UsernameBox->SetHintText(FText::FromString(TEXT("Username (letters and numbers)")));
	UsernameBox->SetMinDesiredWidth(360.f);
	if (UVerticalBoxSlot* UsernameSlot = Panel->AddChildToVerticalBox(UsernameBox))
	{
		UsernameSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		UsernameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	PasswordBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("PasswordBox"));
	PasswordBox->SetHintText(FText::FromString(TEXT("Password")));
	PasswordBox->SetIsPassword(true);
	PasswordBox->SetMinDesiredWidth(360.f);
	if (UVerticalBoxSlot* PasswordSlot = Panel->AddChildToVerticalBox(PasswordBox))
	{
		PasswordSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		PasswordSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	ErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ErrorText"));
	ErrorText->SetText(FText::GetEmpty());
	ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.25f, 0.25f)));
	if (UVerticalBoxSlot* ErrorSlot = Panel->AddChildToVerticalBox(ErrorText))
	{
		ErrorSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	LoginButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LoginButton"));
	UTextBlock* LoginLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoginLabel"));
	LoginLabel->SetText(FText::FromString(TEXT("Login")));
	LoginButton->AddChild(LoginLabel);
	LoginButton->OnClicked.AddDynamic(this, &UTheGoldenAgeLoginWidget::HandleLoginClicked);
	if (UVerticalBoxSlot* LoginSlot = Panel->AddChildToVerticalBox(LoginButton))
	{
		LoginSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	RoleBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RoleBox"));
	RoleBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* RoleSlot = Panel->AddChildToVerticalBox(RoleBox))
	{
		RoleSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	HeroButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("HeroButton"));
	UTextBlock* HeroLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeroLabel"));
	HeroLabel->SetText(FText::FromString(TEXT("Hero")));
	HeroButton->AddChild(HeroLabel);
	HeroButton->OnClicked.AddDynamic(this, &UTheGoldenAgeLoginWidget::HandleHeroClicked);
	if (UHorizontalBoxSlot* HeroSlot = RoleBox->AddChildToHorizontalBox(HeroButton))
	{
		HeroSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		HeroSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	ExplorerButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExplorerButton"));
	UTextBlock* ExplorerLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ExplorerLabel"));
	ExplorerLabel->SetText(FText::FromString(TEXT("Explorer")));
	ExplorerButton->AddChild(ExplorerLabel);
	ExplorerButton->OnClicked.AddDynamic(this, &UTheGoldenAgeLoginWidget::HandleExplorerClicked);
	if (UHorizontalBoxSlot* ExplorerSlot = RoleBox->AddChildToHorizontalBox(ExplorerButton))
	{
		ExplorerSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		ExplorerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	RoleBackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RoleBackButton"));
	UTextBlock* BackLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RoleBackLabel"));
	BackLabel->SetText(FText::FromString(TEXT("Back")));
	RoleBackButton->AddChild(BackLabel);
	RoleBackButton->OnClicked.AddDynamic(this, &UTheGoldenAgeLoginWidget::HandleRoleBackClicked);
	if (UHorizontalBoxSlot* BackSlot = RoleBox->AddChildToHorizontalBox(RoleBackButton))
	{
		BackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
}

void UTheGoldenAgeLoginWidget::RefreshEnabledState()
{
	const bool bCanEditCredentials = !bLoginPending && !bChoosingRole;

	if (LoginButton)
	{
		LoginButton->SetIsEnabled(bConnectionReady && !bLoginPending);
		LoginButton->SetVisibility(bChoosingRole ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (UsernameBox)
	{
		UsernameBox->SetIsEnabled(bCanEditCredentials);
	}

	if (PasswordBox)
	{
		PasswordBox->SetIsEnabled(bCanEditCredentials);
	}

	if (RoleBox)
	{
		RoleBox->SetVisibility(bChoosingRole ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	const bool bCanChooseRole = bChoosingRole && bConnectionReady && !bLoginPending;
	if (HeroButton)
	{
		HeroButton->SetIsEnabled(bCanChooseRole);
	}
	if (ExplorerButton)
	{
		ExplorerButton->SetIsEnabled(bCanChooseRole);
	}
	if (RoleBackButton)
	{
		RoleBackButton->SetIsEnabled(bCanChooseRole);
	}
}

void UTheGoldenAgeLoginWidget::SetConnectionReady(bool bReady)
{
	bConnectionReady = bReady;
	RefreshEnabledState();

	if (StatusText && !bChoosingRole)
	{
		StatusText->SetText(
			bReady ? FText::FromString(TEXT("Connected. Enter your credentials."))
				   : FText::FromString(TEXT("Connecting to server...")));
	}
}

void UTheGoldenAgeLoginWidget::SetLoginPending(bool bPending)
{
	bLoginPending = bPending;
	RefreshEnabledState();
	if (StatusText && bPending)
	{
		StatusText->SetText(FText::FromString(TEXT("Logging in...")));
	}
}

void UTheGoldenAgeLoginWidget::SetErrorMessage(const FString& Message)
{
	if (ErrorText)
	{
		ErrorText->SetText(FText::FromString(Message));
	}
}

void UTheGoldenAgeLoginWidget::ClearErrorMessage()
{
	SetErrorMessage(FString());
}

void UTheGoldenAgeLoginWidget::ShowRoleChoice()
{
	bChoosingRole = true;
	RefreshEnabledState();
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("New player. Choose Hero or Explorer.")));
	}
}

void UTheGoldenAgeLoginWidget::HideRoleChoice()
{
	bChoosingRole = false;
	RefreshEnabledState();
	if (!bLoginPending)
	{
		SetConnectionReady(bConnectionReady);
	}
}

void UTheGoldenAgeLoginWidget::HandleLoginClicked()
{
	if (!UsernameBox || !PasswordBox || bChoosingRole)
	{
		return;
	}

	OnLoginRequested.Broadcast(UsernameBox->GetText().ToString(), PasswordBox->GetText().ToString());
}

void UTheGoldenAgeLoginWidget::HandleHeroClicked()
{
	if (!bChoosingRole)
	{
		return;
	}

	OnRoleChosen.Broadcast(static_cast<uint8>(ETheGoldenAgePlayerRole::Hero));
}

void UTheGoldenAgeLoginWidget::HandleExplorerClicked()
{
	if (!bChoosingRole)
	{
		return;
	}

	OnRoleChosen.Broadcast(static_cast<uint8>(ETheGoldenAgePlayerRole::Explorer));
}

void UTheGoldenAgeLoginWidget::HandleRoleBackClicked()
{
	if (!bChoosingRole)
	{
		return;
	}

	HideRoleChoice();
	OnRoleChoiceCancelled.Broadcast();
}
