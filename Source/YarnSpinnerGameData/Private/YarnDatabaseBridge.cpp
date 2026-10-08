#include "YarnDatabaseBridge.h"
#include "GameDatabaseSubsystem.h"
#include "YarnDialogueRunner.h"
#include <charconv>
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
    bool ParseDatabaseNumber(const FString& Text, float& Value, bool bQuantity = false)
    {
        const FString Trimmed = Text.TrimStartAndEnd();
        const FTCHARToUTF8 Utf8(*Trimmed);
        const char* Start = Utf8.Get();
        const char* End = Start + Utf8.Length();
        if (Start != End && *Start == '+') ++Start;
        double Parsed = 0;
        const auto Result = std::from_chars(Start, End, Parsed);
        if (Result.ec != std::errc() || Result.ptr != End || !FMath::IsFinite(Parsed)) return false;
        if (bQuantity && (Parsed <= 0 || Parsed > UGameDatabaseSubsystem::MaxQuantity || FMath::FloorToDouble(Parsed) != Parsed))
            return false;
        Value = float(Parsed);
        return FMath::IsFinite(Value);
    }
}

bool UYarnDatabaseBridge::WholeAmount(float Amount)
{
    return FMath::IsFinite(Amount) && Amount > 0 && Amount <= UGameDatabaseSubsystem::MaxQuantity
        && FMath::FloorToFloat(Amount) == Amount;
}
bool UYarnDatabaseBridge::WriteAmount(float Amount) const
{
    if (WholeAmount(Amount)) return true;
    Database->LastError = TEXT("Amount must be a positive whole number up to 16777216.");
    return false;
}
void UYarnDatabaseBridge::QueryError(const FString& Message) const
{
    if (!Runner.IsValid()) return;
    Runner->ReportFunctionError(TEXT("Database query failed: ") + Message);
    // Let the VM unwind before closing presenters and cancelling input.
    if (UWorld* World = Runner->GetWorld())
    {
        const TWeakObjectPtr<const UYarnDatabaseBridge> WeakBridge(this);
        const uint64 FailedGeneration = DialogueGeneration;
        World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakBridge, FailedGeneration]()
        {
            if (const auto* Bridge = WeakBridge.Get(); Bridge && Bridge->DialogueGeneration == FailedGeneration
                && Bridge->Runner.IsValid()) Bridge->Runner->StopDialogue();
        }));
    }
}
void UYarnDatabaseBridge::CommandError(const FString& Message) const
{
    UE_LOG(LogTemp, Error, TEXT("Database command failed: %s"), *Message);
    if (Runner.IsValid()) Runner->StopDialogue();
}
void UYarnDatabaseBridge::CheckWrite(bool Success) const
{
    if (!Success) CommandError(Database->LastError);
}
float UYarnDatabaseBridge::DBNumber(const FString& RecordID, const FString& Field) const { float V=0; if (!Database->TryGetNumber(FName(*RecordID), FName(*Field), V)) QueryError(RecordID + TEXT(".") + Field); return V; }
bool UYarnDatabaseBridge::DBBool(const FString& RecordID, const FString& Field) const { bool V=false; if (!Database->TryGetBool(FName(*RecordID), FName(*Field), V)) QueryError(RecordID + TEXT(".") + Field); return V; }
FString UYarnDatabaseBridge::DBString(const FString& RecordID, const FString& Field) const { FString V; if (!Database->TryGetString(FName(*RecordID), FName(*Field), V)) QueryError(RecordID + TEXT(".") + Field); return V; }
bool UYarnDatabaseBridge::HasRecord(const FString& RecordID) const { return Database->HasRecord(FName(*RecordID)); }
float UYarnDatabaseBridge::NPCAffinity(const FString& NPCID) const { float V=0; if (!Database->TryGetNPCAffinity(FName(*NPCID), V)) QueryError(NPCID + TEXT(".affinity")); return V; }
float UYarnDatabaseBridge::ItemCount(const FString& ItemID) const { int32 V=0; if (!Database->TryGetItemCount(FName(*ItemID), V)) QueryError(ItemID); return float(V); }
bool UYarnDatabaseBridge::HasItem(const FString& ItemID, float Amount) const { if (!WholeAmount(Amount)) { QueryError(TEXT("has_item requires a positive whole amount")); return false; } int32 V=0; if (!Database->TryGetItemCount(FName(*ItemID), V)) QueryError(ItemID); return V >= Amount; }
float UYarnDatabaseBridge::Money() const { return float(Database->GetMoney()); }
bool UYarnDatabaseBridge::CanAfford(float Amount) const { if (!WholeAmount(Amount)) { QueryError(TEXT("can_afford requires a positive whole amount")); return false; } return Database->GetMoney() >= Amount; }
FString UYarnDatabaseBridge::EquippedItem(const FString& SlotID) const { FName V; if (!Database->TryGetEquippedItem(FName(*SlotID), V)) QueryError(SlotID); return V.IsNone() ? FString() : V.ToString(); }
void UYarnDatabaseBridge::MoneyAdd(float Amount) { CheckWrite(WriteAmount(Amount) && Database->AddMoney(int32(Amount))); }
void UYarnDatabaseBridge::MoneyRemove(float Amount) { CheckWrite(WriteAmount(Amount) && Database->TrySpendMoney(int32(Amount))); }
void UYarnDatabaseBridge::ItemAdd(const FString& ItemID, float Amount) { CheckWrite(WriteAmount(Amount) && Database->AddItem(FName(*ItemID), int32(Amount))); }
void UYarnDatabaseBridge::ItemRemove(const FString& ItemID, float Amount) { CheckWrite(WriteAmount(Amount) && Database->TryRemoveItem(FName(*ItemID), int32(Amount))); }
void UYarnDatabaseBridge::Equip(const FString& SlotID, const FString& ItemID) { CheckWrite(Database->EquipItem(FName(*SlotID), FName(*ItemID))); }
void UYarnDatabaseBridge::Unequip(const FString& SlotID) { CheckWrite(Database->UnequipItem(FName(*SlotID))); }
void UYarnDatabaseBridge::AffinityAdd(const FString& NPCID, float Delta) { CheckWrite(Database->AddNPCAffinity(FName(*NPCID), Delta)); }
void UYarnDatabaseBridge::SetNumber(const FString& RecordID, const FString& Field, float Value) { CheckWrite(Database->SetNumber(FName(*RecordID), FName(*Field), Value)); }
void UYarnDatabaseBridge::AddNumber(const FString& RecordID, const FString& Field, float Delta) { CheckWrite(Database->AddNumber(FName(*RecordID), FName(*Field), Delta)); }
void UYarnDatabaseBridge::SetBool(const FString& RecordID, const FString& Field, bool Value) { CheckWrite(Database->SetBool(FName(*RecordID), FName(*Field), Value)); }
void UYarnDatabaseBridge::SetString(const FString& RecordID, const FString& Field, const FString& Value) { CheckWrite(Database->SetString(FName(*RecordID), FName(*Field), Value)); }
void UYarnDatabaseBridge::Initialize(UYarnDialogueRunner* InRunner, UGameDatabaseSubsystem* InDatabase)
{
    Runner = InRunner; Database = InDatabase;
    InRunner->OnDialogueStart.AddUniqueDynamic(this, &UYarnDatabaseBridge::DialogueStarted);
    const TWeakObjectPtr<UYarnDatabaseBridge> WeakThis(this);
    InRunner->AddFunction(TEXT("db_number"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 2 || P[0].Type != EYarnValueType::String || P[1].Type != EYarnValueType::String)
        { Self->QueryError(TEXT("db_number: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->DBNumber(P[0].GetStringValue(), P[1].GetStringValue()));
    }, 2);
    InRunner->AddFunction(TEXT("db_bool"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 2 || P[0].Type != EYarnValueType::String || P[1].Type != EYarnValueType::String)
        { Self->QueryError(TEXT("db_bool: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->DBBool(P[0].GetStringValue(), P[1].GetStringValue()));
    }, 2);
    InRunner->AddFunction(TEXT("db_string"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 2 || P[0].Type != EYarnValueType::String || P[1].Type != EYarnValueType::String)
        { Self->QueryError(TEXT("db_string: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->DBString(P[0].GetStringValue(), P[1].GetStringValue()));
    }, 2);
    InRunner->AddFunction(TEXT("db_has_record"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 1 || P[0].Type != EYarnValueType::String)
        { Self->QueryError(TEXT("db_has_record: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->HasRecord(P[0].GetStringValue()));
    }, 1);
    InRunner->AddFunction(TEXT("npc_affinity"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 1 || P[0].Type != EYarnValueType::String)
        { Self->QueryError(TEXT("npc_affinity: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->NPCAffinity(P[0].GetStringValue()));
    }, 1);
    InRunner->AddFunction(TEXT("item_count"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 1 || P[0].Type != EYarnValueType::String)
        { Self->QueryError(TEXT("item_count: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->ItemCount(P[0].GetStringValue()));
    }, 1);
    InRunner->AddFunction(TEXT("has_item"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 2 || P[0].Type != EYarnValueType::String || P[1].Type != EYarnValueType::Number)
        { Self->QueryError(TEXT("has_item: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->HasItem(P[0].GetStringValue(), P[1].GetNumberValue()));
    }, 2);
    InRunner->AddFunction(TEXT("money"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 0)
        { Self->QueryError(TEXT("money: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->Money());
    }, 0);
    InRunner->AddFunction(TEXT("can_afford"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 1 || P[0].Type != EYarnValueType::Number)
        { Self->QueryError(TEXT("can_afford: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->CanAfford(P[0].GetNumberValue()));
    }, 1);
    InRunner->AddFunction(TEXT("equipped_item"), [WeakThis](const TArray<FYarnValue>& P) -> FYarnValue
    {
        auto* Self = WeakThis.Get();
        if (!Self) return FYarnValue();
        if (P.Num() != 1 || P[0].Type != EYarnValueType::String)
        { Self->QueryError(TEXT("equipped_item: wrong arguments")); return FYarnValue(); }
        return FYarnValue(Self->EquippedItem(P[0].GetStringValue()));
    }, 1);
    InRunner->AddCommandHandler(TEXT("money_add"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 1) { Self->CommandError(TEXT("money_add: wrong argument count")); return; }
        float A0;
        if (!ParseDatabaseNumber(P[0], A0, true))
        { Self->CommandError(TEXT("money_add: expected a finite number")); return; }
        Self->MoneyAdd(A0);
    });
    InRunner->AddCommandHandler(TEXT("money_remove"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 1) { Self->CommandError(TEXT("money_remove: wrong argument count")); return; }
        float A0;
        if (!ParseDatabaseNumber(P[0], A0, true))
        { Self->CommandError(TEXT("money_remove: expected a finite number")); return; }
        Self->MoneyRemove(A0);
    });
    InRunner->AddCommandHandler(TEXT("item_add"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 2) { Self->CommandError(TEXT("item_add: wrong argument count")); return; }
        float A1;
        if (!ParseDatabaseNumber(P[1], A1, true))
        { Self->CommandError(TEXT("item_add: expected a finite number")); return; }
        Self->ItemAdd(P[0], A1);
    });
    InRunner->AddCommandHandler(TEXT("item_remove"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 2) { Self->CommandError(TEXT("item_remove: wrong argument count")); return; }
        float A1;
        if (!ParseDatabaseNumber(P[1], A1, true))
        { Self->CommandError(TEXT("item_remove: expected a finite number")); return; }
        Self->ItemRemove(P[0], A1);
    });
    InRunner->AddCommandHandler(TEXT("equip_item"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 2) { Self->CommandError(TEXT("equip_item: wrong argument count")); return; }
        Self->Equip(P[0], P[1]);
    });
    InRunner->AddCommandHandler(TEXT("unequip_item"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 1) { Self->CommandError(TEXT("unequip_item: wrong argument count")); return; }
        Self->Unequip(P[0]);
    });
    InRunner->AddCommandHandler(TEXT("npc_affinity_add"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 2) { Self->CommandError(TEXT("npc_affinity_add: wrong argument count")); return; }
        float A1;
        if (!ParseDatabaseNumber(P[1], A1))
        { Self->CommandError(TEXT("npc_affinity_add: expected a finite number")); return; }
        Self->AffinityAdd(P[0], A1);
    });
    InRunner->AddCommandHandler(TEXT("db_set_number"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 3) { Self->CommandError(TEXT("db_set_number: wrong argument count")); return; }
        float A2;
        if (!ParseDatabaseNumber(P[2], A2))
        { Self->CommandError(TEXT("db_set_number: expected a finite number")); return; }
        Self->SetNumber(P[0], P[1], A2);
    });
    InRunner->AddCommandHandler(TEXT("db_add_number"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 3) { Self->CommandError(TEXT("db_add_number: wrong argument count")); return; }
        float A2;
        if (!ParseDatabaseNumber(P[2], A2))
        { Self->CommandError(TEXT("db_add_number: expected a finite number")); return; }
        Self->AddNumber(P[0], P[1], A2);
    });
    InRunner->AddCommandHandler(TEXT("db_set_bool"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 3) { Self->CommandError(TEXT("db_set_bool: wrong argument count")); return; }
        if (P[2] != TEXT("true") && P[2] != TEXT("false"))
        { Self->CommandError(TEXT("db_set_bool: expected true or false")); return; }
        Self->SetBool(P[0], P[1], P[2] == TEXT("true"));
    });
    InRunner->AddCommandHandler(TEXT("db_set_string"), [WeakThis](const TArray<FString>& P)
    {
        auto* Self = WeakThis.Get(); if (!Self) return;
        if (P.Num() != 3) { Self->CommandError(TEXT("db_set_string: wrong argument count")); return; }
        Self->SetString(P[0], P[1], P[2]);
    });
}
