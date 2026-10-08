> 歷史報告：本頁記錄前一版模組搬移。現在的固定存檔點版本已移除 Actor 保存與 legacy 相容，請看 [目前驗證](CheckpointDatabaseValidation.md)。

# 外掛模組遷移驗證

日期：2026-10-07（Asia/Taipei）。UE **5.8.3-58210709**、Win64、MSVC **14.50.35739**、Windows SDK **10.0.22621.0**。沒有升級依賴。

## 結果

| 範圍 | 結果 |
| --- | --- |
| BuildPlugin：UnrealEditor / Development | 成功 |
| BuildPlugin：UnrealGame / Development | 成功 |
| BuildPlugin：UnrealGame / Shipping | 成功 |
| 原 Blueprint 專案直接建置 UnrealEditor 外掛 | 成功；不需要專案 Target |
| 原 YarnSpinnerTest，移除專案模組後的原生測試 | **71 checks / 0 failures**，退出碼 0，0 errors / 0 warnings |
| 原有 9 個 Blueprint 編譯與 Blueprint SaveGame 自訂欄位 | 通過，退出碼 0，0 errors / 0 warnings |
| 搬移前實際建立的舊原生存檔與 Blueprint 存檔 | 新模組成功載入，中文自訂值保留；舊 Blueprint 編譯通過 |
| 全新 BlueprintOnlyConsumer：沒有 Source、沒有 Modules、沒有 Visitor | **31 checks / 0 failures**，退出碼 0，0 errors / 0 warnings |
| 全新 Blueprint 專案的反射 API／Blueprint SaveGame 自訂欄位 | 通過，退出碼 0，0 errors / 0 warnings |

原專案測試於 **09:10:45 UTC** 完成；Blueprint／舊存檔相容性檢查於 **09:10:56 UTC** 完成。獨立 Blueprint 專案測試於 **09:12:07 UTC** 完成，API 驗證於 **09:12:16 UTC** 完成。

本次 BuildPlugin 包含 Editor DLL 與 Development／Shipping 預編譯資料。**沒有執行整個遊戲的 cook/package，也沒有實際 UMG 遊玩驗證。**

## 有驗證的相容性

* 原 70 項檢查保留：四條真實 Yarn VM 米菈流程、純量資料、位置、背包、InstancedStruct、動態 Actor、移除記錄、跨關卡玩家資料及讀檔競態。
* 額外一項透過 GameSaveSubsystem.LoadSlot 載入搬移前真正產生的原生存檔；不是把新類別物件改 Version 後冒充舊類別存檔。
* 舊 `/Script/YarnSpinnerTest.GameProgressSave` 經插件 Config Core Redirect 指向 `/Script/YarnSpinnerGameData.GameProgressSave`。
* 搬移前建立並儲存的 Blueprint 繼承舊 native SaveGame；搬移後載入、編譯，讀取其舊 slot，自訂中文字串一致。
* 只清理隨機命名的測試 Blueprint／slots；原有 Content 雜湊與搬移開始時一致。
* DefaultGame.ini 僅改 SharedGameDataSettings 區段名稱，保留使用者 Gameplay Defaults。

## 架構與檔案

原專案 `.uproject` 不含 Modules，根目錄沒有 `.Target.cs`／`.Build.cs`／C++ 檔案。新功能在外掛 **YarnSpinnerGameData**；只供測試的 Actor、Struct、SaveGame 和 commandlet 位於 **YarnSpinnerEditor**。

Runtime 模組沒有 `/Game/Dialogue/Visitor` 或硬編碼專案資產依賴；原專案的 wrapper 明確傳入測試 fixture。新的 BlueprintOnlyConsumer 只複製 BuildPlugin 產物即可載入並完成測試。

舊 Source 和舊 root Binaries 已核對雜湊並備份到原專案 `Saved/PluginMigrationBackup`。如檔案監看程序鎖住原 Source 空目錄，保留空目錄不影響 Blueprint 專案判定。

最新 Runtime C++ 自三種外掛建置及測試後未再更改；後續只整理文件、patch、報告與 Perforce。

## 保存的證據

* [原專案 71 項](Validation/ProjectMigration71.json)
* [原專案 Blueprint 清單](Validation/ProjectBlueprints.json)
* [舊類別、Blueprint 與存檔遷移](Validation/LegacyMigration.json)
* [獨立 Blueprint 專案 31 項](Validation/BlueprintOnly31.json)
* [獨立 Blueprint API](Validation/BlueprintOnlyAPI.json)
* [本次 Source／設定雜湊](Validation/PluginSources.json)

完整 log 留在原專案 Saved/Logs/YarnPluginPackageBuild.log、SharedStateBuild.log、SharedGameDataTests.log、SharedGameDataBlueprints.log，以及 Saved/BlueprintOnlyConsumer/Saved/Logs。

重跑方式見 [PluginSetup.md](PluginSetup.md)。舊類別 fixture 僅能在搬移前產生；一次性的遷移驗證已成功並清理自己的 fixture，日常測試不需要再傳 VerifyMigration。

編譯 log 中保留上游 YarnSpinnerEditor 的 OnPostEngineInit 過時 API 警告；編譯成功。本次沒有擴大修改該 API。
