#pragma once

#include "CoreMinimal.h"
#include "YarnVariableStorage.h"
#include "SharedYarnVariableStorage.generated.h"

class UGameVariablesSubsystem;
class UYarnDatabaseBridge;

/** Per-runner context; all persistent values remain in GameDatabaseSubsystem. */
UCLASS(BlueprintType)
class YARNSPINNERGAMEDATA_API USharedYarnVariableStorage : public UObject, public IYarnVariableStorage
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UYarnDatabaseBridge> DatabaseBridge;
    void Initialize(UGameVariablesSubsystem* InVariables);
    virtual void SetString_Implementation(const FString& Name, const FString& Value) override;
    virtual void SetNumber_Implementation(const FString& Name, float Value) override;
    virtual void SetBool_Implementation(const FString& Name, bool Value) override;
    virtual void SetValue_Implementation(const FString& Name, const FYarnValue& Value) override;
    virtual bool TryGetValue_Implementation(const FString& Name, FYarnValue& OutValue) override;
    virtual bool Contains_Implementation(const FString& Name) override;
    virtual void Clear_Implementation() override;
    virtual void SetYarnProject(UYarnProject* Project) override;
    virtual IYarnSmartVariableEvaluator* GetSmartVariableEvaluator() const override;
    virtual void SetSmartVariableEvaluator(IYarnSmartVariableEvaluator* Evaluator, UObject* Owner = nullptr) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UGameVariablesSubsystem> Variables;
    TWeakObjectPtr<UYarnProject> YarnProject;
    TWeakObjectPtr<UObject> EvaluatorOwner;
    IYarnSmartVariableEvaluator* SmartEvaluator = nullptr;
    bool bProjectReady = false;
    void ReportWrite(bool bSuccess, const FString& Name) const;
};
