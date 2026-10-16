#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameDataBlueprintLibrary.generated.h"

class UGameVariablesSubsystem;
class UGameSaveSubsystem;
class UYarnDialogueRunner;
class UGameDatabaseSubsystem;

/** Native hook for other plugins: install extra Yarn functions/commands on every connected runner.
 * Fires after each successful ConnectRunnerToSharedVariables (which may repeat), so handlers must be idempotent. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSharedRunnerConnected, UYarnDialogueRunner*);

UCLASS()
class YARNSPINNERGAMEDATA_API UGameDataBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Game Data", meta=(WorldContext="WorldContextObject"))
    static UGameVariablesSubsystem* GetGameVariables(const UObject* WorldContextObject);
    UFUNCTION(BlueprintPure, Category="Game Data", meta=(WorldContext="WorldContextObject"))
    static UGameSaveSubsystem* GetGameSaves(const UObject* WorldContextObject);
    UFUNCTION(BlueprintPure, Category="Game Data", meta=(WorldContext="WorldContextObject"))
    static UGameDatabaseSubsystem* GetGameDatabase(const UObject* WorldContextObject);

    /** Call on an idle existing runner before StartDialogue. Safe to call repeatedly. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Yarn")
    static bool ConnectRunnerToSharedVariables(UYarnDialogueRunner* Runner, FString& Error);

    /** C++ only. Bind in a module's StartupModule; the delegate lives for the whole process. */
    static FOnSharedRunnerConnected& OnSharedRunnerConnected();
};
