// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgeGhostFish.h"

#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"

namespace GhostFishAnim
{
constexpr float AttackEndToleranceSeconds = 0.001f;

FAnimNode_SequencePlayer* FindAttackSequencePlayer(UAnimInstance* AnimInstance)
{
	const IAnimClassInterface* AnimClass = AnimInstance
		? IAnimClassInterface::GetFromClass(AnimInstance->GetClass())
		: nullptr;
	if (!AnimClass)
	{
		return nullptr;
	}

	for (const FStructProperty* NodeProperty : AnimClass->GetAnimNodeProperties())
	{
		if (!NodeProperty || NodeProperty->Struct != FAnimNode_SequencePlayer::StaticStruct())
		{
			continue;
		}

		FAnimNode_SequencePlayer* Player = NodeProperty->ContainerPtrToValuePtr<FAnimNode_SequencePlayer>(AnimInstance);
		const UAnimSequenceBase* Sequence = Player ? Player->GetSequence() : nullptr;
		if (Sequence && Sequence->GetName().Contains(TEXT("Attack")))
		{
			return Player;
		}
	}

	return nullptr;
}
} // namespace GhostFishAnim

void ATheGoldenAgeGhostFish::HandleNearbyPlayers_Implementation(
	const TArray<AActor*>& NearbyPlayers, float DeltaSeconds)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	const FAnimNode_SequencePlayer* AttackPlayer = GhostFishAnim::FindAttackSequencePlayer(AnimInstance);

	if (AActor* ClosestPlayer = FindClosestPlayer(NearbyPlayers))
	{
		FacePlayer(ClosestPlayer, DeltaSeconds);
		bIsAttacking = true;
		bLoopAttack = true;
	}
	else if (bIsAttacking)
	{
		// Stop the clip from wrapping into another swing, then leave attack once this cycle is over.
		bLoopAttack = false;
		const float PlaybackTime = AttackPlayer ? AttackPlayer->GetCurrentAssetTime() : 0.f;
		const float PlayLength = AttackPlayer ? AttackPlayer->GetCurrentAssetLength() : 0.f;
		if (!AttackPlayer || HasCurrentAttackFinished(PlaybackTime, PlayLength))
		{
			bIsAttacking = false;
			bLoopAttack = true;
		}
	}
	else
	{
		bIsAttacking = false;
		bLoopAttack = true;
	}

	if (bIsAttacking && AttackPlayer)
	{
		AttackSampleTime = AttackPlayer->GetCurrentAssetTime();
		bHasAttackSample = true;
	}
	else
	{
		bHasAttackSample = false;
	}

	PushAnimState();
}

AActor* ATheGoldenAgeGhostFish::FindClosestPlayer(const TArray<AActor*>& NearbyPlayers) const
{
	const FVector Origin = GetActorLocation();
	AActor* ClosestPlayer = nullptr;
	float ClosestDistanceSquared = 0.f;

	for (AActor* Player : NearbyPlayers)
	{
		if (!IsValid(Player))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Origin, Player->GetActorLocation());
		if (!ClosestPlayer || DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestPlayer = Player;
		}
	}

	return ClosestPlayer;
}

void ATheGoldenAgeGhostFish::FacePlayer(const AActor* Player, float DeltaSeconds)
{
	const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	if (ToPlayer.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FRotator DesiredRotation(0.f, ToPlayer.Rotation().Yaw, 0.f);
	const FRotator UpdatedRotation = FMath::RInterpConstantTo(
		GetActorRotation(), DesiredRotation, DeltaSeconds, FacingTurnRate);
	SetActorRotation(FRotator(0.f, UpdatedRotation.Yaw, 0.f));
}

bool ATheGoldenAgeGhostFish::HasCurrentAttackFinished(float PlaybackTime, float PlayLength) const
{
	if (PlayLength <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	// Non-looping playback clamps on the final frame. A wrap means that frame was already passed.
	const bool bReachedEnd = PlaybackTime >= PlayLength - GhostFishAnim::AttackEndToleranceSeconds;
	const bool bWrapped = bHasAttackSample && PlaybackTime + GhostFishAnim::AttackEndToleranceSeconds < AttackSampleTime;
	return bReachedEnd || bWrapped;
}

void ATheGoldenAgeGhostFish::PushAnimState()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	// ABP_GhostFish owns the clips. Gameplay drives which pose is active and whether the attack loops.
	UClass* AnimClass = AnimInstance->GetClass();
	if (FBoolProperty* IsAttackingProperty = FindFProperty<FBoolProperty>(AnimClass, TEXT("bIsAttacking")))
	{
		IsAttackingProperty->SetPropertyValue_InContainer(AnimInstance, bIsAttacking);
	}
	if (FBoolProperty* LoopAttackProperty = FindFProperty<FBoolProperty>(AnimClass, TEXT("bLoopAttack")))
	{
		LoopAttackProperty->SetPropertyValue_InContainer(AnimInstance, bLoopAttack);
	}
}
