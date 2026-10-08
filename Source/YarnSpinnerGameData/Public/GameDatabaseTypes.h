#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameDataTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "GameDatabaseTypes.generated.h"

/** An actor-independent record. Field names have one stable type within a record. */
USTRUCT(BlueprintType)
struct YARNSPINNERGAMEDATA_API FGameDataRecord : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Database") TMap<FName, float> Numbers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Database") TMap<FName, bool> Bools;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Database") TMap<FName, FString> Strings;
};

USTRUCT(BlueprintType)
struct YARNSPINNERGAMEDATA_API FGameItemDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items") FText DisplayName;
    /** Empty means this item cannot be equipped. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items") TArray<FName> EquipmentSlots;
};

/** The single mutable session database, independent of Actors and levels. */
USTRUCT(BlueprintType)
struct YARNSPINNERGAMEDATA_API FGameDatabaseSnapshot
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Database") TMap<FName, FGameDataRecord> Records;
    /** Includes equipped units. Equipping does not consume inventory. Zero stacks are omitted. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Database") TMap<FName, int32> Inventory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Database") TMap<FName, FName> Equipment;
    /** Editable in snapshot copies for migration; live data remains private to the subsystems. */
    UPROPERTY(BlueprintReadWrite, SaveGame, Category="Database") FGameVariableSnapshot YarnVariables;
    /** Project-owned Blueprint struct: chapters, quest arrays, calendar, etc. Value data only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Database") FInstancedStruct CustomGameData;
};

/** Author defaults in a Data Asset. The runtime takes a value copy; this asset is never mutated. */
UCLASS(BlueprintType)
class YARNSPINNERGAMEDATA_API UGameDatabaseDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    /** Row type: GameDataRecord. Row name is the stable record/NPC ID. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Database", meta=(RequiredAssetDataTags="RowStructure=/Script/YarnSpinnerGameData.GameDataRecord")) TSoftObjectPtr<UDataTable> DefaultRecords;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Database") TMap<FName, FGameItemDefinition> Items;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Database") TArray<FName> EquipmentSlots;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Database") FName MoneyItemID = TEXT("money");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Database") FGameDatabaseSnapshot InitialState;
};
