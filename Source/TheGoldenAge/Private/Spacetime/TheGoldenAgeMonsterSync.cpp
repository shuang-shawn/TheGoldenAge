// Copyright Epic Games, Inc. All Rights Reserved.

#include "Spacetime/TheGoldenAgeMonsterSync.h"

#include "Character/TheGoldenAgeMonster.h"
#include "Game/TheGoldenAgeGameMode.h"
#include "ModuleBindings/SpacetimeDBClient.g.h"

namespace
{
constexpr uint8 ChestMonsterKind = 0;
constexpr uint8 GhostFishKind = 1;
}

void UTheGoldenAgeMonsterSync::Deinitialize()
{
	Reset();
	Super::Deinitialize();
}

void UTheGoldenAgeMonsterSync::SetConnection(UDbConnection* InConn)
{
	Conn = InConn;
}

void UTheGoldenAgeMonsterSync::RequestMonsterDamage(uint64 MonsterId, float Amount)
{
	if (!Conn || !Conn->Reducers || MonsterId == 0 || !FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return;
	}

	Conn->Reducers->DamageMonster(MonsterId, Amount);
}

void UTheGoldenAgeMonsterSync::RequestMonsterAttackPlayer(uint64 MonsterId)
{
	if (!Conn || !Conn->Reducers || MonsterId == 0)
	{
		return;
	}

	Conn->Reducers->MonsterAttackPlayer(MonsterId);
}

void UTheGoldenAgeMonsterSync::Reset()
{
	for (TPair<uint64, TWeakObjectPtr<ATheGoldenAgeMonster>>& Pair : Monsters)
	{
		if (ATheGoldenAgeMonster* Monster = Pair.Value.Get())
		{
			Monster->Destroy();
		}
	}
	Monsters.Empty();
	Conn = nullptr;
}

void UTheGoldenAgeMonsterSync::SyncFromConnection()
{
	if (!Conn || !Conn->Db || !Conn->Db->Monster)
	{
		return;
	}

	const TArray<FMonsterType> Rows = Conn->Db->Monster->Iter();
	for (const FMonsterType& Row : Rows)
	{
		ApplyRow(Row);
	}
}

void UTheGoldenAgeMonsterSync::ApplyRow(const FMonsterType& Row)
{
	ATheGoldenAgeMonster* Monster = EnsureMonster(Row);
	if (!Monster)
	{
		return;
	}

	const bool bWasDead = Monster->IsDead();
	Monster->ApplyServerHealth(Row.Health);

	const bool bRespawned = bWasDead && !Monster->IsDead();
	if (bRespawned)
	{
		Monster->HandleRespawned();
	}

	const FVector Location(Row.Position.X, Row.Position.Y, Row.Position.Z);
	const FRotator Rotation(Row.Rotation.Pitch, Row.Rotation.Yaw, Row.Rotation.Roll);
	Monster->ApplyServerState(Location, Rotation, Row.IsAttacking, Row.LoopAttack);

	if (bRespawned)
	{
		Monster->FlushPresentedTransform();
	}
}

ATheGoldenAgeMonster* UTheGoldenAgeMonsterSync::EnsureMonster(const FMonsterType& Row)
{
	if (Row.Id == 0)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const ATheGoldenAgeGameMode* GameMode = World->GetAuthGameMode<ATheGoldenAgeGameMode>();
	if (!GameMode)
	{
		return nullptr;
	}

	const TCHAR* Label = nullptr;
	TSubclassOf<ATheGoldenAgeMonster> MonsterClass;
	if (Row.Kind == ChestMonsterKind)
	{
		MonsterClass = GameMode->ChestMonsterClass;
		Label = TEXT("ChestMonster");
	}
	else if (Row.Kind == GhostFishKind)
	{
		MonsterClass = GameMode->GhostFishClass;
		Label = TEXT("GhostFish");
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown monster kind %u for id %llu"), Row.Kind, Row.Id);
		return nullptr;
	}

	if (!MonsterClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Game mode has no monster class for kind %u"), Row.Kind);
		return nullptr;
	}

	TWeakObjectPtr<ATheGoldenAgeMonster>& Existing = Monsters.FindOrAdd(Row.Id);
	if (ATheGoldenAgeMonster* Monster = Existing.Get())
	{
		if (Monster->IsA(MonsterClass))
		{
			return Monster;
		}

		Monster->Destroy();
		Existing = nullptr;
	}

	const FVector Location(Row.Position.X, Row.Position.Y, Row.Position.Z);
	const FRotator Rotation(Row.Rotation.Pitch, Row.Rotation.Yaw, Row.Rotation.Roll);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ATheGoldenAgeMonster* Spawned = World->SpawnActor<ATheGoldenAgeMonster>(MonsterClass, Location, Rotation, Params);
	if (!Spawned)
	{
		return nullptr;
	}

	Spawned->InitializeFromServer(Row.Id);
#if WITH_EDITOR
	Spawned->SetActorLabel(FString::Printf(TEXT("%s_%s"), Label, *LexToString(Row.Id)));
#endif
	Existing = Spawned;
	return Spawned;
}

void UTheGoldenAgeMonsterSync::HandleMonsterInsert(const FEventContext& Context, const FMonsterType& NewRow)
{
	(void)Context;
	ApplyRow(NewRow);
}

void UTheGoldenAgeMonsterSync::HandleMonsterUpdate(
	const FEventContext& Context,
	const FMonsterType& OldRow,
	const FMonsterType& NewRow)
{
	(void)Context;
	(void)OldRow;
	ApplyRow(NewRow);
}

void UTheGoldenAgeMonsterSync::HandleMonsterDelete(const FEventContext& Context, const FMonsterType& DeletedRow)
{
	(void)Context;
	if (TWeakObjectPtr<ATheGoldenAgeMonster>* Found = Monsters.Find(DeletedRow.Id))
	{
		if (ATheGoldenAgeMonster* Monster = Found->Get())
		{
			Monster->Destroy();
		}
		Monsters.Remove(DeletedRow.Id);
	}
}
