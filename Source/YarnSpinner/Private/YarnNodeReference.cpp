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

#include "YarnNodeReference.h"

#include "YarnProgram.h"

bool FYarnNodeReference::IsValid() const
{
	return YarnProject != nullptr && !NodeName.IsEmpty() && YarnProject->HasNode(NodeName);
}

bool UYarnNodeReferenceLibrary::IsNodeReferenceValid(const FYarnNodeReference& Reference)
{
	return Reference.IsValid();
}

FString UYarnNodeReferenceLibrary::GetNodeReferenceName(const FYarnNodeReference& Reference)
{
	return Reference.NodeName;
}
