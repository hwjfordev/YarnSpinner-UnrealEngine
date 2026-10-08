# 外掛差異

game-data-module-integration.patch 保存模組登錄、建置相依、unity 設定、套件篩選與文件入口的上游整合變更。新增的 YarnSpinnerGameData 模組、Editor GameData 測試、Tests/CheckpointDatabase.*、Config、Scripts、Docs 以完整原始檔保存。

checkpoint-vm-stop.patch：YarnVirtualMachine.Continue 在 RunInstruction callback 後檢查 CurrentNode / Stopped / Error，防止 handler 呼叫 StopDialogue 清除 node 後被再次存取。此問題由 money_remove 扣款失敗的真實 VM 整合測試重現，修正後已納入測試。

重新取得上游前保留整個工作目錄。不要只搬 patch，遗漏新模組；保留原有相容性修正與所有第三方授權。

