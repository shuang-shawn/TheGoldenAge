#include "UI/TheGoldenAgePlayMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UTheGoldenAgePlayMenuWidget::UTheGoldenAgePlayMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UTheGoldenAgePlayMenuWidget::RebuildWidget()
{
	BuildLayout();
	return Super::RebuildWidget();
}

void UTheGoldenAgePlayMenuWidget::BuildLayout()
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

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayPanel"));
	PanelBackground->SetContent(Panel);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("The Golden Age")));
	if (UVerticalBoxSlot* TitleSlot = Panel->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
	}

	PlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayButton"));
	UTextBlock* PlayLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayLabel"));
	PlayLabel->SetText(FText::FromString(TEXT("Play")));
	PlayButton->AddChild(PlayLabel);
	PlayButton->OnClicked.AddDynamic(this, &UTheGoldenAgePlayMenuWidget::HandlePlayClicked);
	Panel->AddChildToVerticalBox(PlayButton);
}

void UTheGoldenAgePlayMenuWidget::HandlePlayClicked()
{
	OnPlayRequested.Broadcast();
}
