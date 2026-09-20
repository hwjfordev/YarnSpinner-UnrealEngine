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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YarnNodeReference.generated.h"

class UYarnProject;

 /**
 * A node in a Yarn projj, picked from a list in the editor! Like Unity!
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnNodeReference
{
	GENERATED_BODY()

	/** The project the node lives in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	TObjectPtr<UYarnProject> YarnProject;

	/** The name of the node, as it appears in the script. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FString NodeName;

	FYarnNodeReference() = default;

	FYarnNodeReference(UYarnProject* InYarnProject, const FString& InNodeName)
		: YarnProject(InYarnProject)
		, NodeName(InNodeName)
	{
	}

	/** True when the project is set and actually contains this node. */
	bool IsValid() const;
};

UCLASS()
class YARNSPINNER_API UYarnNodeReferenceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True when the project is set and actually contains this node. */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner")
	static bool IsNodeReferenceValid(const FYarnNodeReference& Reference);

	/** The node's name, or an empty string if the reference is not set. */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner")
	static FString GetNodeReferenceName(const FYarnNodeReference& Reference);
};
