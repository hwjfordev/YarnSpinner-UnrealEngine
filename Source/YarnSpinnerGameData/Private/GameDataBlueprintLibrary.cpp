#include "GameDataBlueprintLibrary.h"
#include "GameVariablesSubsystem.h"
#include "GameSaveSubsystem.h"
#include "GameDatabaseSubsystem.h"
#include "YarnDatabaseBridge.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SharedYarnVariableStorage.h"
#include "YarnDialogueRunner.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

UGameVariablesSubsystem* UGameDataBlueprintLibrary::GetGameVariables(const UObject* WorldContextObject)
{
    UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject);
    return GI ? GI->GetSubsystem<UGameVariablesSubsystem>() : nullptr;
}
UGameSaveSubsystem* UGameDataBlueprintLibrary::GetGameSaves(const UObject* WorldContextObject)
{
    UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject);
    return GI ? GI->GetSubsystem<UGameSaveSubsystem>() : nullptr;
}
UGameDatabaseSubsystem* UGameDataBlueprintLibrary::GetGameDatabase(const UObject* WorldContextObject)
{
    UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject);
    return GI ? GI->GetSubsystem<UGameDatabaseSubsystem>() : nullptr;
}
bool UGameDataBlueprintLibrary::ConnectRunnerToSharedVariables(UYarnDialogueRunner* Runner, FString& Error)
{
    Error.Reset();
    if (!IsValid(Runner)) { Error = TEXT("Runner is invalid."); return false; }
    if (Runner->IsDialogueRunning()) { Error = TEXT("Cannot replace storage while dialogue is running."); return false; }
    UGameVariablesSubsystem* Variables = GetGameVariables(Runner);
    if (!Variables || !Variables->bDefaultsReady) { Error = TEXT("GameVariablesSubsystem defaults are unavailable."); return false; }
    if (!Runner->YarnProject || !Variables->RegisterYarnProject(Runner->YarnProject))
    { Error = Runner->YarnProject ? Variables->LastError : TEXT("Assign a Yarn Project to the runner first."); return false; }

    USharedYarnVariableStorage* Storage = Cast<USharedYarnVariableStorage>(Runner->VariableStorage.GetObject());
    if (!Storage || Storage->GetOuter() != Runner) Storage = NewObject<USharedYarnVariableStorage>(Runner);
    Storage->Initialize(Variables);
    Storage->SetYarnProject(Runner->YarnProject);
    Storage->SetSmartVariableEvaluator(Runner, Runner);
    Runner->VariableStorage.SetObject(Storage);
    Runner->VariableStorage.SetInterface(Storage);
    Variables->TrackRunner(Runner);
    if (Runner->HasBegunPlay()) Runner->SetYarnProject(Runner->YarnProject);
    if (!Storage->DatabaseBridge) Storage->DatabaseBridge = NewObject<UYarnDatabaseBridge>(Storage);
    Storage->DatabaseBridge->Initialize(Runner, GetGameDatabase(Runner));
    return true;
}
