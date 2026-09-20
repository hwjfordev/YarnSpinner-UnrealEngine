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

#include "YarnEffects.h"

#include "Components/Widget.h"
#include "Containers/Ticker.h"

void UYarnWidgetEffect::Activate()
{
	if (!TargetWidget)
	{
		Finish();
		return;
	}

	if (Duration <= 0.0f)
	{
		ApplyProgress(1.0f);
		Finish();
		return;
	}

	Elapsed = 0.0f;
	ApplyProgress(0.0f);

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UYarnWidgetEffect::Tick));
}

bool UYarnWidgetEffect::Tick(float DeltaTime)
{
	if (!TargetWidget || CancellationToken.IsAnyCancellationRequested())
	{
		ApplyProgress(1.0f);
		Finish();
		return false;
	}

	Elapsed += DeltaTime;

	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
	ApplyProgress(Alpha);

	if (Alpha >= 1.0f)
	{
		Finish();
		return false;
	}

	return true;
}

void UYarnWidgetEffect::Finish()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	OnFinished.Broadcast();
	SetReadyToDestroy();
}

void UYarnWidgetEffect::SetReadyToDestroy()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	Super::SetReadyToDestroy();
}

UYarnFadeWidgetEffect* UYarnFadeWidgetEffect::YarnFadeWidget(UWidget* Widget, float From, float To, float Duration, FYarnLineCancellationToken CancellationToken)
{
	UYarnFadeWidgetEffect* Effect = NewObject<UYarnFadeWidgetEffect>();
	Effect->TargetWidget = Widget;
	Effect->From = From;
	Effect->To = To;
	Effect->Duration = Duration;
	Effect->CancellationToken = CancellationToken;
	return Effect;
}

UYarnFadeWidgetEffect* UYarnFadeWidgetEffect::YarnFadeWidgetIn(UWidget* Widget, float Duration, FYarnLineCancellationToken CancellationToken)
{
	return YarnFadeWidget(Widget, 0.0f, 1.0f, Duration, CancellationToken);
}

UYarnFadeWidgetEffect* UYarnFadeWidgetEffect::YarnFadeWidgetOut(UWidget* Widget, float Duration, FYarnLineCancellationToken CancellationToken)
{
	return YarnFadeWidget(Widget, 1.0f, 0.0f, Duration, CancellationToken);
}

void UYarnFadeWidgetEffect::ApplyProgress(float Alpha)
{
	if (TargetWidget)
	{
		TargetWidget->SetRenderOpacity(FMath::Lerp(From, To, Alpha));
	}
}

UYarnPunchScaleEffect* UYarnPunchScaleEffect::YarnPunchScale(UWidget* Widget, float Strength, float Duration, FYarnLineCancellationToken CancellationToken)
{
	UYarnPunchScaleEffect* Effect = NewObject<UYarnPunchScaleEffect>();
	Effect->TargetWidget = Widget;
	Effect->Strength = Strength;
	Effect->Duration = Duration;
	Effect->CancellationToken = CancellationToken;
	return Effect;
}

void UYarnPunchScaleEffect::ApplyProgress(float Alpha)
{
	if (!TargetWidget)
	{
		return;
	}

	const float Curve = FMath::Sin(Alpha * PI);
	const float Scale = 1.0f + Strength * Curve;
	TargetWidget->SetRenderScale(FVector2D(Scale, Scale));
}

UYarnShakeEffect* UYarnShakeEffect::YarnShake(UWidget* Widget, float Strength, float Duration, float Frequency, FYarnLineCancellationToken CancellationToken)
{
	UYarnShakeEffect* Effect = NewObject<UYarnShakeEffect>();
	Effect->TargetWidget = Widget;
	Effect->Strength = Strength;
	Effect->Duration = Duration;
	Effect->Frequency = Frequency;
	Effect->CancellationToken = CancellationToken;
	return Effect;
}

void UYarnShakeEffect::ApplyProgress(float Alpha)
{
	if (!TargetWidget)
	{
		return;
	}

	if (Alpha >= 1.0f)
	{
		TargetWidget->SetRenderTranslation(FVector2D::ZeroVector);
		return;
	}

	const float Falloff = 1.0f - Alpha;
	const float Offset = FMath::Sin(Alpha * Duration * Frequency * 2.0f * PI) * Strength * Falloff;
	TargetWidget->SetRenderTranslation(FVector2D(Offset, 0.0f));
}

void UYarnEffectsLibrary::SetWidgetOpacity(UWidget* Widget, float Opacity)
{
	if (Widget)
	{
		Widget->SetRenderOpacity(Opacity);
	}
}

void UYarnEffectsLibrary::SetWidgetScale(UWidget* Widget, float Scale)
{
	if (Widget)
	{
		Widget->SetRenderScale(FVector2D(Scale, Scale));
	}
}

void UYarnEffectsLibrary::SetWidgetOffset(UWidget* Widget, FVector2D Offset)
{
	if (Widget)
	{
		Widget->SetRenderTranslation(Offset);
	}
}
