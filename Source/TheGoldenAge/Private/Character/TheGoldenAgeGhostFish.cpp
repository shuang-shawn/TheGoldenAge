// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/TheGoldenAgeGhostFish.h"

#include "Character/TheGoldenAgeAttackAnimConstants.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"

namespace GhostFishAnim
{
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

void ATheGoldenAgeGhostFish::UpdatePresentedAnim()
{
	if (IsDead())
	{
		bIsAttacking = false;
		bLoopAttack = false;
		PushAnimState();
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	const FAnimNode_SequencePlayer* AttackPlayer = GhostFishAnim::FindAttackSequencePlayer(AnimInstance);

	ApplyAttackState(WantsLoopingAttack(), AttackPlayer);

	if (bIsAttacking && AttackPlayer)
	{
		const float PlaybackTime = AttackPlayer->GetCurrentAssetTime();
		const float PlayLength = AttackPlayer->GetCurrentAssetLength();
		AttackSampleTime = PlaybackTime;
		bHasAttackSample = true;
		TryMonsterAttackHit(PlaybackTime, PlayLength);
	}
	else
	{
		bHasAttackSample = false;
	}

	PushAnimState();
}

void ATheGoldenAgeGhostFish::ApplyAttackState(bool bShouldLoop, const FAnimNode_SequencePlayer* AttackPlayer)
{
	if (bShouldLoop)
	{
		bIsAttacking = true;
		bLoopAttack = true;
		return;
	}

	if (!bIsAttacking)
	{
		bLoopAttack = false;
		return;
	}

	// Stop the clip from wrapping into another swing, then leave attack once this cycle is over.
	bLoopAttack = false;
	const float PlaybackTime = AttackPlayer ? AttackPlayer->GetCurrentAssetTime() : 0.f;
	const float PlayLength = AttackPlayer ? AttackPlayer->GetCurrentAssetLength() : 0.f;
	if (!AttackPlayer || HasCurrentAttackFinished(PlaybackTime, PlayLength))
	{
		bIsAttacking = false;
		bLoopAttack = false;
	}
}

bool ATheGoldenAgeGhostFish::HasCurrentAttackFinished(float PlaybackTime, float PlayLength) const
{
	if (PlayLength <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	// Non-looping playback clamps on the final frame. A wrap means that frame was already passed.
	const bool bReachedEnd = PlaybackTime >= PlayLength - TheGoldenAgeAttackAnim::AttackEndToleranceSeconds;
	const bool bWrapped = bHasAttackSample && PlaybackTime + TheGoldenAgeAttackAnim::AttackEndToleranceSeconds < AttackSampleTime;
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

	// ABP_GhostFish owns the clips. The server says whether the attack should keep looping.
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
