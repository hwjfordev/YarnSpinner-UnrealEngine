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

#include "YarnMarkup.h"
#include "YarnProgram.h"
#include "YarnUnicodeNormalization.h"

namespace
{
	struct FExpectedAttribute
	{
		FString Name;
		int32 Position = 0;
		int32 Length = 0;
	};

	struct FMarkupCase
	{
		FString Input;
		FString ExpectedText;
		bool bAddImplicitCharacter = false;
		TArray<FExpectedAttribute> Attributes;
	};

	FString FromCodepoints(const TArray<uint32>& Codepoints)
	{
		FString Result;
		for (uint32 Codepoint : Codepoints)
		{
			if (Codepoint > 0xFFFF && sizeof(TCHAR) == 2)
			{
				const uint32 Adjusted = Codepoint - 0x10000;
				Result.AppendChar(static_cast<TCHAR>(0xD800 + (Adjusted >> 10)));
				Result.AppendChar(static_cast<TCHAR>(0xDC00 + (Adjusted & 0x3FF)));
				continue;
			}
			Result.AppendChar(static_cast<TCHAR>(Codepoint));
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnMarkupCoreParityTest, "YarnSpinner.Markup.CoreParity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnMarkupCoreParityTest::RunTest(const FString& Parameters)
{
	const FString AcuteAccent = FromCodepoints({ 0x301 });
	const FString Smiley = FromCodepoints({ 0x1F642 });

	TArray<FMarkupCase> Cases;
	Cases.Add({ TEXT("this is line without markup"), TEXT("this is line without markup"), false, {} });
	Cases.Add({ TEXT("[a]this is line with basic markup[/a]"), TEXT("this is line with basic markup"), false,
		{ { TEXT("a"), 0, 30 } } });
	Cases.Add({ TEXT("[a]this is line with [b]nested basic[/b] markup[/a]"), TEXT("this is line with nested basic markup"), false,
		{ { TEXT("a"), 0, 37 }, { TEXT("b"), 18, 12 } } });
	Cases.Add({ TEXT("this is a[nomarkup] line with [b]nomarkup hiding[/b] markup[/nomarkup] elements"),
		TEXT("this is a line with [b]nomarkup hiding[/b] markup elements"), false,
		{ { TEXT("nomarkup"), 9, 40 } } });
	Cases.Add({ TEXT("[a]This is [b]some [c]markup[/b] with[/c] closing tag issues inside a valid tag[/a]"),
		TEXT("This is some markup with closing tag issues inside a valid tag"), false,
		{ { TEXT("a"), 0, 62 }, { TEXT("b"), 8, 11 }, { TEXT("c"), 13, 11 } } });
	Cases.Add({ TEXT("this is a line with non-replacement[a/]  markup"), TEXT("this is a line with non-replacement markup"), false,
		{ { TEXT("a"), 35, 0 } } });
	Cases.Add({ TEXT("this is a line with \\[markup=false var = 12]two [markup2]markup[/] inside of it"),
		TEXT("this is a line with [markup=false var = 12]two markup inside of it"), false,
		{ { TEXT("markup2"), 47, 6 } } });
	Cases.Add({ TEXT("Mae: hello there"), TEXT("Mae: hello there"), true,
		{ { TEXT("character"), 0, 5 } } });
	Cases.Add({ TEXT("[a]xx[b]yy[/a]zz[/b]"), TEXT("xxyyzz"), false,
		{ { TEXT("a"), 0, 4 }, { TEXT("b"), 2, 4 } } });
	Cases.Add({ TEXT("start [markup=-1 /] end"), TEXT("start end"), false,
		{ { TEXT("markup"), 6, 0 } } });
	Cases.Add({ FromCodepoints({ 0xE1 }) + TEXT(" [a]S[/a]"), FromCodepoints({ 0xE1 }) + TEXT(" S"), false,
		{ { TEXT("a"), 2, 1 } } });
	Cases.Add({ TEXT("cafe") + AcuteAccent + TEXT(" [a]S[/a]"), FromCodepoints({ 0x63, 0x61, 0x66, 0xE9 }) + TEXT(" S"), false,
		{ { TEXT("a"), 5, 1 } } });
	Cases.Add({ TEXT("emoji ") + Smiley + TEXT(" [a]tail[/a]"), TEXT("emoji ") + Smiley + TEXT(" tail"), false,
		{ { TEXT("a"), 9, 4 } } });

	for (const FMarkupCase& Case : Cases)
	{
		const FYarnMarkupParseResult Result = UYarnMarkupLibrary::ParseMarkupFull(Case.Input, TEXT("en"), Case.bAddImplicitCharacter);

		TestEqual(FString::Printf(TEXT("text for `%s`"), *Case.Input), Result.Text, Case.ExpectedText);
		TestEqual(FString::Printf(TEXT("attribute count for `%s`"), *Case.Input), Result.Attributes.Num(), Case.Attributes.Num());

		for (const FExpectedAttribute& Expected : Case.Attributes)
		{
			const FYarnMarkupAttribute* Actual = Result.Attributes.FindByPredicate([&Expected](const FYarnMarkupAttribute& Attribute)
			{
				return Attribute.Name.Equals(Expected.Name, ESearchCase::CaseSensitive);
			});

			if (!Actual)
			{
				AddError(FString::Printf(TEXT("`%s` is missing the [%s] attribute"), *Case.Input, *Expected.Name));
				continue;
			}

			TestEqual(FString::Printf(TEXT("[%s] position in `%s`"), *Expected.Name, *Case.Input), Actual->Position, Expected.Position);
			TestEqual(FString::Printf(TEXT("[%s] length in `%s`"), *Expected.Name, *Case.Input), Actual->Length, Expected.Length);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnUnicodeNormalizationTest, "YarnSpinner.Unicode.NFC",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnUnicodeNormalizationTest::RunTest(const FString& Parameters)
{
	auto CheckNFC = [this](const TArray<uint32>& Input, const TArray<uint32>& Expected, const TCHAR* What)
	{
		const FString Normalized = FYarnUnicodeNormalization::NFC(FromCodepoints(Input));
		TestEqual(What, Normalized, FromCodepoints(Expected));
	};

	CheckNFC({ 0x65, 0x301 }, { 0xE9 }, TEXT("e plus combining acute composes"));
	CheckNFC({ 0x41, 0x30A }, { 0xC5 }, TEXT("A plus combining ring composes"));
	CheckNFC({ 0x1100, 0x1161 }, { 0xAC00 }, TEXT("hangul jamo compose into a syllable"));
	CheckNFC({ 0x71, 0x307, 0x323 }, { 0x71, 0x323, 0x307 }, TEXT("combining marks reorder by combining class"));
	CheckNFC({ 0x1E9B, 0x323 }, { 0x1E9B, 0x323 }, TEXT("composition exclusions are left alone"));
	CheckNFC({ 0x3A9 }, { 0x3A9 }, TEXT("already-composed text is unchanged"));
	CheckNFC({ 0x78 }, { 0x78 }, TEXT("plain ascii is unchanged"));
	CheckNFC({ 0xAC00, 0x300 }, { 0xAC00, 0x300 }, TEXT("hangul syllable with a trailing mark is unchanged"));
	CheckNFC({ 0x1F642, 0x65, 0x301 }, { 0x1F642, 0xE9 }, TEXT("text outside the basic plane survives normalisation"));

	TestEqual(TEXT("empty string is unchanged"), FYarnUnicodeNormalization::NFC(FString()), FString());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnNodeNameCaseTest, "YarnSpinner.VM.NodeNamesAreCaseSensitive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnNodeNameCaseTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Program.Nodes.Add(Node.Name, Node);

	TestTrue(TEXT("the node is found by its exact name"), Program.HasNode(TEXT("Start")));
	TestFalse(TEXT("a differently-cased name does not find the node"), Program.HasNode(TEXT("start")));
	TestFalse(TEXT("an upper-cased name does not find the node"), Program.HasNode(TEXT("START")));
	TestNull(TEXT("GetNode refuses a differently-cased name"), Program.GetNode(TEXT("start")));
	TestNotNull(TEXT("GetNode accepts the exact name"), Program.GetNode(TEXT("Start")));

	return true;
}

#endif
