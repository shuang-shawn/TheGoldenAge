#pragma once

#include "CoreMinimal.h"
#include "ModuleBindings/SpacetimeDBClient.g.h"
#include "Player/TheGoldenAgePlayerRole.h"
#include "Subsystems/WorldSubsystem.h"
#include "TheGoldenAgePlayerSync.generated.h"

class APawn;
class UTheGoldenAgeLoginWidget;
class UTheGoldenAgePlayMenuWidget;

/**
 * Connects a running game to the TheGoldenAge SpacetimeDB database and
 * keeps the public player table in sync with pawn transforms.
 * Each username keeps a persistent pawn in the world. The local client
 * possesses only its own pawn after login.
 */
UCLASS()
class THEGOLDENAGE_API UTheGoldenAgePlayerSync : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	static const TCHAR* ServerUri;
	static const TCHAR* DatabaseIdentity;
	static const TCHAR* HeroPawnClassPath;
	static const TCHAR* ExplorerPawnClassPath;
	static constexpr float SyncIntervalSeconds = 0.01f;

	static bool IsValidUsername(const FString& Username, FString& OutError);

	bool IsLoggedIn() const { return bLoggedIn; }
	const FString& GetLocalUsername() const { return LocalUsername; }

	/** Calls damage_player for the logged-in player. The server writes the new health. */
	void RequestLocalDamage(float Amount);

	/** Pushes transform and attack presentation to the server immediately (e.g. attack start/end). */
	void RequestLocalTransformPublish();

private:
	void Connect();
	void Disconnect();
	void SendLocalTransform();
	void ApplyPlayerRow(const FPlayerType& Row);
	void SyncAllPlayersFromCache();
	void InitSpawnAnchorFromLevel();
	APawn* EnsurePlayerPawn(const FPlayerType& Row);
	const TCHAR* GetPawnClassPathForRole(uint8 Role) const;
	void SubmitLogin(const FString& Username, const FString& Password, uint8 Role);
	void PossessLocalPlayerPawn();
	void ReleaseLocalPossession();
	void ConfigurePawnPresence(APawn* Pawn, bool bLocallyControlled) const;
	APawn* GetLocalPawn() const;
	bool IsLocalPlayerRow(const FPlayerType& Row) const;
	bool IsUnsetSpawnTransform(const FPlayerType& Row) const;
	FVector ResolveSpawnLocation(const FPlayerType& Row) const;
	/** Spawn anchor plus a nearby clear capsule spot when the anchor overlaps blockers. */
	FVector ResolvePlacementLocation(const FPlayerType& Row, const APawn* Pawn) const;
	FRotator ResolveSpawnRotation(const FPlayerType& Row) const;
	void DestroyAllPlayerPawns();

	void ShowPlayMenu();
	void DismissPlayMenu();
	void ShowLoginWidget();
	void DismissLoginWidget();
	void SetGameplayInputBlocked(bool bBlocked);

	UFUNCTION()
	void HandlePlayRequested();

	UFUNCTION()
	void HandleLoginRequested(const FString& Username, const FString& Password);

	UFUNCTION()
	void HandleRoleChosen(uint8 Role);

	UFUNCTION()
	void HandleRoleChoiceCancelled();

	UFUNCTION()
	void HandleLoginReducer(
		const FReducerEventContext& Context,
		const FString& Name,
		const FString& Password,
		uint8 Role);

	UFUNCTION()
	void HandleReducerError(const FReducerEventContext& Context, const FString& Error);

	UFUNCTION()
	void HandleConnect(UDbConnection* InConn, FSpacetimeDBIdentity Identity, const FString& Token);

	UFUNCTION()
	void HandleConnectError(const FString& Error);

	UFUNCTION()
	void HandleDisconnect(UDbConnection* InConn, const FString& Error);

	UFUNCTION()
	void HandleSubscriptionApplied(FSubscriptionEventContext Context);

	UFUNCTION()
	void HandlePlayerInsert(const FEventContext& Context, const FPlayerType& NewRow);

	UFUNCTION()
	void HandlePlayerUpdate(const FEventContext& Context, const FPlayerType& OldRow, const FPlayerType& NewRow);

	UPROPERTY()
	TObjectPtr<UDbConnection> Conn;

	UPROPERTY()
	TObjectPtr<UTheGoldenAgeLoginWidget> LoginWidget;

	UPROPERTY()
	TObjectPtr<UTheGoldenAgePlayMenuWidget> PlayMenuWidget;

	FSpacetimeDBIdentity LocalIdentity;
	FSpacetimeDBIdentity LocalPlayerIdentity;
	bool bHasLocalIdentity = false;
	bool bHasLocalPlayerIdentity = false;
	bool bSubscriptionApplied = false;
	bool bConnectionReady = false;
	bool bLoggedIn = false;
	bool bLoginPending = false;
	bool bAwaitingRoleChoice = false;
	bool bLocalPossessionReady = false;
	bool bPlayChosen = false;
	bool bHasSpawnAnchor = false;
	FString LocalUsername;
	FString PendingUsername;
	FString PendingPassword;
	FString LastConnectionError;
	float TimeSinceSync = 0.f;

	FVector SpawnAnchorLocation = FVector::ZeroVector;
	FRotator SpawnAnchorRotation = FRotator::ZeroRotator;

	TMap<FSpacetimeDBIdentity, TWeakObjectPtr<APawn>> PlayerPawns;
};
