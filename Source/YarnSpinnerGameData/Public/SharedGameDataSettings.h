#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "YarnProgram.h"
#include "GameDataTypes.h"
#include "GameDatabaseTypes.h"
#include "SharedGameDataSettings.generated.h"

/** Initial values are loaded once per game session, before any level actor is needed. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Shared Game Data"))
class YARNSPINNERGAMEDATA_API USharedGameDataSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }

    UPROPERTY(Config, EditAnywhere, Category="Defaults")
    TArray<TSoftObjectPtr<UYarnProject>> DefaultYarnProjects;

    /** Extra variables owned by gameplay. Names use the same $name convention as Yarn. */
    UPROPERTY(Config, EditAnywhere, Category="Defaults")
    FGameVariableSnapshot GameplayDefaults;

    /** NPC/record DataTable, item definitions, equipment and custom progress defaults. */
    UPROPERTY(Config, EditAnywhere, Category="Database")
    TSoftObjectPtr<UGameDatabaseDefinition> DatabaseDefinition;

    /** Written into new saves. Mismatches require a registered migration handler at load time. */
    UPROPERTY(Config, EditAnywhere, Category="Save", meta=(ClampMin="1", UIMin="1"))
    int32 SaveDataVersion = 1;
};
