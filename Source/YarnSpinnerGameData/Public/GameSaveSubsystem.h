#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameDatabaseTypes.h"
#include "GameSaveSubsystem.generated.h"
class UGameDatabaseSubsystem;
class UGameVariablesSubsystem;
class UGameProgressSave;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGameSaveOperationCompleted, const FString&, SlotName, bool, bSuccess, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGameNewSessionStarted);

UCLASS()
class YARNSPINNERGAMEDATA_API UGameSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    /** Accepts an async snapshot write. Use OnSaveCompleted for the result. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Save", meta=(AdvancedDisplay="UserIndex"))
    bool SaveSlot(const FString& SlotName, int32 UserIndex = 0);
    /** On success the complete database is ready. Your custom progress determines where to resume. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Save", meta=(AdvancedDisplay="UserIndex"))
    bool LoadSlot(const FString& SlotName, int32 UserIndex = 0);
    UFUNCTION(BlueprintCallable, Category="Game Data|Save") bool StartNewGame();
    UFUNCTION(BlueprintPure, Category="Game Data|Save", meta=(AdvancedDisplay="UserIndex"))
    bool DoesSlotExist(const FString& SlotName, int32 UserIndex = 0) const;
    /** Keeps one strong reference until replaced/unregistered or GameInstance shutdown. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Save") bool RegisterSaveMigrationHandler(UObject* Handler);
    UFUNCTION(BlueprintCallable, Category="Game Data|Save") bool UnregisterSaveMigrationHandler();
    UFUNCTION(BlueprintPure, Category="Game Data|Save") int32 GetSaveDataVersion() const;
    UPROPERTY(BlueprintAssignable, Category="Game Data|Save") FGameSaveOperationCompleted OnSaveCompleted;
    UPROPERTY(BlueprintAssignable, Category="Game Data|Save") FGameSaveOperationCompleted OnLoadCompleted;
    UPROPERTY(BlueprintAssignable, Category="Game Data|Save") FGameNewSessionStarted OnNewGameStarted;
    UPROPERTY(BlueprintReadOnly, Category="Game Data|Save") bool bBusy = false;
    UPROPERTY(BlueprintReadOnly, Category="Game Data|Save") FString LastError;
    UPROPERTY(BlueprintReadOnly, Category="Game Data|Save") FString LastSuccessfulSlot;
private:
    UPROPERTY(Transient) TObjectPtr<UGameDatabaseSubsystem> Database;
    UPROPERTY(Transient) TObjectPtr<UGameVariablesSubsystem> Variables;
    UPROPERTY(Transient) TObjectPtr<UGameProgressSave> PendingSave;
    UPROPERTY(Transient) TObjectPtr<UObject> MigrationHandler;
    bool bShuttingDown = false;
    bool ValidateRequest(const FString& SlotName, int32 UserIndex);
    bool CanReplaceState();
    static bool ValidSlotName(const FString& SlotName);
};
