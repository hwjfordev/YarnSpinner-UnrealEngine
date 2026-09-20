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

#pragma once

#include "CoreMinimal.h"
#include "YarnDialoguePresenter.h"
#include "YarnTestPlanPresenter.generated.h"

UENUM()
enum class EYarnTestPlanEventKind : uint8
{
	Line,
	Options,
	Command,
	Stop
};

USTRUCT()
struct FYarnTestPlanEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EYarnTestPlanEventKind Kind = EYarnTestPlanEventKind::Line;

	UPROPERTY()
	FYarnLocalizedLine Line;

	UPROPERTY()
	FYarnOptionSet Options;

	UPROPERTY()
	FString CommandText;
};

UCLASS(Transient, NotBlueprintable, HideDropdown)
class UYarnTestPlanPresenter : public UYarnDialoguePresenter
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FYarnTestPlanEvent> Events;

	bool bCommandPending = false;

	bool HasPendingLine() const { return CurrentLineFinishedCallback.IsBound(); }

	bool HasPendingOptions() const { return CurrentOptionSelectedCallback.IsBound(); }

	void ResetForRun()
	{
		Events.Reset();
		bCommandPending = false;
		CurrentLineFinishedCallback.Unbind();
		CurrentOptionSelectedCallback.Unbind();
	}

	UFUNCTION()
	void HandleUnhandledCommand(const FString& CommandText)
	{
		FYarnTestPlanEvent& Event = Events.AddDefaulted_GetRef();
		Event.Kind = EYarnTestPlanEventKind::Command;
		Event.CommandText = CommandText;
		bCommandPending = true;
	}

	virtual void RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry) override
	{
		FYarnTestPlanEvent& Event = Events.AddDefaulted_GetRef();
		Event.Kind = EYarnTestPlanEventKind::Line;
		Event.Line = Line;
	}

	virtual void RunOptions_Implementation(const FYarnOptionSet& Options) override
	{
		FYarnTestPlanEvent& Event = Events.AddDefaulted_GetRef();
		Event.Kind = EYarnTestPlanEventKind::Options;
		Event.Options = Options;
	}

	virtual void OnDialogueComplete_Implementation() override
	{
		CurrentLineFinishedCallback.Unbind();
		CurrentOptionSelectedCallback.Unbind();
		bCommandPending = false;
		FYarnTestPlanEvent& Event = Events.AddDefaulted_GetRef();
		Event.Kind = EYarnTestPlanEventKind::Stop;
	}
};
