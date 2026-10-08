# 單一版本與 Blueprint Migration：驗證紀錄

日期：2026-10-07（Asia/Taipei）。UE 5.8.3-58210709 / MSVC 14.50.35739 / SDK 10.0.22621.0 / Yarn Spinner 3.2.8-alpha9，未升級依賴。

## 已完成

| 項目 | 結果 |
| --- | --- |
| 原專案 C++ Editor 建置 | 成功，exit 0 |
| 原專案整合測試 | **104 checks / 0 failures**，exit 0；包含 Visitor 四條真實 Yarn 流程 |
| 全新 Blueprint-only consumer | **66 checks / 0 failures**，exit 0；0 errors / 0 warnings |
| BuildPlugin | Win64 Editor Development、Game Development、Game Shipping 全部成功 |
| Blueprint API | 新介面、註冊／取消註冊／版本查詢存在；GameCheckpoint 已不存在 |
| 原專案 10 個 Blueprint | **9 個成功，1 個需要換接線**；見下方，不視為整體 Blueprint 驗證通過 |
| 既有 Content | **260 個檔案，0 個變更** |

主測試完成 14:15:28 UTC；全新 consumer 完成 14:21:51 UTC。最後成功的 C++ 建置後只更新文件、驗證脚本及證據；runtime / test C++ 未再變更。

## Migration 實際覆蓋

- Save Slot 不再要求 Checkpoint；磁碟存檔寫入設定的 Save Data Version。
- 相同版本略過 handler，但仍驗證完整內容。
- 不同版本且沒有 handler 時失敗，既有 runtime 值及 replacement 通知數不變。
- null／未實作介面的 handler 註冊失敗。
- handler 收到真正 SavedVersion、CurrentVersion、舊檔 database；舊資料缺少新版必要欄位時先轉換再驗證。
- handler 拒絕、輸出無效 inventory、缺少 handler、無效版本，都不提交候選資料。
- 成功後完整 NPC／Inventory／自訂 Struct／Yarn 資料可用，completion 與 replacement 各一次。
- 載入不重寫 slot；下一次正常 Save 才寫入新版；再次載入同版不再呼叫 handler。
- SavedVersion > CurrentVersion 也會呼叫 handler，測試 handler 明確拒絕降版。
- callback 期間 nested Save/Load/NewGame、替換/取消 handler 均被 bBusy 擋下。
- 非同步讀取途中 gameplay 改動，拒絕舊 snapshot；handler 若自行改 live data，拒絕提交候選值（不回滾 handler 副作用）。
- 使用 Unreal 編譯真正 Blueprint interface function graph：傳遞 LoadedDatabase，依 SavedVersion < CurrentVersion 回傳結果；成功與拒絕兩條路徑都經 LoadSlot 執行。
- 原資料庫測試保留真實 Blueprint UserDefinedStruct、Array/Map 的磁碟 round trip、NPC defaults、0/false/空字串與官方 Yarn VM。

這不是對所有未來 Struct 變更的保證。callback 只收到 Unreal 成功反序列化的資料；刪除舊資產/欄位前仍需規劃讀取及轉換。

## 現有專案需要手動換接線／設定

1. **BP_MyThirdPersonCharacter**：舊 Make Game Checkpoint 接在 Save Slot 的 Checkpoint pin。刪除舊 Make 節點，重新建立 Save Slot 並接回原本 Exec、Target、Slot Name、Return Value；把想保存的進度先寫回 ST_GameProgress / Custom Game Data。此 BP 目前 BS_ERROR，其餘 9 個 BS_UP_TO_DATE。沒有自動儲存或修改使用者正在編輯的資產。
2. **DA_GameDatabase**：目前 Inventory 只有 money=100，Equipment 卻有 body=coat。資料驗證要求穿戴物品必須持有；如要保留初始穿戴，Inventory 加 coat=1，否則移除初始 Equipment 的 body 項目。未修改該資產。這造成第一輪整合測試在初始化退出；功能測試改為使用獨立定義，避免受使用者編輯中的資料干擾。
3. WBP_YarnDebugHUD 仍有既有 /Script/YarnSpinnerTest linker warning，但 Blueprint 本身編譯成功。

上述兩項完成前，不宣稱原專案已可直接 Play。沒有進行實際 UI 遊玩或整個遊戲 cook/package。

## 交付與證據

- 完整外掛：原專案 Saved/Pkg/Migration；新版 Blueprint consumer：Saved/MigrationConsumer。
- 變更前檔案備份：Saved/SaveMigrationBackup。舊 .sav 與 Content 未刪除。
- Perforce pending CL 48，未提交；使用者原本 default changelist 保留。
- [104 項主測試](Validation/SaveMigrationProject104.json)
- [66 項獨立 consumer](Validation/SaveMigrationConsumer66.json)
- [原 Blueprint 狀態（含失敗）](Validation/SaveMigrationBlueprints.json)
- [原始碼及 Content 雜湊摘要](Validation/SaveMigrationSources.json)

完整 logs：Saved/Logs/MigrationPluginBuild.log、SharedGameDataTests.log、MigrationBlueprints.log、MigrationDefaults.log；consumer 的 Saved/Logs/SharedGameDataTests.log。

建置期間修正了一次 UHT 介面預設實作宣告；負面測試改用非抽象、未實作介面的物件，避免測試本身觸發 UObject ensure。Python 設定反射改用原生 SaveDataVersion 名稱。最後結果以上表為準，沒有把早期失敗當成功。

[Blueprint 換版教學](SaveMigration.md) · [流程圖](SaveSystemArchitecture.md)
