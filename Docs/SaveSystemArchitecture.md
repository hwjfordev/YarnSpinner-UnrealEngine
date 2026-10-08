# Runtime Database、版本與 Blueprint 轉換流程

[Database 教學](DatabaseQuickStart.md) · [Migration handler 接法](SaveMigration.md)

## 模組與檔案依賴

```mermaid
flowchart TD
    Editor["YarnSpinnerEditor<br/>匯入 / YSLS / 整合測試"] --> Data["YarnSpinnerGameData<br/>Database / Save"]
    Data --> Yarn["YarnSpinner<br/>官方 VM / Runner"]
    Editor --> Yarn
    Project["專案 Blueprint<br/>ST_GameProgress / Migration Handler"] --> Data
```

下列檔案位於 Source/YarnSpinnerGameData，Public/*.h 定義 Blueprint API，Private/*.cpp 實作。

```mermaid
flowchart TD
    Settings["SharedGameDataSettings.h<br/>定義資產 / Yarn defaults / SaveDataVersion"] --> Types["GameDatabaseTypes.h<br/>DataTable row / snapshot"]
    DB["GameDatabaseSubsystem.cpp<br/>唯一 runtime State / Defaults"] --> Types
    DB --> Settings
    Vars["GameVariablesSubsystem.cpp<br/>Yarn 型別 API / 宣告 / 通知"] --> DB
    Storage["SharedYarnVariableStorage.cpp<br/>Yarn variable interface"] --> Vars
    Library["GameDataBlueprintLibrary.cpp<br/>Get / Connect"] --> Storage
    Library --> Bridge["YarnDatabaseBridge.cpp<br/>functions / commands"]
    Bridge --> DB
    Save["GameSaveSubsystem.cpp<br/>Async I/O / New Game / 驗證與發布"] --> DB
    Save --> Vars
    Save --> Settings
    Save --> Envelope["GameProgressSave.h<br/>Format 識別 / Version / Database / 時間"]
    Save --> Interface["GameSaveMigrationHandler.h<br/>MigrateSaveData 介面"]
    Handler["你的 BP_SaveMigrationHandler<br/>只轉換候選資料"] -. 實作 .-> Interface
    Runner["SharedYarnDialogueRunner.cpp<br/>選用便利元件"] --> Library
```

GameVariablesSubsystem 操作 Database.State.YarnVariables，沒有另一份值。CustomGameData 是你自訂的 ST_GameProgress；外掛不認識其中的日數、地圖或存檔點欄位。

## 初始化與新遊戲

```mermaid
sequenceDiagram
    participant Engine as Unreal Engine
    participant GI as BP_GameInstance 實例
    participant DB as GameDatabaseSubsystem
    participant Vars as GameVariablesSubsystem
    participant Save as GameSaveSubsystem
    participant UI as 選單 / PlayerController
    Engine->>GI: 建立 GameInstance 並初始化 subsystem collection
    GI->>DB: Initialize（引擎自動）
    DB->>DB: 載入 DataAsset / DataTable，建立 Defaults 與 State 副本
    GI->>Vars: Initialize（依賴 Database）
    Vars->>DB: 補入 Yarn / Gameplay runtime defaults
    GI->>Save: Initialize（依賴 Variables）
    UI->>Save: RegisterSaveMigrationHandler(Handler)
    Note over GI,UI: 不用手動 Initialize；BP GameInstance Event Init 比 subsystem 更早
    UI->>Save: StartNewGame
    Save->>DB: State = Defaults，重設 Yarn defaults
    Save-->>UI: replacement / OnNewGameStarted 通知
    UI->>DB: 讀 CustomGameData 決定起始場景
```

Director BeginPlay → ConnectRunnerToSharedVariables → 註冊 storage、functions、commands → 才開始對話。New Game 不刪檔、不取消 handler、不自動開地圖。

## 對話與玩法

```mermaid
sequenceDiagram
    participant VM as Yarn VM
    participant Bridge as YarnDatabaseBridge
    participant DB as Runtime Database
    participant BP as Gameplay / UI
    VM->>Bridge: npc_affinity("npc.mira")
    Bridge->>DB: TryGetNPCAffinity
    DB-->>VM: Number
    BP->>DB: AddNPCAffinity("npc.mira", 5)
    DB-->>BP: OnDataChanged
    VM->>Bridge: money_remove 30
    Bridge->>DB: TrySpendMoney(30)
    Note over Bridge,VM: 失敗停止對話；成功完成同步指令
```

## 載入與 migration

```mermaid
sequenceDiagram
    participant UI as 你的 Blueprint
    participant Save as GameSaveSubsystem
    participant Disk as Slot 檔案
    participant Handler as 註冊的 Migration Handler
    participant DB as Runtime Database
    UI->>Save: LoadSlot("Main")
    Save->>Save: 記錄 revision / CurrentVersion，bBusy=true
    Save->>Disk: AsyncLoadGameFromSlot
    Disk-->>Save: LoadedDatabase / SavedVersion
    Save->>Save: 檢查檔案身分與載入期間的資料變動
    alt SavedVersion 不等於 CurrentVersion
        Save->>Handler: MigrateSaveData(SavedVersion, CurrentVersion, LoadedDatabase)
        Handler-->>Save: Success / MigratedDatabase / ErrorMessage
    else 版本相同
        Save->>Save: Candidate = LoadedDatabase
    end
    Save->>Save: 再查 revision / 對話 / 版本，驗證 Candidate
    alt 全部成功
        Save->>DB: 一次替換 State，補缺少的 Yarn defaults
        DB-->>UI: OnDatabaseReplaced / OnStateReplaced
        Save-->>UI: bBusy=false；OnLoadCompleted(true)
        UI->>DB: GetCustomGameData，讀 ST_GameProgress
        UI->>UI: 依遊戲進度重建場景
    else 失敗
        Save-->>UI: bBusy=false；OnLoadCompleted(false, Error)
    end
    Note over Disk,DB: Load 不寫回 slot；失敗不提交候選資料
```

沒有 handler 而版本不同會失敗。較高版本也交由 handler 決定是否支援。Handler 僅修改候選值；若自行修改 live data，revision 防護拒絕提交，但不回滾 handler 自行產生的副作用。

SaveSlot：複製呼叫當下的 Database + SaveDataVersion → AsyncSaveGameToSlot → OnSaveCompleted。StartingCheckpoint / CurrentCheckpoint / FGameCheckpoint 已移除，存檔位置與恢复入口均由 CustomGameData 決定。

## 測試入口

Source/YarnSpinnerEditor/Private/Tests/GameData：SharedGameDataTestCommandlet.cpp 包含 DatabaseChecks.inl 與 SaveMigrationChecks.inl。真實 Yarn VM、磁碟存讀、獨立 GameInstance、Blueprint Struct、實際編譯並執行的 Blueprint migration interface graph。

Scripts/TestGameData.ps1 檢查本次退出碼與新報告；Scripts/VerifyGameDataAPI.py 檢查反射 API 與既有 Blueprint 編譯。