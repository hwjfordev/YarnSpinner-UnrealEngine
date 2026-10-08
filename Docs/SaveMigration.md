# 單一存檔版本與 Blueprint Migration Handler

本版保留 Game Variables API、Yarn storage 與完整 runtime database。移除 Starting Checkpoint、GameCheckpoint、Current Checkpoint；遊戲進度及恢復入口全由你的 ST_GameProgress 決定。

## 1. 新遊戲與存檔

DA_GameDatabase → Initial State → Custom Game Data 填 ST_GameProgress 的新遊戲預設值，例如 CurrentDay、CurrentLocationID、LastSavePointID。外掛不要求這些欄位或名稱。

Project Settings → Game → Shared Game Data → Save → **Save Data Version**：填正整數，首次發行通常為 1。Game Variables / Gameplay Defaults 保留；這個版本號不必等於產品版本。

```text
New Game 按鈕
  → Get Game Saves → Start New Game
  → 成功：Get Game Database → Get Custom Game Data
  → 解包 ST_GameProgress → 依自己的欄位開啟起始地圖

存檔點互動
  → 把當前進度寫回 Custom Game Data
  → Get Game Saves → Save Slot("Main")
  → On Save Completed：Success
```

Save Slot 已沒有 Checkpoint pin；進階 User Index 仍存在。請刪除舊 Make Game Checkpoint 節點，重新建立/更新舊 Save Slot 節點及接線。Return=true 只表示接受非同步請求；最終結果由 completion event 回報。Return=false 時讀 Last Error，不會額外發 completion event。

## 2. 建立 Blueprint handler

1. 建立 Object 父類別的 Blueprint，命名 BP_SaveMigrationHandler。
2. Class Settings → Implemented Interfaces → 加入 **Game Save Migration Handler**。
3. 在 My Blueprint 的 Interfaces 中開啟 **Migrate Save Data**。它有回傳值，因此是同步 Function，不是 latent event。
4. 輸入：Saved Version、Current Version、Loaded Database。
5. 輸出：Success、Migrated Database、Error Message。

先把 Loaded Database 複製到函式的區域變數 Candidate。只修改 Candidate；最後把完整 Candidate 接到 Migrated Database。不要從 New Game 的全新 snapshot 開始而意外丟掉玩家其他資料。

例如 V1 → V2 新增進度資訊：

```text
SavedVersion == 1 AND CurrentVersion == 2
  → Candidate = Loaded Database
  → Break Candidate → Custom Game Data
  → Get Instanced Struct Value（ST_GameProgress）
  → 根據旧 CurrentDay 補上新版欄位
  → Make Instanced Struct
  → Set Members in Game Database Snapshot（Candidate 的 Custom Game Data）
  → Return：Success=true，MigratedDatabase=Candidate

不支援的版本組合
  → Return：Success=false，ErrorMessage="不支援此存檔版本"
```

升級 NPC Records 時也是修改副本：取出 Map 裡的 Record、編輯 Number/Bool/String Maps，再 Add 同一個 key 寫回 Candidate.Records。Yarn Variables 的 snapshot 也可編輯；不能把同一名稱改成與目前宣告衝突的型別。

若要 V1 → V3，handler 自行依序執行 V1→V2、V2→V3。外掛只呼叫一次，要求最終輸出符合 Current Version。不要用 0、false 或空陣列判斷舊版本，要用 Saved Version。

## 3. 在 Load 前註冊

```text
PlayerController / 主選單 BeginPlay
  → Construct Object from Class：BP_SaveMigrationHandler
       Outer：Get Game Instance
  → Get Game Saves → Register Save Migration Handler（Return Value）
  → 註冊成功後才允許 Load Slot
```

Subsystem 會持有 handler 的強參考，直到被替換、Unregister Save Migration Handler 或 GameInstance 結束。不需要掛 Actor Component。也可讓既有 BP_GameInstance 實作這個介面，再在選單 BeginPlay 把 Get Game Instance 傳入註冊；不必 Cast。

不要在 BP_GameInstance Event Init 取得 subsystem，該事件早於 UE 的 subsystem 初始化。每個 GameInstance 只註冊一個 handler；新的有效註冊取代前一個，無效註冊不清掉舊 handler。New Game 不取消註冊。忙碌時禁止替換/取消 handler。

## 4. 載入與失敗契約

```text
Load Slot("Main")
  → 讀取成暫存 snapshot
  → 版本相同：略過 handler
  → 版本不同：呼叫已註冊 handler
  → 驗證最終 snapshot
  → 一次替換 runtime database
  → On Load Completed：Success=true
  → 你的 Blueprint 讀 ST_GameProgress 決定場景與入口
```

- 新舊版本不同都會呼叫 handler，包括 Saved Version > Current Version。由 handler 拒絕不支援的降版。
- 沒有 handler、handler 失敗或回傳無效資料，外掛不套用 snapshot，也不發 On Database Replaced。On Load Completed 回 false，Error / Last Error 有原因。
- 即使版本相同仍驗證資料，不會繞過完整性檢查。
- 載入/轉換期間保持 bBusy；不要 nested Save/Load/NewGame、切換地圖、開始對話或修改 live subsystem。handler 是資料轉換函式，不是遊戲流程事件。
- 外掛在 handler 前後檢查 runtime revision。若遊戲改動資料，放棄載入；不回滾你自行產生的遊戲副作用，因此 handler 必須只修改候選副本。
- 成功時完整 database 已可讀，再發出 replacement / completion 通知。刷新 UI 的 replacement 回呼不要啟動另一筆存讀檔。
- Load 永遠不自動覆寫原 slot；下一次正常 Save 才寫入新版資料與目前版本號。
- Save 捕捉呼叫當下的 database 與版本；Load 使用請求開始時的目標版本，途中若版本改變會拒絕提交。

## 5. 可相容範圍

單一公開版本號為 Save Data Version；檔案內的 Format 字串只是格式識別，保留既有 YarnSpinner.CheckpointDatabase 名稱，並不是另一個版本。

callback 的前提是 Unreal 能把檔案讀成 UGameProgressSave / FGameDatabaseSnapshot。保留必要的舊 Struct 資產與待轉換欄位；callback 不能復原反序列化時已經丟失的資料。Struct 自身預設值與 Data Asset 上的實例值不同，不能假設新增欄位會自動合併 DA_GameDatabase 的值。

新版新增 NPC/欄位，請在 migration 補進 Candidate，再交給新版 schema 驗證。舊版曾放在固定 Checkpoint 的內容不會再提供给 handler；尚未發行的開發存檔若需要那些資訊，請先在舊版本轉存至 Custom Game Data。本次沒有刪除任何既有 .sav。

保存每個已發行版本的代表性存檔，實際測試舊版→新版，尤其是新增欄位、ID 變更、改型別與 Struct 改名。不要把同版本存讀成功當成任意未來版本都相容。

[完整 Database 教學](DatabaseQuickStart.md) · [流程圖](SaveSystemArchitecture.md)
