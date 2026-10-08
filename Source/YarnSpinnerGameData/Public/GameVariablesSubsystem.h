#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameDataTypes.h"
#include "GameVariablesSubsystem.generated.h"

class UYarnProject;
class UYarnDialogueRunner;
class UGameSaveSubsystem;
class UGameDatabaseSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGameVariableChanged, const FString&, VariableName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGameVariablesReplaced);

/** One authoritative variable set per GameInstance. No actor is needed to access it. */
UCLASS()
class YARNSPINNERGAMEDATA_API UGameVariablesSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintPure, Category="Game Data|Variables")
    bool TryGetNumber(const FString& Name, float& Value) const;
    UFUNCTION(BlueprintPure, Category="Game Data|Variables")
    bool TryGetBool(const FString& Name, bool& Value) const;
    UFUNCTION(BlueprintPure, Category="Game Data|Variables")
    bool TryGetString(const FString& Name, FString& Value) const;

    UFUNCTION(BlueprintCallable, Category="Game Data|Variables")
    bool SetNumber(const FString& Name, float Value);
    UFUNCTION(BlueprintCallable, Category="Game Data|Variables")
    bool SetBool(const FString& Name, bool Value);
    UFUNCTION(BlueprintCallable, Category="Game Data|Variables")
    bool SetString(const FString& Name, const FString& Value);
    /** Requires an existing number; a typo must not silently create a new balance. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Variables")
    bool AddNumber(const FString& Name, float Delta, float& NewValue);

    UFUNCTION(BlueprintPure, Category="Game Data|Variables")
    bool Contains(const FString& Name) const;
    UFUNCTION(BlueprintPure, Category="Game Data|Variables")
    FGameVariableSnapshot GetSnapshot() const;
    UFUNCTION(BlueprintPure, Category="Game Data|Debug")
    FString GetFormattedVariables() const;

    /** Register declarations without overwriting current values. Conflicts reject the entire registration. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Defaults")
    bool RegisterYarnProject(UYarnProject* Project);
    UFUNCTION(BlueprintCallable, Category="Game Data|Defaults")
    bool RegisterDefaults(const FGameVariableSnapshot& NewDefaults);

    /** Read the new value using a typed getter. Fired once for an actual single-value change. */
    UPROPERTY(BlueprintAssignable, Category="Game Data|Events")
    FGameVariableChanged OnVariableChanged;
    /** Reload all displayed data after load/new game/default registration. */
    UPROPERTY(BlueprintAssignable, Category="Game Data|Events")
    FGameVariablesReplaced OnStateReplaced;
    UPROPERTY(BlueprintReadOnly, Category="Game Data|Status")
    FString LastError;
    UPROPERTY(BlueprintReadOnly, Category="Game Data|Status")
    bool bDefaultsReady = false;

    bool HasActiveDialogue() const;
    void TrackRunner(UYarnDialogueRunner* Runner);
    uint64 GetRevision() const { return Revision; }
    // Used by the save subsystem and integration tests. Validation completes before any mutation.
    bool RestoreSnapshot(const FGameVariableSnapshot& Snapshot, bool bNotify = true);
    bool ValidateRestoreSnapshot(const FGameVariableSnapshot& Snapshot, FString& Error) const;
    void ResetToDefaults(bool bNotify = true);
    static bool ValidateSnapshot(const FGameVariableSnapshot& Snapshot, FString& Error);

private:
    UPROPERTY(Transient)
    TObjectPtr<UGameDatabaseSubsystem> Database;
    UPROPERTY(Transient)
    FGameVariableSnapshot Defaults;
    // Weak handles only for guarding load/reset. These do not own level actors or their data.
    TArray<TWeakObjectPtr<UYarnDialogueRunner>> Runners;
    uint64 Revision = 0;
    bool CanSet(const FString& Name, uint8 Type);
};
