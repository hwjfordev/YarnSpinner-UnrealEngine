#include "GameDatabaseSubsystem.h"
#include "SharedGameDataSettings.h"
#include "GameVariablesSubsystem.h"
#include "UObject/UnrealType.h"

namespace Database
{
    uint8 TypeOf(const FGameDataRecord& R, FName Field)
    { return R.Numbers.Contains(Field) ? 1 : R.Bools.Contains(Field) ? 2 : R.Strings.Contains(Field) ? 3 : 0; }

    // Save only value data, not runtime object identity. Inspect nested containers and BP structs.
    bool ValueData(const FProperty* Property, const void* Value, FString& Error, int32 Depth);
    bool StructData(const UScriptStruct* Struct, const void* Data, FString& Error, int32 Depth)
    {
        if (!Struct || !Data) return true;
        if (Depth > 32) { Error = TEXT("Custom data exceeds the supported nesting depth."); return false; }
        for (TFieldIterator<FProperty> It(Struct); It; ++It)
            for (int32 Index = 0; Index < It->ArrayDim; ++Index)
                if (!ValueData(*It, It->ContainerPtrToValuePtr<void>(Data, Index), Error, Depth + 1)) return false;
        return true;
    }
    bool ValueData(const FProperty* Property, const void* Value, FString& Error, int32 Depth)
    {
        if (Depth > 32) { Error = TEXT("Custom data exceeds the supported nesting depth."); return false; }
        if (Property->IsA<FSoftObjectProperty>()) return true;
        if (Property->IsA<FObjectPropertyBase>() || Property->IsA<FInterfaceProperty>()
            || Property->IsA<FDelegateProperty>() || Property->IsA<FMulticastDelegateProperty>())
        { Error = TEXT("Custom data must use values, IDs or soft asset references, not live objects/delegates."); return false; }
        if (const auto* Number = CastField<FNumericProperty>(Property); Number && Number->IsFloatingPoint()
            && !FMath::IsFinite(Number->GetFloatingPointPropertyValue(Value)))
        { Error = TEXT("Custom data contains a non-finite number."); return false; }
        if (const auto* Struct = CastField<FStructProperty>(Property))
        {
            if (Struct->Struct == FInstancedStruct::StaticStruct())
            {
                const auto& Instance = *static_cast<const FInstancedStruct*>(Value);
                return StructData(Instance.GetScriptStruct(), Instance.GetMemory(), Error, Depth + 1);
            }
            return StructData(Struct->Struct, Value, Error, Depth + 1);
        }
        if (const auto* Array = CastField<FArrayProperty>(Property))
        {
            FScriptArrayHelper Helper(Array, Value);
            for (int32 I = 0; I < Helper.Num(); ++I)
                if (!ValueData(Array->Inner, Helper.GetRawPtr(I), Error, Depth + 1)) return false;
        }
        if (const auto* Map = CastField<FMapProperty>(Property))
        {
            FScriptMapHelper Helper(Map, Value);
            for (int32 I = 0; I < Helper.GetMaxIndex(); ++I)
                if (Helper.IsValidIndex(I) && (!ValueData(Map->KeyProp, Helper.GetKeyPtr(I), Error, Depth + 1)
                    || !ValueData(Map->ValueProp, Helper.GetValuePtr(I), Error, Depth + 1))) return false;
        }
        if (const auto* Set = CastField<FSetProperty>(Property))
        {
            FScriptSetHelper Helper(Set, Value);
            for (int32 I = 0; I < Helper.GetMaxIndex(); ++I)
                if (Helper.IsValidIndex(I) && !ValueData(Set->ElementProp, Helper.GetElementPtr(I), Error, Depth + 1)) return false;
        }
        return true;
    }
}

bool UGameDatabaseSubsystem::ValidID(FName ID)
{
    if (ID.IsNone()) return false;
    const FString Text = ID.ToString();
    if (Text.Len() > 128) return false;
    for (TCHAR C : Text) if (FChar::IsWhitespace(C) || FChar::IsControl(C)) return false;
    return true;
}

void UGameDatabaseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const auto Configured = GetDefault<USharedGameDataSettings>()->DatabaseDefinition;
    MoneyItemID = TEXT("money");
    if (!Configured.IsNull())
    {
        const auto* Definition = Configured.LoadSynchronous();
        if (!Definition) { LastError = TEXT("Database Definition could not be loaded."); return; }
        ItemDefinitions = Definition->Items;
        Slots = Definition->EquipmentSlots;
        MoneyItemID = Definition->MoneyItemID;
        Defaults = Definition->InitialState;
        if (!Definition->DefaultRecords.IsNull())
        {
            const auto* Table = Definition->DefaultRecords.LoadSynchronous();
            if (!Table || Table->GetRowStruct() != FGameDataRecord::StaticStruct())
            { LastError = TEXT("Default Records must be a DataTable with GameDataRecord rows."); return; }
            for (const auto& Pair : Table->GetRowMap())
            {
                if (Defaults.Records.Contains(Pair.Key))
                { LastError = FString::Printf(TEXT("Duplicate default record: %s"), *Pair.Key.ToString()); return; }
                Defaults.Records.Add(Pair.Key, *reinterpret_cast<const FGameDataRecord*>(Pair.Value));
            }
        }
    }
    else
    {
        FGameItemDefinition Money;
        Money.DisplayName = FText::FromString(TEXT("Money"));
        ItemDefinitions.Add(MoneyItemID, Money);
    }
    if (!ValidID(MoneyItemID) || !ItemDefinitions.Contains(MoneyItemID))
    { LastError = TEXT("Money Item ID must refer to a defined item."); return; }
    TSet<FName> UniqueSlots;
    for (FName Slot : Slots)
    {
        if (!ValidID(Slot) || UniqueSlots.Contains(Slot))
        { LastError = TEXT("Equipment slots must be valid and unique."); return; }
        UniqueSlots.Add(Slot);
    }
    for (const auto& Item : ItemDefinitions)
    {
        if (!ValidID(Item.Key)) { LastError = TEXT("Invalid item definition ID."); return; }
        for (FName Slot : Item.Value.EquipmentSlots)
            if (!UniqueSlots.Contains(Slot)) { LastError = TEXT("Item references an undefined equipment slot."); return; }
    }
    if (!ItemDefinitions[MoneyItemID].EquipmentSlots.IsEmpty())
    { LastError = TEXT("Money cannot be equipped."); return; }
    // Yarn defaults are registered through GameVariablesSubsystem after this subsystem initializes.
    if (Defaults.YarnVariables.Numbers.Num() || Defaults.YarnVariables.Bools.Num() || Defaults.YarnVariables.Strings.Num())
    { LastError = TEXT("Configure Yarn defaults through Yarn Projects / Gameplay Defaults."); return; }
    if (!ValidateSnapshot(Defaults, LastError)) return;
    State = Defaults;
    bReady = true;
}

bool UGameDatabaseSubsystem::ValidateRecord(FName ID, const FGameDataRecord& Record, FString& Error)
{
    if (!ValidID(ID)) { Error = TEXT("Record ID is invalid."); return false; }
    TSet<FName> Seen;
    auto Field = [&Seen, &Error](FName Key)
    {
        if (!ValidID(Key) || Seen.Contains(Key)) { Error = TEXT("Record fields must have valid, unique names across types."); return false; }
        Seen.Add(Key); return true;
    };
    for (const auto& P : Record.Numbers)
        if (!Field(P.Key) || !FMath::IsFinite(P.Value)) { Error = TEXT("Invalid numeric field."); return false; }
    for (const auto& P : Record.Bools) if (!Field(P.Key)) return false;
    for (const auto& P : Record.Strings) if (!Field(P.Key)) return false;
    return true;
}

bool UGameDatabaseSubsystem::ValidateSnapshot(const FGameDatabaseSnapshot& Candidate, FString& Error) const
{
    Error.Reset();
    for (const auto& Pair : Candidate.Records)
        if (!ValidateRecord(Pair.Key, Pair.Value, Error)) return false;
    // Authored records and field types are the schema for this fresh-development format.
    for (const auto& Pair : Defaults.Records)
    {
        const auto* Found = Candidate.Records.Find(Pair.Key);
        if (!Found) { Error = TEXT("Save is missing an authored record."); return false; }
        for (const auto& Field : Pair.Value.Numbers)
            if (Database::TypeOf(*Found, Field.Key) != 1) { Error = TEXT("Numeric field schema differs."); return false; }
        for (const auto& Field : Pair.Value.Bools)
            if (Database::TypeOf(*Found, Field.Key) != 2) { Error = TEXT("Bool field schema differs."); return false; }
        for (const auto& Field : Pair.Value.Strings)
            if (Database::TypeOf(*Found, Field.Key) != 3) { Error = TEXT("String field schema differs."); return false; }
    }
    for (const auto& Pair : Candidate.Inventory)
        if (!ItemDefinitions.Contains(Pair.Key) || Pair.Value <= 0 || Pair.Value > MaxQuantity)
        { Error = TEXT("Inventory has an unknown item or invalid quantity."); return false; }
    TMap<FName, int32> EquippedCounts;
    for (const auto& Pair : Candidate.Equipment)
    {
        const auto* Definition = ItemDefinitions.Find(Pair.Value);
        if (!Slots.Contains(Pair.Key) || !Definition || !Definition->EquipmentSlots.Contains(Pair.Key))
        { Error = TEXT("Equipment item/slot is invalid."); return false; }
        const int32 Required = ++EquippedCounts.FindOrAdd(Pair.Value);
        if (Candidate.Inventory.FindRef(Pair.Value) < Required)
        { Error = TEXT("Equipped items must be owned in sufficient quantity."); return false; }
    }
    if (Defaults.CustomGameData.IsValid() &&
        Defaults.CustomGameData.GetScriptStruct() != Candidate.CustomGameData.GetScriptStruct())
    { Error = TEXT("Custom game data type differs from the configured default struct."); return false; }
    if (!Database::StructData(Candidate.CustomGameData.GetScriptStruct(), Candidate.CustomGameData.GetMemory(), Error, 0)) return false;
    return UGameVariablesSubsystem::ValidateSnapshot(Candidate.YarnVariables, Error);
}

bool UGameDatabaseSubsystem::CanWrite()
{
    LastError.Reset();
    if (!bReady) { LastError = TEXT("Database defaults are unavailable. Check Database Definition."); return false; }
    return true;
}
void UGameDatabaseSubsystem::Changed(FName RecordID, FName Field)
{
    ++Revision;
    OnDataChanged.Broadcast(RecordID, Field);
}
bool UGameDatabaseSubsystem::HasRecord(FName ID) const { return State.Records.Contains(ID); }
bool UGameDatabaseSubsystem::TryGetRecord(FName ID, FGameDataRecord& Record) const
{
    const auto* Found = State.Records.Find(ID);
    Record = Found ? *Found : FGameDataRecord();
    return Found != nullptr;
}
bool UGameDatabaseSubsystem::CreateRecord(FName ID, const FGameDataRecord& Record)
{
    if (!CanWrite() || !ValidateRecord(ID, Record, LastError)) return false;
    if (HasRecord(ID)) { LastError = TEXT("Record already exists."); return false; }
    State.Records.Add(ID, Record); Changed(ID, NAME_None); return true;
}
bool UGameDatabaseSubsystem::CanWriteField(FName ID, FName Field, uint8 Type)
{
    if (!CanWrite()) return false;
    const auto* Record = State.Records.Find(ID);
    if (!Record || Database::TypeOf(*Record, Field) != Type)
    { LastError = FString::Printf(TEXT("Missing or wrong-type field: %s.%s"), *ID.ToString(), *Field.ToString()); return false; }
    return true;
}
bool UGameDatabaseSubsystem::TryGetNumber(FName ID, FName Field, float& Value) const
{
    const auto* Record = State.Records.Find(ID);
    const float* Found = Record ? Record->Numbers.Find(Field) : nullptr;
    Value = Found ? *Found : 0.f; return Found != nullptr;
}
bool UGameDatabaseSubsystem::TryGetBool(FName ID, FName Field, bool& Value) const
{
    const auto* Record = State.Records.Find(ID);
    const bool* Found = Record ? Record->Bools.Find(Field) : nullptr;
    Value = Found ? *Found : false; return Found != nullptr;
}
bool UGameDatabaseSubsystem::TryGetString(FName ID, FName Field, FString& Value) const
{
    const auto* Record = State.Records.Find(ID);
    const FString* Found = Record ? Record->Strings.Find(Field) : nullptr;
    Value = Found ? *Found : FString(); return Found != nullptr;
}
bool UGameDatabaseSubsystem::SetNumber(FName ID, FName Field, float Value)
{
    if (!CanWriteField(ID, Field, 1)) return false;
    if (!FMath::IsFinite(Value)) { LastError = TEXT("Numbers must be finite."); return false; }
    float& Current = State.Records[ID].Numbers[Field];
    if (Current != Value) { Current = Value; Changed(ID, Field); }
    return true;
}
bool UGameDatabaseSubsystem::SetBool(FName ID, FName Field, bool Value)
{
    if (!CanWriteField(ID, Field, 2)) return false;
    bool& Current = State.Records[ID].Bools[Field];
    if (Current != Value) { Current = Value; Changed(ID, Field); }
    return true;
}
bool UGameDatabaseSubsystem::SetString(FName ID, FName Field, const FString& Value)
{
    if (!CanWriteField(ID, Field, 3)) return false;
    FString& Current = State.Records[ID].Strings[Field];
    if (Current != Value) { Current = Value; Changed(ID, Field); }
    return true;
}
bool UGameDatabaseSubsystem::AddNumber(FName ID, FName Field, float Delta)
{
    if (!CanWriteField(ID, Field, 1)) return false;
    return SetNumber(ID, Field, State.Records[ID].Numbers[Field] + Delta);
}
bool UGameDatabaseSubsystem::TryGetNPCAffinity(FName ID, float& Value) const { return TryGetNumber(ID, TEXT("affinity"), Value); }
bool UGameDatabaseSubsystem::AddNPCAffinity(FName ID, float Delta) { return AddNumber(ID, TEXT("affinity"), Delta); }
bool UGameDatabaseSubsystem::TryGetItemDefinition(FName ID, FGameItemDefinition& Definition) const
{
    const auto* Found = ItemDefinitions.Find(ID);
    Definition = Found ? *Found : FGameItemDefinition(); return Found != nullptr;
}
bool UGameDatabaseSubsystem::TryGetItemCount(FName ID, int32& Count) const
{
    Count = State.Inventory.FindRef(ID); return ItemDefinitions.Contains(ID);
}
bool UGameDatabaseSubsystem::CheckItemAmount(FName ID, int32 Amount)
{
    if (!CanWrite()) return false;
    if (!ItemDefinitions.Contains(ID) || Amount <= 0 || Amount > MaxQuantity)
    { LastError = TEXT("Use a known item ID and a positive whole quantity up to 16777216."); return false; }
    return true;
}
bool UGameDatabaseSubsystem::AddItem(FName ID, int32 Amount)
{
    if (!CheckItemAmount(ID, Amount)) return false;
    const int64 Total = int64(State.Inventory.FindRef(ID)) + Amount;
    if (Total > MaxQuantity) { LastError = TEXT("Item quantity exceeds 16777216."); return false; }
    State.Inventory.Add(ID, int32(Total)); Changed(TEXT("inventory"), ID); return true;
}
bool UGameDatabaseSubsystem::TryRemoveItem(FName ID, int32 Amount)
{
    if (!CheckItemAmount(ID, Amount)) return false;
    const int32 Remaining = State.Inventory.FindRef(ID) - Amount;
    int32 Equipped = 0;
    for (const auto& Pair : State.Equipment) if (Pair.Value == ID) ++Equipped;
    if (Remaining < Equipped) { LastError = TEXT("Insufficient unequipped items / money."); return false; }
    if (Remaining == 0) State.Inventory.Remove(ID); else State.Inventory.Add(ID, Remaining);
    Changed(TEXT("inventory"), ID); return true;
}
int32 UGameDatabaseSubsystem::GetMoney() const { return State.Inventory.FindRef(MoneyItemID); }
bool UGameDatabaseSubsystem::AddMoney(int32 Amount) { return AddItem(MoneyItemID, Amount); }
bool UGameDatabaseSubsystem::TrySpendMoney(int32 Amount) { return TryRemoveItem(MoneyItemID, Amount); }
bool UGameDatabaseSubsystem::TryGetEquippedItem(FName Slot, FName& ID) const
{
    ID = State.Equipment.FindRef(Slot); return Slots.Contains(Slot);
}
bool UGameDatabaseSubsystem::EquipItem(FName Slot, FName ID)
{
    if (!CanWrite()) return false;
    FGameDatabaseSnapshot Candidate = State;
    Candidate.Equipment.Add(Slot, ID);
    if (!ValidateSnapshot(Candidate, LastError)) return false;
    if (State.Equipment.FindRef(Slot) != ID)
    { State.Equipment.Add(Slot, ID); Changed(TEXT("equipment"), Slot); }
    return true;
}
bool UGameDatabaseSubsystem::UnequipItem(FName Slot)
{
    if (!CanWrite()) return false;
    if (!Slots.Contains(Slot)) { LastError = TEXT("Unknown equipment slot."); return false; }
    if (State.Equipment.Remove(Slot)) Changed(TEXT("equipment"), Slot);
    return true;
}
bool UGameDatabaseSubsystem::SetCustomGameData(const FInstancedStruct& Data)
{
    if (!CanWrite()) return false;
    FGameDatabaseSnapshot Candidate = State;
    Candidate.CustomGameData = Data;
    if (!ValidateSnapshot(Candidate, LastError)) return false;
    if (State.CustomGameData != Data) { State.CustomGameData = Data; Changed(TEXT("progress"), NAME_None); }
    return true;
}

