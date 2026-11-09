#include "Spacetime/TheGoldenAgePlayerSync.h"

#include "Character/TheGoldenAgePlayerCharacter.h"
#include "Spacetime/TheGoldenAgeMonsterSync.h"
#include "Connection/Credentials.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "ModuleBindings/Tables/PlayerTable.g.h"
#include "UI/TheGoldenAgeLoginWidget.h"
#include "UI/TheGoldenAgePlayMenuWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"

const TCHAR* UTheGoldenAgePlayerSync::ServerUri = TEXT("https://maincloud.spacetimedb.com");
const TCHAR* UTheGoldenAgePlayerSync::DatabaseIdentity = TEXT("c2009abca22226b9b908c5d99782eb9da9ffa21e9de7158c7f17a6c84c3d5b13");
const TCHAR* UTheGoldenAgePlayerSync::HeroPawnClassPath = TEXT("/Game/_Main/Blueprint/BP_Hero.BP_Hero_C");
const TCHAR* UTheGoldenAgePlayerSync::ExplorerPawnClassPath = TEXT("/Game/_Main/Blueprint/BP_Explorer.BP_Explorer_C");

namespace
{
constexpr float SpawnTransformEpsilon = 1.f;
constexpr int32 LoginViewportZOrder = 100;
constexpr int32 PlayMenuViewportZOrder = 80;
}

bool UTheGoldenAgePlayerSync::IsValidUsername(const FString& Username, FString& OutError)
{
	if (Username.IsEmpty())
	{
		OutError = TEXT("Username cannot be empty");
		return false;
	}

	if (Username.Len() > 24)
	{
		OutError = TEXT("Username must be 24 characters or fewer");
		return false;
	}

	for (const TCHAR Character : Username)
	{
		const bool IsLetter = FChar::IsAlpha(Character);
		const bool IsDigit = FChar::IsDigit(Character);
		if (!IsLetter && !IsDigit)
		{
			OutError = TEXT("Username must contain only letters and numbers");
			return false;
		}
	}

	OutError.Reset();
	return true;
}

bool UTheGoldenAgePlayerSync::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UTheGoldenAgePlayerSync::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InitSpawnAnchorFromLevel();
	ReleaseLocalPossession();
	ShowPlayMenu();
	Connect();
}

void UTheGoldenAgePlayerSync::Deinitialize()
{
	DismissLoginWidget();
	DismissPlayMenu();
	Disconnect();
	Super::Deinitialize();
}

void UTheGoldenAgePlayerSync::Tick(float DeltaTime)
{
	if (!bSubscriptionApplied || !bLoggedIn || !bLocalPossessionReady || !Conn)
	{
		return;
	}

	TimeSinceSync += DeltaTime;
	if (TimeSinceSync < SyncIntervalSeconds)
	{
		return;
	}

	TimeSinceSync = 0.f;
	SendLocalTransform();
}

TStatId UTheGoldenAgePlayerSync::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTheGoldenAgePlayerSync, STATGROUP_Tickables);
}

bool UTheGoldenAgePlayerSync::IsTickable() const
{
	return Conn != nullptr && bSubscriptionApplied && bLoggedIn && bLocalPossessionReady;
}

void UTheGoldenAgePlayerSync::InitSpawnAnchorFromLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<ATheGoldenAgePlayerCharacter*> PlacedPawns;
	for (TActorIterator<ATheGoldenAgePlayerCharacter> It(World); It; ++It)
	{
		if (ATheGoldenAgePlayerCharacter* PlacedPawn = *It)
		{
			PlacedPawns.Add(PlacedPawn);
		}
	}

	for (ATheGoldenAgePlayerCharacter* PlacedPawn : PlacedPawns)
	{
		if (!bHasSpawnAnchor)
		{
			SpawnAnchorLocation = PlacedPawn->GetActorLocation();
			SpawnAnchorRotation = PlacedPawn->GetActorRotation();
			bHasSpawnAnchor = true;
		}
		PlacedPawn->Destroy();
	}
}

void UTheGoldenAgePlayerSync::ShowPlayMenu()
{
	UWorld* World = GetWorld();
	if (!World || bPlayChosen)
	{
		return;
	}

	APlayerController* Controller = World->GetFirstPlayerController();
	if (!Controller)
	{
		return;
	}

	if (!PlayMenuWidget)
	{
		PlayMenuWidget = CreateWidget<UTheGoldenAgePlayMenuWidget>(Controller, UTheGoldenAgePlayMenuWidget::StaticClass());
		if (PlayMenuWidget)
		{
			PlayMenuWidget->OnPlayRequested.AddDynamic(this, &UTheGoldenAgePlayerSync::HandlePlayRequested);
		}
	}

	if (PlayMenuWidget && !PlayMenuWidget->IsInViewport())
	{
		PlayMenuWidget->AddToViewport(PlayMenuViewportZOrder);
	}

	SetGameplayInputBlocked(true);
}

void UTheGoldenAgePlayerSync::DismissPlayMenu()
{
	UWorld* World = GetWorld();
	TArray<UTheGoldenAgePlayMenuWidget*> Widgets;
	for (TObjectIterator<UTheGoldenAgePlayMenuWidget> It; It; ++It)
	{
		UTheGoldenAgePlayMenuWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->GetWorld() != World)
		{
			continue;
		}
		Widgets.Add(Widget);
	}

	for (UTheGoldenAgePlayMenuWidget* Widget : Widgets)
	{
		Widget->OnPlayRequested.RemoveDynamic(this, &UTheGoldenAgePlayerSync::HandlePlayRequested);
		Widget->RemoveFromParent();
	}

	PlayMenuWidget = nullptr;
}

void UTheGoldenAgePlayerSync::ShowLoginWidget()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* Controller = World->GetFirstPlayerController();
	if (!Controller)
	{
		return;
	}

	if (!LoginWidget)
	{
		for (TObjectIterator<UTheGoldenAgeLoginWidget> It; It; ++It)
		{
			UTheGoldenAgeLoginWidget* Existing = *It;
			if (IsValid(Existing) && Existing->GetWorld() == World && Existing->IsInViewport())
			{
				return;
			}
		}

		LoginWidget = CreateWidget<UTheGoldenAgeLoginWidget>(Controller, UTheGoldenAgeLoginWidget::StaticClass());
		if (LoginWidget)
		{
			LoginWidget->OnLoginRequested.AddDynamic(this, &UTheGoldenAgePlayerSync::HandleLoginRequested);
			LoginWidget->OnRoleChosen.AddDynamic(this, &UTheGoldenAgePlayerSync::HandleRoleChosen);
			LoginWidget->OnRoleChoiceCancelled.AddDynamic(this, &UTheGoldenAgePlayerSync::HandleRoleChoiceCancelled);
		}
	}

	if (LoginWidget)
	{
		if (!LoginWidget->IsInViewport())
		{
			LoginWidget->AddToViewport(LoginViewportZOrder);
		}
		LoginWidget->SetConnectionReady(bConnectionReady);
		LoginWidget->SetLoginPending(bLoginPending);
		if (!bConnectionReady && !LastConnectionError.IsEmpty())
		{
			LoginWidget->SetErrorMessage(LastConnectionError);
		}
		SetGameplayInputBlocked(true);
	}
}

void UTheGoldenAgePlayerSync::DismissLoginWidget()
{
	UWorld* World = GetWorld();
	TArray<UTheGoldenAgeLoginWidget*> Widgets;
	for (TObjectIterator<UTheGoldenAgeLoginWidget> It; It; ++It)
	{
		UTheGoldenAgeLoginWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->GetWorld() != World)
		{
			continue;
		}
		Widgets.Add(Widget);
	}

	for (UTheGoldenAgeLoginWidget* Widget : Widgets)
	{
		Widget->OnLoginRequested.RemoveDynamic(this, &UTheGoldenAgePlayerSync::HandleLoginRequested);
		Widget->OnRoleChosen.RemoveDynamic(this, &UTheGoldenAgePlayerSync::HandleRoleChosen);
		Widget->OnRoleChoiceCancelled.RemoveDynamic(this, &UTheGoldenAgePlayerSync::HandleRoleChoiceCancelled);
		Widget->SetVisibility(ESlateVisibility::Collapsed);
		Widget->RemoveFromParent();
	}

	LoginWidget = nullptr;
	SetGameplayInputBlocked(false);
}

void UTheGoldenAgePlayerSync::SetGameplayInputBlocked(bool bBlocked)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* Controller = World->GetFirstPlayerController();
	if (!Controller)
	{
		return;
	}

	if (bBlocked)
	{
		Controller->SetShowMouseCursor(true);
		FInputModeUIOnly InputMode;
		if (LoginWidget && LoginWidget->IsInViewport())
		{
			InputMode.SetWidgetToFocus(LoginWidget->TakeWidget());
		}
		else if (PlayMenuWidget && PlayMenuWidget->IsInViewport())
		{
			InputMode.SetWidgetToFocus(PlayMenuWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(InputMode);
	}
	else
	{
		Controller->SetShowMouseCursor(true);
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(InputMode);
	}
}

void UTheGoldenAgePlayerSync::HandlePlayRequested()
{
	if (bPlayChosen || bLoggedIn)
	{
		return;
	}

	bPlayChosen = true;
	DismissPlayMenu();
	ShowLoginWidget();
}

void UTheGoldenAgePlayerSync::Connect()
{
	if (Conn)
	{
		return;
	}

	FOnConnectDelegate ConnectDelegate;
	BIND_DELEGATE_SAFE(ConnectDelegate, this, UTheGoldenAgePlayerSync, HandleConnect);
	FOnDisconnectDelegate DisconnectDelegate;
	BIND_DELEGATE_SAFE(DisconnectDelegate, this, UTheGoldenAgePlayerSync, HandleDisconnect);
	FOnConnectErrorDelegate ConnectErrorDelegate;
	BIND_DELEGATE_SAFE(ConnectErrorDelegate, this, UTheGoldenAgePlayerSync, HandleConnectError);

	UCredentials::Init(TEXT("TheGoldenAge"));
	const FString Token = UCredentials::LoadToken();

	UDbConnectionBuilder* Builder = UDbConnection::Builder()
		->WithUri(ServerUri)
		->WithDatabaseName(DatabaseIdentity)
		->OnConnect(ConnectDelegate)
		->OnDisconnect(DisconnectDelegate)
		->OnConnectError(ConnectErrorDelegate);

	if (!Token.IsEmpty())
	{
		Builder->WithToken(Token);
	}

	Conn = Builder->Build();
	if (Conn)
	{
		Conn->SetAutoTicking(true);
	}
}

void UTheGoldenAgePlayerSync::DestroyAllPlayerPawns()
{
	for (TPair<FSpacetimeDBIdentity, TWeakObjectPtr<APawn>>& Pair : PlayerPawns)
	{
		if (APawn* Pawn = Pair.Value.Get())
		{
			Pawn->Destroy();
		}
	}
	PlayerPawns.Empty();
}

void UTheGoldenAgePlayerSync::Disconnect()
{
	UTheGoldenAgeMonsterSync* Monsters = GetWorld() ? GetWorld()->GetSubsystem<UTheGoldenAgeMonsterSync>() : nullptr;
	if (Conn)
	{
		Conn->Reducers->OnLogin.RemoveDynamic(this, &UTheGoldenAgePlayerSync::HandleLoginReducer);
		Conn->OnUnhandledReducerError.RemoveDynamic(this, &UTheGoldenAgePlayerSync::HandleReducerError);
		if (Monsters && Conn->Db && Conn->Db->Monster)
		{
			Conn->Db->Monster->OnInsert.RemoveDynamic(Monsters, &UTheGoldenAgeMonsterSync::HandleMonsterInsert);
			Conn->Db->Monster->OnUpdate.RemoveDynamic(Monsters, &UTheGoldenAgeMonsterSync::HandleMonsterUpdate);
			Conn->Db->Monster->OnDelete.RemoveDynamic(Monsters, &UTheGoldenAgeMonsterSync::HandleMonsterDelete);
		}
	}

	if (Monsters)
	{
		Monsters->Reset();
	}

	DestroyAllPlayerPawns();

	if (Conn)
	{
		Conn->Disconnect();
		Conn = nullptr;
	}

	bHasLocalIdentity = false;
	bHasLocalPlayerIdentity = false;
	LocalPlayerIdentity = FSpacetimeDBIdentity();
	bSubscriptionApplied = false;
	bConnectionReady = false;
	bLoggedIn = false;
	bLoginPending = false;
	bAwaitingRoleChoice = false;
	bLocalPossessionReady = false;
	LocalUsername.Reset();
	PendingUsername.Reset();
	PendingPassword.Reset();
	TimeSinceSync = 0.f;
}

void UTheGoldenAgePlayerSync::HandleConnect(UDbConnection* InConn, FSpacetimeDBIdentity Identity, const FString& Token)
{
	Conn = InConn;
	LocalIdentity = Identity;
	bHasLocalIdentity = true;
	LastConnectionError.Reset();
	UCredentials::SaveToken(Token);

	UE_LOG(LogTemp, Log, TEXT("SpacetimeDB connected as %s"), *Identity.ToHex());

	if (!InConn || !InConn->Db || !InConn->Db->Player)
	{
		return;
	}

	InConn->SetAutoTicking(true);
	InConn->Db->Player->OnInsert.AddDynamic(this, &UTheGoldenAgePlayerSync::HandlePlayerInsert);
	InConn->Db->Player->OnUpdate.AddDynamic(this, &UTheGoldenAgePlayerSync::HandlePlayerUpdate);

	InConn->Reducers->OnLogin.AddDynamic(this, &UTheGoldenAgePlayerSync::HandleLoginReducer);
	InConn->OnUnhandledReducerError.AddDynamic(this, &UTheGoldenAgePlayerSync::HandleReducerError);

	if (UTheGoldenAgeMonsterSync* Monsters = GetWorld()->GetSubsystem<UTheGoldenAgeMonsterSync>())
	{
		Monsters->SetConnection(InConn);
		if (InConn->Db->Monster)
		{
			InConn->Db->Monster->OnInsert.AddDynamic(Monsters, &UTheGoldenAgeMonsterSync::HandleMonsterInsert);
			InConn->Db->Monster->OnUpdate.AddDynamic(Monsters, &UTheGoldenAgeMonsterSync::HandleMonsterUpdate);
			InConn->Db->Monster->OnDelete.AddDynamic(Monsters, &UTheGoldenAgeMonsterSync::HandleMonsterDelete);
		}
	}

	FOnSubscriptionApplied AppliedDelegate;
	BIND_DELEGATE_SAFE(AppliedDelegate, this, UTheGoldenAgePlayerSync, HandleSubscriptionApplied);
	InConn->SubscriptionBuilder()
		->OnApplied(AppliedDelegate)
		->SubscribeToAllTables();
}

void UTheGoldenAgePlayerSync::HandleConnectError(const FString& Error)
{
	UE_LOG(LogTemp, Error, TEXT("SpacetimeDB connection failed: %s"), *Error);
	LastConnectionError = Error;
	bConnectionReady = false;
	if (LoginWidget)
	{
		LoginWidget->SetConnectionReady(false);
		LoginWidget->SetErrorMessage(Error);
	}
}

void UTheGoldenAgePlayerSync::HandleDisconnect(UDbConnection* InConn, const FString& Error)
{
	(void)InConn;
	UE_LOG(LogTemp, Warning, TEXT("SpacetimeDB disconnected: %s"), *Error);

	bSubscriptionApplied = false;
	bConnectionReady = false;
	bLoggedIn = false;
	bLoginPending = false;
	bAwaitingRoleChoice = false;
	bLocalPossessionReady = false;
	bHasLocalPlayerIdentity = false;
	LocalPlayerIdentity = FSpacetimeDBIdentity();
	LocalUsername.Reset();
	PendingUsername.Reset();
	PendingPassword.Reset();
	LastConnectionError = Error;

	ReleaseLocalPossession();

	if (UTheGoldenAgeMonsterSync* Monsters = GetWorld()->GetSubsystem<UTheGoldenAgeMonsterSync>())
	{
		Monsters->SetConnection(nullptr);
	}

	if (bPlayChosen)
	{
		ShowLoginWidget();
	}
}

void UTheGoldenAgePlayerSync::HandleSubscriptionApplied(FSubscriptionEventContext Context)
{
	bSubscriptionApplied = true;
	bConnectionReady = true;
	LastConnectionError.Reset();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("SpacetimeDB subscribed to player table (%d rows)"),
		Context.Db && Context.Db->Player ? Context.Db->Player->Count() : 0);

	SyncAllPlayersFromCache();
	if (UTheGoldenAgeMonsterSync* Monsters = GetWorld()->GetSubsystem<UTheGoldenAgeMonsterSync>())
	{
		Monsters->SyncFromConnection();
	}

	if (LoginWidget)
	{
		LoginWidget->SetConnectionReady(true);
	}
}

void UTheGoldenAgePlayerSync::SyncAllPlayersFromCache()
{
	if (!Conn || !Conn->Db || !Conn->Db->Player)
	{
		return;
	}

	const TArray<FPlayerType> Rows = Conn->Db->Player->Iter();
	for (const FPlayerType& Row : Rows)
	{
		ApplyPlayerRow(Row);
	}
}

void UTheGoldenAgePlayerSync::SubmitLogin(const FString& Username, const FString& Password, uint8 Role)
{
	// Copy first. Hero/Explorer passes PendingUsername, and the reset below would clear that reference.
	const FString NameCopy = Username;
	const FString PasswordCopy = Password;
	bAwaitingRoleChoice = false;
	PendingUsername.Reset();
	PendingPassword.Reset();
	bLoginPending = true;
	if (LoginWidget)
	{
		LoginWidget->ClearErrorMessage();
		LoginWidget->SetLoginPending(true);
		LoginWidget->HideRoleChoice();
	}

	Conn->Reducers->Login(NameCopy, PasswordCopy, Role);
}

void UTheGoldenAgePlayerSync::HandleLoginRequested(const FString& Username, const FString& Password)
{
	if (!Conn || !Conn->Reducers || !Conn->Db || !Conn->Db->Player || !Conn->Db->Player->Name
		|| bLoginPending || bLoggedIn || !bConnectionReady || bAwaitingRoleChoice)
	{
		return;
	}

	FString ValidationError;
	if (!IsValidUsername(Username, ValidationError))
	{
		if (LoginWidget)
		{
			LoginWidget->SetErrorMessage(ValidationError);
		}
		return;
	}

	if (Password.IsEmpty())
	{
		if (LoginWidget)
		{
			LoginWidget->SetErrorMessage(TEXT("Password cannot be empty"));
		}
		return;
	}

	const FPlayerType Existing = Conn->Db->Player->Name->Find(Username);
	if (!Existing.Name.IsEmpty())
	{
		SubmitLogin(Username, Password, static_cast<uint8>(ETheGoldenAgePlayerRole::Hero));
		return;
	}

	PendingUsername = Username;
	PendingPassword = Password;
	bAwaitingRoleChoice = true;
	if (LoginWidget)
	{
		LoginWidget->ClearErrorMessage();
		LoginWidget->ShowRoleChoice();
	}
}

void UTheGoldenAgePlayerSync::HandleRoleChosen(uint8 Role)
{
	if (!bAwaitingRoleChoice || bLoginPending || bLoggedIn || !Conn || !Conn->Reducers)
	{
		return;
	}

	if (Role != static_cast<uint8>(ETheGoldenAgePlayerRole::Hero)
		&& Role != static_cast<uint8>(ETheGoldenAgePlayerRole::Explorer))
	{
		if (LoginWidget)
		{
			LoginWidget->SetErrorMessage(TEXT("Choose Hero or Explorer"));
		}
		return;
	}

	SubmitLogin(PendingUsername, PendingPassword, Role);
}

void UTheGoldenAgePlayerSync::HandleRoleChoiceCancelled()
{
	bAwaitingRoleChoice = false;
	PendingUsername.Reset();
	PendingPassword.Reset();
}

void UTheGoldenAgePlayerSync::HandleLoginReducer(
	const FReducerEventContext& Context,
	const FString& Name,
	const FString& Password,
	uint8 Role)
{
	(void)Password;
	(void)Role;

	if (!bLoginPending)
	{
		return;
	}

	if (Conn && Context.Event.CallerConnectionId != Conn->GetConnectionId())
	{
		return;
	}

	bLoginPending = false;

	if (!Context.Event.Status.IsCommitted())
	{
		const FString Error = Context.Event.Status.IsFailed()
			? Context.Event.Status.GetAsFailed()
			: TEXT("Login failed");
		if (LoginWidget)
		{
			LoginWidget->SetLoginPending(false);
			LoginWidget->HideRoleChoice();
			LoginWidget->SetErrorMessage(Error);
		}
		return;
	}

	bLoggedIn = true;
	LocalUsername = Name;
	bHasLocalPlayerIdentity = false;
	LocalPlayerIdentity = FSpacetimeDBIdentity();

	if (LoginWidget)
	{
		LoginWidget->SetLoginPending(false);
	}

	FPlayerType PlayerRow;
	if (Conn && Conn->Db && Conn->Db->Player)
	{
		PlayerRow = Conn->Db->Player->Name->Find(Name);
		ApplyPlayerRow(PlayerRow);
	}

	if (APawn* LocalPawn = GetLocalPawn())
	{
		const FVector Location = ResolveSpawnLocation(PlayerRow);
		const FRotator Rotation = ResolveSpawnRotation(PlayerRow);
		LocalPawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (APawn* EnsuredPawn = EnsurePlayerPawn(PlayerRow))
	{
		const FVector Location = ResolveSpawnLocation(PlayerRow);
		const FRotator Rotation = ResolveSpawnRotation(PlayerRow);
		EnsuredPawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	const bool bNeedsInitialPublish = IsUnsetSpawnTransform(PlayerRow);
	if (!bLocalPossessionReady)
	{
		PossessLocalPlayerPawn();
	}

	DismissLoginWidget();
	TimeSinceSync = SyncIntervalSeconds;

	if (bNeedsInitialPublish)
	{
		SendLocalTransform();
	}

	if (UTheGoldenAgeMonsterSync* Monsters = GetWorld()->GetSubsystem<UTheGoldenAgeMonsterSync>())
	{
		Monsters->SyncFromConnection();
	}

	UE_LOG(LogTemp, Log, TEXT("Logged in as %s"), *LocalUsername);
}

void UTheGoldenAgePlayerSync::HandleReducerError(const FReducerEventContext& Context, const FString& Error)
{
	if (!bLoginPending)
	{
		return;
	}

	if (Conn && Context.Event.CallerConnectionId != Conn->GetConnectionId())
	{
		return;
	}

	bLoginPending = false;
	bAwaitingRoleChoice = false;
	PendingUsername.Reset();
	PendingPassword.Reset();
	if (LoginWidget)
	{
		LoginWidget->SetLoginPending(false);
		LoginWidget->HideRoleChoice();
		LoginWidget->SetErrorMessage(Error);
	}
}

void UTheGoldenAgePlayerSync::HandlePlayerInsert(const FEventContext& Context, const FPlayerType& NewRow)
{
	(void)Context;
	ApplyPlayerRow(NewRow);
}

void UTheGoldenAgePlayerSync::HandlePlayerUpdate(const FEventContext& Context, const FPlayerType& OldRow, const FPlayerType& NewRow)
{
	(void)Context;
	(void)OldRow;
	ApplyPlayerRow(NewRow);
}

void UTheGoldenAgePlayerSync::RequestLocalDamage(float Amount)
{
	if (!Conn || !Conn->Reducers || !bLoggedIn || !FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return;
	}

	Conn->Reducers->DamagePlayer(Amount);
}

void UTheGoldenAgePlayerSync::SendLocalTransform()
{
	if (!Conn || !Conn->Reducers || !bLoggedIn || !bLocalPossessionReady)
	{
		return;
	}

	const APawn* Pawn = GetLocalPawn();
	if (!Pawn)
	{
		return;
	}

	if (const ATheGoldenAgePlayerCharacter* PlayerCharacter = Cast<ATheGoldenAgePlayerCharacter>(Pawn))
	{
		if (PlayerCharacter->IsDead())
		{
			return;
		}
	}

	const FVector Location = Pawn->GetActorLocation();
	const FRotator Rotation = Pawn->GetActorRotation();

	FDbVector3Type Position;
	Position.X = Location.X;
	Position.Y = Location.Y;
	Position.Z = Location.Z;

	FDbRotatorType DbRotation;
	DbRotation.Pitch = Rotation.Pitch;
	DbRotation.Yaw = Rotation.Yaw;
	DbRotation.Roll = Rotation.Roll;

	Conn->Reducers->UpdatePlayerTransform(Position, DbRotation);
}

APawn* UTheGoldenAgePlayerSync::GetLocalPawn() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const APlayerController* Controller = World->GetFirstPlayerController();
	return Controller ? Controller->GetPawn() : nullptr;
}

bool UTheGoldenAgePlayerSync::IsLocalPlayerRow(const FPlayerType& Row) const
{
	return bHasLocalPlayerIdentity && Row.Identity == LocalPlayerIdentity;
}

bool UTheGoldenAgePlayerSync::IsUnsetSpawnTransform(const FPlayerType& Row) const
{
	const FVector Location(Row.Position.X, Row.Position.Y, Row.Position.Z);
	const FRotator Rotation(Row.Rotation.Pitch, Row.Rotation.Yaw, Row.Rotation.Roll);
	return Location.IsNearlyZero(SpawnTransformEpsilon)
		&& FMath::IsNearlyZero(Rotation.Pitch, SpawnTransformEpsilon)
		&& FMath::IsNearlyZero(Rotation.Yaw, SpawnTransformEpsilon)
		&& FMath::IsNearlyZero(Rotation.Roll, SpawnTransformEpsilon);
}

FVector UTheGoldenAgePlayerSync::ResolveSpawnLocation(const FPlayerType& Row) const
{
	if (IsUnsetSpawnTransform(Row) && bHasSpawnAnchor)
	{
		return SpawnAnchorLocation;
	}

	return FVector(Row.Position.X, Row.Position.Y, Row.Position.Z);
}

FRotator UTheGoldenAgePlayerSync::ResolveSpawnRotation(const FPlayerType& Row) const
{
	if (IsUnsetSpawnTransform(Row) && bHasSpawnAnchor)
	{
		return SpawnAnchorRotation;
	}

	return FRotator(Row.Rotation.Pitch, Row.Rotation.Yaw, Row.Rotation.Roll);
}

void UTheGoldenAgePlayerSync::ConfigurePawnPresence(APawn* Pawn, bool bLocallyControlled) const
{
	if (!Pawn)
	{
		return;
	}

	Pawn->SetReplicates(false);
	Pawn->SetReplicateMovement(false);
	Pawn->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
	// Remote pawns still tick so locomotion can follow snapped transforms. Movement simulation stays off.
	Pawn->SetActorTickEnabled(true);

	if (UCharacterMovementComponent* Movement = Pawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		Movement->SetComponentTickEnabled(bLocallyControlled);
		if (bLocallyControlled)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
		else
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (bLocallyControlled)
			{
				Pawn->EnableInput(Controller);
			}
			else
			{
				Pawn->DisableInput(Controller);
			}
		}
	}
}

const TCHAR* UTheGoldenAgePlayerSync::GetPawnClassPathForRole(uint8 Role) const
{
	return Role == static_cast<uint8>(ETheGoldenAgePlayerRole::Explorer)
		? ExplorerPawnClassPath
		: HeroPawnClassPath;
}

APawn* UTheGoldenAgePlayerSync::EnsurePlayerPawn(const FPlayerType& Row)
{
	if (Row.Name.IsEmpty() || Row.Identity == FSpacetimeDBIdentity())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const TCHAR* PawnClassPath = GetPawnClassPathForRole(Row.Role);
	UClass* PlayerClass = LoadClass<APawn>(nullptr, PawnClassPath);
	if (!PlayerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load player pawn class: %s"), PawnClassPath);
		return nullptr;
	}

	TWeakObjectPtr<APawn>& Existing = PlayerPawns.FindOrAdd(Row.Identity);
	if (APawn* Pawn = Existing.Get())
	{
		if (Pawn->IsA(PlayerClass))
		{
			return Pawn;
		}

		Pawn->Destroy();
		Existing = nullptr;
	}

	const FVector Location = ResolveSpawnLocation(Row);
	const FRotator Rotation = ResolveSpawnRotation(Row);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* SpawnedPawn = World->SpawnActor<APawn>(PlayerClass, Location, Rotation, Params);
	if (!SpawnedPawn)
	{
		return nullptr;
	}

	ConfigurePawnPresence(SpawnedPawn, false);
#if WITH_EDITOR
	SpawnedPawn->SetActorLabel(Row.Name);
#endif

	Existing = SpawnedPawn;
	return SpawnedPawn;
}

void UTheGoldenAgePlayerSync::ReleaseLocalPossession()
{
	UWorld* World = GetWorld();
	if (World)
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			Controller->UnPossess();
		}
	}

	for (const TPair<FSpacetimeDBIdentity, TWeakObjectPtr<APawn>>& Pair : PlayerPawns)
	{
		ConfigurePawnPresence(Pair.Value.Get(), false);
	}

	bLocalPossessionReady = false;
}

void UTheGoldenAgePlayerSync::PossessLocalPlayerPawn()
{
	UWorld* World = GetWorld();
	if (!World || !bHasLocalPlayerIdentity)
	{
		return;
	}

	APlayerController* Controller = World->GetFirstPlayerController();
	const TWeakObjectPtr<APawn>* Found = PlayerPawns.Find(LocalPlayerIdentity);
	APawn* Pawn = Found ? Found->Get() : nullptr;
	if (!Controller || !Pawn)
	{
		bLocalPossessionReady = false;
		return;
	}

	for (const TPair<FSpacetimeDBIdentity, TWeakObjectPtr<APawn>>& Pair : PlayerPawns)
	{
		ConfigurePawnPresence(Pair.Value.Get(), false);
	}

	Controller->Possess(Pawn);
	ConfigurePawnPresence(Pawn, true);
	bLocalPossessionReady = true;
}

void UTheGoldenAgePlayerSync::ApplyPlayerRow(const FPlayerType& Row)
{
	if (Row.Name.IsEmpty() || Row.Identity == FSpacetimeDBIdentity())
	{
		return;
	}

	const bool bClaimLocal = bLoggedIn && Row.Name == LocalUsername;
	if (bClaimLocal)
	{
		LocalPlayerIdentity = Row.Identity;
		bHasLocalPlayerIdentity = true;
	}

	APawn* Pawn = EnsurePlayerPawn(Row);
	if (!Pawn)
	{
		return;
	}

	ATheGoldenAgePlayerCharacter* PlayerCharacter = Cast<ATheGoldenAgePlayerCharacter>(Pawn);
	const bool bWasDead = PlayerCharacter && PlayerCharacter->IsDead();
	if (PlayerCharacter)
	{
		PlayerCharacter->ApplyServerHealth(Row.Health);
	}

	const bool bRespawned = bWasDead && PlayerCharacter && !PlayerCharacter->IsDead();
	const bool bLocalAuthoritative = IsLocalPlayerRow(Row) && bLocalPossessionReady && GetLocalPawn() == Pawn;
	if (bLocalAuthoritative && !bRespawned)
	{
		return;
	}

	const FVector Location = ResolveSpawnLocation(Row);
	const FRotator Rotation = ResolveSpawnRotation(Row);
	Pawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (bRespawned)
	{
		PlayerCharacter->HandleRespawned();
	}

	if (bClaimLocal && !bLocalPossessionReady)
	{
		const bool bNeedsInitialPublish = IsUnsetSpawnTransform(Row);
		PossessLocalPlayerPawn();
		if (bNeedsInitialPublish)
		{
			SendLocalTransform();
		}
	}
}
