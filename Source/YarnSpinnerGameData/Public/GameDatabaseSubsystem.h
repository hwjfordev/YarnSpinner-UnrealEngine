#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameDatabaseTypes.h"
#include "GameDatabaseSubsystem.generated.h"

class UGameVariablesSubsystem;
class UGameSaveSubsystem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGameDatabaseChanged, FName, RecordID, FName, Field);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGameDatabaseReplaced);

/** Authoritative, actor-free session database. All mutations go through validated APIs. */
UCLASS()
class YARNSPINNERGAMEDATA_API UGameDatabaseSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    // Yarn numbers are floats; this is the largest consecutive exactly representable integer.
    static constexpr int32 MaxQuantity = 16777216;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintPure, Category="Game Data|Database") bool HasRecord(FName RecordID) const;
    UFUNCTION(BlueprintPure, Category="Game Data|Database") bool TryGetRecord(FName RecordID, FGameDataRecord& Record) const;
    /** Explicit creation only; setters reject missing records/fields and type mismatches. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Database") bool CreateRecord(FName RecordID, const FGameDataRecord& Record);
    UFUNCTION(BlueprintPure, Category="Game Data|Database") bool TryGetNumber(FName RecordID, FName Field, float& Value) const;
    UFUNCTION(BlueprintPure, Category="Game Data|Database") bool TryGetBool(FName RecordID, FName Field, bool& Value) const;
    UFUNCTION(BlueprintPure, Category="Game Data|Database") bool TryGetString(FName RecordID, FName Field, FString& Value) const;
    UFUNCTION(BlueprintCallable, Category="Game Data|Database") bool SetNumber(FName RecordID, FName Field, float Value);
    UFUNCTION(BlueprintCallable, Category="Game Data|Database") bool SetBool(FName RecordID, FName Field, bool Value);
    UFUNCTION(BlueprintCallable, Category="Game Data|Database") bool SetString(FName RecordID, FName Field, const FString& Value);
    UFUNCTION(BlueprintCallable, Category="Game Data|Database") bool AddNumber(FName RecordID, FName Field, float Delta);
    /** Affinity is the number field "affinity" on the supplied NPC record ID. */
    UFUNCTION(BlueprintPure, Category="Game Data|NPC") bool TryGetNPCAffinity(FName NPCID, float& Value) const;
    UFUNCTION(BlueprintCallable, Category="Game Data|NPC") bool AddNPCAffinity(FName NPCID, float Delta);

    UFUNCTION(BlueprintPure, Category="Game Data|Inventory") bool TryGetItemDefinition(FName ItemID, FGameItemDefinition& Definition) const;
    /** Known but unowned items return true with Count=0. Unknown IDs return false. */
    UFUNCTION(BlueprintPure, Category="Game Data|Inventory") bool TryGetItemCount(FName ItemID, int32& Count) const;
    UFUNCTION(BlueprintCallable, Category="Game Data|Inventory") bool AddItem(FName ItemID, int32 Amount = 1);
    /** Fails without mutation if insufficient, invalid, or removing equipped units. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Inventory") bool TryRemoveItem(FName ItemID, int32 Amount = 1);
    UFUNCTION(BlueprintPure, Category="Game Data|Inventory") TMap<FName, int32> GetInventory() const { return State.Inventory; }
    UFUNCTION(BlueprintPure, Category="Game Data|Money") int32 GetMoney() const;
    UFUNCTION(BlueprintPure, Category="Game Data|Money") FName GetMoneyItemID() const { return MoneyItemID; }
    UFUNCTION(BlueprintCallable, Category="Game Data|Money") bool AddMoney(int32 Amount);
    UFUNCTION(BlueprintCallable, Category="Game Data|Money") bool TrySpendMoney(int32 Amount);

    UFUNCTION(BlueprintPure, Category="Game Data|Equipment") bool TryGetEquippedItem(FName SlotID, FName& ItemID) const;
    UFUNCTION(BlueprintCallable, Category="Game Data|Equipment") bool EquipItem(FName SlotID, FName ItemID);
    UFUNCTION(BlueprintCallable, Category="Game Data|Equipment") bool UnequipItem(FName SlotID);

    UFUNCTION(BlueprintPure, Category="Game Data|Database") FGameDatabaseSnapshot GetSnapshot() const { return State; }
    UFUNCTION(BlueprintPure, Category="Game Data|Progress") FInstancedStruct GetCustomGameData() const { return State.CustomGameData; }
    /** Copies your struct into the database. Re-submit after editing the copy returned by Get. */
    UFUNCTION(BlueprintCallable, Category="Game Data|Progress") bool SetCustomGameData(const FInstancedStruct& Data);
    UPROPERTY(BlueprintAssignable, Category="Game Data|Events") FGameDatabaseChanged OnDataChanged;
    UPROPERTY(BlueprintAssignable, Category="Game Data|Events") FGameDatabaseReplaced OnDatabaseReplaced;
    UPROPERTY(BlueprintReadOnly, Category="Game Data|Status") FString LastError;
    UPROPERTY(BlueprintReadOnly, Category="Game Data|Status") bool bReady = false;

    uint64 GetRevision() const { return Revision; }
    bool ValidateSnapshot(const FGameDatabaseSnapshot& Candidate, FString& Error) const;
    static bool ValidateRecord(FName ID, const FGameDataRecord& Record, FString& Error);
    static bool ValidID(FName ID);
private:
    friend class UGameVariablesSubsystem;
    friend class UGameSaveSubsystem;
    UPROPERTY(Transient) FGameDatabaseSnapshot State;
    UPROPERTY(Transient) FGameDatabaseSnapshot Defaults;
    UPROPERTY(Transient) TMap<FName, FGameItemDefinition> ItemDefinitions;
    UPROPERTY(Transient) TArray<FName> Slots;
    FName MoneyItemID;
    uint64 Revision = 0;
    bool CanWrite();
    bool CanWriteField(FName RecordID, FName Field, uint8 Type);
    bool CheckItemAmount(FName ItemID, int32 Amount);
    void Changed(FName RecordID, FName Field);
};
