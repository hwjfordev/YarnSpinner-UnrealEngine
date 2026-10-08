#pragma once
#include "CoreMinimal.h"
#include "GameSaveMigrationHandler.h"
#include "DatabaseTestTypes.generated.h"
USTRUCT()
struct FCheckpointTestProgress
{
    GENERATED_BODY()
    UPROPERTY() FString Chapter;
    UPROPERTY() TArray<FName> CompletedQuests;
    UPROPERTY() TMap<FName, int32> Counters;
};

UCLASS()
class UGameSaveMigrationTestHandler : public UObject, public IGameSaveMigrationHandler
{
    GENERATED_BODY()
public:
    int32 Calls = 0;
    int32 LastSaved = 0;
    int32 LastCurrent = 0;
    bool bReject = false;
    bool bInvalidResult = false;
    bool bMutateRuntime = false;
    bool bNestedRejected = false;
    UPROPERTY() FGameDatabaseSnapshot Replacement;
    UPROPERTY() FGameDatabaseSnapshot Received;
    UPROPERTY() TObjectPtr<class UGameSaveSubsystem> Saves;
    UPROPERTY() TObjectPtr<class UGameDatabaseSubsystem> Database;
    virtual bool MigrateSaveData_Implementation(int32 SavedVersion, int32 CurrentVersion,
        const FGameDatabaseSnapshot& LoadedDatabase, FGameDatabaseSnapshot& MigratedDatabase, FString& ErrorMessage) override;
};
