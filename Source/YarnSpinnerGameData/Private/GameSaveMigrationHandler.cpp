#include "GameSaveMigrationHandler.h"

bool IGameSaveMigrationHandler::MigrateSaveData_Implementation(int32 SavedVersion, int32 CurrentVersion,
    const FGameDatabaseSnapshot& LoadedDatabase, FGameDatabaseSnapshot& MigratedDatabase, FString& ErrorMessage)
{
    MigratedDatabase = LoadedDatabase;
    ErrorMessage = FString::Printf(TEXT("No conversion implemented from save version %d to %d."), SavedVersion, CurrentVersion);
    return false;
}
