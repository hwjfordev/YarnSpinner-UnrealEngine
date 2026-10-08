#include "GameSaveSubsystem.h"
#include "GameDatabaseSubsystem.h"
#include "GameVariablesSubsystem.h"
#include "GameProgressSave.h"
#include "GameSaveMigrationHandler.h"
#include "SharedGameDataSettings.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UGameSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGameVariablesSubsystem>();
    Database = GetGameInstance()->GetSubsystem<UGameDatabaseSubsystem>();
    Variables = GetGameInstance()->GetSubsystem<UGameVariablesSubsystem>();
}
void UGameSaveSubsystem::Deinitialize()
{
    bShuttingDown = true; PendingSave = nullptr; MigrationHandler = nullptr;
    Super::Deinitialize();
}
bool UGameSaveSubsystem::ValidSlotName(const FString& Slot)
{
    if (Slot.IsEmpty() || Slot.Len() > 96) return false;
    for (TCHAR C : Slot)
        if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_' || C == '-')) return false;
    return true;
}
int32 UGameSaveSubsystem::GetSaveDataVersion() const
{
    return GetDefault<USharedGameDataSettings>()->SaveDataVersion;
}
bool UGameSaveSubsystem::RegisterSaveMigrationHandler(UObject* Handler)
{
    LastError.Reset();
    if (bShuttingDown || bBusy) { LastError = TEXT("Cannot change the migration handler during save/load or shutdown."); return false; }
    if (!IsValid(Handler) || !Handler->GetClass()->ImplementsInterface(UGameSaveMigrationHandler::StaticClass()))
    { LastError = TEXT("Handler must implement Game Save Migration Handler."); return false; }
    MigrationHandler = Handler;
    return true;
}
bool UGameSaveSubsystem::UnregisterSaveMigrationHandler()
{
    LastError.Reset();
    if (bShuttingDown || bBusy) { LastError = TEXT("Cannot change the migration handler during save/load or shutdown."); return false; }
    MigrationHandler = nullptr;
    return true;
}
bool UGameSaveSubsystem::ValidateRequest(const FString& Slot, int32 UserIndex)
{
    LastError.Reset();
    if (bShuttingDown || !Database || !Database->bReady || !Variables || !Variables->bDefaultsReady)
    { LastError = TEXT("Game database defaults are unavailable."); return false; }
    if (bBusy) { LastError = TEXT("A save/load is already in progress."); return false; }
    if (GetSaveDataVersion() < 1) { LastError = TEXT("Save Data Version must be at least 1."); return false; }
    if (!ValidSlotName(Slot) || UserIndex < 0)
    { LastError = TEXT("Use a nonempty slot of letters, digits, _ or -, and nonnegative User Index."); return false; }
    return true;
}
bool UGameSaveSubsystem::CanReplaceState()
{
    LastError.Reset();
    if (bShuttingDown || !Database || !Database->bReady || !Variables || !Variables->bDefaultsReady)
    { LastError = TEXT("Game database defaults are unavailable."); return false; }
    if (bBusy) { LastError = TEXT("Wait for the current save/load."); return false; }
    if (Variables->HasActiveDialogue())
    { LastError = TEXT("Stop dialogue before loading or starting a new game."); return false; }
    return true;
}
bool UGameSaveSubsystem::SaveSlot(const FString& Slot, int32 UserIndex)
{
    if (!ValidateRequest(Slot, UserIndex)) return false;
    if (Variables->HasActiveDialogue())
    { LastError = TEXT("Finish dialogue before saving at a checkpoint."); return false; }
    const FGameDatabaseSnapshot Snapshot = Database->GetSnapshot();
    if (!Database->ValidateSnapshot(Snapshot, LastError)
        || !Variables->ValidateRestoreSnapshot(Snapshot.YarnVariables, LastError)) return false;
    PendingSave = NewObject<UGameProgressSave>(this);
    PendingSave->Database = Snapshot; PendingSave->Version = GetSaveDataVersion();
    PendingSave->SavedAtUtc = FDateTime::UtcNow(); bBusy = true;
    UGameplayStatics::AsyncSaveGameToSlot(PendingSave, Slot, UserIndex,
        FAsyncSaveGameToSlotDelegate::CreateWeakLambda(this, [this](const FString& Name, int32, bool bSuccess)
        {
            if (bShuttingDown) return;
            PendingSave = nullptr; bBusy = false;
            LastError = bSuccess ? FString() : TEXT("Writing the save slot failed.");
            if (bSuccess) LastSuccessfulSlot = Name;
            const FString Error = LastError;
            OnSaveCompleted.Broadcast(Name, bSuccess, Error);
        }));
    return true;
}
bool UGameSaveSubsystem::LoadSlot(const FString& Slot, int32 UserIndex)
{
    if (!ValidateRequest(Slot, UserIndex) || !CanReplaceState()) return false;
    bBusy = true;
    const uint64 StartRevision = Database->GetRevision();
    const int32 CurrentVersion = GetSaveDataVersion();
    UGameplayStatics::AsyncLoadGameFromSlot(Slot, UserIndex,
        FAsyncLoadGameFromSlotDelegate::CreateWeakLambda(this, [this, StartRevision, CurrentVersion](const FString& Name, int32, USaveGame* Loaded)
        {
            if (bShuttingDown) return;
            FString Error;
            const UGameProgressSave* Save = Cast<UGameProgressSave>(Loaded);
            if (!Save || Save->GetClass() != UGameProgressSave::StaticClass())
                Error = TEXT("Slot is missing, corrupt or uses another save class.");
            // Format is a stable identity marker, not a second configurable version.
            else if (Save->Format != TEXT("YarnSpinner.CheckpointDatabase") || Save->Version < 1)
                Error = TEXT("Unrecognized database save format or invalid version.");
            else if (Database->GetRevision() != StartRevision || Variables->HasActiveDialogue())
                Error = TEXT("Gameplay changed during loading; retry while database updates and dialogue are stopped.");
            FGameDatabaseSnapshot Candidate;
            if (Error.IsEmpty())
            {
                Candidate = Save->Database;
                if (Save->Version != CurrentVersion)
                {
                    if (!IsValid(MigrationHandler))
                        Error = FString::Printf(TEXT("Save version %d differs from %d. Register a migration handler before Load Slot."), Save->Version, CurrentVersion);
                    else
                    {
                        FString MigrationError;
                        if (!IGameSaveMigrationHandler::Execute_MigrateSaveData(MigrationHandler, Save->Version, CurrentVersion,
                            Save->Database, Candidate, MigrationError))
                            Error = MigrationError.IsEmpty() ? TEXT("Save migration was rejected by the handler.") : MigrationError;
                    }
                }
            }
            // A handler must only edit its candidate. Recheck after any user Blueprint execution.
            if (bShuttingDown) return;
            if (Error.IsEmpty() && (Database->GetRevision() != StartRevision || Variables->HasActiveDialogue()
                || GetSaveDataVersion() != CurrentVersion))
                Error = TEXT("Gameplay or save version changed during loading/migration; candidate was not applied.");
            if (Error.IsEmpty()) Database->ValidateSnapshot(Candidate, Error);
            if (Error.IsEmpty()) Variables->ValidateRestoreSnapshot(Candidate.YarnVariables, Error);
            if (Error.IsEmpty())
            {
                // Commit only the fully migrated and validated value copy. Loading never writes the slot.
                Database->State = MoveTemp(Candidate);
                Variables->RestoreSnapshot(Database->State.YarnVariables, false);
                LastSuccessfulSlot = Name;
                Database->OnDatabaseReplaced.Broadcast();
                Variables->OnStateReplaced.Broadcast();
            }
            bBusy = false; LastError = Error;
            OnLoadCompleted.Broadcast(Name, Error.IsEmpty(), Error);
        }));
    return true;
}
bool UGameSaveSubsystem::StartNewGame()
{
    if (!CanReplaceState()) return false;
    TGuardValue<bool> Guard(bBusy, true);
    Database->State = Database->Defaults;
    Variables->ResetToDefaults(false);
    LastSuccessfulSlot.Reset();
    Database->OnDatabaseReplaced.Broadcast();
    Variables->OnStateReplaced.Broadcast();
    OnNewGameStarted.Broadcast();
    return true;
}
bool UGameSaveSubsystem::DoesSlotExist(const FString& Slot, int32 UserIndex) const
{
    return ValidSlotName(Slot) && UserIndex >= 0 && UGameplayStatics::DoesSaveGameExist(Slot, UserIndex);
}
