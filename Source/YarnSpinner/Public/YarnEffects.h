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
#include "Containers/Ticker.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YarnCancellationToken.h"
#include "YarnEffects.generated.h"

class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnEffectFinished);

 /**
 * Base class for the teh widget effects below. Each effect runs from the core
 * ticker, finishes early when its cancellation token is cancelled or a hurry
 * up is requested, and always leaves the widget in its finished state!
 */
UCLASS(Abstract)
class YARNSPINNER_API UYarnWidgetEffect : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires when the effect finishes, whether it ran to completion or was hurried. */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Effects")
	FOnYarnEffectFinished OnFinished;

	virtual void Activate() override;

	virtual void SetReadyToDestroy() override;

protected:
	UPROPERTY()
	TObjectPtr<UWidget> TargetWidget;

	FYarnLineCancellationToken CancellationToken;

	float Duration = 0.0f;
	float Elapsed = 0.0f;

	/** Applies the effect at the given progress,from 0 to 1. */
	virtual void ApplyProgress(float Alpha) PURE_VIRTUAL(UYarnWidgetEffect::ApplyProgress, );

	bool Tick(float DeltaTime);

	void Finish();

private:
	FTSTicker::FDelegateHandle TickerHandle;
};

 /**
 * Fades a widget's render opacity from one value to another...
 */
UCLASS()
class YARNSPINNER_API UYarnFadeWidgetEffect : public UYarnWidgetEffect
{
	GENERATED_BODY()

public:
	 /**
	 * Fade a widget's render opacity over time.
	 * @param Widget The widget to fade.
	 * @param From The opacity to start from.
	 * @param To The opacity to end at.
	 * @param Duration How long the fade takes, in seconds. Zero applies it immediately.
	 * @param CancellationToken Cancelling this token finishes the fade early.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Effects", meta = (BlueprintInternalUseOnly = "true"))
	static UYarnFadeWidgetEffect* YarnFadeWidget(UWidget* Widget, float From, float To, float Duration, FYarnLineCancellationToken CancellationToken);

	/** Fade a widget in from fully transparent. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Effects", meta = (BlueprintInternalUseOnly = "true"))
	static UYarnFadeWidgetEffect* YarnFadeWidgetIn(UWidget* Widget, float Duration, FYarnLineCancellationToken CancellationToken);

	/** Fade a widget out to fully transparent. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Effects", meta = (BlueprintInternalUseOnly = "true"))
	static UYarnFadeWidgetEffect* YarnFadeWidgetOut(UWidget* Widget, float Duration, FYarnLineCancellationToken CancellationToken);

protected:
	virtual void ApplyProgress(float Alpha) override;

private:
	float From = 0.0f;
	float To = 1.0f;
};

 /**
 * Scales a widget up and back down again.
 */
UCLASS()
class YARNSPINNER_API UYarnPunchScaleEffect : public UYarnWidgetEffect
{
	GENERATED_BODY()

public:
	 /**
	 * Scale a widget up and back to its original size.
	 * @param Widget The widget to scale.
	 * @param Strength How far past its original size the widget grows.
	 * @param Duration How long the whole PUNCH! (think Italian Spiderman) takes, in seconds.
	 * @param CancellationToken Cancelling this token finishes the punch early.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Effects", meta = (BlueprintInternalUseOnly = "true"))
	static UYarnPunchScaleEffect* YarnPunchScale(UWidget* Widget, float Strength, float Duration, FYarnLineCancellationToken CancellationToken);

protected:
	virtual void ApplyProgress(float Alpha) override;

private:
	float Strength = 0.2f;
};

 /**
 * Shakes a widget from side to side!
 */
UCLASS()
class YARNSPINNER_API UYarnShakeEffect : public UYarnWidgetEffect
{
	GENERATED_BODY()

public:
	 /**
	 * Shake a widget around its resting position.
	 * @param Widget The widget to shake.
	 * @param Strength How far the widget moves, in slate units.
	 * @param Duration How long the shake lasts, in seconds.
	 * @param Frequency How many times per second the widget changes direction.
	 * @param CancellationToken Cancelling this token finishes the shake early.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Effects", meta = (BlueprintInternalUseOnly = "true"))
	static UYarnShakeEffect* YarnShake(UWidget* Widget, float Strength, float Duration, float Frequency, FYarnLineCancellationToken CancellationToken);

protected:
	virtual void ApplyProgress(float Alpha) override;

private:
	float Strength = 5.0f;
	float Frequency = 20.0f;
};

 /**
 * Immediate versions of the effects above, for code that wants to set a
 * widget's state without waiting. Aw yeah!
 */
UCLASS()
class YARNSPINNER_API UYarnEffectsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Set a widget's render opacity right now. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Effects")
	static void SetWidgetOpacity(UWidget* Widget, float Opacity);

	/** Set a widget's render scale right now. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Effects")
	static void SetWidgetScale(UWidget* Widget, float Scale);

	/** Move a widget away from its resting position right now. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Effects")
	static void SetWidgetOffset(UWidget* Widget, FVector2D Offset);
};
