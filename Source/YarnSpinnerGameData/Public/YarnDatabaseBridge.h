#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "YarnDatabaseBridge.generated.h"
class UGameDatabaseSubsystem;
class UYarnDialogueRunner;

/** Per-runner bridge. ConnectRunnerToSharedVariables installs this automatically, including cooked builds. */
UCLASS()
class YARNSPINNERGAMEDATA_API UYarnDatabaseBridge : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(UYarnDialogueRunner* InRunner, UGameDatabaseSubsystem* InDatabase);
    UFUNCTION(meta=(YarnFunction="db_number")) float DBNumber(const FString& RecordID, const FString& Field) const;
    UFUNCTION(meta=(YarnFunction="db_bool")) bool DBBool(const FString& RecordID, const FString& Field) const;
    UFUNCTION(meta=(YarnFunction="db_string")) FString DBString(const FString& RecordID, const FString& Field) const;
    UFUNCTION(meta=(YarnFunction="db_has_record")) bool HasRecord(const FString& RecordID) const;
    UFUNCTION(meta=(YarnFunction="npc_affinity")) float NPCAffinity(const FString& NPCID) const;
    UFUNCTION(meta=(YarnFunction="item_count")) float ItemCount(const FString& ItemID) const;
    UFUNCTION(meta=(YarnFunction="has_item")) bool HasItem(const FString& ItemID, float Amount) const;
    UFUNCTION(meta=(YarnFunction="money")) float Money() const;
    UFUNCTION(meta=(YarnFunction="can_afford")) bool CanAfford(float Amount) const;
    UFUNCTION(meta=(YarnFunction="equipped_item")) FString EquippedItem(const FString& SlotID) const;
    UFUNCTION(meta=(YarnCommand="money_add")) void MoneyAdd(float Amount);
    UFUNCTION(meta=(YarnCommand="money_remove")) void MoneyRemove(float Amount);
    UFUNCTION(meta=(YarnCommand="item_add")) void ItemAdd(const FString& ItemID, float Amount);
    UFUNCTION(meta=(YarnCommand="item_remove")) void ItemRemove(const FString& ItemID, float Amount);
    UFUNCTION(meta=(YarnCommand="equip_item")) void Equip(const FString& SlotID, const FString& ItemID);
    UFUNCTION(meta=(YarnCommand="unequip_item")) void Unequip(const FString& SlotID);
    UFUNCTION(meta=(YarnCommand="npc_affinity_add")) void AffinityAdd(const FString& NPCID, float Delta);
    UFUNCTION(meta=(YarnCommand="db_set_number")) void SetNumber(const FString& RecordID, const FString& Field, float Value);
    UFUNCTION(meta=(YarnCommand="db_add_number")) void AddNumber(const FString& RecordID, const FString& Field, float Delta);
    UFUNCTION(meta=(YarnCommand="db_set_bool")) void SetBool(const FString& RecordID, const FString& Field, bool Value);
    UFUNCTION(meta=(YarnCommand="db_set_string")) void SetString(const FString& RecordID, const FString& Field, const FString& Value);
private:
    UPROPERTY(Transient) TObjectPtr<UGameDatabaseSubsystem> Database;
    TWeakObjectPtr<UYarnDialogueRunner> Runner;
    uint64 DialogueGeneration = 0;
    UFUNCTION() void DialogueStarted() { ++DialogueGeneration; }
    void QueryError(const FString& Message) const;
    void CommandError(const FString& Message) const;
    void CheckWrite(bool Success) const;
    bool WriteAmount(float Amount) const;
    static bool WholeAmount(float Amount);
};
