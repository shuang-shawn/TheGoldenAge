// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleBindings/Tables/MonsterTable.g.h"
#include "Subsystems/WorldSubsystem.h"
#include "TheGoldenAgeMonsterSync.generated.h"

class ATheGoldenAgeMonster;
class UDbConnection;

/**
 * Spawns monsters from the public monster table and sends claim, update, and
 * release reducers for the local client. UTheGoldenAgePlayerSync owns the
 * connection and forwards table events here.
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

	bool RequestClaim(uint64 MonsterId);
	bool ReportState(uint64 MonsterId, const FVector& Location, const FRotator& Rotation, bool bAttacking, bool bLoopAttack);
	bool Release(uint64 MonsterId);

	void NotifyClaimRejected(uint64 MonsterId);
	void NotifyReleaseRejected(uint64 MonsterId);
	void NotifyDisconnected();

	UFUNCTION()
	void HandleMonsterInsert(const FEventContext& Context, const FMonsterType& NewRow);

	UFUNCTION()
	void HandleMonsterUpdate(const FEventContext& Context, const FMonsterType& OldRow, const FMonsterType& NewRow);

	UFUNCTION()
	void HandleMonsterDelete(const FEventContext& Context, const FMonsterType& DeletedRow);

private:
	FString GetLocalUsername() const;
	void ApplyRow(const FMonsterType& Row);
	ATheGoldenAgeMonster* EnsureMonster(const FMonsterType& Row);
	ATheGoldenAgeMonster* FindMonster(uint64 MonsterId) const;

	UPROPERTY()
	TObjectPtr<UDbConnection> Conn;

	TMap<uint64, TWeakObjectPtr<ATheGoldenAgeMonster>> Monsters;
};
