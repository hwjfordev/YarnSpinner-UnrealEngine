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

#include "YarnNodeReferenceCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "YarnNodeReference.h"
#include "YarnProgram.h"

#define LOCTEXT_NAMESPACE "YarnNodeReference"

TSharedRef<IPropertyTypeCustomization> FYarnNodeReferenceCustomization::MakeInstance()
{
	return MakeShared<FYarnNodeReferenceCustomization>();
}

void FYarnNodeReferenceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ProjectHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FYarnNodeReference, YarnProject));
	NodeNameHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FYarnNodeReference, NodeName));

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text(this, &FYarnNodeReferenceCustomization::GetSelectedNodeText)
			.ToolTipText(this, &FYarnNodeReferenceCustomization::GetNodeTooltipText)
			.Font(CustomizationUtils.GetRegularFont())
		];
}

void FYarnNodeReferenceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (!ProjectHandle.IsValid() || !NodeNameHandle.IsValid())
	{
		return;
	}

	ChildBuilder.AddProperty(ProjectHandle.ToSharedRef());

	RefreshNodeOptions();

	ProjectHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FYarnNodeReferenceCustomization::RefreshNodeOptions));

	ChildBuilder.AddCustomRow(LOCTEXT("NodeRowFilter", "Node"))
		.NameContent()
		[
			NodeNameHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&NodeOptions)
			.OnGenerateWidget_Lambda([this](TSharedPtr<FString> Option)
			{
				return MakeNodeOptionWidget(Option);
			})
			.OnSelectionChanged(this, &FYarnNodeReferenceCustomization::OnNodeSelected)
			.ToolTipText(this, &FYarnNodeReferenceCustomization::GetNodeTooltipText)
			[
				SNew(STextBlock)
				.Text(this, &FYarnNodeReferenceCustomization::GetSelectedNodeText)
				.Font(CustomizationUtils.GetRegularFont())
			]
		];
}

void FYarnNodeReferenceCustomization::RefreshNodeOptions()
{
	NodeOptions.Reset();

	if (!ProjectHandle.IsValid())
	{
		return;
	}

	UObject* ProjectObject = nullptr;
	ProjectHandle->GetValue(ProjectObject);

	const UYarnProject* Project = Cast<UYarnProject>(ProjectObject);
	if (!Project)
	{
		return;
	}

	TArray<FString> Names = Project->NodeNames;
	if (Names.Num() == 0)
	{
		Project->Program.Nodes.GenerateKeyArray(Names);
	}
	Names.Sort();

	for (const FString& Name : Names)
	{
		NodeOptions.Add(MakeShared<FString>(Name));
	}
}

FText FYarnNodeReferenceCustomization::GetSelectedNodeText() const
{
	if (!NodeNameHandle.IsValid())
	{
		return LOCTEXT("NoNode", "None");
	}

	FString NodeName;
	NodeNameHandle->GetValue(NodeName);

	if (NodeName.IsEmpty())
	{
		return LOCTEXT("NoNode", "None");
	}

	return FText::FromString(NodeName);
}

FText FYarnNodeReferenceCustomization::GetNodeTooltipText() const
{
	if (!ProjectHandle.IsValid())
	{
		return FText::GetEmpty();
	}

	UObject* ProjectObject = nullptr;
	ProjectHandle->GetValue(ProjectObject);

	if (!Cast<UYarnProject>(ProjectObject))
	{
		return LOCTEXT("PickProjectFirst", "Choose a Yarn project to list its nodes.");
	}

	FString NodeName;
	if (NodeNameHandle.IsValid())
	{
		NodeNameHandle->GetValue(NodeName);
	}

	const UYarnProject* Project = Cast<UYarnProject>(ProjectObject);
	if (!NodeName.IsEmpty() && Project && !Project->HasNode(NodeName))
	{
		return FText::Format(LOCTEXT("MissingNode", "'{0}' is not a node in this project."), FText::FromString(NodeName));
	}

	return LOCTEXT("PickNode", "The node this reference points at.");
}

void FYarnNodeReferenceCustomization::OnNodeSelected(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (!Selection.IsValid() || !NodeNameHandle.IsValid())
	{
		return;
	}

	NodeNameHandle->SetValue(*Selection);
}

TSharedRef<SWidget> FYarnNodeReferenceCustomization::MakeNodeOptionWidget(TSharedPtr<FString> Option) const
{
	return SNew(STextBlock).Text(FText::FromString(Option.IsValid() ? *Option : FString()));
}

#undef LOCTEXT_NAMESPACE
