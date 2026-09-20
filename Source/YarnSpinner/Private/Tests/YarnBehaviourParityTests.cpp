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

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tests/AutomationCommon.h"
#include "YarnBlueprintLibrary.h"
#include "YarnDialogueRunner.h"
#include "YarnLocalization.h"
#include "YarnMarkup.h"
#include "YarnProgram.h"
#include "YarnVariableStorage.h"

namespace
{
	struct FPluralCase
	{
		const TCHAR* Locale;
		const TCHAR* Value;
		const TCHAR* ExpectedCase;
	};

	const TCHAR* PluralMarkupTemplate = TEXT("[plural value=VALUE zero=ZERO one=ONE two=TWO few=FEW many=MANY other=OTHER /]");
	const TCHAR* OrdinalMarkupTemplate = TEXT("[ordinal value=VALUE zero=ZERO one=ONE two=TWO few=FEW many=MANY other=OTHER /]");

	FString RunMarkupCase(const TCHAR* Template, const FPluralCase& Case)
	{
		const FString Markup = FString(Template).Replace(TEXT("VALUE"), Case.Value, ESearchCase::CaseSensitive);
		return UYarnMarkupLibrary::ParseMarkupFull(Markup, Case.Locale, false).Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnPluralRulesTest, "YarnSpinner.Markup.CardinalPluralRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnPluralRulesTest::RunTest(const FString& Parameters)
{
	const FPluralCase Cases[] = {
		{ TEXT("en"), TEXT("1"), TEXT("ONE") },
		{ TEXT("en"), TEXT("0"), TEXT("OTHER") },
		{ TEXT("en"), TEXT("1.5"), TEXT("OTHER") },
		{ TEXT("pt"), TEXT("0"), TEXT("ONE") },
		{ TEXT("fr"), TEXT("0.5"), TEXT("ONE") },
		{ TEXT("es"), TEXT("1"), TEXT("ONE") },
		{ TEXT("kn"), TEXT("0"), TEXT("ONE") },
		{ TEXT("mr"), TEXT("0"), TEXT("OTHER") },
		{ TEXT("is"), TEXT("21"), TEXT("ONE") },
		{ TEXT("he"), TEXT("2"), TEXT("TWO") },
		{ TEXT("mt"), TEXT("3"), TEXT("FEW") },
		{ TEXT("mk"), TEXT("11"), TEXT("OTHER") },
		{ TEXT("br"), TEXT("13"), TEXT("OTHER") },
		{ TEXT("ru"), TEXT("2"), TEXT("FEW") },
		{ TEXT("cs"), TEXT("1.5"), TEXT("MANY") },
		{ TEXT("da"), TEXT("1.5"), TEXT("ONE") },
		{ TEXT("lv"), TEXT("0.1"), TEXT("ONE") },
		{ TEXT("cy"), TEXT("6"), TEXT("MANY") },
		{ TEXT("ga"), TEXT("7"), TEXT("MANY") },
		{ TEXT("gd"), TEXT("11"), TEXT("ONE") },
		{ TEXT("sl"), TEXT("102"), TEXT("TWO") },
		{ TEXT("ja"), TEXT("1"), TEXT("OTHER") },
		{ TEXT("pl"), TEXT("2"), TEXT("FEW") },
		{ TEXT("ar"), TEXT("3"), TEXT("FEW") },
		{ TEXT("en"), TEXT("-1"), TEXT("ONE") },
		{ TEXT("en"), TEXT("-2"), TEXT("OTHER") },
		{ TEXT("ru"), TEXT("-2"), TEXT("FEW") },
		{ TEXT("pl"), TEXT("-21"), TEXT("MANY") },
		{ TEXT("is"), TEXT("-21"), TEXT("ONE") },
		{ TEXT("fr"), TEXT("-0.5"), TEXT("ONE") },
	};

	for (const FPluralCase& Case : Cases)
	{
		TestEqual(FString::Printf(TEXT("cardinal %s %s"), Case.Locale, Case.Value),
			RunMarkupCase(PluralMarkupTemplate, Case), FString(Case.ExpectedCase));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnOrdinalRulesTest, "YarnSpinner.Markup.OrdinalPluralRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnOrdinalRulesTest::RunTest(const FString& Parameters)
{
	const FPluralCase Cases[] = {
		{ TEXT("en"), TEXT("1"), TEXT("ONE") },
		{ TEXT("en"), TEXT("2"), TEXT("TWO") },
		{ TEXT("en"), TEXT("3"), TEXT("FEW") },
		{ TEXT("en"), TEXT("11"), TEXT("OTHER") },
		{ TEXT("en"), TEXT("21"), TEXT("ONE") },
		{ TEXT("sv"), TEXT("11"), TEXT("OTHER") },
		{ TEXT("sv"), TEXT("12"), TEXT("OTHER") },
		{ TEXT("mk"), TEXT("7"), TEXT("MANY") },
		{ TEXT("cy"), TEXT("0"), TEXT("ZERO") },
		{ TEXT("uk"), TEXT("23"), TEXT("FEW") },
		{ TEXT("it"), TEXT("11"), TEXT("MANY") },
		{ TEXT("gd"), TEXT("12"), TEXT("TWO") },
		{ TEXT("hu"), TEXT("5"), TEXT("ONE") },
		{ TEXT("pt"), TEXT("1"), TEXT("OTHER") },
		{ TEXT("en"), TEXT("-1"), TEXT("ONE") },
		{ TEXT("en"), TEXT("-2"), TEXT("TWO") },
		{ TEXT("mk"), TEXT("-1"), TEXT("ONE") },
	};

	for (const FPluralCase& Case : Cases)
	{
		TestEqual(FString::Printf(TEXT("ordinal %s %s"), Case.Locale, Case.Value),
			RunMarkupCase(OrdinalMarkupTemplate, Case), FString(Case.ExpectedCase));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnLineProviderTest, "YarnSpinner.Localisation.BuiltinLineProvider",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnLineProviderTest::RunTest(const FString& Parameters)
{
	UYarnProject* Project = NewObject<UYarnProject>();
	Project->BaseLanguage = TEXT("en");
	Project->BaseStringTable.Add(TEXT("line:hello"), TEXT("Mae: Hello there"));
	Project->BaseStringTable.Add(TEXT("line:count"), TEXT("You have {0} apples"));
	Project->BaseStringTable.Add(TEXT("line:source"), TEXT("The shadowed text"));
	Project->BaseStringTable.Add(TEXT("line:shadow"), FString());
	Project->LineMetadata.Add(TEXT("line:hello"), TEXT("greeting lastline"));
	Project->LineMetadata.Add(TEXT("line:shadow"), TEXT("shadow:source"));

	FYarnLocalization French;
	French.Strings.Add(TEXT("line:hello"), TEXT("Mae: Bonjour"));
	Project->Localizations.Add(TEXT("fr"), French);

	UYarnBuiltinLineProvider* Provider = NewObject<UYarnBuiltinLineProvider>();
	Provider->SetYarnProject(Project);
	Provider->SetLocaleCode(TEXT("en"));

	{
		const FYarnLocalizedLine Line = Provider->GetLocalizedLine(FYarnLine(TEXT("line:hello")));
		TestEqual(TEXT("base text is used"), Line.Text.ToString(), FString(TEXT("Mae: Hello there")));
		TestEqual(TEXT("the character name is pulled out"), Line.CharacterName, FString(TEXT("Mae")));
		TestTrue(TEXT("metadata comes through"), Line.Metadata.Contains(TEXT("greeting")));
	}

	{
		TArray<FString> Substitutions;
		Substitutions.Add(TEXT("3"));
		const FYarnLocalizedLine Line = Provider->GetLocalizedLine(FYarnLine(TEXT("line:count"), Substitutions));
		TestEqual(TEXT("substitutions are expanded"), Line.Text.ToString(), FString(TEXT("You have 3 apples")));
	}

	{
		const FYarnLocalizedLine Line = Provider->GetLocalizedLine(FYarnLine(TEXT("line:shadow")));
		TestEqual(TEXT("a shadow line uses its source's text"), Line.Text.ToString(), FString(TEXT("The shadowed text")));
		TestEqual(TEXT("and remembers where that text came from"), Line.ShadowSourceLineID, FString(TEXT("line:source")));
	}

	{
		Provider->SetLocaleCode(TEXT("fr"));
		const FYarnLocalizedLine Line = Provider->GetLocalizedLine(FYarnLine(TEXT("line:hello")));
		TestEqual(TEXT("a localised string wins over base text"), Line.Text.ToString(), FString(TEXT("Mae: Bonjour")));

		const FYarnLocalizedLine Untranslated = Provider->GetLocalizedLine(FYarnLine(TEXT("line:count")));
		TestEqual(TEXT("an untranslated line falls back to base text"), Untranslated.Text.ToString(), FString(TEXT("You have {0} apples")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVariableSaveLoadTest, "YarnSpinner.VariableStorage.SaveAndLoad",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVariableSaveLoadTest::RunTest(const FString& Parameters)
{
	const FString SlotName = TEXT("YarnSpinnerAutomationTestSlot");

	UYarnDialogueRunner* Runner = NewObject<UYarnDialogueRunner>();
	UYarnInMemoryVariableStorage* Storage = NewObject<UYarnInMemoryVariableStorage>();
	Runner->VariableStorage = TScriptInterface<IYarnVariableStorage>(Storage);

	IYarnVariableStorage::Execute_SetValue(Storage, TEXT("$gold"), FYarnValue(42.0f));
	IYarnVariableStorage::Execute_SetValue(Storage, TEXT("$name"), FYarnValue(FString(TEXT("Mae"))));
	IYarnVariableStorage::Execute_SetValue(Storage, TEXT("$met"), FYarnValue(true));

	TestTrue(TEXT("variables save"), UYarnBlueprintLibrary::SaveVariablesToSlot(Runner, SlotName, 0));
	TestTrue(TEXT("the slot exists afterwards"), UYarnBlueprintLibrary::DoesVariableSlotExist(SlotName, 0));

	IYarnVariableStorage::Execute_Clear(Storage);
	FYarnValue Cleared;
	TestFalse(TEXT("clearing really does clear"), IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$gold"), Cleared));

	TestTrue(TEXT("variables load"), UYarnBlueprintLibrary::LoadVariablesFromSlot(Runner, SlotName, 0));

	FYarnValue Gold;
	TestTrue(TEXT("the number came back"), IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$gold"), Gold));
	TestEqual(TEXT("with its value"), Gold.ConvertToNumber(), 42.0f);

	FYarnValue Name;
	TestTrue(TEXT("the string came back"), IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$name"), Name));
	TestEqual(TEXT("with its value"), Name.ConvertToString(), FString(TEXT("Mae")));

	FYarnValue Met;
	TestTrue(TEXT("the bool came back"), IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$met"), Met));
	TestTrue(TEXT("with its value"), Met.ConvertToBool());

	TestTrue(TEXT("the slot deletes"), UYarnBlueprintLibrary::DeleteVariableSlot(SlotName, 0));
	TestFalse(TEXT("and is gone"), UYarnBlueprintLibrary::DoesVariableSlotExist(SlotName, 0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnFormatFunctionTest, "YarnSpinner.Functions.Format",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnFormatFunctionTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	AActor* Actor = WorldWrapper.GetTestWorld()->SpawnActor<AActor>();
	if (!Actor)
	{
		AddError(TEXT("could not spawn an actor in the test world"));
		return false;
	}

	UYarnInMemoryVariableStorage* Storage = NewObject<UYarnInMemoryVariableStorage>(Actor);
	Storage->RegisterComponent();

	UYarnDialogueRunner* Runner = NewObject<UYarnDialogueRunner>(Actor);
	Runner->VariableStorage = TScriptInterface<IYarnVariableStorage>(Storage);
	Runner->RegisterComponent();

	auto RunFormat = [this, Runner, Storage](const FString& FormatString, float Value) -> FString
	{
		UYarnProject* Project = NewObject<UYarnProject>();
		FYarnNode Node;
		Node.Name = TEXT("Start");
		Node.Instructions.Add(FYarnInstruction::PushString(FormatString));
		Node.Instructions.Add(FYarnInstruction::PushFloat(Value));
		Node.Instructions.Add(FYarnInstruction::PushFloat(2.0f));
		Node.Instructions.Add(FYarnInstruction::CallFunction(TEXT("format")));
		Node.Instructions.Add(FYarnInstruction::StoreVariable(TEXT("$formatted")));
		Node.Instructions.Add(FYarnInstruction::Pop());
		Node.Instructions.Add(FYarnInstruction::Stop());
		Project->Program.Nodes.Add(Node.Name, Node);

		Runner->SetYarnProject(Project);
		Runner->StartDialogue(TEXT("Start"));

		FYarnValue Result;
		IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$formatted"), Result);
		return Result.ConvertToString();
	};

	TestEqual(TEXT("plain substitution"), RunFormat(TEXT("{0}"), 1.0f), FString(TEXT("1")));
	TestEqual(TEXT("decimals are kept"), RunFormat(TEXT("{0}"), 3.14159f), FString(TEXT("3.14159")));
	TestEqual(TEXT("fixed-point rounds to even"), RunFormat(TEXT("{0:F2}"), 0.125f), FString(TEXT("0.12")));
	TestEqual(TEXT("fixed-point rounds to even again"), RunFormat(TEXT("{0:F2}"), 0.375f), FString(TEXT("0.38")));
	TestEqual(TEXT("whole numbers keep their decimals"), RunFormat(TEXT("{0:F2}"), 7.0f), FString(TEXT("7.00")));
	TestEqual(TEXT("large numbers use scientific notation"), RunFormat(TEXT("{0}"), 1e9f), FString(TEXT("1E+09")));
	TestEqual(TEXT("small numbers use scientific notation"), RunFormat(TEXT("{0}"), 1e-5f), FString(TEXT("1E-05")));
	TestEqual(TEXT("text around the marker is kept"), RunFormat(TEXT("You have {0} apples"), 3.0f), FString(TEXT("You have 3 apples")));

	WorldWrapper.DestroyTestWorld(true);
	return true;
}

#endif
