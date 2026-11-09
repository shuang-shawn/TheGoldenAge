#include "UI/TheGoldenAgeLoginWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
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
}

void UTheGoldenAgeLoginWidget::RefreshEnabledState()
{
	if (LoginButton)
	{
		LoginButton->SetIsEnabled(bConnectionReady && !bLoginPending);
	}

	if (UsernameBox)
	{
		UsernameBox->SetIsEnabled(!bLoginPending);
	}

	if (PasswordBox)
	{
		PasswordBox->SetIsEnabled(!bLoginPending);
	}
}

void UTheGoldenAgeLoginWidget::SetConnectionReady(bool bReady)
{
	bConnectionReady = bReady;
	RefreshEnabledState();

	if (StatusText)
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

void UTheGoldenAgeLoginWidget::HandleLoginClicked()
{
	if (!UsernameBox || !PasswordBox)
	{
		return;
	}

	OnLoginRequested.Broadcast(UsernameBox->GetText().ToString(), PasswordBox->GetText().ToString());
}
