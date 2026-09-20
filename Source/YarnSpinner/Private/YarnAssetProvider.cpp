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

#include "YarnAssetProvider.h"

#include "Engine/AssetManager.h"
#include "YarnProgram.h"
#include "YarnSpinnerModule.h"

void UYarnProjectAssetProvider::SetAssetContext(UYarnProject* InYarnProject, const FString& InLocaleCode)
{
	YarnProject = InYarnProject;
	LocaleCode = InLocaleCode;
}

FString UYarnProjectAssetProvider::FindTaggedAssetPath(const FString& LineID) const
{
	if (!YarnProject || MetadataPrefix.IsEmpty())
	{
		return FString();
	}

	const FString* Metadata = YarnProject->LineMetadata.Find(LineID);
	if (!Metadata)
	{
		return FString();
	}

	const FString Prefix = MetadataPrefix + TEXT(":");

	TArray<FString> Tags;
	Metadata->ParseIntoArray(Tags, TEXT(" "));
	for (const FString& Tag : Tags)
	{
		if (Tag.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			return Tag.Mid(Prefix.Len());
		}
	}

	return FString();
}

FString UYarnProjectAssetProvider::MakeLocalisedAssetPath(const FString& LineID) const
{
	if (!YarnProject)
	{
		return FString();
	}

	FString Locale = LocaleCode;
	if (Locale.IsEmpty())
	{
		Locale = YarnProject->BaseLanguage;
	}

	const FYarnLocalization* Localization = YarnProject->Localizations.Find(Locale);
	if (!Localization && Locale.Len() > 2)
	{
		Localization = YarnProject->Localizations.Find(Locale.Left(2));
	}

	if (!Localization || Localization->AssetsPath.IsEmpty())
	{
		return FString();
	}

	if (!Localization->AssetsPath.StartsWith(TEXT("/")))
	{
		UE_LOG(LogYarnSpinner, Verbose, TEXT("YarnProjectAssetProvider: assets path '%s' is not a content path (expected e.g. /Game/Audio/VO) - skipping localised asset lookup"),
			*Localization->AssetsPath);
		return FString();
	}

	FString AssetName = LineID;
	AssetName.RemoveFromStart(TEXT("line:"));

	return FString::Printf(TEXT("%s/%s"), *Localization->AssetsPath, *AssetName);
}

UObject* UYarnProjectAssetProvider::LoadAssetAtPath(const FString& AssetPath, UClass* AssetClass) const
{
	if (AssetPath.IsEmpty() || !AssetClass)
	{
		return nullptr;
	}

	return StaticLoadObject(AssetClass, nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

UObject* UYarnProjectAssetProvider::GetAssetForLine_Implementation(const FYarnLocalizedLine& Line, UClass* AssetClass)
{
	if (!AssetClass)
	{
		return nullptr;
	}

	const FString LineID = Line.RawLine.LineID;

	if (UObject* Tagged = LoadAssetAtPath(FindTaggedAssetPath(LineID), AssetClass))
	{
		return Tagged;
	}

	const FString EffectiveLineID = Line.ShadowSourceLineID.IsEmpty() ? LineID : Line.ShadowSourceLineID;
	if (UObject* Localised = LoadAssetAtPath(MakeLocalisedAssetPath(EffectiveLineID), AssetClass))
	{
		return Localised;
	}

	if (!Line.ShadowSourceLineID.IsEmpty())
	{
		if (UObject* ShadowTagged = LoadAssetAtPath(FindTaggedAssetPath(Line.ShadowSourceLineID), AssetClass))
		{
			return ShadowTagged;
		}
	}

	return nullptr;
}

void UYarnProjectAssetProvider::PrepareAssetsForLines_Implementation(const TArray<FString>& LineIDs, UClass* AssetClass)
{
	if (!YarnProject)
	{
		return;
	}

	TArray<FSoftObjectPath> AssetsToLoad;
	for (const FString& LineID : LineIDs)
	{
		const FString TaggedPath = FindTaggedAssetPath(LineID);
		if (!TaggedPath.IsEmpty())
		{
			AssetsToLoad.Add(FSoftObjectPath(TaggedPath));
		}

		const FString LocalisedPath = MakeLocalisedAssetPath(LineID);
		if (!LocalisedPath.IsEmpty())
		{
			AssetsToLoad.Add(FSoftObjectPath(LocalisedPath));
		}
	}

	if (AssetsToLoad.Num() > 0)
	{
		PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(AssetsToLoad);
	}
}
