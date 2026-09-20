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
#include "IPropertyTypeCustomization.h"

class FYarnNodeReferenceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	TSharedPtr<IPropertyHandle> ProjectHandle;
	TSharedPtr<IPropertyHandle> NodeNameHandle;
	TArray<TSharedPtr<FString>> NodeOptions;

	void RefreshNodeOptions();
	FText GetSelectedNodeText() const;
	FText GetNodeTooltipText() const;
	void OnNodeSelected(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> MakeNodeOptionWidget(TSharedPtr<FString> Option) const;
};
