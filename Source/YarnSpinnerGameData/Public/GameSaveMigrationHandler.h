#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameDatabaseTypes.h"
#include "GameSaveMigrationHandler.generated.h"

/** Implement on a Blueprint object and register it with Get Game Saves before Load Slot. */
UINTERFACE(BlueprintType, Blueprintable)
class YARNSPINNERGAMEDATA_API UGameSaveMigrationHandler : public UInterface
{
    GENERATED_BODY()
};

class YARNSPINNERGAMEDATA_API IGameSaveMigrationHandler
{
    GENERATED_BODY()
public:
    /** Synchronous, candidate-only conversion. Copy LoadedDatabase, edit that copy, and return it.
     * Do not modify live subsystems, start dialogue, travel, or perform nested save/load here.
     * Called for BOTH older and newer versions. Reject versions you do not support.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Game Data|Save", meta=(ReturnDisplayName="Success"))
    bool MigrateSaveData(int32 SavedVersion, int32 CurrentVersion, const FGameDatabaseSnapshot& LoadedDatabase,
        FGameDatabaseSnapshot& MigratedDatabase, FString& ErrorMessage);
    virtual bool MigrateSaveData_Implementation(int32 SavedVersion, int32 CurrentVersion,
        const FGameDatabaseSnapshot& LoadedDatabase, FGameDatabaseSnapshot& MigratedDatabase, FString& ErrorMessage);
};
