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

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// our own header - must be included first for unreal header tool
#include "YarnVoiceOverPresenter.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

// we need the dialogue runner to access the line provider and cancellation tokens.
// presenters communicate with the runner to signal when they're done.
#include "YarnDialogueRunner.h"

#include "YarnLocalization.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

// logging macros (UE_LOG with LogYarnSpinner category)
#include "YarnSpinnerModule.h"

// FTimerManager for SetTimer/ClearTimer - we use timers for pre/post delays
// and to defer operations to avoid recursion.
#include "TimerManager.h"

// UWorld - needed to access GetTimerManager() since timer manager is per-world.
// also used to check if the world is valid before setting timers.
#include "Engine/World.h"

// AActor - GetOwner() returns an AActor, we need this for the audio component
// attachment and to find existing audio components on the owning actor.
#include "GameFramework/Actor.h"

// ----------------------------------------------------------------------------
// constructor
// ----------------------------------------------------------------------------

UYarnVoiceOverPresenter::UYarnVoiceOverPresenter()
{
	// we need ticking for fade-out and playback completion detection, but we
	// don't want it running all the time - only when we're actually playing
	// audio or fading out. so we enable tick capability but start disabled.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ----------------------------------------------------------------------------
// lifecycle
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::BeginPlay()
{
	Super::BeginPlay();

	// make sure we have an audio component ready to go before dialogue starts.
	// this avoids any hitches when the first line comes through.
	EnsureAudioComponent();
}

void UYarnVoiceOverPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// this is critical for preventing crashes. if we have timers pending when
	// the component is destroyed (level transition, actor destruction, etc),
	// those timers would fire and try to call methods on a dead object.
	// we clear them here to be safe.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreStartTimerHandle);
		World->GetTimerManager().ClearTimer(PostCompleteTimerHandle);
	}

	// stop any audio that's currently playing. we don't fade out here because
	// the component is being destroyed anyway - just cut it immediately.
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
	}

	// reset state flags so if this component somehow gets reused (pooling),
	// it starts fresh.
	bIsPlaying = false;
	bIsFadingOut = false;

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------------
// tick - used for fade-out animation and detecting playback completion
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// handle the fade-out animation when audio is interrupted. we gradually
	// reduce the volume over FadeOutTimeOnInterrupt seconds to avoid an
	// abrupt audio cut which sounds jarring.
	if (bIsFadingOut && AudioComponent)
	{
		if (FadeOutTimeOnInterrupt > 0.0f)
		{
			// advance the fade progress based on elapsed time
			FadeOutProgress += DeltaTime / FadeOutTimeOnInterrupt;

			if (FadeOutProgress >= 1.0f)
			{
				// fade complete - clamp to 1, restore original volume setting
				// (so it's correct for the next clip), stop playback, and
				// disable ticking since we're done.
				FadeOutProgress = 1.0f;
				bIsFadingOut = false;

				AudioComponent->SetVolumeMultiplier(OriginalVolume);
				AudioComponent->Stop();

				SetComponentTickEnabled(false);
				CompleteLine();
			}
			else
			{
				// still fading - interpolate volume from original down to zero.
				// this gives us a smooth fade-out curve.
				float NewVolume = FMath::Lerp(OriginalVolume, 0.0f, FadeOutProgress);
				AudioComponent->SetVolumeMultiplier(NewVolume);
			}
		}
		else
		{
			// fade time is zero, so just cut immediately. this is useful when
			// you want instant interruption without any fade.
			bIsFadingOut = false;
			AudioComponent->Stop();
			SetComponentTickEnabled(false);
			CompleteLine();
		}
	}
	else if (bIsPlaying && AudioComponent && !AudioComponent->IsPlaying())
	{
		OnAudioFinished();
	}
}

// ----------------------------------------------------------------------------
// dialogue presenter interface implementation
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::OnDialogueStarted_Implementation()
{
	// nothing to do here - we set up the audio component in BeginPlay and
	// actual playback starts when RunLine is called. subclasses can override
	// this if they need to do setup when dialogue begins.
}

void UYarnVoiceOverPresenter::OnDialogueComplete_Implementation()
{
	// dialogue has ended, so clean up any audio that might still be playing.
	// this handles the case where dialogue is stopped mid-line.
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
	}

	// reset all state
	bIsPlaying = false;
	bIsFadingOut = false;
	SetComponentTickEnabled(false);

	// clear any pending timers. this is important because if we had a
	// pre-start delay timer pending, we don't want it to fire after
	// dialogue has ended.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreStartTimerHandle);
		World->GetTimerManager().ClearTimer(PostCompleteTimerHandle);
	}
}

void UYarnVoiceOverPresenter::RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry)
{
	// store the current line so we can reference it later (for logging, etc)
	CurrentLine = Line;

	// make sure we have a valid audio component before trying to play anything
	EnsureAudioComponent();

	// if there's already audio playing from a previous line, stop it. this
	// shouldn't normally happen if the dialogue flow is working correctly,
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
	}
	bIsFadingOut = false;
	PendingClip = nullptr;

	// get the audio clip for this line. this calls the virtual function which
	// can be overridden in blueprints or c++ subclasses to provide custom
	// audio lookup logic.
	USoundBase* VoiceOverClip = GetVoiceOverClip(Line);

	if (!VoiceOverClip)
	{
		// no audio for this line - that's fine, not all lines need voice over.
		// log a warning so developers know, then optionally advance to the
		// next line if configured to do so.
		UE_LOG(LogYarnSpinner, Warning, TEXT("VoiceOverPresenter: No audio clip for line '%s'"), *Line.RawLine.LineID);

		if (bEndLineWhenVoiceOverComplete)
		{
			OnLinePresentationComplete();
		}
		return;
	}

	// start playback, optionally with a delay. the delay is useful for
	if (WaitTimeBeforeStart > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			PendingClip = VoiceOverClip;
			World->GetTimerManager().SetTimer(
				PreStartTimerHandle,
				this,
				&UYarnVoiceOverPresenter::StartPendingPlayback,
				WaitTimeBeforeStart,
				false  // don't loop
			);
		}
	}
	else
	{
		// no delay - start immediately
		StartPlayback(VoiceOverClip);
	}
}

// ----------------------------------------------------------------------------
// audio clip lookup
// ----------------------------------------------------------------------------

USoundBase* UYarnVoiceOverPresenter::GetVoiceOverClip_Implementation(const FYarnLocalizedLine& Line)
{
	// the asset provider finds clips either via a line's metadata
	// tag, teh localisation's assets folder, or whatever a game supplies in
	// its place... override this in your subclass or blueprint or whatever 
	// to do something else for voice over!
	if (UObject* ProviderObject = GetAssetProviderObject())
	{
		return Cast<USoundBase>(
			IYarnAssetProvider::Execute_GetAssetForLine(ProviderObject, Line, USoundBase::StaticClass()));
	}

	return nullptr;
}

UObject* UYarnVoiceOverPresenter::GetAssetProviderObject() const
{
	UYarnDialogueRunner* Runner = GetDialogueRunner();
	if (!Runner)
	{
		return nullptr;
	}

	return Runner->AssetProvider.GetObject();
}

FString UYarnVoiceOverPresenter::MakeLocalizedClipAssetPath(const FString& LineID) const
{
	if (UYarnProjectAssetProvider* Provider = Cast<UYarnProjectAssetProvider>(GetAssetProviderObject()))
	{
		return Provider->MakeLocalisedAssetPath(LineID);
	}

	return FString();
}

USoundBase* UYarnVoiceOverPresenter::ResolveClipFromLocalizedAssetsPath(const FString& LineID)
{
	const FString AssetPath = MakeLocalizedClipAssetPath(LineID);
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	return Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
}

void UYarnVoiceOverPresenter::OnPrepareForLines_Implementation(const TArray<FString>& LineIDs)
{
	// preload the audio for the upcoming lines so playback starts without a
	// synchronous load hitch when each line runs.
	if (UObject* ProviderObject = GetAssetProviderObject())
	{
		IYarnAssetProvider::Execute_PrepareAssetsForLines(ProviderObject, LineIDs, USoundBase::StaticClass());
	}
}

// ----------------------------------------------------------------------------
// audio component management
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::EnsureAudioComponent()
{
	if (!AudioComponent)
	{
		if (GetOwner())
		{
			AudioComponent = NewObject<UAudioComponent>(GetOwner(), NAME_None, RF_Transient);

			// configure for dialogue playback - we don't want it to auto-play
			// or auto-destroy, and we default to 2d (non-spatialised) audio
			// since dialogue is usually played at full volume regardless of
			// listener position.
			AudioComponent->bAutoActivate = false;
			AudioComponent->bAutoDestroy = false;
			AudioComponent->bAllowSpatialization = false;

			// register and attach to the actor so it moves with them
			AudioComponent->RegisterComponent();
			AudioComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}

// ----------------------------------------------------------------------------
// playback control
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::StartPendingPlayback()
{
	USoundBase* Clip = PendingClip;
	PendingClip = nullptr;
	StartPlayback(Clip);
}

void UYarnVoiceOverPresenter::OnNextLineRequested_Implementation()
{
	if (bIsPlaying && !bIsFadingOut && FadeOutTimeOnInterrupt > 0.0f)
	{
		BeginFadeOut();
		return;
	}

	if (bIsFadingOut)
	{
		return;
	}

	Super::OnNextLineRequested_Implementation();
}

void UYarnVoiceOverPresenter::StartPlayback(USoundBase* AudioClip)
{
	// safety check - if we don't have what we need, just complete the line
	if (!AudioComponent || !AudioClip)
	{
		CompleteLine();
		return;
	}

	// set the sound and start playing
	AudioComponent->SetSound(AudioClip);
	AudioComponent->Play();

	// update state - we're now playing, not fading
	bIsPlaying = true;
	bIsFadingOut = false;

	// enable ticking so we can detect when playback finishes
	SetComponentTickEnabled(true);

	// notify listeners that playback has started
	OnVoiceOverStarted.Broadcast();

	UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverPresenter: Playing audio for line '%s'"), *CurrentLine.RawLine.LineID);
}

void UYarnVoiceOverPresenter::OnAudioFinished()
{
	// audio finished playing naturally (not interrupted)
	bIsPlaying = false;
	SetComponentTickEnabled(false);

	UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverPresenter: Audio finished for line '%s'"), *CurrentLine.RawLine.LineID);

	// apply a post-complete delay if configured. this gives a natural pause
	// after the voice over finishes before moving to the next line.
	if (WaitTimeAfterComplete > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				PostCompleteTimerHandle,
				this,
				&UYarnVoiceOverPresenter::CompleteLine,
				WaitTimeAfterComplete,
				false  // don't loop
			);
		}
	}
	else
	{
		// no delay - complete immediately
		CompleteLine();
	}
}

void UYarnVoiceOverPresenter::CompleteLine()
{
	// notify listeners that voice over is complete (whether it finished
	// naturally or was interrupted and faded out)
	OnVoiceOverComplete.Broadcast();

	if (!bEndLineWhenVoiceOverComplete)
	{
		// We're not driving line completion - let another presenter (or
		// manual input) decide when to advance. Existing behaviour.
		return;
	}

	// We are driving. The question is whether our completion came because
	// the audio reached its natural end (we drove the line) or because
	// someone cancelled us mid-play (the line was already ending). The
	// token tells us: if it's already cancelled, we were a passenger;
	// otherwise, we're the reason the line is over.
	if (CurrentLineCancellationToken.IsCancellationRequested())
	{
		OnLinePresentationComplete();
	}
	else
	{
		OnLinePresentationCompleteAndEndLine();
	}
}

void UYarnVoiceOverPresenter::BeginFadeOut()
{
	// called when the current line is interrupted (e.g., player skipped ahead).
	// instead of abruptly cutting the audio, we fade it out smoothly.

	if (!AudioComponent)
	{
		// no audio component means nothing to fade - just complete
		CompleteLine();
		return;
	}

	// store the current volume so we can restore it after the fade (for the
	// next clip) and as the starting point for the fade interpolation.
	OriginalVolume = AudioComponent->VolumeMultiplier;

	// reset progress and start the fade
	FadeOutProgress = 0.0f;
	bIsFadingOut = true;

	// enable ticking so the fade animation runs
	SetComponentTickEnabled(true);

	UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverPresenter: Beginning fade-out for interrupted line"));
}
