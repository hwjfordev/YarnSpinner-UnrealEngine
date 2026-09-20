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
#include "Engine/StreamableManager.h"
#include "UObject/Interface.h"
#include "YarnSpinnerCore.h"
#include "YarnAssetProvider.generated.h"

class UYarnProject;

UINTERFACE(Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnAssetProvider : public UInterface
{
	GENERATED_BODY()
};

 /**
 * Finds asset that goes with a line. Could be a voice over clip, a portrait, a
 * subtitle track, whatever, anything keyed off the line...
 *
 * Implement this to change how assets are located without touching the
 * presenters that use them! Magic.
 */
class YARNSPINNER_API IYarnAssetProvider
{
	GENERATED_BODY()

public:
	 /**
	 * Find the asset of the given class for a line.
	 * @param Line The line being presented.
	 * @param AssetClass The kind of asset wanted, for example Sound Base.
	 * @return The asset, or nothing if this line has none.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Assets")
	UObject* GetAssetForLine(const FYarnLocalizedLine& Line, UClass* AssetClass);

	 /**
	 * Start loading the assets for lines that are coming up, so they are ready
	 * when those lines run.
	 * @param LineIDs The lines that are about to run.
	 * @param AssetClass The kind of asset wanted.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Assets")
	void PrepareAssetsForLines(const TArray<FString>& LineIDs, UClass* AssetClass);
};

 /**
 * The asset provider used unless a project supplies its own...
 *
 * It looks for an asset in three places, in thiss order: a metadata tag on line
 * naming an asset path (`#audio:/Game/VO/Hello`), the localisation's assets
 * folder for the current locale, and, for a shadow line, the same two places
 * for the line it shadows...
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnProjectAssetProvider : public UObject, public IYarnAssetProvider
{
	GENERATED_BODY()

public:
	 /**
	 * The metadata tag that names an asset path for a line, without the colon!!
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Assets")
	FString MetadataPrefix = TEXT("audio");

	 /**
	 * Point the provider at a project and locale.. The dialogue runner does
	 * this for you when the dialogue starts!
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Assets")
	void SetAssetContext(UYarnProject* InYarnProject, const FString& InLocaleCode);

	 /**
	 * Build the path an asset would live at for a line.. or an empty string if
	 * the project has no no assets folder for the current locale.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Assets")
	FString MakeLocalisedAssetPath(const FString& LineID) const;

	virtual UObject* GetAssetForLine_Implementation(const FYarnLocalizedLine& Line, UClass* AssetClass) override;
	virtual void PrepareAssetsForLines_Implementation(const TArray<FString>& LineIDs, UClass* AssetClass) override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UYarnProject> YarnProject;

	UPROPERTY(Transient)
	FString LocaleCode;

	FString FindTaggedAssetPath(const FString& LineID) const;

	UObject* LoadAssetAtPath(const FString& AssetPath, UClass* AssetClass) const;

private:
	TSharedPtr<FStreamableHandle> PreloadHandle;
};
