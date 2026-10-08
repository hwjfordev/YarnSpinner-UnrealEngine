#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "YarnDialoguePresenter.h"
#include "SharedGameDataTestCommandlet.generated.h"

/** Internal headless integration fixture, not a gameplay presenter. */
UCLASS(NotBlueprintable, Transient)
class USharedGameDataTestPresenter : public UYarnDialoguePresenter
{
    GENERATED_BODY()
public:
    bool bLinePending = false;
    bool bOptionsPending = false;
    FYarnOptionSet CurrentOptions;
    int32 LineCount = 0;
    virtual void RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry) override
    { ++LineCount; bLinePending = true; }
    virtual void RunOptions_Implementation(const FYarnOptionSet& Options) override
    { CurrentOptions = Options; bOptionsPending = true; }
    virtual void OnDialogueComplete_Implementation() override
    { bLinePending = bOptionsPending = false; }
};

UCLASS(Transient)
class USharedGameDataTestObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 Changes = 0;
    int32 Replacements = 0;
    int32 SaveCompletions = 0;
    int32 LoadCompletions = 0;
    bool bLastSuccess = false;
    UFUNCTION() void Changed(const FString& Name) { ++Changes; }
    UFUNCTION() void Replaced() { ++Replacements; }
    UFUNCTION() void Saved(const FString& Slot, bool bSuccess, const FString& Error)
    { ++SaveCompletions; bLastSuccess = bSuccess; }
    UFUNCTION() void Loaded(const FString& Slot, bool bSuccess, const FString& Error)
    { ++LoadCompletions; bLastSuccess = bSuccess; }
};

UCLASS()
class USharedGameDataTestCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USharedGameDataTestCommandlet();
    virtual int32 Main(const FString& Params) override;
};
