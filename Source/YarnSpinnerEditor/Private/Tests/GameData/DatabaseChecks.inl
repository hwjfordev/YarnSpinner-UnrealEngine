void RunDatabaseChecks(FReport& R)
{
    auto* Settings = GetMutableDefault<USharedGameDataSettings>();
    auto* Definition = NewObject<UGameDatabaseDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
    Definition->AddToRoot();
    auto* Table = NewObject<UDataTable>(Definition);
    Table->RowStruct = FGameDataRecord::StaticStruct();
    FGameDataRecord Mira;
    Mira.Numbers.Add(TEXT("affinity"), 0);
    Mira.Bools.Add(TEXT("met"), false);
    Mira.Strings.Add(TEXT("quest"), TEXT(""));
    Table->AddRow(TEXT("npc.mira"), Mira);
    FGameDataRecord Alan = Mira; Alan.Numbers[TEXT("affinity")] = 20;
    Table->AddRow(TEXT("npc.alan"), Alan);
    FGameDataRecord Game; Game.Numbers.Add(TEXT("chapter"), 1);
    Table->AddRow(TEXT("game"), Game);
    Definition->DefaultRecords = Table;
    Definition->Items.Add(TEXT("money"), FGameItemDefinition());
    Definition->Items.Add(TEXT("potion"), FGameItemDefinition());
    FGameItemDefinition Coat; Coat.EquipmentSlots.Add(TEXT("body"));
    Definition->Items.Add(TEXT("coat"), Coat);
    Definition->EquipmentSlots.Add(TEXT("body"));
    Definition->InitialState.Inventory.Add(TEXT("money"), 100);
    Definition->InitialState.Inventory.Add(TEXT("coat"), 1);
    FCheckpointTestProgress Progress;
    Progress.Chapter = TEXT("Day1");
    Definition->InitialState.CustomGameData.InitializeAs<FCheckpointTestProgress>(Progress);
    TGuardValue<TSoftObjectPtr<UGameDatabaseDefinition>> DefinitionGuard(Settings->DatabaseDefinition, Definition);
    const FString Slot = TEXT("CheckpointTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    {
        FFixture F;
        if (!F.Initialize()) { R.Check(false, TEXT("Database fixture initializes")); Definition->RemoveFromRoot(); return; }
        auto* DB = F.World.GetTestWorld()->GetGameInstance()->GetSubsystem<UGameDatabaseSubsystem>();
        float Affinity = -1;
        bool Met = true;
        FString Quest;
        R.Check(DB->bReady && DB->TryGetNPCAffinity(TEXT("npc.mira"), Affinity) && Affinity == 0,
            TEXT("NPC DataTable defaults exist with no NPC actor"));
        R.Check(DB->TryGetBool(TEXT("npc.mira"), TEXT("met"), Met) && !Met
            && DB->TryGetString(TEXT("npc.mira"), TEXT("quest"), Quest) && Quest.IsEmpty(),
            TEXT("Database distinguishes false/empty values from missing fields"));
        R.Check(!DB->TryGetNumber(TEXT("missing"), TEXT("affinity"), Affinity)
            && !DB->SetNumber(TEXT("npc.mira"), TEXT("typo"), 5)
            && !DB->SetBool(TEXT("npc.mira"), TEXT("affinity"), true),
            TEXT("Unknown records/fields and wrong-type writes fail"));
        R.Check(DB->AddNPCAffinity(TEXT("npc.mira"), 7)
            && Table->FindRow<FGameDataRecord>(TEXT("npc.mira"), TEXT("test"))->Numbers[TEXT("affinity")] == 0,
            TEXT("Runtime NPC mutation never modifies authored DataTable"));
        FGameDataRecord Dynamic; Dynamic.Strings.Add(TEXT("result"), TEXT("new"));
        R.Check(DB->CreateRecord(TEXT("runtime.record"), Dynamic) && !DB->CreateRecord(TEXT("runtime.record"), Dynamic),
            TEXT("Explicit dynamic records are supported without duplicate creation"));
        R.Check(DB->GetMoney() == 100 && DB->AddMoney(20) && DB->TrySpendMoney(30) && DB->GetMoney() == 90,
            TEXT("Money helpers use the inventory currency item"));
        int32 Count = -1;
        R.Check(DB->TryGetItemCount(TEXT("money"), Count) && Count == 90
            && DB->TryGetItemCount(TEXT("potion"), Count) && Count == 0 && !DB->TryGetItemCount(TEXT("typo"), Count),
            TEXT("Known empty stacks differ from unknown item IDs"));
        R.Check(!DB->TrySpendMoney(91) && !DB->AddMoney(-1) && !DB->TrySpendMoney(0)
            && !DB->AddMoney(UGameDatabaseSubsystem::MaxQuantity) && DB->GetMoney() == 90,
            TEXT("Insufficient, negative, zero and overflowing money operations leave balance unchanged"));
        R.Check(!DB->EquipItem(TEXT("typo"), TEXT("coat")) && !DB->EquipItem(TEXT("body"), TEXT("potion"))
            && DB->EquipItem(TEXT("body"), TEXT("coat")) && !DB->TryRemoveItem(TEXT("coat"), 1),
            TEXT("Equipment requires a compatible slot and owned units; equipped items cannot be removed"));
        FName Equipped;
        R.Check(DB->TryGetEquippedItem(TEXT("body"), Equipped) && Equipped == TEXT("coat"),
            TEXT("Clothing is stored as an item ID"));
        auto Custom = DB->GetCustomGameData();
        auto& Mutable = Custom.GetMutable<FCheckpointTestProgress>();
        Mutable.Chapter = TEXT("Day3 自訂進度");
        Mutable.CompletedQuests = {TEXT("meet.mira"), TEXT("find.key")};
        Mutable.Counters.Add(TEXT("visits"), 3);
        R.Check(DB->GetCustomGameData().Get<FCheckpointTestProgress>().Chapter == TEXT("Day1")
            && DB->SetCustomGameData(Custom), TEXT("Custom progress structs use explicit value-copy updates"));
        F.Variables->SetBool(TEXT("$db_false"), false);
        F.Variables->SetNumber(TEXT("$db_zero"), 0);
        F.Variables->SetString(TEXT("$db_empty"), TEXT(""));
        R.Check(DB->GetSnapshot().YarnVariables.Numbers.Contains(TEXT("$db_zero")),
            TEXT("Shared Yarn variables are owned by the same runtime database"));
        R.Check(F.Saves->SaveSlot(Slot) && !F.Saves->SaveSlot(Slot),
            TEXT("Async database save accepts one operation only"));
        DB->AddMoney(1); DB->AddNPCAffinity(TEXT("npc.mira"), 1);
        R.Check(F.WaitForIO() && F.Observer->bLastSuccess && F.Observer->SaveCompletions == 1,
            TEXT("Database save completes exactly once"));
        R.Check(F.Saves->LoadSlot(Slot) && F.WaitForIO() && F.Observer->bLastSuccess
            && DB->GetMoney() == 90 && DB->TryGetNPCAffinity(TEXT("npc.mira"), Affinity) && Affinity == 7,
            TEXT("Load immediately restores the captured database without actor/application phase"));
        R.Check(F.Saves->LoadSlot(Slot), TEXT("Database race load accepted"));
        DB->AddNPCAffinity(TEXT("npc.alan"), 1);
        R.Check(F.WaitForIO() && !F.Observer->bLastSuccess
            && DB->TryGetNPCAffinity(TEXT("npc.alan"), Affinity) && Affinity == 21,
            TEXT("Concurrent NPC mutation rejects stale async load"));
    }
    {
        FFixture F;
        if (!F.Initialize()) { R.Check(false, TEXT("Fresh database initializes")); Definition->RemoveFromRoot(); return; }
        auto* DB = F.World.GetTestWorld()->GetGameInstance()->GetSubsystem<UGameDatabaseSubsystem>();
        R.Check(DB->GetMoney() == 100, TEXT("Fresh GameInstance has independent defaults"));
        R.Check(F.Saves->LoadSlot(Slot) && F.WaitForIO() && F.Observer->bLastSuccess,
            TEXT("Disk save loads into a fresh GameInstance before any NPC exists"));
        const auto Data = DB->GetSnapshot();
        R.Check(Data.Records.Contains(TEXT("runtime.record")) && Data.Records[TEXT("npc.mira")].Numbers[TEXT("affinity")] == 7
            && Data.Equipment.FindRef(TEXT("body")) == TEXT("coat") && Data.Inventory.FindRef(TEXT("money")) == 90,
            TEXT("Disk preserves NPC, player inventory, clothing and dynamic records"));
        R.Check(Data.CustomGameData.Get<FCheckpointTestProgress>().Chapter == TEXT("Day3 自訂進度")
            && Data.CustomGameData.Get<FCheckpointTestProgress>().CompletedQuests.Num() == 2
            && Data.CustomGameData.Get<FCheckpointTestProgress>().Counters.FindRef(TEXT("visits")) == 3,
            TEXT("Custom progress struct strings, arrays and maps survive disk round trip"));
        R.Check(Data.YarnVariables.Numbers.Contains(TEXT("$db_zero")) && !Data.YarnVariables.Bools[TEXT("$db_false")]
            && Data.YarnVariables.Strings[TEXT("$db_empty")].IsEmpty(), TEXT("Yarn zero, false and empty survive disk save"));
        R.Check(F.Saves->LoadSlot(Slot + TEXT("_missing")) && F.WaitForIO() && !F.Observer->bLastSuccess
            && DB->GetMoney() == 90, TEXT("Missing slots preserve all live database state"));
        auto* Invalid = Cast<UGameProgressSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
        Invalid->Database.Inventory.Add(TEXT("money"), -10);
        UGameplayStatics::SaveGameToSlot(Invalid, Slot, 0);
        R.Check(F.Saves->LoadSlot(Slot) && F.WaitForIO() && !F.Observer->bLastSuccess && DB->GetMoney() == 90,
            TEXT("Invalid inventory rejects the entire load without partial replacement"));
        R.Check(F.Saves->StartNewGame() && DB->GetMoney() == 100 && !DB->HasRecord(TEXT("runtime.record"))
            && DB->GetCustomGameData().Get<FCheckpointTestProgress>().Chapter == TEXT("Day1")
            && !F.Variables->Contains(TEXT("$db_empty")), TEXT("New Game resets database, custom progress and Yarn session keys"));
        R.Check(F.Saves->DoesSlotExist(Slot), TEXT("New Game does not delete disk saves"));

        const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("YarnSpinner"))->GetBaseDir();
        const FString Temp = FPaths::ProjectSavedDir() / TEXT("Tests/CheckpointCompiler");
        IFileManager::Get().MakeDirectory(*Temp, true);
        for (const TCHAR* File : {TEXT("CheckpointDatabase.yarn"), TEXT("CheckpointDatabase.yarnproject")})
            IFileManager::Get().Copy(*(Temp / File), *(PluginDir / TEXT("Tests") / File), true);
        bool Cancelled = false;
        auto* Factory = NewObject<UYarnProjectFactory>();
        F.Project = Cast<UYarnProject>(Factory->FactoryCreateFile(UYarnProject::StaticClass(), GetTransientPackage(),
            NAME_None, RF_Transient, Temp / TEXT("CheckpointDatabase.yarnproject"), TEXT(""), GWarn, Cancelled));
        R.Check(F.Project != nullptr, TEXT("Database Yarn fixture compiles/imports through official ysc"));
        if (F.Project)
        {
            USharedGameDataTestPresenter* Presenter = nullptr;
            auto* Runner = F.MakeRunner(Presenter, false);
            R.Check(Runner && F.Play(Runner, Presenter, {}, TEXT("DatabaseFlow")),
                TEXT("Existing Connect node installs database functions/commands in a real Yarn VM"));
            R.Check(F.Number(TEXT("$query_money")) == 95 && F.Number(TEXT("$query_items")) == 2
                && F.Number(TEXT("$query_affinity")) == 5 && F.Number(TEXT("$query_other_npc")) == 20
                && F.Number(TEXT("$query_chapter")) == 2, TEXT("VM queries and mutations use shared database including offscreen NPCs"));
            bool B = false; FString S;
            R.Check(F.Variables->TryGetBool(TEXT("$query_met"), B) && B
                && F.Variables->TryGetString(TEXT("$query_quest"), S) && S == TEXT("active")
                && F.Variables->TryGetString(TEXT("$query_equipment"), S) && S == TEXT("coat"),
                TEXT("VM bool/string/equipment functions preserve their types"));
            R.Check(F.Variables->TryGetBool(TEXT("$query_has_item"), B) && B
                && F.Variables->TryGetBool(TEXT("$query_can_afford"), B) && B
                && F.Variables->TryGetBool(TEXT("$query_has_record"), B) && B,
                TEXT("VM inventory and affordability predicates work"));
            const int32 Before = DB->GetMoney();
            const auto YarnVerbosity = LogYarnSpinner.GetVerbosity();
            const auto TempVerbosity = LogTemp.GetVerbosity();
            // These failures are intentional; assertions below still check the resulting state.
            LogYarnSpinner.SetVerbosity(ELogVerbosity::Fatal);
            LogTemp.SetVerbosity(ELogVerbosity::Fatal);
            for (const TCHAR* Node : {TEXT("QueryMissing"), TEXT("CommandInsufficient"), TEXT("CommandFraction"), TEXT("CommandGarbage")})
            {
                Runner->StartDialogue(Node);
                for (int32 Tick = 0; Tick < 3; ++Tick) F.Tick();
                R.Check(!Runner->IsDialogueRunning() && DB->GetMoney() == Before,
                    FString::Printf(TEXT("Invalid database operation stops dialogue without later mutation: %s"), Node));
            }
            Runner->StartDialogue(TEXT("QueryMissing"));
            Runner->StopDialogue();
            Runner->StartDialogue(TEXT("HoldLine"));
            for (int32 Tick = 0; Tick < 3; ++Tick) F.Tick();
            R.Check(Runner->IsDialogueRunning(), TEXT("A stale query-error callback cannot stop a new conversation"));
            Runner->StopDialogue();
            LogYarnSpinner.SetVerbosity(YarnVerbosity);
            LogTemp.SetVerbosity(TempVerbosity);
        }
        R.Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Tests clean only their unique checkpoint slot"));
    }
    // A real Blueprint-authored struct, not merely a native C++ struct in InstancedStruct.
    auto* BPStruct = FStructureEditorUtils::CreateUserDefinedStruct(GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UUserDefinedStruct::StaticClass(), TEXT("CheckpointProgressProbe")), RF_Transient);
    BPStruct->AddToRoot();
    FEdGraphPinType StringType;
    StringType.PinCategory = UEdGraphSchema_K2::PC_String;
    const bool Added = FStructureEditorUtils::AddVariable(BPStruct, StringType);
    FStrProperty* NoteProperty = nullptr;
    for (TFieldIterator<FStrProperty> It(BPStruct); It; ++It) { NoteProperty = *It; break; }
    R.Check(Added && NoteProperty, TEXT("Custom game progress supports an actual Blueprint Struct"));
    if (NoteProperty)
    {
        FInstancedStruct Initial;
        Initial.InitializeAs(BPStruct);
        NoteProperty->SetPropertyValue_InContainer(Initial.GetMutableMemory(), TEXT("預設章節"));
        Definition->InitialState.CustomGameData = Initial;
        {
            FFixture F;
            if (F.Initialize())
            {
                auto* DB = F.World.GetTestWorld()->GetGameInstance()->GetSubsystem<UGameDatabaseSubsystem>();
                auto Modified = DB->GetCustomGameData();
                NoteProperty->SetPropertyValue_InContainer(Modified.GetMutableMemory(), TEXT("Day3 自訂 Blueprint 進度"));
                R.Check(DB->SetCustomGameData(Modified) && F.Saves->SaveSlot(Slot)
                    && F.WaitForIO() && F.Observer->bLastSuccess, TEXT("Blueprint custom progress writes through database SaveSlot"));
            }
            else R.Check(false, TEXT("Blueprint progress save fixture initializes"));
        }
        {
            FFixture F;
            if (F.Initialize())
            {
                auto* DB = F.World.GetTestWorld()->GetGameInstance()->GetSubsystem<UGameDatabaseSubsystem>();
                R.Check(F.Saves->LoadSlot(Slot) && F.WaitForIO() && F.Observer->bLastSuccess
                    && NoteProperty->GetPropertyValue_InContainer(DB->GetCustomGameData().GetMemory()) == TEXT("Day3 自訂 Blueprint 進度"),
                    TEXT("Blueprint custom progress restores into a fresh GameInstance"));
                R.Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Blueprint progress test cleans its unique slot"));
            }
            else R.Check(false, TEXT("Blueprint progress load fixture initializes"));
        }
    }
    BPStruct->RemoveFromRoot();
    Definition->RemoveFromRoot();
}
