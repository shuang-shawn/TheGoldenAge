// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleBindings/Tables/MonsterTable.g.h"
#include "Subsystems/WorldSubsystem.h"
#include "TheGoldenAgeMonsterSync.generated.h"

class ATheGoldenAgeMonster;
class UDbConnection;

/**
 * Spawns monsters from the public monster table and applies each row.
 * The server writes those rows. This client does not claim or simulate them.
 * UTheGoldenAgePlayerSync owns the connection and forwards table events here.
 */
UCLASS()
class THEGOLDENAGE_API UTheGoldenAgeMonsterSync : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static const TCHAR* ChestMonsterClassPath;
	static const TCHAR* GhostFishClassPath;

	virtual void Deinitialize() override;

	void SetConnection(UDbConnection* InConn);
	void SyncFromConnection();
	void Reset();

	/** Calls damage_monster for a server row id. Requires login. */
	void RequestMonsterDamage(uint64 MonsterId, float Amount);

	UFUNCTION()
	void HandleMonsterInsert(const FEventContext& Context, const FMonsterType& NewRow);

	UFUNCTION()
	void HandleMonsterUpdate(const FEventContext& Context, const FMonsterType& OldRow, const FMonsterType& NewRow);

	UFUNCTION()
	void HandleMonsterDelete(const FEventContext& Context, const FMonsterType& DeletedRow);

private:
	void ApplyRow(const FMonsterType& Row);
	ATheGoldenAgeMonster* EnsureMonster(const FMonsterType& Row);

	UPROPERTY()
	TObjectPtr<UDbConnection> Conn;

	TMap<uint64, TWeakObjectPtr<ATheGoldenAgeMonster>> Monsters;
};
