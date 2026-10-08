# Blueprint 專案安裝

三個模組：YarnSpinner（官方 runtime）、YarnSpinnerGameData（資料庫／版本化存檔）、YarnSpinnerEditor（工具與測試）。資料庫不依賴任何固定專案資產。

1. 使用對應 UE/平台版本的完整建置外掛，放進專案 Plugins 並啟用 YarnSpinner。不需要專案 Source 或 Target.cs。
2. 設定 ysc 路徑並正常匯入 YarnProject。
3. Project Settings → Game → Shared Game Data：填 Default Yarn Projects、Database Definition、Save Data Version。
4. 依 [DatabaseQuickStart.md](DatabaseQuickStart.md) 建立 NPC DataTable、道具定義與自訂進度 Struct。
5. 保留 Connect Runner To Shared Variables，開始對話前呼叫一次。每個 Runner 都要連接。

原生外掛仍需要對應版本 binaries；只複製 Source 不代表可以直接在沒有工具鏈的電腦載入。請使用完整 BuildPlugin 輸出，包括預編譯 Intermediate。

## 建置可攜套件

```powershell
./Scripts/BuildGameDataPlugin.ps1 -EngineRoot "你的 Unreal 安裝目錄" -OutputDirectory "尚不存在的輸出目錄"
```

BuildPlugin 驗證 Editor / Game Development / Game Shipping 外掛模組，不等於整個專案 cook/package。

## 測試

```powershell
./Scripts/TestGameData.ps1 -ProjectFile "你的專案.uproject" -EngineRoot "你的 Unreal 安裝目錄"
```

需設定可用 ysc；通用測試使用外掛 Tests/CheckpointDatabase.yarn。原米菈範例可加 -YarnProject /Game/Dialogue/Visitor.Visitor，重跑四條真實對話。生成測試資料放 Saved/Tests，不覆寫 Content。

存檔格式識別為 YarnSpinner.CheckpointDatabase，版本由專案設定。版本不同時需註冊 [Blueprint migration handler](SaveMigration.md)；Actor 保存已移除，原 .sav 未刪除。

必要的 Blueprint 節點型別路徑轉向保留於 Config/DefaultYarnSpinner.ini，避免現有未重存資產失去 Connect/Get Game Saves 節點。沒有舊 SaveGame class 或 package-wide redirect。

[架構流程圖](SaveSystemArchitecture.md)

