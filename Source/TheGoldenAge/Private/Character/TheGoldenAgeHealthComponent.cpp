// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgeHealthComponent.h"

UTheGoldenAgeHealthComponent::UTheGoldenAgeHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTheGoldenAgeHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(MaxHealth, 1.f);
	if (!bServerHealthApplied)
	{
		Health = MaxHealth;
		bIsDead = false;
		OnHealthChanged.Broadcast(Health, MaxHealth);
	}
}

void UTheGoldenAgeHealthComponent::ApplyServerHealth(float NewHealth)
{
	if (!FMath::IsFinite(NewHealth))
	{
		return;
	}

	MaxHealth = FMath::Max(MaxHealth, 1.f);
	const float Clamped = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	const bool bNowDead = Clamped <= 0.f;
	if (FMath::IsNearlyEqual(Health, Clamped) && bIsDead == bNowDead)
	{
		return;
	}

	const bool bBecameDead = bNowDead && !bIsDead;
	bServerHealthApplied = true;
	Health = Clamped;
	bIsDead = bNowDead;
	OnHealthChanged.Broadcast(Health, MaxHealth);
	if (bBecameDead)
	{
		OnDied.Broadcast();
	}
}

float UTheGoldenAgeHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.f ? Health / MaxHealth : 0.f;
}
