"""Compile existing assets without saving them; verify new reflected API. Runtime tests cover BP struct disk I/O."""
import json, os
from datetime import datetime, timezone
import unreal
required_types = ["GameVariablesSubsystem", "GameDatabaseSubsystem", "GameSaveSubsystem",
    "GameDataBlueprintLibrary", "SharedYarnVariableStorage", "SharedYarnDialogueRunner",
    "GameSaveMigrationHandler", "GameProgressSave", "GameDatabaseDefinition", "GameDataRecord", "GameDatabaseSnapshot"]
for name in required_types:
    if not hasattr(unreal, name): raise RuntimeError("Missing reflected API: " + name)
if not unreal.load_class(None, "/Script/YarnSpinnerGameData.YarnDatabaseBridge"):
    raise RuntimeError("Missing internal Yarn bridge UClass")
removed_types = ["GameCheckpoint", "PersistentActorComponent", "WorldSaveRegistrySubsystem", "GameWorldStateSubsystem", "GameSaveParticipant"]
for name in removed_types:
    if hasattr(unreal, name): raise RuntimeError("Obsolete API still present: " + name)
for name in ["register_save_migration_handler", "unregister_save_migration_handler", "get_save_data_version"]:
    if not hasattr(unreal.GameSaveSubsystem, name): raise RuntimeError("Missing save API: " + name)
if hasattr(unreal.GameSaveSubsystem, "current_checkpoint"):
    raise RuntimeError("Obsolete CurrentCheckpoint remains")
settings = unreal.get_default_object(unreal.load_class(None, "/Script/YarnSpinnerGameData.SharedGameDataSettings"))
if settings.get_editor_property("SaveDataVersion") < 1: raise RuntimeError("Invalid save version")
checked = []
statuses = {}
for path in globals().get("HOST_BLUEPRINTS", []):
    asset = unreal.load_asset(path)
    if not asset: raise RuntimeError("Cannot load Blueprint: " + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    checked.append(path)
    statuses[path] = str(asset.get_editor_property("Status"))
output = os.path.join(unreal.Paths.project_saved_dir(), "Tests", "SharedGameDataBlueprints.json")
os.makedirs(os.path.dirname(output), exist_ok=True)
with open(output, "w", encoding="utf-8") as f:
    json.dump({"timestamp_utc": datetime.now(timezone.utc).isoformat(), "reflected_types": required_types,
        "removed_types": removed_types, "attempted_blueprints": checked, "blueprint_statuses": statuses,
        "assets_saved": False, "note": "Check process exit and current compiler log as well."}, f, indent=2)
unreal.log("SHARED_DATA_BLUEPRINT_API_VERIFIED")
failed = [path for path, status in statuses.items() if "error" in status.lower()]
if failed: raise RuntimeError("Blueprints need updated connections: " + ", ".join(failed))
