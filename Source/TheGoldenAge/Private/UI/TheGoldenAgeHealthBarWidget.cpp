// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TheGoldenAgeHealthBarWidget.h"

#include "Character/TheGoldenAgeHealthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Styling/SlateTypes.h"

UTheGoldenAgeHealthBarWidget::UTheGoldenAgeHealthBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UTheGoldenAgeHealthBarWidget::RebuildWidget()
{
	BuildLayout();
	return Super::RebuildWidget();
}

void UTheGoldenAgeHealthBarWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	FSlateBrush BackgroundBrush = FSlateColorBrush(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f));
	FSlateBrush FillBrush = FSlateColorBrush(FLinearColor::White);
	BackgroundBrush.ImageSize = FVector2f(32.f, 8.f);
	FillBrush.ImageSize = FVector2f(32.f, 8.f);
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.SetBackgroundImage(BackgroundBrush);
	Style.SetFillImage(FillBrush);
	Style.SetMarqueeImage(FillBrush);
	Bar->SetWidgetStyle(Style);
	Bar->SetPercent(1.f);
	Bar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.72f, 0.22f, 1.f));

	if (UCanvasPanelSlot* BarSlot = RootCanvas->AddChildToCanvas(Bar))
	{
		BarSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BarSlot->SetOffsets(FMargin(0.f));
	}

	if (HealthComponent)
	{
		HandleHealthChanged(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
	}
}

void UTheGoldenAgeHealthBarWidget::SetHealthComponent(UTheGoldenAgeHealthComponent* InHealth)
{
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &UTheGoldenAgeHealthBarWidget::HandleHealthChanged);
	}

	HealthComponent = InHealth;
	if (!HealthComponent)
	{
		return;
	}

	HealthComponent->OnHealthChanged.AddDynamic(this, &UTheGoldenAgeHealthBarWidget::HandleHealthChanged);
	HandleHealthChanged(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
}

void UTheGoldenAgeHealthBarWidget::HandleHealthChanged(float InHealth, float InMaxHealth)
{
	if (!Bar)
	{
		return;
	}

	const float Percent = InMaxHealth > 0.f ? FMath::Clamp(InHealth / InMaxHealth, 0.f, 1.f) : 0.f;
	Bar->SetPercent(Percent);

	FLinearColor FillColor(0.15f, 0.72f, 0.22f, 1.f);
	if (Percent <= 0.25f)
	{
		FillColor = FLinearColor(0.75f, 0.12f, 0.1f, 1.f);
	}
	else if (Percent <= 0.5f)
	{
		FillColor = FLinearColor(0.85f, 0.65f, 0.1f, 1.f);
	}
	Bar->SetFillColorAndOpacity(FillColor);
}
