#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameDatabaseTypes.h"
#include "GameProgressSave.generated.h"

/** Stable save envelope. Game progress lives only in Database.CustomGameData. */
UCLASS()
class YARNSPINNERGAMEDATA_API UGameProgressSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) FString Format = TEXT("YarnSpinner.CheckpointDatabase");
    /** Zero is invalid/unversioned. SaveSlot explicitly writes the configured version. */
    UPROPERTY(SaveGame) int32 Version = 0;
    UPROPERTY(SaveGame) FGameDatabaseSnapshot Database;
    UPROPERTY(SaveGame) FDateTime SavedAtUtc;
};
