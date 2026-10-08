#include "SharedYarnDialogueRunner.h"
#include "GameDataBlueprintLibrary.h"

void USharedYarnDialogueRunner::BeginPlay()
{
    FString Error;
    bSharedStorageReady = UGameDataBlueprintLibrary::ConnectRunnerToSharedVariables(this, Error);
    if (!bSharedStorageReady)
    {
        bAutoStart = false;
        UE_LOG(LogTemp, Error, TEXT("Shared Yarn runner cannot initialize: %s"), *Error);
        // Mark the component begun, but do not initialize a VM backed by accidental local state.
        UActorComponent::BeginPlay();
        return;
    }
    Super::BeginPlay();
}
