#pragma once

#include "CoreMinimal.h"
#include "YarnDialogueRunner.h"
#include "SharedYarnDialogueRunner.generated.h"

/** Drop-in runner component that installs shared storage before the base runner initializes. */
UCLASS(ClassGroup=(YarnSpinner), Blueprintable, meta=(BlueprintSpawnableComponent))
class YARNSPINNERGAMEDATA_API USharedYarnDialogueRunner : public UYarnDialogueRunner
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    UPROPERTY(BlueprintReadOnly, Category="Game Data|Yarn")
    bool bSharedStorageReady = false;
};
