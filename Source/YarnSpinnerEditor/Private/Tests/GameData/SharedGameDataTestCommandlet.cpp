#include "Tests/GameData/SharedGameDataTestCommandlet.h"

#if WITH_EDITOR
#include "GameVariablesSubsystem.h"
#include "GameSaveSubsystem.h"
#include "GameProgressSave.h"
#include "GameDataBlueprintLibrary.h"
#include "SharedYarnDialogueRunner.h"
#include "SharedYarnVariableStorage.h"
#include "YarnSmartVariables.h"
#include "YarnSpinnerModule.h"
#include "SharedGameDataSettings.h"
#include "GameDatabaseSubsystem.h"
#include "Tests/GameData/DatabaseTestTypes.h"
#include "YarnProjectFactory.h"
#include "YarnLocalization.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/StructureEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/UnrealType.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/AutomationCommon.h"
#include "Kismet/GameplayStatics.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

namespace SharedDataTests
{
    // The reusable world tests need no project assets; the optional Mira fixture is supplied by the host.
    FString FixtureProjectPath;
    struct FReport
    {
        TArray<TSharedPtr<FJsonValue>> Checks;
        int32 Failures = 0;
        void Check(bool bPassed, const FString& Name)
        {
            auto Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), Name);
            Item->SetBoolField(TEXT("passed"), bPassed);
            Checks.Add(MakeShared<FJsonValueObject>(Item));
            if (!bPassed) { ++Failures; UE_LOG(LogTemp, Error, TEXT("SHARED_DATA_TEST FAILED: %s"), *Name); }
        }
    };

    struct FGameInstanceTestWorld : FTestWorldWrapper
    {
        virtual bool CreateTestWorld(EWorldType::Type Type) override
        {
            // Give the GI a real WorldContext, as in the game, so GI subsystem GetWorld works.
            UGameInstance* GI = NewObject<UGameInstance>(GEngine);
            GI->InitializeStandalone();
            TestWorld = GI->GetWorld();
            TestWorld->SetShouldTick(false);
            TestWorld->AddToRoot();
            return TestWorld != nullptr;
        }
    };

    struct FFixture
    {
        FGameInstanceTestWorld World;
        UGameVariablesSubsystem* Variables = nullptr;
        UGameSaveSubsystem* Saves = nullptr;
        USharedGameDataTestObserver* Observer = nullptr;
        UYarnProject* Project = nullptr;
        bool Initialize()
        {
            if (!World.CreateTestWorld(EWorldType::Game)) return false;
            UWorld* W = World.GetTestWorld();
            W->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
            if (!World.BeginPlayInTestWorld()) return false;
            Variables = W->GetGameInstance()->GetSubsystem<UGameVariablesSubsystem>();
            Saves = W->GetGameInstance()->GetSubsystem<UGameSaveSubsystem>();
            if (!FixtureProjectPath.IsEmpty()) Project = LoadObject<UYarnProject>(nullptr, *FixtureProjectPath);
            else
            {
                FGameVariableSnapshot Defaults;
                Defaults.Numbers.Add(TEXT("$gold"), 100);
                if (!Variables->RegisterDefaults(Defaults)) return false;
            }
            Observer = NewObject<USharedGameDataTestObserver>(W->GetGameInstance());
            Observer->AddToRoot();
            Variables->OnVariableChanged.AddDynamic(Observer, &USharedGameDataTestObserver::Changed);
            Variables->OnStateReplaced.AddDynamic(Observer, &USharedGameDataTestObserver::Replaced);
            Saves->OnSaveCompleted.AddDynamic(Observer, &USharedGameDataTestObserver::Saved);
            Saves->OnLoadCompleted.AddDynamic(Observer, &USharedGameDataTestObserver::Loaded);
            const bool bReady = Variables->bDefaultsReady && (FixtureProjectPath.IsEmpty() || Project);
            if (!bReady) UE_LOG(LogTemp, Error, TEXT("Fixture initialization: variables=%s; database=%s; project=%s"),
                *Variables->LastError, *W->GetGameInstance()->GetSubsystem<UGameDatabaseSubsystem>()->LastError, *FixtureProjectPath);
            return bReady;
        }
        ~FFixture() { if (Observer) Observer->RemoveFromRoot(); }
        void Tick()
        {
            FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
            // UE's platform SaveGame callbacks are dispatched on the core ticker.
            FTSTicker::GetCoreTicker().Tick(0.02f);
            World.TickTestWorld(0.02f);
        }
        bool WaitForIO()
        {
            const double Deadline = FPlatformTime::Seconds() + 20.0;
            while (Saves->bBusy && FPlatformTime::Seconds() < Deadline)
            { Tick(); FPlatformProcess::Sleep(0.002f); }
            return !Saves->bBusy;
        }
        UYarnDialogueRunner* MakeRunner(USharedGameDataTestPresenter*& Presenter, bool bUseSubclass = true)
        {
            AActor* Actor = World.GetTestWorld()->SpawnActor<AActor>();
            Presenter = NewObject<USharedGameDataTestPresenter>(Actor);
            Presenter->RegisterComponent();
            UYarnDialogueRunner* Runner = bUseSubclass ? NewObject<USharedYarnDialogueRunner>(Actor) : NewObject<UYarnDialogueRunner>(Actor);
            Runner->YarnProject = Project;
            Runner->DialoguePresenters.Add(Presenter);
            Runner->RegisterComponent();
            if (!bUseSubclass)
            {
                FString Error;
                if (!UGameDataBlueprintLibrary::ConnectRunnerToSharedVariables(Runner, Error)) return nullptr;
            }
            // This fixture locates choices by authored source text, independent of the host UI language.
            if (auto* Provider = Cast<UYarnBuiltinLineProvider>(Runner->LineProvider); Provider && Project)
            { Provider->SetLocaleCode(Project->BaseLanguage); }
            return Runner;
        }
        bool Play(UYarnDialogueRunner* Runner, USharedGameDataTestPresenter* Presenter, const TArray<FString>& Choices, const FString& Start = TEXT("Start"))
        {
            int32 ChoiceIndex = 0;
            Runner->StartDialogue(Start);
            for (int32 Step = 0; Step < 2000 && Runner->IsDialogueRunning(); ++Step)
            {
                if (Presenter->bLinePending)
                {
                    Presenter->bLinePending = false;
                    Presenter->OnLinePresentationComplete();
                }
                else if (Presenter->bOptionsPending)
                {
                    if (!Choices.IsValidIndex(ChoiceIndex)) return false;
                    int32 ID = INDEX_NONE;
                    for (const FYarnOption& Option : Presenter->CurrentOptions.Options)
                        if (Option.bIsAvailable && Option.Line.Text.ToString().Contains(Choices[ChoiceIndex])) { ID = Option.OptionID; break; }
                    if (ID == INDEX_NONE)
                    {
                        UE_LOG(LogTemp, Error, TEXT("Missing available choice: %s"), *Choices[ChoiceIndex]);
                        return false;
                    }
                    ++ChoiceIndex;
                    Presenter->bOptionsPending = false;
                    Presenter->OnOptionSelected(ID);
                }
                Tick();
            }
            return !Runner->IsDialogueRunning() && ChoiceIndex == Choices.Num() && Presenter->LineCount > 0;
        }
        float Number(const TCHAR* Name) { float Value = -999; Variables->TryGetNumber(Name, Value); return Value; }

    };

    class FTestEvaluator : public IYarnSmartVariableEvaluator
    {
    public:
        virtual bool TryGetSmartVariableAsBool(const FString&, bool&) override { return false; }
        virtual bool TryGetSmartVariableAsFloat(const FString&, float&) override { return false; }
        virtual bool TryGetSmartVariableAsString(const FString&, FString&) override { return false; }
        virtual bool TryGetSmartVariable(const FString& Name, FYarnValue& Value) override
        { if (Name != TEXT("$computed")) return false; Value = FYarnValue(42.f); return true; }
    };

    #include "Tests/GameData/DatabaseChecks.inl"
    #include "Tests/GameData/SaveMigrationChecks.inl"

}
#endif

USharedGameDataTestCommandlet::USharedGameDataTestCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 USharedGameDataTestCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    using namespace SharedDataTests;
    FReport Report;
    TGuardValue<int32> VersionGuard(GetMutableDefault<USharedGameDataSettings>()->SaveDataVersion, 1);
    // Feature fixtures own their schemas; the user's editable database is checked separately.
    TGuardValue<TSoftObjectPtr<UGameDatabaseDefinition>> DefinitionGuard(
        GetMutableDefault<USharedGameDataSettings>()->DatabaseDefinition, TSoftObjectPtr<UGameDatabaseDefinition>());
    FParse::Value(*Params, TEXT("YarnProject="), FixtureProjectPath);
    if (!FixtureProjectPath.IsEmpty())
    {
    const FString Slot = TEXT("SharedState_Test_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    {
        FFixture F;
        if (!F.Initialize()) return 2;
        Report.Check(F.Number(TEXT("$gold")) == 100, TEXT("Declarations available before creating any runner"));
        const FGameVariableSnapshot Initial = F.Variables->GetSnapshot();
        Report.Check(Initial.Numbers.Contains(TEXT("$affinity")) && Initial.Numbers[TEXT("$affinity")] == 0
            && Initial.Bools.Contains(TEXT("$heard_story")) && !Initial.Bools[TEXT("$heard_story")], TEXT("Untouched zero/false defaults are included"));
        bool Bool = true;
        Report.Check(!F.Variables->TryGetBool(TEXT("$missing"), Bool) && !Bool, TEXT("Missing and false are distinguishable"));
        const int32 Changes = F.Observer->Changes;
        F.Variables->SetNumber(TEXT("$gold"), 100);
        F.Variables->SetNumber(TEXT("$gold"), 150);
        Report.Check(F.Observer->Changes == Changes + 1, TEXT("Only actual changes emit per-variable notification"));
        Report.Check(!F.Variables->SetBool(TEXT("$gold"), true) && F.Number(TEXT("$gold")) == 150, TEXT("Wrong-type writes preserve state"));
        FGameVariableSnapshot Bad = Initial;
        Bad.Bools.Add(TEXT("$gold"), false);
        Report.Check(!F.Variables->RestoreSnapshot(Bad) && F.Number(TEXT("$gold")) == 150, TEXT("Invalid snapshot rejects atomically"));
        FGameVariableSnapshot Conflict;
        Conflict.Numbers.Add(TEXT("$gold"), 200);
        Conflict.Bools.Add(TEXT("$must_not_leak"), true);
        Report.Check(!F.Variables->RegisterDefaults(Conflict) && !F.Variables->Contains(TEXT("$must_not_leak")), TEXT("Conflicting declarations reject atomically"));
        FGameVariableSnapshot OldSave;
        OldSave.Numbers.Add(TEXT("$gold"), 7);
        Report.Check(F.Variables->RestoreSnapshot(OldSave) && F.Variables->Contains(TEXT("$heard_story")), TEXT("Partial variable snapshots fill authored defaults"));
        Report.Check(F.Saves->StartNewGame(), TEXT("New game restores defaults"));

        USharedGameDataTestPresenter* Presenter = nullptr;
        UYarnDialogueRunner* Runner = F.MakeRunner(Presenter);
        Report.Check(Runner && Cast<USharedYarnVariableStorage>(Runner->VariableStorage.GetObject()), TEXT("Runner subclass installs native shared storage"));
        Report.Check(F.Play(Runner, Presenter, {TEXT("告辭")}), TEXT("Mira flow 1: introduction and goodbye"));
        Report.Check(F.Number(TEXT("$talk_count")) == 1 && F.Number(TEXT("$gold")) == 100, TEXT("VM writes are visible through subsystem"));
        F.Saves->StartNewGame();
        Report.Check(F.Play(Runner, Presenter, {TEXT("買一瓶"), TEXT("成交"), TEXT("告辭")}), TEXT("Mira flow 2: purchase"));
        Report.Check(F.Number(TEXT("$gold")) == 70 && F.Number(TEXT("$potion_count")) == 1 && F.Number(TEXT("$total_spent")) == 30, TEXT("Purchase updates all shared values"));

        Runner->StartDialogue(TEXT("Start"));
        FString Error;
        Report.Check(!F.Saves->StartNewGame() && !F.Saves->LoadSlot(Slot) && !UGameDataBlueprintLibrary::ConnectRunnerToSharedVariables(Runner, Error), TEXT("Active dialogue blocks reset/load/storage replacement"));
        Runner->StopDialogue();
        F.Saves->StartNewGame();
        F.Variables->SetNumber(TEXT("$gold"), 500);
        F.Variables->SetNumber(TEXT("$potion_count"), 2);
        Report.Check(F.Play(Runner, Presenter, {TEXT("問問有沒有工作"), TEXT("沒問題"), TEXT("把藥水交給"), TEXT("告辭")}), TEXT("Mira flow 3: gameplay grants items then quest turn-in"));
        FString Quest;
        F.Variables->TryGetString(TEXT("$quest_state"), Quest);
        Report.Check(F.Number(TEXT("$gold")) == 580 && F.Number(TEXT("$potion_count")) == 0 && Quest == TEXT("done"), TEXT("VM sees external gameplay writes and grants reward"));

        // Destroy level objects; a new runner must use the existing session values.
        Runner->GetOwner()->Destroy();
        F.Tick();
        Runner = F.MakeRunner(Presenter, false);
        Report.Check(F.Number(TEXT("$gold")) == 580 && Runner, TEXT("Replacing actor and attaching an existing runner preserves data"));
        Report.Check(F.Play(Runner, Presenter, {TEXT("聊聊天"), TEXT("稱讚"), TEXT("告辭")}), TEXT("Mira flow 4: another conversation after actor recreation"));
        Report.Check(F.Number(TEXT("$talk_count")) == 2, TEXT("Conversation count persists across runners"));
        UObject* Storage = Runner->VariableStorage.GetObject();
        IYarnVariableStorage::Execute_SetBool(Storage, TEXT("$save_false"), false);
        IYarnVariableStorage::Execute_SetNumber(Storage, TEXT("$save_zero"), 0);
        IYarnVariableStorage::Execute_SetString(Storage, TEXT("$save_empty"), TEXT(""));
        IYarnVariableStorage::Execute_SetNumber(Storage, TEXT("$Yarn.Internal.Visiting.Test"), 3);
        FYarnValue Value;
        Report.Check(IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$save_false"), Value) && !Value.GetBoolValue(), TEXT("Storage Execute interface round-trips false"));

        FTestEvaluator Evaluator;
        UObject* EvaluatorOwner = NewObject<USharedGameDataTestObserver>();
        auto* Adapter = Cast<USharedYarnVariableStorage>(Storage);
        Adapter->SetSmartVariableEvaluator(&Evaluator, EvaluatorOwner);
        Report.Check(IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$computed"), Value) && Value.GetNumberValue() == 42
            && !F.Variables->Contains(TEXT("$computed")), TEXT("Smart variables are evaluated without persisting computed values"));
        EvaluatorOwner->MarkAsGarbage();
        Report.Check(Adapter->GetSmartVariableEvaluator() == nullptr, TEXT("Expired evaluator owner prevents dangling calls"));
        Adapter->SetSmartVariableEvaluator(Runner, Runner);

        Report.Check(F.Saves->SaveSlot(Slot), TEXT("Async save accepted"));
        Report.Check(!F.Saves->SaveSlot(Slot) && !F.Saves->StartNewGame(), TEXT("Overlapping operations rejected"));
        // Verify that a save captures request-time data, not later gameplay changes.
        F.Variables->SetNumber(TEXT("$gold"), 999);
        Report.Check(F.WaitForIO() && F.Observer->bLastSuccess && F.Observer->SaveCompletions == 1, TEXT("Async save completes exactly once"));
    }
    {
        FFixture F;
        if (!F.Initialize()) return 2;
        Report.Check(F.Number(TEXT("$gold")) == 100, TEXT("New GameInstance starts with its own independent state"));
        Report.Check(F.Saves->LoadSlot(Slot) && F.WaitForIO() && F.Observer->bLastSuccess, TEXT("Async disk load into a fresh GameInstance succeeds"));
        const auto Loaded = F.Variables->GetSnapshot();
        Report.Check(F.Number(TEXT("$gold")) == 580 && Loaded.Numbers.Contains(TEXT("$save_zero"))
            && Loaded.Numbers[TEXT("$save_zero")] == 0 && Loaded.Bools.Contains(TEXT("$save_false"))
            && !Loaded.Bools[TEXT("$save_false")] && Loaded.Strings.Contains(TEXT("$save_empty"))
            && Loaded.Strings[TEXT("$save_empty")].IsEmpty() && F.Number(TEXT("$Yarn.Internal.Visiting.Test")) == 3,
            TEXT("Disk round-trip preserves snapshot, zero, false, empty string and Yarn internal state"));
        USharedGameDataTestPresenter* Presenter = nullptr;
        UYarnDialogueRunner* Runner = F.MakeRunner(Presenter);
        Report.Check(F.Number(TEXT("$gold")) == 580, TEXT("Runner created after loading does not reset loaded values"));
        Report.Check(F.Play(Runner, Presenter, {TEXT("告辭")}, TEXT("Hub")), TEXT("Loaded state can resume at an explicit safe Yarn node"));
        Report.Check(F.Saves->LoadSlot(Slot + TEXT("_missing")) && F.WaitForIO() && !F.Observer->bLastSuccess && F.Number(TEXT("$gold")) == 580, TEXT("Missing-slot load preserves live state"));
        Report.Check(F.Saves->LoadSlot(Slot), TEXT("Race test load accepted"));
        F.Variables->SetNumber(TEXT("$gold"), 581);
        Report.Check(F.WaitForIO() && !F.Observer->bLastSuccess && F.Number(TEXT("$gold")) == 581, TEXT("Stale async load cannot overwrite newer gameplay"));
        UGameProgressSave* FutureSave = NewObject<UGameProgressSave>();
        FutureSave->Version = 999;
        UGameplayStatics::SaveGameToSlot(FutureSave, Slot, 0);
        Report.Check(F.Saves->LoadSlot(Slot) && F.WaitForIO() && !F.Observer->bLastSuccess && F.Number(TEXT("$gold")) == 581, TEXT("Unsupported save version preserves live state"));
        Report.Check(F.Saves->StartNewGame() && F.Number(TEXT("$gold")) == 100 && !F.Variables->Contains(TEXT("$save_empty")), TEXT("New game removes session-only and tracking keys"));
        Report.Check(!F.Saves->SaveSlot(TEXT("../invalid")), TEXT("Invalid slot paths are rejected"));
        Report.Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Test cleans only its uniquely named save slot"));
    }

    }
    RunDatabaseChecks(Report);
    RunSaveMigrationChecks(Report);

    auto Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("passed"), Report.Failures == 0);
    Root->SetNumberField(TEXT("failures"), Report.Failures);
    Root->SetStringField(TEXT("timestamp_utc"), FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
    Root->SetArrayField(TEXT("checks"), Report.Checks);
    FString Json;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
    const FString Path = FPaths::ProjectSavedDir() / TEXT("Tests/SharedGameData.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    if (!FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return 3;
    UE_LOG(LogTemp, Display, TEXT("SHARED_GAME_DATA_TESTS: %d checks, %d failures"), Report.Checks.Num(), Report.Failures);
    return Report.Failures ? 1 : 0;
#else
    return 1;
#endif
}
