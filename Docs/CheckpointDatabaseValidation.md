# 固定存檔點 Database 驗證（歷史）

本文件記錄前一版的驗證。固定 Checkpoint 已移除，最新版本與範圍請看 [SaveMigrationValidation.md](SaveMigrationValidation.md)。

日期：2026-10-07（Asia/Taipei）。環境：UE 5.8.3-58210709、MSVC 14.50.35739、Windows SDK 10.0.22621.0；Yarn Spinner 3.2.8-alpha9、現有專案 ysc。沒有升级依賴。

## 結果

| 驗證 | 結果 |
| --- | --- |
| 原 Blueprint 專案 UnrealEditor C++ 建置 | 成功，退出碼 0 |
| 原專案完整整合測試 | **82 checks / 0 failures**，退出碼 0 |
| 原有 10 個 Blueprint 編譯與新 API 反射 | 通過，退出碼 0；未儲存資產 |
| BuildPlugin Editor Development | 成功 |
| BuildPlugin UnrealGame Development | 成功 |
| BuildPlugin UnrealGame Shipping | 成功 |
| 全新無 Source/Modules Blueprint 專案 | **43 checks / 0 failures**，退出碼 0，0 errors / 0 warnings |
| 既有 Content 雜湊 | **257 個檔案，0 個改變** |
| 原 .uproject / Source | 無專案 Modules；Source 中無 C++ 檔案 |

原專案 runtime 測試完成：11:47:40 UTC；Blueprint 編譯：11:42:52 UTC；獨立 Blueprint consumer：11:52:24 UTC。Blueprint 檢查後的 C++ 修改只加強數字 parser，不改 Blueprint 簽名；最後的完整 runtime 測試及三種套件建置均包含此修正。

## 實際覆蓋

- DataTable NPC defaults 在沒有 NPC Actor 的情況下可查詢，runtime 修改不改原表。
- NPC1 的對話可按 ID 查 NPC2；Number/Bool/String、0/false/空字串、動態記錄。
- 金錢與 inventory 同一來源；正整數、數量上限、扣款不足、不改負數、錯誤字串/小數拒絕。
- 衣服以 slot/item ID 保存；裝備需持有、slot 相容、禁止移除已裝備單位。
- 原生 Struct 的 String/Array/Map 與**真正 Blueprint UserDefinedStruct** 自訂進度，透過 SaveSlot/LoadSlot 到磁碟，再在新 GameInstance 還原。
- 固定 CheckpointID/LevelName/Day/ProgressID，無 Actor/world apply 階段。
- Async request-time snapshot、重複操作拒絕、載入期間資料改動的 revision 防護、損壞內容與缺失 slot 不覆蓋目前資料。
- New Game 重設資料而不刪檔。
- 原 Visitor 四條流程仍使用官方 Yarn VM，shared storage 的 Execute interface、smart variables、內部 tracking 完整保存。
- 新測試 .yarnproject 經官方 ysc/Factory 匯入，Connect 自動註冊的 10 個 functions 與 11 個 commands 具有 YSLS metadata。
- 錯誤 function/command 停止對話，後续指令不執行；過期錯誤回呼不會停止新對話。
- 跨專案使用相同 BuildPlugin 產物，不需要專案 native module。

## 原外掛修正與失敗紀錄

整合測試重現上游 VM 在 command callback StopDialogue 後存取 null CurrentNode；加入最小 guard，原流程與錯誤流程都通過。修正見 Patches/checkpoint-vm-stop.patch。

首次 BuildPlugin 輸出路徑過長，UBT 因 260 字元限制拒絕。改用 Saved/Pkg/Checkpoint 後，三種 target 全部成功；未修改引擎或 OS 設定。

原專案載入 WBP_YarnDebugHUD 時仍有一項舊 /Script/YarnSpinnerTest package 的 linker warning。必要 class/struct/delegate redirect 已保留，該 Blueprint 編譯通過、0 compiler errors。沒有為清理序列化路徑而重存使用者資產；全新 consumer 無此 warning。

## 交付與範圍

- 完整 Win64 外掛套件：原專案 Saved/Pkg/Checkpoint。
- 全新 Blueprint consumer：原專案 Saved/CheckpointConsumer。
- 程式變更延續 Perforce pending changelist 48，未提交。
- Source/設定備份：Saved/CheckpointRefactorBackup。舊 .sav 未刪除。
- 沒有執行實際 UMG/UI 遊玩，也沒有整個遊戲 cook/package。BuildPlugin 成功只代表外掛模組與預編譯資料成功。
- 遊戲打包仍需確保自己的 DatabaseDefinition、DataTable、Blueprint Struct 等資產被 cook 收錄。
- 本次不替使用者改寫 Content 或 Visitor.yarn；Database Definition 由使用者依教學建立與指派。Visitor.ysls.json 已更新為 Editor 真正生成的定義。

## 可追蹤證據

- [原專案 82 項](Validation/CheckpointProject82.json)
- [獨立 Blueprint 43 項](Validation/CheckpointConsumer43.json)
- [10 個 Blueprint 與 API](Validation/CheckpointBlueprints.json)
- [原始碼與 Content 驗證摘要](Validation/CheckpointSources.json)

完整 logs 留在 Saved/Logs/SharedGameDataTests.log、CheckpointBlueprints.log、CheckpointPluginBuild.log 與 consumer 的 Saved/Logs。最後建置後只更新文件與證據，runtime/test C++ 未再變更。

[Blueprint 教學](DatabaseQuickStart.md) · [架構流程圖](SaveSystemArchitecture.md)

