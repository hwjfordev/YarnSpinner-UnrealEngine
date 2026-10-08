#pragma once

#include "CoreMinimal.h"
#include "GameDataTypes.generated.h"

/** Serializable values only. Computed Yarn smart variables are evaluated by the runner. */
USTRUCT(BlueprintType)
struct YARNSPINNERGAMEDATA_API FGameVariableSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Game Data")
    TMap<FString, float> Numbers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Game Data")
    TMap<FString, FString> Strings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Game Data")
    TMap<FString, bool> Bools;
};

