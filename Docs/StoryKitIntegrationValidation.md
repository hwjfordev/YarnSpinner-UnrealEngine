# StoryKit 整合與驗證

`storykit-integration` 分支可與 [StoryKit / StoryKitYarn](https://github.com/hwjfordev/StoryKit) 合用。Yarn 本身不依賴 StoryKit；橋接插件負責將任務與 Facts 寫入 `GameDatabase.Sections["StoryKit"]`，並註冊 Yarn 的任務指令／函式。

## 使用方式

1. 使用本分支的 Yarn 插件，以及 StoryKit repo 中的 `Plugins/StoryKit`、`Plugins/StoryKitYarn`。
2. Runner 照常使用 `Connect Runner To Shared Variables` 或 `Shared Yarn Dialogue Runner`；橋接會自動安裝。
3. Save／Load／New Game 一律使用 Yarn 的 Game Save Subsystem。直接回傳值表示請求是否接受，最終結果由完成事件回報。
4. StoryKit Action 執行中會阻擋存檔；Load／New Game 會取消可被替換的任務動作。既有的對話進行中禁止存讀限制仍適用。

## 本次修正

相對 `a13e378faae279bf36c20ec97838dca516c895ca`：

- `UYarnDialogueInstance` 加入 dialogue generation。停止、重新開始或切換劇本會使舊的 `ContinueDeferred` 回呼失效，避免前一段對話推進新對話、跳過 blocking command。同步指令已停止對話時不再排入延遲回呼。
- 通用 Section 測試改用 `YarnDatabaseTest`，避免測試用結構占用正式的 `StoryKit` 資料區。

真實 VM 的跨對話回歸測試位於 StoryKit repo 的 `StoryKit.Yarn.VM.InvalidInput` 與 `StoryKit.Yarn.VM.StaleContinuation`。測試使用官方 `ysc` 編譯的 `.yarnc`、真實 Runner 與 Level Sequence Player；不是模擬回傳值。

## 2026-10-16 驗證

環境：Windows 11 x64、UE 5.8.3（CL 58210709）、MSVC 14.50.35739、Windows SDK 10.0.22621.0、`ysc 3.2.2`。本次提交的 C++ 與已測試內容相同；提交準備只補充文件及報告。

| 環境 | 結果 |
|---|---|
| Yarn 單獨啟用 | Editor 建置成功；60/60 Automation、73/73 SharedGameData 檢查通過 |
| StoryKit 單獨啟用 | Editor 建置成功；5/5 Automation 通過 |
| Yarn + StoryKit + StoryKitYarn | Editor 建置成功；60/60 Yarn、11/11 StoryKit Automation 通過 |
| 三插件合用，包含米菈劇本 | 111/111 SharedGameData 檢查通過，commandlet 退出碼 0 |
| 三插件合用的遊戲目標 | Win64 Development／Shipping 編譯、連結成功 |
| Threshold 原型隔離副本 | 重新建置／匯入後，四條既有遊戲流程全部通過 |

73 項與 111 項檢查有重疊，不代表 184 項不同功能。存讀檢查涵蓋資料庫、版本遷移、自訂 Blueprint 結構及舊檔補上新宣告預設值。橋接測試另外驗證任務部分進度與 Yarn 變數一起存到磁碟，並由全新 GameInstance 讀回後繼續任務。

Yarn 的四項帶警告測試已檢視：fixture 未設定 Runner／Project，以及大型測試劇本的變數大小寫衝突。沒有失敗或未執行的測試；不要使用僅大小寫不同的 Yarn 變數名稱表示不同資料。

本 repo 保存 [Yarn 單獨 Automation](Validation/StoryKitIntegration-2026-10-16/YarnOnly-YarnSpinner.json)、[獨立存讀檢查](Validation/StoryKitIntegration-2026-10-16/YarnOnly-SharedGameData.json)、[三插件的 Yarn Automation](Validation/StoryKitIntegration-2026-10-16/Combined-YarnSpinner.json)、[111 項存讀與米菈檢查](Validation/StoryKitIntegration-2026-10-16/Combined-Mira-SharedGameData.json)。完整建置紀錄、StoryKit 測試與原型報告見 [StoryKit 驗證紀錄](https://github.com/hwjfordev/StoryKit/blob/main/VALIDATION.md)。

## 重跑

關閉使用這些插件的 Editor，執行 StoryKit repo 的 `Scripts/TestPlugins.ps1`，指定 `-EngineRoot`、本 Yarn checkout 的 `-YarnPluginDirectory`、`-YscPath`，加 `-BuildRuntime` 可驗證 Development／Shipping 編譯。腳本建立隔離 host，檢查本次 JSON 報告而不只依賴程序退出碼。

Yarn 自身的存讀檢查也可用 `Scripts/TestGameData.ps1`。米菈的額外流程需要先匯入專案劇本、設定 Default Yarn Projects，並傳入 `-YarnProject /Game/Dialogue/Visitor.Visitor`。

本次沒有手動 UI 遊玩、完整 cook／package、其他平台或多人遊戲驗證；Shipping 編譯成功不等於完成發行打包。
