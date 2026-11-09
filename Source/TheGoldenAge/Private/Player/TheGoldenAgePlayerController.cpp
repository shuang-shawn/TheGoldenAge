// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/TheGoldenAgePlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

ATheGoldenAgePlayerController::ATheGoldenAgePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCCtx(
		TEXT("/Game/_Main/Input/IMC_Default.IMC_Default"));
	if (IMCCtx.Succeeded())
	{
		DefaultMappingContext = IMCCtx.Object;
	}
}

void ATheGoldenAgePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!DefaultMappingContext)
	{
		DefaultMappingContext = LoadObject<UInputMappingContext>(
			nullptr,
			TEXT("/Game/_Main/Input/IMC_Default.IMC_Default"));
	}

	AddDefaultMappingContext();
}

void ATheGoldenAgePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetViewTarget(InPawn);
	AddDefaultMappingContext();
}

void ATheGoldenAgePlayerController::AddDefaultMappingContext()
{
	if (!DefaultMappingContext)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			InputSubsystem->AddMappingContext(DefaultMappingContext, DefaultMappingContextPriority);
		}
	}
}
