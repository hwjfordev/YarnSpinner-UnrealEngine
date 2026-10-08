#include "Tests/GameData/DatabaseTestTypes.h"
#include "GameSaveSubsystem.h"
#include "GameDatabaseSubsystem.h"

bool UGameSaveMigrationTestHandler::MigrateSaveData_Implementation(int32 SavedVersion, int32 CurrentVersion,
    const FGameDatabaseSnapshot& LoadedDatabase, FGameDatabaseSnapshot& MigratedDatabase, FString& ErrorMessage)
{
    ++Calls; LastSaved = SavedVersion; LastCurrent = CurrentVersion; Received = LoadedDatabase;
    if (bReject || SavedVersion > CurrentVersion) { ErrorMessage = TEXT("This test handler rejects that version."); return false; }
    bNestedRejected = !Saves->StartNewGame() && !Saves->LoadSlot(TEXT("nested")) && !Saves->SaveSlot(TEXT("nested"))
        && !Saves->UnregisterSaveMigrationHandler() && !Saves->RegisterSaveMigrationHandler(this);
    MigratedDatabase = Replacement;
    if (bInvalidResult) MigratedDatabase.Inventory.Add(TEXT("money"), -1);
    if (bMutateRuntime) Database->AddMoney(1);
    return true;
}
