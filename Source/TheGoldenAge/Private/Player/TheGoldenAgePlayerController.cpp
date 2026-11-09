// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/TheGoldenAgePlayerController.h"

#include "Character/TheGoldenAgePlayerCharacter.h"
#include "Spacetime/TheGoldenAgeMonsterSync.h"
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

void ATheGoldenAgePlayerController::DamageLocalPlayer(float Amount)
{
	if (ATheGoldenAgePlayerCharacter* PlayerCharacter = Cast<ATheGoldenAgePlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->ApplyDamage(Amount);
	}
}

void ATheGoldenAgePlayerController::DamageMonster(uint64 MonsterId, float Amount)
{
	if (UWorld* World = GetWorld())
	{
		if (UTheGoldenAgeMonsterSync* MonsterSync = World->GetSubsystem<UTheGoldenAgeMonsterSync>())
		{
			MonsterSync->RequestMonsterDamage(MonsterId, Amount);
		}
	}
}

bool ATheGoldenAgePlayerController::GetMouseCursorGroundLocation(FVector& OutGroundLocation, float PlaneZ) const
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	FVector WorldLocation = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLocation, WorldDirection))
	{
		return false;
	}

	const float DirectionZ = WorldDirection.Z;
	if (FMath::Abs(DirectionZ) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float TraceT = (PlaneZ - WorldLocation.Z) / DirectionZ;
	if (TraceT < 0.f)
	{
		return false;
	}

	OutGroundLocation = WorldLocation + WorldDirection * TraceT;
	OutGroundLocation.Z = PlaneZ;
	return true;
}

bool ATheGoldenAgePlayerController::GetMouseCursorGroundLocationForPawn(FVector& OutGroundLocation) const
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return false;
	}

	return GetMouseCursorGroundLocation(OutGroundLocation, ControlledPawn->GetActorLocation().Z);
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
