// Copyright Epic Games, Inc. All Rights Reserved.

#include "Spacetime/TheGoldenAgeMonsterSync.h"

#include "Character/TheGoldenAgeMonster.h"
#include "ModuleBindings/SpacetimeDBClient.g.h"
#include "Spacetime/TheGoldenAgePlayerSync.h"

const TCHAR* UTheGoldenAgeMonsterSync::ChestMonsterClassPath = TEXT("/Game/_Main/Blueprint/BP_ChestMonster.BP_ChestMonster_C");
const TCHAR* UTheGoldenAgeMonsterSync::GhostFishClassPath = TEXT("/Game/_Main/Blueprint/BP_GhostFish.BP_GhostFish_C");

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

FString UTheGoldenAgeMonsterSync::GetLocalUsername() const
{
	const UWorld* World = GetWorld();
	const UTheGoldenAgePlayerSync* Players = World ? World->GetSubsystem<UTheGoldenAgePlayerSync>() : nullptr;
	if (!Players || !Players->IsLoggedIn())
	{
		return FString();
	}

	return Players->GetLocalUsername();
}

void UTheGoldenAgeMonsterSync::ApplyRow(const FMonsterType& Row)
{
	ATheGoldenAgeMonster* Monster = EnsureMonster(Row);
	if (!Monster)
	{
		return;
	}

	const FVector Location(Row.Position.X, Row.Position.Y, Row.Position.Z);
	const FRotator Rotation(Row.Rotation.Pitch, Row.Rotation.Yaw, Row.Rotation.Roll);
	Monster->ApplyServerState(Row.Owner, Location, Rotation, Row.IsAttacking, Row.LoopAttack, GetLocalUsername());
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

	const TCHAR* ClassPath = nullptr;
	const TCHAR* Label = nullptr;
	if (Row.Kind == ChestMonsterKind)
	{
		ClassPath = ChestMonsterClassPath;
		Label = TEXT("ChestMonster");
	}
	else if (Row.Kind == GhostFishKind)
	{
		ClassPath = GhostFishClassPath;
		Label = TEXT("GhostFish");
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown monster kind %u for id %llu"), Row.Kind, Row.Id);
		return nullptr;
	}

	UClass* MonsterClass = LoadClass<ATheGoldenAgeMonster>(nullptr, ClassPath);
	if (!MonsterClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load monster class: %s"), ClassPath);
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

ATheGoldenAgeMonster* UTheGoldenAgeMonsterSync::FindMonster(uint64 MonsterId) const
{
	const TWeakObjectPtr<ATheGoldenAgeMonster>* Found = Monsters.Find(MonsterId);
	return Found ? Found->Get() : nullptr;
}

bool UTheGoldenAgeMonsterSync::RequestClaim(uint64 MonsterId)
{
	if (!Conn || !Conn->Reducers || GetLocalUsername().IsEmpty() || MonsterId == 0)
	{
		return false;
	}

	Conn->Reducers->ClaimMonster(MonsterId);
	return true;
}

bool UTheGoldenAgeMonsterSync::ReportState(
	uint64 MonsterId,
	const FVector& Location,
	const FRotator& Rotation,
	bool bAttacking,
	bool bLoopAttack)
{
	if (!Conn || !Conn->Reducers || GetLocalUsername().IsEmpty() || MonsterId == 0)
	{
		return false;
	}

	FDbVector3Type Position;
	Position.X = Location.X;
	Position.Y = Location.Y;
	Position.Z = Location.Z;

	FDbRotatorType DbRotation;
	DbRotation.Pitch = Rotation.Pitch;
	DbRotation.Yaw = Rotation.Yaw;
	DbRotation.Roll = Rotation.Roll;

	Conn->Reducers->UpdateMonster(MonsterId, Position, DbRotation, bAttacking, bLoopAttack);
	return true;
}

bool UTheGoldenAgeMonsterSync::Release(uint64 MonsterId)
{
	if (!Conn || !Conn->Reducers || GetLocalUsername().IsEmpty() || MonsterId == 0)
	{
		return false;
	}

	Conn->Reducers->ReleaseMonster(MonsterId);
	return true;
}

void UTheGoldenAgeMonsterSync::NotifyClaimRejected(uint64 MonsterId)
{
	if (ATheGoldenAgeMonster* Monster = FindMonster(MonsterId))
	{
		Monster->NotifyClaimRejected();
	}
}

void UTheGoldenAgeMonsterSync::NotifyReleaseRejected(uint64 MonsterId)
{
	if (ATheGoldenAgeMonster* Monster = FindMonster(MonsterId))
	{
		Monster->NotifyReleaseRejected();
	}
}

void UTheGoldenAgeMonsterSync::NotifyDisconnected()
{
	for (const TPair<uint64, TWeakObjectPtr<ATheGoldenAgeMonster>>& Pair : Monsters)
	{
		if (ATheGoldenAgeMonster* Monster = Pair.Value.Get())
		{
			Monster->NotifyDisconnected();
		}
	}
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
