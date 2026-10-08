// Compile a real Blueprint interface implementation: return LoadedDatabase and SavedVersion < CurrentVersion.
UBlueprint* MakeMigrationBlueprint()
{
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("MigrationProbe")), BPTYPE_Normal);
    if (!BP) return nullptr;
    BP->AddToRoot();
    const bool Added = FBlueprintEditorUtils::ImplementNewInterface(BP,
        FTopLevelAssetPath(UGameSaveMigrationHandler::StaticClass()));
    UEdGraph* Graph = nullptr;
    for (const auto& Interface : BP->ImplementedInterfaces)
        for (UEdGraph* G : Interface.Graphs) if (G->GetFName() == TEXT("MigrateSaveData")) Graph = G;
    if (!Added || !Graph) { BP->RemoveFromRoot(); return nullptr; }
    UK2Node_FunctionEntry* Entry = nullptr;
    UK2Node_FunctionResult* Result = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (auto* E = Cast<UK2Node_FunctionEntry>(Node)) Entry = E;
        if (auto* E = Cast<UK2Node_FunctionResult>(Node)) Result = E;
    }
    if (!Entry || !Result) { BP->RemoveFromRoot(); return nullptr; }
    auto* Less = NewObject<UK2Node_CallFunction>(Graph);
    Less->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Less_IntInt")));
    Graph->AddNode(Less); Less->CreateNewGuid(); Less->PostPlacedNewNode(); Less->AllocateDefaultPins();
    auto Link = [Graph](UEdGraphPin* A, UEdGraphPin* B)
    { return A && B && (A->LinkedTo.Contains(B) || Graph->GetSchema()->TryCreateConnection(A, B)); };
    const bool Linked = Link(Entry->FindPin(UEdGraphSchema_K2::PN_Then), Result->FindPin(UEdGraphSchema_K2::PN_Execute))
        && Link(Entry->FindPin(TEXT("LoadedDatabase")), Result->FindPin(TEXT("MigratedDatabase")))
        && Link(Entry->FindPin(TEXT("SavedVersion")), Less->FindPin(TEXT("A")))
        && Link(Entry->FindPin(TEXT("CurrentVersion")), Less->FindPin(TEXT("B")))
        && Link(Less->FindPin(UEdGraphSchema_K2::PN_ReturnValue), Result->FindPin(UEdGraphSchema_K2::PN_ReturnValue));
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (!Linked || BP->Status == BS_Error || !BP->GeneratedClass) { BP->RemoveFromRoot(); return nullptr; }
    return BP;
}

void RunSaveMigrationChecks(FReport& R)
{
    auto* Settings = GetMutableDefault<USharedGameDataSettings>();
    TGuardValue<int32> VersionGuard(Settings->SaveDataVersion, 3);
    auto* Definition = NewObject<UGameDatabaseDefinition>();
    Definition->AddToRoot();
    Definition->Items.Add(TEXT("money"), FGameItemDefinition());
    Definition->InitialState.Inventory.Add(TEXT("money"), 100);
    FGameDataRecord NewNPC; NewNPC.Numbers.Add(TEXT("affinity"), 0); NewNPC.Bools.Add(TEXT("new_field"), false);
    Definition->InitialState.Records.Add(TEXT("npc.mira"), NewNPC);
    FCheckpointTestProgress Initial; Initial.Chapter = TEXT("Day1");
    Definition->InitialState.CustomGameData.InitializeAs<FCheckpointTestProgress>(Initial);
    TGuardValue<TSoftObjectPtr<UGameDatabaseDefinition>> DefinitionGuard(Settings->DatabaseDefinition, Definition);
    const FString Slot = TEXT("MigrationTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FFixture F;
    if (!F.Initialize()) { R.Check(false, TEXT("Migration fixture initializes")); Definition->RemoveFromRoot(); return; }
    auto* DB = F.World.GetTestWorld()->GetGameInstance()->GetSubsystem<UGameDatabaseSubsystem>();
    auto Load = [&F, &Slot]() { return F.Saves->LoadSlot(Slot) && F.WaitForIO(); };
    R.Check(F.Saves->GetSaveDataVersion() == 3 && F.Saves->SaveSlot(Slot) && F.WaitForIO() && F.Observer->bLastSuccess,
        TEXT("SaveSlot needs no checkpoint and uses configured version"));
    auto* OnDisk = Cast<UGameProgressSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    R.Check(OnDisk && OnDisk->Version == 3, TEXT("Configured version is physically written into disk save"));
    if (!OnDisk) { Definition->RemoveFromRoot(); return; }
    OnDisk->AddToRoot();
    OnDisk->Version = 1;
    OnDisk->Database.Inventory[TEXT("money")] = 47;
    OnDisk->Database.Records[TEXT("npc.mira")].Bools.Remove(TEXT("new_field"));
    OnDisk->Database.CustomGameData.GetMutable<FCheckpointTestProgress>().Chapter = TEXT("Day3");
    R.Check(UGameplayStatics::SaveGameToSlot(OnDisk, Slot, 0), TEXT("Old-version fixture with missing new field writes to disk"));
    const int32 BeforeReplaced = F.Observer->Replacements;
    R.Check(Load() && !F.Observer->bLastSuccess && DB->GetMoney() == 100 && F.Observer->Replacements == BeforeReplaced,
        TEXT("Mismatched version without handler preserves live state and emits no replacement"));
    R.Check(!F.Saves->RegisterSaveMigrationHandler(nullptr) && !F.Saves->RegisterSaveMigrationHandler(NewObject<USharedGameDataTestObserver>()),
        TEXT("Registration rejects null and objects without the migration interface"));
    auto* Handler = NewObject<UGameSaveMigrationTestHandler>();
    Handler->Saves = F.Saves; Handler->Database = DB;
    Handler->Replacement = OnDisk->Database;
    Handler->Replacement.Records[TEXT("npc.mira")].Bools.Add(TEXT("new_field"), true);
    Handler->Replacement.CustomGameData.GetMutable<FCheckpointTestProgress>().Counters.Add(TEXT("unlocked"), 3);
    R.Check(F.Saves->RegisterSaveMigrationHandler(Handler), TEXT("One interface handler registers successfully"));
    Handler->bReject = true;
    R.Check(Load() && !F.Observer->bLastSuccess && F.Saves->LastError.Contains(TEXT("test handler rejects"))
        && DB->GetMoney() == 100 && F.Observer->Replacements == BeforeReplaced,
        TEXT("Handler rejection propagates its error without changing runtime state"));
    Handler->bReject = false; Handler->bInvalidResult = true;
    R.Check(Load() && !F.Observer->bLastSuccess && DB->GetMoney() == 100 && F.Observer->Replacements == BeforeReplaced,
        TEXT("Invalid migrated output fails validation without partial publication"));
    Handler->bInvalidResult = false;
    const int32 BeforeCompleted = F.Observer->LoadCompletions;
    R.Check(Load() && F.Observer->bLastSuccess && Handler->LastSaved == 1 && Handler->LastCurrent == 3
        && Handler->Received.Inventory.FindRef(TEXT("money")) == 47 && Handler->bNestedRejected,
        TEXT("Handler receives both versions and disk data; nested operations and handler changes are blocked"));
    R.Check(DB->GetMoney() == 47 && DB->GetCustomGameData().Get<FCheckpointTestProgress>().Chapter == TEXT("Day3")
        && DB->GetCustomGameData().Get<FCheckpointTestProgress>().Counters.FindRef(TEXT("unlocked")) == 3
        && F.Observer->LoadCompletions == BeforeCompleted + 1 && F.Observer->Replacements == BeforeReplaced + 1,
        TEXT("Migration repairs old schema before validation and publishes one complete database"));
    auto* StillOld = Cast<UGameProgressSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    R.Check(StillOld && StillOld->Version == 1 && !StillOld->Database.Records[TEXT("npc.mira")].Bools.Contains(TEXT("new_field")),
        TEXT("Successful load does not rewrite the old slot"));
    R.Check(F.Saves->SaveSlot(Slot) && F.WaitForIO() && F.Observer->bLastSuccess, TEXT("Normal save persists migrated runtime data"));
    auto* Upgraded = Cast<UGameProgressSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    const int32 Calls = Handler->Calls;
    R.Check(Upgraded && Upgraded->Version == 3 && Load() && F.Observer->bLastSuccess && Handler->Calls == Calls,
        TEXT("Same-version load bypasses migration after saving upgraded data"));
    OnDisk->Version = 9; UGameplayStatics::SaveGameToSlot(OnDisk, Slot, 0);
    R.Check(Load() && !F.Observer->bLastSuccess && Handler->LastSaved == 9 && Handler->LastCurrent == 3 && DB->GetMoney() == 47,
        TEXT("Newer versions also reach the handler and can be rejected"));
    OnDisk->Version = 1; UGameplayStatics::SaveGameToSlot(OnDisk, Slot, 0);
    const int32 BeforeRace = Handler->Calls;
    R.Check(F.Saves->LoadSlot(Slot), TEXT("Migration race request is accepted"));
    DB->AddMoney(1);
    R.Check(F.WaitForIO() && !F.Observer->bLastSuccess && Handler->Calls == BeforeRace && DB->GetMoney() == 48,
        TEXT("Concurrent gameplay prevents migration from running on a stale load"));
    Handler->bMutateRuntime = true;
    R.Check(Load() && !F.Observer->bLastSuccess && DB->GetMoney() == 49,
        TEXT("A handler that changes live data cannot overwrite those changes with its candidate"));
    Handler->bMutateRuntime = false;
    R.Check(F.Saves->UnregisterSaveMigrationHandler() && Load() && !F.Observer->bLastSuccess && DB->GetMoney() == 49,
        TEXT("Unregister removes the callback and future mismatches fail safely"));
    OnDisk->Version = 0; UGameplayStatics::SaveGameToSlot(OnDisk, Slot, 0);
    R.Check(Load() && !F.Observer->bLastSuccess, TEXT("Invalid zero save version is rejected"));
    {
        TGuardValue<int32> InvalidVersion(Settings->SaveDataVersion, 0);
        R.Check(!F.Saves->LoadSlot(Slot) && !F.Saves->SaveSlot(Slot), TEXT("Invalid configured version rejects I/O"));
    }
    R.Check(F.Saves->StartNewGame() && DB->GetMoney() == 100
        && DB->GetCustomGameData().Get<FCheckpointTestProgress>().Chapter == TEXT("Day1"),
        TEXT("New Game resets custom progress without any fixed checkpoint"));

    UBlueprint* BP = MakeMigrationBlueprint();
    R.Check(BP != nullptr, TEXT("A real Blueprint can implement the migration interface with data and version pins"));
    if (BP)
    {
        UObject* BPHandler = NewObject<UObject>(F.World.GetTestWorld()->GetGameInstance(), BP->GeneratedClass);
        OnDisk->Version = 2; OnDisk->Database = DB->GetSnapshot();
        OnDisk->Database.Inventory[TEXT("money")] = 33;
        UGameplayStatics::SaveGameToSlot(OnDisk, Slot, 0);
        R.Check(F.Saves->RegisterSaveMigrationHandler(BPHandler) && Load() && F.Observer->bLastSuccess && DB->GetMoney() == 33,
            TEXT("Blueprint migration function executes during LoadSlot and returns the loaded data"));
        OnDisk->Version = 9; UGameplayStatics::SaveGameToSlot(OnDisk, Slot, 0);
        R.Check(Load() && !F.Observer->bLastSuccess && DB->GetMoney() == 33,
            TEXT("Blueprint handler receives actual version values and rejects unsupported downgrade"));
        F.Saves->UnregisterSaveMigrationHandler();
        BP->RemoveFromRoot();
    }
    OnDisk->RemoveFromRoot();
    R.Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Migration tests clean only their unique slot"));
    Definition->RemoveFromRoot();
}
