#include "GameVariablesSubsystem.h"
#include "GameDatabaseSubsystem.h"
#include "Engine/GameInstance.h"
#include "SharedGameDataSettings.h"
#include "YarnDialogueRunner.h"
#include "YarnProgram.h"

namespace GameData
{
    static uint8 TypeOf(const FGameVariableSnapshot& S, const FString& Name)
    {
        return S.Numbers.Contains(Name) ? 1 : S.Strings.Contains(Name) ? 2 : S.Bools.Contains(Name) ? 3 : 0;
    }

    static bool ValidName(const FString& Name)
    {
        if (Name.Len() < 2 || !Name.StartsWith(TEXT("$"))) return false;
        for (TCHAR Char : Name) if (FChar::IsWhitespace(Char) || FChar::IsControl(Char)) return false;
        return true;
    }

    template<typename T> static void FillMissing(TMap<FString, T>& Values, const TMap<FString, T>& Defaults)
    {
        for (const auto& Pair : Defaults) if (!Values.Contains(Pair.Key)) Values.Add(Pair);
    }

    static void FillMissing(FGameVariableSnapshot& Values, const FGameVariableSnapshot& Defaults)
    {
        FillMissing(Values.Numbers, Defaults.Numbers);
        FillMissing(Values.Strings, Defaults.Strings);
        FillMissing(Values.Bools, Defaults.Bools);
    }

    template<typename T> static bool Compatible(const TMap<FString, T>& Incoming, uint8 Type,
        const FGameVariableSnapshot& Existing, FString& Error)
    {
        for (const auto& Pair : Incoming)
        {
            const uint8 OldType = TypeOf(Existing, Pair.Key);
            if (OldType != 0 && OldType != Type)
            {
                Error = FString::Printf(TEXT("Type conflict for %s."), *Pair.Key);
                return false;
            }
        }
        return true;
    }

    static bool Compatible(const FGameVariableSnapshot& Incoming, const FGameVariableSnapshot& Existing, FString& Error)
    {
        return Compatible(Incoming.Numbers, 1, Existing, Error) && Compatible(Incoming.Strings, 2, Existing, Error)
            && Compatible(Incoming.Bools, 3, Existing, Error);
    }

    template<typename T> static bool SameDefaults(const TMap<FString, T>& Incoming,
        const TMap<FString, T>& Existing, FString& Error)
    {
        for (const auto& Pair : Incoming)
        {
            if (const T* Old = Existing.Find(Pair.Key); Old && *Old != Pair.Value)
            {
                Error = FString::Printf(TEXT("Conflicting declarations for %s. All projects must agree on its default."), *Pair.Key);
                return false;
            }
        }
        return true;
    }
}

void UGameVariablesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGameDatabaseSubsystem>();
    Database = GetGameInstance()->GetSubsystem<UGameDatabaseSubsystem>();
    if (!Database || !Database->bReady) { LastError = TEXT("Database initialization failed."); return; }
    const USharedGameDataSettings* Settings = GetDefault<USharedGameDataSettings>();
    bDefaultsReady = RegisterDefaults(Settings->GameplayDefaults);
    for (const auto& SoftProject : Settings->DefaultYarnProjects)
    {
        if (!RegisterYarnProject(SoftProject.LoadSynchronous()))
        {
            bDefaultsReady = false;
            UE_LOG(LogTemp, Error, TEXT("Shared Game Data defaults failed (%s): %s"), *SoftProject.ToString(), *LastError);
        }
    }
}

bool UGameVariablesSubsystem::ValidateSnapshot(const FGameVariableSnapshot& S, FString& Error)
{
    Error.Reset();
    TSet<FString> Seen;
    auto Check = [&Seen, &Error](const FString& Name)
    {
        if (!GameData::ValidName(Name) || Seen.Contains(Name))
        {
            Error = FString::Printf(TEXT("Invalid or duplicate variable name: %s"), *Name);
            return false;
        }
        Seen.Add(Name);
        return true;
    };
    for (const auto& Pair : S.Numbers)
    {
        if (!Check(Pair.Key)) return false;
        if (!FMath::IsFinite(Pair.Value)) { Error = TEXT("A number is NaN or infinite."); return false; }
    }
    for (const auto& Pair : S.Strings) if (!Check(Pair.Key)) return false;
    for (const auto& Pair : S.Bools) if (!Check(Pair.Key)) return false;
    return true;
}

bool UGameVariablesSubsystem::CanSet(const FString& Name, uint8 Type)
{
    LastError.Reset();
    if (!Database || !Database->bReady) { LastError = TEXT("Database is unavailable."); return false; }
    if (!GameData::ValidName(Name)) { LastError = TEXT("Variable names must start with $ and contain no whitespace."); return false; }
    const uint8 OldType = GameData::TypeOf(Database->State.YarnVariables, Name);
    if (OldType && OldType != Type) { LastError = FString::Printf(TEXT("Cannot change type of %s."), *Name); return false; }
    return true;
}

bool UGameVariablesSubsystem::TryGetNumber(const FString& Name, float& Value) const
{
    const float* Found = Database->State.YarnVariables.Numbers.Find(Name);
    Value = Found ? *Found : 0.f;
    return Found != nullptr;
}
bool UGameVariablesSubsystem::TryGetBool(const FString& Name, bool& Value) const
{
    const bool* Found = Database->State.YarnVariables.Bools.Find(Name);
    Value = Found ? *Found : false;
    return Found != nullptr;
}
bool UGameVariablesSubsystem::TryGetString(const FString& Name, FString& Value) const
{
    const FString* Found = Database->State.YarnVariables.Strings.Find(Name);
    Value = Found ? *Found : FString();
    return Found != nullptr;
}

bool UGameVariablesSubsystem::SetNumber(const FString& Name, float Value)
{
    if (!CanSet(Name, 1)) return false;
    if (!FMath::IsFinite(Value)) { LastError = TEXT("A number must be finite."); return false; }
    if (const float* Old = Database->State.YarnVariables.Numbers.Find(Name); Old && *Old == Value) return true;
    Database->State.YarnVariables.Numbers.Add(Name, Value);
    ++Revision;
    ++Database->Revision;
    OnVariableChanged.Broadcast(Name);
    return true;
}
bool UGameVariablesSubsystem::SetString(const FString& Name, const FString& Value)
{
    if (!CanSet(Name, 2)) return false;
    if (const FString* Old = Database->State.YarnVariables.Strings.Find(Name); Old && *Old == Value) return true;
    Database->State.YarnVariables.Strings.Add(Name, Value);
    ++Revision;
    ++Database->Revision;
    OnVariableChanged.Broadcast(Name);
    return true;
}
bool UGameVariablesSubsystem::SetBool(const FString& Name, bool Value)
{
    if (!CanSet(Name, 3)) return false;
    if (const bool* Old = Database->State.YarnVariables.Bools.Find(Name); Old && *Old == Value) return true;
    Database->State.YarnVariables.Bools.Add(Name, Value);
    ++Revision;
    ++Database->Revision;
    OnVariableChanged.Broadcast(Name);
    return true;
}
bool UGameVariablesSubsystem::AddNumber(const FString& Name, float Delta, float& NewValue)
{
    if (!TryGetNumber(Name, NewValue)) { LastError = FString::Printf(TEXT("Number not found: %s"), *Name); return false; }
    const float Result = NewValue + Delta;
    if (!SetNumber(Name, Result)) return false;
    NewValue = Result;
    return true;
}
bool UGameVariablesSubsystem::Contains(const FString& Name) const { return GameData::TypeOf(Database->State.YarnVariables, Name) != 0; }

bool UGameVariablesSubsystem::RegisterDefaults(const FGameVariableSnapshot& NewDefaults)
{
    if (!ValidateSnapshot(NewDefaults, LastError) || !GameData::Compatible(NewDefaults, Database->State.YarnVariables, LastError)
        || !GameData::Compatible(NewDefaults, Defaults, LastError)
        || !GameData::SameDefaults(NewDefaults.Numbers, Defaults.Numbers, LastError)
        || !GameData::SameDefaults(NewDefaults.Strings, Defaults.Strings, LastError)
        || !GameData::SameDefaults(NewDefaults.Bools, Defaults.Bools, LastError)) return false;

    const int32 Before = Defaults.Numbers.Num() + Defaults.Strings.Num() + Defaults.Bools.Num();
    GameData::FillMissing(Defaults, NewDefaults);
    GameData::FillMissing(Database->State.YarnVariables, Defaults);
    if (Before != Defaults.Numbers.Num() + Defaults.Strings.Num() + Defaults.Bools.Num())
    {
        ++Revision;
        ++Database->Revision;
        OnStateReplaced.Broadcast();
    }
    return true;
}

bool UGameVariablesSubsystem::RegisterYarnProject(UYarnProject* Project)
{
    if (!Project) { LastError = TEXT("A configured Yarn project could not be loaded."); return false; }
    FGameVariableSnapshot Initial;
    for (const auto& Pair : Project->Program.InitialValues)
    {
        switch (Pair.Value.Type)
        {
        case EYarnValueType::Number: Initial.Numbers.Add(Pair.Key, Pair.Value.GetNumberValue()); break;
        case EYarnValueType::String: Initial.Strings.Add(Pair.Key, Pair.Value.GetStringValue()); break;
        case EYarnValueType::Bool: Initial.Bools.Add(Pair.Key, Pair.Value.GetBoolValue()); break;
        default: LastError = FString::Printf(TEXT("Unsupported initial value: %s"), *Pair.Key); return false;
        }
    }
    return RegisterDefaults(Initial);
}

bool UGameVariablesSubsystem::ValidateRestoreSnapshot(const FGameVariableSnapshot& Snapshot, FString& Error) const
{
    return ValidateSnapshot(Snapshot, Error) && GameData::Compatible(Snapshot, Defaults, Error);
}

bool UGameVariablesSubsystem::RestoreSnapshot(const FGameVariableSnapshot& Snapshot, bool bNotify)
{
    if (!ValidateRestoreSnapshot(Snapshot, LastError)) return false;
    FGameVariableSnapshot Candidate = Snapshot;
    GameData::FillMissing(Candidate, Defaults);
    Database->State.YarnVariables = MoveTemp(Candidate);
    ++Revision;
    ++Database->Revision;
    if (bNotify) OnStateReplaced.Broadcast();
    return true;
}

void UGameVariablesSubsystem::ResetToDefaults(bool bNotify)
{
    Database->State.YarnVariables = Defaults;
    LastError.Reset();
    ++Revision;
    ++Database->Revision;
    if (bNotify) OnStateReplaced.Broadcast();
}

void UGameVariablesSubsystem::TrackRunner(UYarnDialogueRunner* Runner)
{
    Runners.RemoveAll([](const auto& Ref) { return !Ref.IsValid(); });
    Runners.AddUnique(Runner);
}
bool UGameVariablesSubsystem::HasActiveDialogue() const
{
    for (const auto& Ref : Runners) if (Ref.IsValid() && Ref->IsDialogueRunning()) return true;
    return false;
}

FString UGameVariablesSubsystem::GetFormattedVariables() const
{
    TArray<FString> Lines;
    for (const auto& Pair : Database->State.YarnVariables.Numbers) Lines.Add(FString::Printf(TEXT("%s = %s"), *Pair.Key, *FString::SanitizeFloat(Pair.Value)));
    for (const auto& Pair : Database->State.YarnVariables.Strings) Lines.Add(FString::Printf(TEXT("%s = \"%s\""), *Pair.Key, *Pair.Value));
    for (const auto& Pair : Database->State.YarnVariables.Bools) Lines.Add(FString::Printf(TEXT("%s = %s"), *Pair.Key, Pair.Value ? TEXT("true") : TEXT("false")));
    Lines.Sort();
    return FString::Join(Lines, TEXT("\n"));
}

FGameVariableSnapshot UGameVariablesSubsystem::GetSnapshot() const { return Database ? Database->State.YarnVariables : FGameVariableSnapshot(); }
