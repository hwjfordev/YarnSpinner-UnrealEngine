# Runtime Database、自訂進度與存檔：Blueprint 教學

本版保存玩家、所有 NPC、Yarn 變數與自訂遊戲進度。資料不依賴 Actor、BeginPlay 或目前關卡；不保存 Transform、不重建 NPC，也沒有 Apply Loaded World。程式位於 YarnSpinnerGameData 模組。

## 1. 作者填預設值，runtime 修改副本

1. Content Browser → Miscellaneous → Data Table，Row Structure 選 **GameDataRecord**，命名 DT_NPCDefaults。
2. 每列 Row Name 是穩定 ID，例如 `npc.mira`、`npc.alan`。不是 Actor 名稱。
3. 選取 npc.mira，填 Numbers Map：affinity=0；Bools Map：met=false；Strings Map：quest="none"。
4. npc.alan 可填 affinity=20、met=false、quest="none"。可增加其他 Number/Bool/String 欄位，不需要修改 C++。
5. 也可建立 `game` 列，例如 Numbers.chapter=1、Bools.power_on=false，提供對話可直接查詢的遊戲進度。
6. 建立 Data Asset，Class 選 **GameDatabaseDefinition**，命名 DA_GameDatabase。
7. DA 的 Default Records 指向 DT_NPCDefaults。
8. Project Settings → Game → Shared Game Data → Database Definition 指向 DA_GameDatabase。
9. Default Yarn Projects 仍填入你的 Visitor。Yarn 來源修改後正常重新匯入。

同一列的欄位名稱不可跨型別重複。ID/欄位是 FName，因此不區分大小寫；Yarn $variable 名称則有大小寫之分。

正式遊戲打包時，把 Database Definition、DataTable 與自訂 Blueprint Struct 納入 Asset Manager 或明確的 cook 收錄設定。Editor 可載入不等於打包一定包含所有 soft-reference 資產。

新 GameInstance 讀取定義並建立值副本；New Game 回到這份初始值。Load 使用存檔數值，不會以預設值蓋掉 NPC 進度。遊玩時修改 runtime，不會寫回 DataTable/DataAsset。編輯 defaults 後，結束並重新開始 Play 才建立新 session。

Initial State.Records 也可直接填寫，適合少量全域資料；不要與 Default Records 的 Row Name 重複。Create Record 可明確新增本輪才產生的記錄，這些也會保存。

## 2. 道具、金錢與穿戴

在 DA_GameDatabase 的 Items Map 定義：
| ID | Display Name | Equipment Slots |
| --- | --- | --- |
| money | 金錢 | 空 |
| potion | 藥水 | 空 |
| coat | 外套 | body |

DA 的 Money Item ID 設為 money；Equipment Slots 陣列加入 body。

Initial State.Inventory 可填 money=100、potion=2、coat=1。
Initial State.Equipment 可填 body=coat。

沒有設定 Database Definition 時，外掛提供空的預設資料庫，僅定義 money 道具，數量 0。NPC/衣服/藥水需要由你的定義資產建立。

Blueprint：
```text
Get Game Database → Get Money
Get Game Database → Add Money(50)
Get Game Database → Try Spend Money(30) → Branch(Return Value)
Get Game Database → Add Item("potion", 1)
Get Game Database → Try Remove Item("potion", 1)
Get Game Database → Equip Item("body", "coat")
Get Game Database → Unequip Item("body")
```

金錢只有 Inventory[money] 一份數值；不要再維護另一份 $gold 餘額。現有 Yarn 劇本的 $gold/$GoldenCoins 不會自動轉成 money，本版未改寫你的劇本或數值設定。

衣服仍算在 Inventory 中，裝備不扣數量。不能移除正在穿戴的那一件；先 Unequip。每個穿戴 slot 需要一個持有單位。UI 用 Get Inventory + Try Get Item Definition 顯示名稱；用 Get Money Item ID 區分金錢，不必另存另一個 Gold。

增減 Amount 必須是正整數。數量上限 16,777,216，保持與 Yarn float 整數精度一致。失敗回 false，Last Error 說明原因；不產生負餘額或部分扣除。

## 3. 查 NPC 與修改資料

```text
Get Game Database
  → Try Get NPC Affinity("npc.mira")
      Return Value = 是否找到
      Value        = 好感度（0 也是正常值）

Get Game Database
  → Add NPC Affinity("npc.mira", 5)

Get Game Database
  → Try Get Number("npc.alan", "affinity")
  → Try Get Bool("npc.mira", "met")
  → Set String("npc.mira", "quest", "active")
```

NPCAffinity 是 Numbers["affinity"] 的便利入口，沒有另外保存另一份數字。NPC Actor 不存在、尚未登場或在另一張地圖，結果都一樣。

Set / Add 欄位必須已存在且型別正確，拼字錯誤不會生成意外欄位。新記錄使用 Create Record，並傳入完整的 Game Data Record。

Try Get Record / Get Snapshot 回傳值副本。修改這份副本不會改到資料庫；一般欄位用 Set Number / Bool / String 提交。

## 4. 自訂遊戲進度（Custom Game Save）

建立 Blueprint Struct，例如 ST_GameProgress：
- Chapter：String
- CompletedQuests：Array<Name>
- Counters：Map<Name, Integer>
- 其他專案需要的值欄位。

在 DA_GameDatabase → Initial State → Custom Game Data 選擇此 Struct 並填預設值。這樣 New Game、型別檢查及存檔會使用同一個結構。

```text
Get Game Database → Get Custom Game Data
  → Get Instanced Struct Value（輸出接 ST_GameProgress）
  → Set Members in ST_GameProgress
  → Make Instanced Struct
  → Get Game Database：Set Custom Game Data
```

第一次建立資料也可直接 Make ST_GameProgress → Make Instanced Struct → Set Custom Game Data。保存時會一起序列化，不需要額外 SaveGame 子 Blueprint 或 Capture/Restore hooks。

Get 回傳副本，修改後一定要 Set 回去。使用值、ID、Struct、Array、Map、Soft Asset Reference；不能放 live Actor/Object Reference 或 delegate。版本不同時可呼叫 Blueprint migration handler，先轉換候選 snapshot 再驗證及套用，見 [SaveMigration.md](SaveMigration.md)。

通用 Yarn db_* 讀的是 Records 的型別欄位。任意自訂 Struct 不會自動暴露給 Yarn；你的 Blueprint function handler 可 Get Custom Game Data、解包後計算並回傳 Yarn Value。需要劇本經常查詢的簡單值，直接放 game 記錄，避免在兩處重複維護。

## 5. Dialogue Director 與 Yarn

保留現有接線：
```text
Director BeginPlay（Runner Auto Start=false）
  → Connect Runner To Shared Variables
  → 成功後才允許 Start Dialogue
```

它現在同時安裝 shared storage 與資料庫 handlers。不需要新增 command handler Actor；Shared Yarn Dialogue Runner 也會自動完成。下列名稱由外掛保留，若你要覆寫，自訂註冊必須在 Connect 後完成。

| Yarn function（唯讀） | 回傳 |
| --- | --- |
| money() | 金錢數量 |
| can_afford(30) | 能否負擔正整數金額 |
| item_count("potion") | 道具數量 |
| has_item("potion", 2) | 是否有足夠數量 |
| equipped_item("body") | 穿戴 ID；空 slot 回空字串 |
| npc_affinity("npc.mira") | NPC 好感度 |
| db_number("game", "chapter") | Number |
| db_bool("npc.mira", "met") | Bool |
| db_string("npc.mira", "quest") | String |
| db_has_record("npc.alan") | 記錄是否存在 |

修改使用同步 commands：
```yarn
<<money_add 50>>
<<if can_afford(30)>>
    <<money_remove 30>>
    <<item_add "potion" 1>>
<<endif>>
<<item_remove "potion" 1>>
<<equip_item "body" "coat">>
<<unequip_item "body">>
<<npc_affinity_add "npc.mira" 5>>
<<db_set_number "game" "chapter" 3>>
<<db_add_number "npc.mira" "affinity" -2>>
<<db_set_bool "npc.mira" "met" true>>
<<db_set_string "npc.mira" "quest" "done">>

<<if npc_affinity("npc.alan") >= 20>>
米菈: 艾倫似乎很信任你。
<<endif>>
```

查不到欄位、型別錯誤、扣款不足等會明確報錯並停止對話，避免失敗後劇本仍發獎勵。can_afford / has_item / db_has_record 適合預先分支。多個 commands 不是一筆跨命令交易；複雜商店購買請用自己的玩法函式統一驗證後操作。

數值 command 不接受小數數量、NaN、無限值。好感度的 Delta 可為正負有限浮點數。

編譯/儲存 Blueprint 後，在 YarnProject 的右鍵選单 Generate YSLS File 生成正確名稱、參數與回傳型別。VS Code project Definitions 引用產生的 .ysls.json。Runtime registration 由 Connect 實際完成，YSLS 本身不註冊物件。

用數字初始值確立接收變數的 Number 型別，避免編譯器缺少自訂 function 型別資訊時無法推導，例如：
```yarn
<<declare $display_money = 0>>
<<set $display_money = money()>>
```
display_money 是查詢當下的副本，不會持續同步金錢。完整可編譯例子見 Tests/CheckpointDatabase.yarn。Gameplay Defaults 只提供 runtime 共享變數的預設值，不會替 Yarn 編譯器宣告變數；Game Variables API 保留。

## 6. 存檔與載入

PlayerController/選單先綁 On Save Completed / On Load Completed，並在 Load 前註冊 migration handler（如需跨版本讀取）。不要在 BP_GameInstance Event Init 取 subsystem，該事件早於初始化。

```text
Save：更新 ST_GameProgress → Set Custom Game Data → Save Slot("Main")
Load：停止對話/資料更新 → Load Slot("Main") → On Load Completed 成功
      → Get Custom Game Data → 解包 ST_GameProgress → 決定地圖與入口
New Game：停止對話 → Start New Game 成功
      → 讀取已重設的 ST_GameProgress → 開啟起始地圖
```

Save Slot 沒有 Checkpoint pin。外掛保存完整 Database，不強制遊戲的進度欄位。是否站在合法存檔點由你的遊戲決定；在該互動呼叫 Save 即可。

Save/Load 的 Return=true 表示接受請求；最終結果由 completion event 回報。拒絕請求回 false + Last Error，不另發 completion event。Load 先轉換、驗證，再一次發布資料；失敗不套用候選資料，也不自動改寫原 slot。

Start New Game 還原 Database Definition 的初始資料及 Yarn defaults，不刪除磁碟存檔、不取消 migration handler，也不自動開地圖。不要在每次地圖 BeginPlay 自動 New Game，以免清掉剛載入的進度。

Project Settings → Game → Shared Game Data → Save Data Version 是唯一可設定版本號。完整設定與 handler 接法見 **[SaveMigration.md](SaveMigration.md)**。

## 7. UI 通知

一般欄位、道具或穿戴變更：On Data Changed(RecordID, Field)。
- NPC/遊戲欄位：該 Record ID 與欄位。
- 道具：inventory、ItemID。
- 穿戴：equipment、SlotID。
- 自訂進度：progress、None。

整份 New Game / Load：On Database Replaced，UI 重新讀取需要的內容。
Yarn 變數仍使用 GameVariablesSubsystem.On Variable Changed / On State Replaced。
不要在刷新 UI 的回呼中啟動另一個存讀檔。

## 已移除與仍保留

已移除 PersistentActorComponent、WorldSaveRegistrySubsystem、GameWorldStateSubsystem、GameSaveParticipant、world structs、ApplyLoadedWorld、ImportLegacySnapshot、SaveGameClass hooks，以及固定 GameCheckpoint / StartingCheckpoint / CurrentCheckpoint。

Format 識別字仍為 YarnSpinner.CheckpointDatabase；版本由 Save Data Version 設定，版本不同需要 migration handler。Slot 可用 Main、Slot_01，不再強制 SharedState_ 前綴；沒有刪除你的現有 .sav。

Config 只保留現有 Blueprint 節點所需的少量 class/struct/delegate 路徑轉向；沒有舊 SaveGame class 或整個舊 module package 的 redirect。這是為了讓既有 Connect/Get Game Saves 節點能載入。
