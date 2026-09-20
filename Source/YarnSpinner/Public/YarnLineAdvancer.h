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
#include "InputCoreTypes.h"
#include "YarnDialoguePresenter.h"
#include "YarnLineAdvancer.generated.h"

class UYarnDialogueRunner;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnAdvanceRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnHurryUpRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnOptionHurryUpRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnDialogueCancellationRequested);

 /**
 * How the line advancer learns that the player wants to move on.
 */
UENUM(BlueprintType)
enum class EYarnLineAdvancerInput : uint8
{
	/** Poll the keys configured below. */
	Keys UMETA(DisplayName = "Keys"),
	/** Nothing is polled; call the Request functions yourself. */
	Manual UMETA(DisplayName = "Manual")
};

 /**
 * Turns player input into requests to hurry up, advance, or cancel dialogue...
 *
 * This is a presenter which means it itt can see where each line/options block starts
 * and ends, but it never delays: it reports itself finished as soon as
 * content arrives and leaves teh actual presentation to the other presenters..
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnLineAdvancer : public UYarnDialoguePresenter
{
	GENERATED_BODY()

public:
	UYarnLineAdvancer();

	/** The dialogue runner that receives the requests. Found on this actor if left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer")
	TObjectPtr<UYarnDialogueRunner> DialogueRunnerOverride;

	/** While false, input is ignored and no requests are made. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer")
	bool bInputEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer")
	EYarnLineAdvancerInput InputMode = EYarnLineAdvancerInput::Keys;

	 /**
	 * When true one control both hurries up a line that is still appearing
	 * and advances aline that has finished. When false, hurrying up and
	 * advancing have separate keys...
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer")
	bool bCombineHurryUpAndAdvance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer", meta = (EditCondition = "InputMode == EYarnLineAdvancerInput::Keys"))
	FKey AdvanceKey = EKeys::SpaceBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer", meta = (EditCondition = "InputMode == EYarnLineAdvancerInput::Keys && !bCombineHurryUpAndAdvance"))
	FKey HurryUpKey = EKeys::LeftShift;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer", meta = (EditCondition = "InputMode == EYarnLineAdvancerInput::Keys"))
	FKey OptionHurryUpKey = EKeys::SpaceBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer", meta = (EditCondition = "InputMode == EYarnLineAdvancerInput::Keys"))
	FKey CancelDialogueKey;

	 /**
	 * Asking to advance this many times during one line cancels the dialogue.
	 * Zero turns this off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer", meta = (ClampMin = "0"))
	int32 AdvanceRequestsBeforeCancelling = 0;

	 /**
	 * Ignore input on the same frame that content arrives, so one key press
	 * can't both choose an option and hurry up the line that follows it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Line Advancer")
	bool bIgnoreInputOnContentFrame = true;

	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Line Advancer")
	FOnYarnAdvanceRequested OnAdvanceRequested;

	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Line Advancer")
	FOnYarnHurryUpRequested OnLineHurryUpRequested;

	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Line Advancer")
	FOnYarnOptionHurryUpRequested OnOptionHurryUpRequested;

	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Line Advancer")
	FOnYarnDialogueCancellationRequested OnDialogueCancellationRequested;

	 /**
	 * Ask the current line to finish appearing immediately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Line Advancer")
	void RequestLineHurryUp();

	 /**
	 * Ask for the next piece of content. Hurries the current line up first if
	 * it is still appearing and the two controls are combined.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Line Advancer")
	void RequestNextLine();

	 /**
	 * Ask the options that are currently showing to finish appearing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Line Advancer")
	void RequestOptionHurryUp();

	 /**
	 * Stop the dialogue where it is.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Line Advancer")
	void RequestDialogueCancellation();

	/** True while a line is being presented. */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Line Advancer")
	bool IsPresentingLine() const { return bLineIsPresenting; }

	/** True while options are being presented. */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Line Advancer")
	bool IsPresentingOptions() const { return bOptionsArePresenting; }

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual bool CanHandleOptions() const override { return false; }

	virtual void RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry) override;
	virtual void RunOptions_Implementation(const FYarnOptionSet& Options) override;
	virtual void OnDialogueComplete_Implementation() override;

private:
	bool bLineIsPresenting = false;
	bool bOptionsArePresenting = false;
	int32 AdvancesThisLine = 0;
	uint64 FrameContentReceived = 0;

	bool bAdvanceWasPressed = false;
	bool bHurryUpWasPressed = false;
	bool bOptionHurryUpWasPressed = false;
	bool bCancelWasPressed = false;

	UYarnDialogueRunner* ResolveRunner();
	bool IsKeyPressed(const FKey& Key) const;
	bool ConsumeKeyPress(const FKey& Key, bool& bWasPressed) const;
	bool ShouldIgnoreInputThisFrame() const;
};
