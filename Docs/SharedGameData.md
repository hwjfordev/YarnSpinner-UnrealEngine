# 共用 Yarn 變數與資料庫

本版主教學：[DatabaseQuickStart.md](DatabaseQuickStart.md)。呼叫流程：[SaveSystemArchitecture.md](SaveSystemArchitecture.md)。

現有 `Connect Runner To Shared Variables` 保留；新增自動註冊資料庫 functions / commands。

```text
Get Game Variables → Try Get Number("$flag_count")
Get Game Variables → Set Bool("$heard_story", true)
Get Game Variables → Get Formatted Variables
Get Game Database → Try Get NPC Affinity("npc.mira")
Get Game Database → Get Money / Add Money / Try Spend Money
Get Game Saves → Save Slot / Load Slot / Start New Game
```

Yarn variables 與 Records、Inventory、Equipment、CustomGameData 都位於同一個 runtime database snapshot。GameVariablesSubsystem 提供原本的型別 API、宣告預設值、smart-variable 支援與通知。

NPC 與跨角色資料請建立 DataTable rows；金錢用 inventory ItemID；不要同步兩份餘額。全域敘事變數仍可使用 $name。GameInstance Subsystems 自動建立，不需要 Cast BP_GameInstance。

Default Yarn Projects / Gameplay Defaults 設定保留。False、0、空字串完整保存；TryGet 的 Return Value 代表找到，Value 才是內容。

Save 是完整 Database，恢復入口由 CustomGameData 中的 ST_GameProgress 決定。Load 成功時資料已恢復；不再有固定 Checkpoint 或 Apply Loaded World。版本與 Blueprint handler 見 [SaveMigration.md](SaveMigration.md)。

