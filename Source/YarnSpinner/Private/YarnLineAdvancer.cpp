// ============================================================================
//
//  Yarn Spinner for Unreal Engine
//
//  Copyright (c) Yarn Spinner Pty. Ltd. All Rights Reserved.
//
//  Yarn Spinner is a trademark of Secret Lab Pty. Ltd., used under license.
//
//  This code is subject to the terms and conditions of the license found in
//  the LICENSE.md file in the root of this repository.
//
//  For help, support, and more information, visit:
//    https://yarnspinner.dev
//    https://docs.yarnspinner.dev
//
// ============================================================================

#include "YarnLineAdvancer.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "YarnDialogueRunner.h"
#include "YarnSpinnerModule.h"

UYarnLineAdvancer::UYarnLineAdvancer()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UYarnLineAdvancer::BeginPlay()
{
	Super::BeginPlay();
	ResolveRunner();
}

UYarnDialogueRunner* UYarnLineAdvancer::ResolveRunner()
{
	if (DialogueRunnerOverride)
	{
		return DialogueRunnerOverride;
	}

	if (AActor* Owner = GetOwner())
	{
		DialogueRunnerOverride = Owner->FindComponentByClass<UYarnDialogueRunner>();
	}

	if (!DialogueRunnerOverride)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnLineAdvancer: no dialogue runner set, and none found on %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("this actor"));
	}

	return DialogueRunnerOverride;
}

bool UYarnLineAdvancer::IsKeyPressed(const FKey& Key) const
{
	if (!Key.IsValid())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (const APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		return PlayerController->IsInputKeyDown(Key);
	}

	return false;
}

bool UYarnLineAdvancer::ConsumeKeyPress(const FKey& Key, bool& bWasPressed) const
{
	const bool bIsDown = IsKeyPressed(Key);
	const bool bJustPressed = bIsDown && !bWasPressed;
	bWasPressed = bIsDown;
	return bJustPressed;
}

bool UYarnLineAdvancer::ShouldIgnoreInputThisFrame() const
{
	return bIgnoreInputOnContentFrame && GFrameCounter == FrameContentReceived;
}

void UYarnLineAdvancer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInputEnabled || InputMode != EYarnLineAdvancerInput::Keys)
	{
		return;
	}

	const bool bAdvancePressed = ConsumeKeyPress(AdvanceKey, bAdvanceWasPressed);
	const bool bHurryUpPressed = bCombineHurryUpAndAdvance ? false : ConsumeKeyPress(HurryUpKey, bHurryUpWasPressed);
	const bool bOptionHurryUpPressed = ConsumeKeyPress(OptionHurryUpKey, bOptionHurryUpWasPressed);
	const bool bCancelPressed = ConsumeKeyPress(CancelDialogueKey, bCancelWasPressed);

	if (ShouldIgnoreInputThisFrame())
	{
		return;
	}

	if (bCancelPressed)
	{
		RequestDialogueCancellation();
		return;
	}

	if (bOptionsArePresenting)
	{
		if (bOptionHurryUpPressed)
		{
			RequestOptionHurryUp();
		}
		return;
	}

	if (bHurryUpPressed)
	{
		RequestLineHurryUp();
	}

	if (bAdvancePressed)
	{
		RequestNextLine();
	}
}

void UYarnLineAdvancer::RequestLineHurryUp()
{
	if (!bInputEnabled)
	{
		return;
	}

	OnLineHurryUpRequested.Broadcast();

	if (UYarnDialogueRunner* Runner = ResolveRunner())
	{
		Runner->RequestHurryUp();
	}
}

void UYarnLineAdvancer::RequestNextLine()
{
	if (!bInputEnabled)
	{
		return;
	}

	UYarnDialogueRunner* Runner = ResolveRunner();
	if (!Runner)
	{
		return;
	}

	if (bCombineHurryUpAndAdvance && bLineIsPresenting && !Runner->IsHurryUpRequested())
	{
		OnLineHurryUpRequested.Broadcast();
		Runner->RequestHurryUp();
		return;
	}

	++AdvancesThisLine;

	if (AdvanceRequestsBeforeCancelling > 0 && AdvancesThisLine >= AdvanceRequestsBeforeCancelling)
	{
		RequestDialogueCancellation();
		return;
	}

	OnAdvanceRequested.Broadcast();
	Runner->RequestNextLine();
}

void UYarnLineAdvancer::RequestOptionHurryUp()
{
	if (!bInputEnabled)
	{
		return;
	}

	OnOptionHurryUpRequested.Broadcast();

	if (UYarnDialogueRunner* Runner = ResolveRunner())
	{
		Runner->RequestHurryUpOption();
	}
}

void UYarnLineAdvancer::RequestDialogueCancellation()
{
	if (!bInputEnabled)
	{
		return;
	}

	OnDialogueCancellationRequested.Broadcast();

	if (UYarnDialogueRunner* Runner = ResolveRunner())
	{
		Runner->StopDialogue();
	}

	bLineIsPresenting = false;
	bOptionsArePresenting = false;
	AdvancesThisLine = 0;
}

void UYarnLineAdvancer::RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry)
{
	bLineIsPresenting = true;
	bOptionsArePresenting = false;
	AdvancesThisLine = 0;
	FrameContentReceived = GFrameCounter;

	OnLinePresentationComplete();
}

void UYarnLineAdvancer::RunOptions_Implementation(const FYarnOptionSet& Options)
{
	bOptionsArePresenting = true;
	bLineIsPresenting = false;
	FrameContentReceived = GFrameCounter;
}

void UYarnLineAdvancer::OnDialogueComplete_Implementation()
{
	bLineIsPresenting = false;
	bOptionsArePresenting = false;
	AdvancesThisLine = 0;
}
