#include "SharedYarnVariableStorage.h"
#include "GameVariablesSubsystem.h"
#include "GameSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "YarnSmartVariables.h"

void USharedYarnVariableStorage::Initialize(UGameVariablesSubsystem* InVariables) { Variables = InVariables; }
void USharedYarnVariableStorage::ReportWrite(bool bSuccess, const FString& Name) const
{
    if (!bSuccess) UE_LOG(LogTemp, Error, TEXT("Shared Yarn storage rejected %s: %s"), *Name,
        Variables ? *Variables->LastError : TEXT("Storage is not initialized."));
}
void USharedYarnVariableStorage::SetString_Implementation(const FString& Name, const FString& Value)
{
    ReportWrite(Variables && bProjectReady && Variables->SetString(Name, Value), Name);
}
void USharedYarnVariableStorage::SetNumber_Implementation(const FString& Name, float Value)
{
    ReportWrite(Variables && bProjectReady && Variables->SetNumber(Name, Value), Name);
}
void USharedYarnVariableStorage::SetBool_Implementation(const FString& Name, bool Value)
{
    ReportWrite(Variables && bProjectReady && Variables->SetBool(Name, Value), Name);
}
void USharedYarnVariableStorage::SetValue_Implementation(const FString& Name, const FYarnValue& Value)
{
    switch (Value.Type)
    {
    case EYarnValueType::Number: SetNumber_Implementation(Name, Value.GetNumberValue()); break;
    case EYarnValueType::String: SetString_Implementation(Name, Value.GetStringValue()); break;
    case EYarnValueType::Bool: SetBool_Implementation(Name, Value.GetBoolValue()); break;
    default: UE_LOG(LogTemp, Error, TEXT("Shared Yarn storage does not accept None: %s"), *Name); break;
    }
}
bool USharedYarnVariableStorage::TryGetValue_Implementation(const FString& Name, FYarnValue& OutValue)
{
    OutValue = FYarnValue();
    if (!Variables || !bProjectReady) return false;
    float Number;
    bool Bool;
    FString String;
    if (Variables->TryGetNumber(Name, Number)) { OutValue = FYarnValue(Number); return true; }
    if (Variables->TryGetBool(Name, Bool)) { OutValue = FYarnValue(Bool); return true; }
    if (Variables->TryGetString(Name, String)) { OutValue = FYarnValue(String); return true; }
    if (IYarnSmartVariableEvaluator* Evaluator = GetSmartVariableEvaluator())
        return Evaluator->TryGetSmartVariable(Name, OutValue);
    return false;
}
bool USharedYarnVariableStorage::Contains_Implementation(const FString& Name)
{
    FYarnValue Value;
    return TryGetValue_Implementation(Name, Value);
}
void USharedYarnVariableStorage::Clear_Implementation()
{
    if (Variables)
    {
        UGameSaveSubsystem* Saves = Variables->GetGameInstance()->GetSubsystem<UGameSaveSubsystem>();
        if (!Saves->StartNewGame()) UE_LOG(LogTemp, Warning, TEXT("Shared Yarn Clear rejected: %s"), *Saves->LastError);
    }
}
void USharedYarnVariableStorage::SetYarnProject(UYarnProject* Project)
{
    YarnProject = Project;
    bProjectReady = Variables && Project && Variables->RegisterYarnProject(Project);
    if (!bProjectReady) UE_LOG(LogTemp, Error, TEXT("Shared Yarn project defaults could not be registered: %s"),
        Variables ? *Variables->LastError : TEXT("No GameVariablesSubsystem."));
}
IYarnSmartVariableEvaluator* USharedYarnVariableStorage::GetSmartVariableEvaluator() const
{
    return EvaluatorOwner.IsValid() ? SmartEvaluator : nullptr;
}
void USharedYarnVariableStorage::SetSmartVariableEvaluator(IYarnSmartVariableEvaluator* Evaluator, UObject* Owner)
{
    // Never retain an unchecked native pointer after its runner leaves the world.
    SmartEvaluator = Evaluator;
    EvaluatorOwner = Owner;
}
