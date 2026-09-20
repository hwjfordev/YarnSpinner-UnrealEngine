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
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "YarnDialogueRunner.h"
#include "YarnLocalization.h"
#include "YarnProgram.h"
#include "YarnProtobufParser.h"
#include "YarnSaliency.h"
#include "YarnTestPlanPresenter.h"
#include "YarnVariableStorage.h"
#include "YarnVirtualMachine.h"

namespace YarnTestPlan
{
	const TCHAR* StartNodeName = TEXT("Start");
	const TCHAR* LocaleCode = TEXT("en");

	enum class EToken : uint8
	{
		EndOfFile,
		Separator,
		Environment,
		Start,
		Line,
		Star,
		Option,
		Disabled,
		Command,
		Stop,
		Select,
		Set,
		Equals,
		Saliency,
		Node,
		Comment,
		Whitespace,
		Bool,
		Identifier,
		Hashtag,
		Variable,
		Number,
		Text,
	};

	struct FToken
	{
		EToken Type = EToken::EndOfFile;
		FString Text;
	};

	struct FLiteral
	{
		const TCHAR* Text;
		EToken Type;
	};

	const FLiteral Literals[] = {
		{ TEXT("---"), EToken::Separator },
		{ TEXT("environment:"), EToken::Environment },
		{ TEXT("start:"), EToken::Start },
		{ TEXT("line:"), EToken::Line },
		{ TEXT("*"), EToken::Star },
		{ TEXT("option:"), EToken::Option },
		{ TEXT("[disabled]"), EToken::Disabled },
		{ TEXT("command:"), EToken::Command },
		{ TEXT("stop"), EToken::Stop },
		{ TEXT("select:"), EToken::Select },
		{ TEXT("set:"), EToken::Set },
		{ TEXT("="), EToken::Equals },
		{ TEXT("saliency:"), EToken::Saliency },
		{ TEXT("node:"), EToken::Node },
	};

	enum class EStepKind : uint8
	{
		Line,
		Option,
		Command,
		Stop,
		Select,
		Set,
		Saliency,
		Node,
	};

	struct FStep
	{
		EStepKind Kind = EStepKind::Stop;
		FString Text;
		bool bAnyText = false;
		TArray<FString> Hashtags;
		bool bAvailable = true;
		int32 Index = 0;
		FString Name;
		FYarnValue Value;
	};

	struct FRun
	{
		FString StartNode;
		TArray<FStep> Steps;
	};

	bool HasAt(const FString& Source, int32 Position, const TCHAR* Literal)
	{
		const int32 Length = FCString::Strlen(Literal);
		return Position + Length <= Source.Len() && FCString::Strncmp(*Source + Position, Literal, Length) == 0;
	}

	bool IsIdentifierStart(TCHAR C)
	{
		return (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || C == '_';
	}

	bool IsDigit(TCHAR C)
	{
		return C >= '0' && C <= '9';
	}

	bool IsSpace(TCHAR C)
	{
		return C == ' ' || C == '\t' || C == '\r' || C == '\n';
	}

	int32 MatchComment(const FString& Source, int32 Position)
	{
		if (!HasAt(Source, Position, TEXT("//")))
		{
			return 0;
		}
		int32 Index = Position + 2;
		while (Index < Source.Len() && Source[Index] != '\r' && Source[Index] != '\n')
		{
			++Index;
		}
		return Index - Position;
	}

	int32 MatchWhitespace(const FString& Source, int32 Position)
	{
		int32 Index = Position;
		while (Index < Source.Len() && IsSpace(Source[Index]))
		{
			++Index;
		}
		return Index - Position;
	}

	int32 MatchBool(const FString& Source, int32 Position)
	{
		if (HasAt(Source, Position, TEXT("true")))
		{
			return 4;
		}
		if (HasAt(Source, Position, TEXT("false")))
		{
			return 5;
		}
		return 0;
	}

	int32 MatchIdentifier(const FString& Source, int32 Position)
	{
		if (Position >= Source.Len() || !IsIdentifierStart(Source[Position]))
		{
			return 0;
		}
		int32 Index = Position + 1;
		while (Index < Source.Len() && (IsIdentifierStart(Source[Index]) || IsDigit(Source[Index])))
		{
			++Index;
		}
		return Index - Position;
	}

	int32 MatchHashtag(const FString& Source, int32 Position)
	{
		if (Source[Position] != '#')
		{
			return 0;
		}
		int32 Index = Position + 1;
		while (Index < Source.Len() && !IsSpace(Source[Index]) && Source[Index] != '#')
		{
			++Index;
		}
		return Index == Position + 1 ? 0 : Index - Position;
	}

	int32 MatchVariable(const FString& Source, int32 Position)
	{
		if (Source[Position] != '$')
		{
			return 0;
		}
		const int32 IdentifierLength = MatchIdentifier(Source, Position + 1);
		return IdentifierLength == 0 ? 0 : IdentifierLength + 1;
	}

	int32 MatchNumber(const FString& Source, int32 Position)
	{
		int32 Index = Position;
		while (Index < Source.Len() && IsDigit(Source[Index]))
		{
			++Index;
		}
		return Index - Position;
	}

	int32 MatchText(const FString& Source, int32 Position)
	{
		if (Source[Position] != '`')
		{
			return 0;
		}
		const int32 Closing = Source.Find(TEXT("`"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Position + 1);
		return Closing == INDEX_NONE ? 0 : Closing - Position + 1;
	}

	bool Tokenize(const FString& Source, TArray<FToken>& OutTokens, FString& OutError)
	{
		int32 Position = 0;
		while (Position < Source.Len())
		{
			int32 BestLength = 0;
			EToken BestType = EToken::EndOfFile;
			for (const FLiteral& Literal : Literals)
			{
				const int32 Length = FCString::Strlen(Literal.Text);
				if (Length > BestLength && HasAt(Source, Position, Literal.Text))
				{
					BestLength = Length;
					BestType = Literal.Type;
				}
			}
			const TPair<int32, EToken> RuleMatches[] = {
				{ MatchComment(Source, Position), EToken::Comment },
				{ MatchWhitespace(Source, Position), EToken::Whitespace },
				{ MatchBool(Source, Position), EToken::Bool },
				{ MatchIdentifier(Source, Position), EToken::Identifier },
				{ MatchHashtag(Source, Position), EToken::Hashtag },
				{ MatchVariable(Source, Position), EToken::Variable },
				{ MatchNumber(Source, Position), EToken::Number },
				{ MatchText(Source, Position), EToken::Text },
			};
			for (const TPair<int32, EToken>& RuleMatch : RuleMatches)
			{
				if (RuleMatch.Key > BestLength)
				{
					BestLength = RuleMatch.Key;
					BestType = RuleMatch.Value;
				}
			}
			if (BestLength == 0)
			{
				OutError = FString::Printf(TEXT("token recognition error at offset %d: '%s'"), Position, *Source.Mid(Position, 20));
				return false;
			}
			if (BestType != EToken::Comment && BestType != EToken::Whitespace)
			{
				OutTokens.Add({ BestType, Source.Mid(Position, BestLength) });
			}
			Position += BestLength;
		}
		OutTokens.Add({ EToken::EndOfFile, TEXT("<EOF>") });
		return true;
	}

	class FParser
	{
	public:
		bool Parse(const FString& Source, TArray<FRun>& OutRuns, FString& OutError)
		{
			if (!Tokenize(Source, Tokens, OutError))
			{
				return false;
			}

			if (PeekType() == EToken::Environment)
			{
				Next();
				if (PeekType() != EToken::Identifier)
				{
					return SyntaxError(TEXT("environment:"), TEXT("IDENTIFIER"), OutError);
				}
				const FString Environment = Next().Text;
				if (PeekType() != EToken::Separator)
				{
					return SyntaxError(TEXT("environment: ") + Environment, TEXT("'---'"), OutError);
				}
				Next();
			}

			while (true)
			{
				FRun& Run = OutRuns.AddDefaulted_GetRef();
				Run.StartNode = StartNodeName;
				if (PeekType() == EToken::Start)
				{
					Next();
					if (PeekType() != EToken::Identifier)
					{
						return SyntaxError(TEXT("start:"), TEXT("IDENTIFIER"), OutError);
					}
					Run.StartNode = Next().Text;
				}
				while (IsStepStart(PeekType()))
				{
					FStep Step;
					if (!ParseStep(Step, OutError))
					{
						return false;
					}
					Run.Steps.Add(MoveTemp(Step));
				}
				if (Run.Steps.Num() == 0)
				{
					OutError = FString::Printf(TEXT("expected a step but found '%s'"), *PeekText());
					return false;
				}
				if (PeekType() == EToken::Separator)
				{
					Next();
					continue;
				}
				if (PeekType() == EToken::EndOfFile)
				{
					return true;
				}
				OutError = FString::Printf(TEXT("unexpected '%s'"), *PeekText());
				return false;
			}
		}

	private:
		TArray<FToken> Tokens;
		int32 TokenIndex = 0;

		static bool IsStepStart(EToken Type)
		{
			switch (Type)
			{
			case EToken::Line:
			case EToken::Option:
			case EToken::Command:
			case EToken::Stop:
			case EToken::Select:
			case EToken::Set:
			case EToken::Saliency:
			case EToken::Node:
				return true;
			default:
				return false;
			}
		}

		EToken PeekType() const
		{
			return Tokens[TokenIndex].Type;
		}

		const FString& PeekText() const
		{
			return Tokens[TokenIndex].Text;
		}

		const FToken& Next()
		{
			const FToken& Token = Tokens[TokenIndex];
			if (TokenIndex < Tokens.Num() - 1)
			{
				++TokenIndex;
			}
			return Token;
		}

		bool SyntaxError(const FString& After, const FString& Expected, FString& OutError) const
		{
			OutError = FString::Printf(TEXT("expected %s after '%s' but found '%s'"), *Expected, *After, *PeekText());
			return false;
		}

		static FString TrimBackticks(const FString& Text)
		{
			return Text.Mid(1, Text.Len() - 2);
		}

		TArray<FString> ParseHashtags()
		{
			TArray<FString> Hashtags;
			while (PeekType() == EToken::Hashtag)
			{
				Hashtags.Add(Next().Text.Mid(1));
			}
			return Hashtags;
		}

		bool ParseStep(FStep& OutStep, FString& OutError)
		{
			const FToken Keyword = Next();
			switch (Keyword.Type)
			{
			case EToken::Line:
				OutStep.Kind = EStepKind::Line;
				if (PeekType() == EToken::Text)
				{
					OutStep.Text = TrimBackticks(Next().Text);
				}
				else if (PeekType() == EToken::Star)
				{
					Next();
					OutStep.bAnyText = true;
				}
				else
				{
					return SyntaxError(TEXT("line:"), TEXT("TEXT or '*'"), OutError);
				}
				OutStep.Hashtags = ParseHashtags();
				return true;

			case EToken::Option:
				OutStep.Kind = EStepKind::Option;
				if (PeekType() != EToken::Text)
				{
					return SyntaxError(TEXT("option:"), TEXT("TEXT"), OutError);
				}
				OutStep.Text = TrimBackticks(Next().Text);
				OutStep.Hashtags = ParseHashtags();
				if (PeekType() == EToken::Disabled)
				{
					Next();
					OutStep.bAvailable = false;
				}
				return true;

			case EToken::Command:
				OutStep.Kind = EStepKind::Command;
				if (PeekType() != EToken::Text)
				{
					return SyntaxError(TEXT("command:"), TEXT("TEXT"), OutError);
				}
				OutStep.Text = TrimBackticks(Next().Text);
				return true;

			case EToken::Stop:
				OutStep.Kind = EStepKind::Stop;
				return true;

			case EToken::Select:
				OutStep.Kind = EStepKind::Select;
				if (PeekType() != EToken::Number)
				{
					return SyntaxError(TEXT("select:"), TEXT("NUMBER"), OutError);
				}
				OutStep.Index = FCString::Atoi(*Next().Text) - 1;
				return true;

			case EToken::Set:
			{
				OutStep.Kind = EStepKind::Set;
				if (PeekType() != EToken::Variable)
				{
					return SyntaxError(TEXT("set:"), TEXT("VARIABLE"), OutError);
				}
				OutStep.Name = Next().Text;
				if (PeekType() != EToken::Equals)
				{
					return SyntaxError(TEXT("set: ") + OutStep.Name, TEXT("'='"), OutError);
				}
				Next();
				if (PeekType() == EToken::Bool)
				{
					OutStep.Value = FYarnValue(Next().Text.Equals(TEXT("true"), ESearchCase::CaseSensitive));
					return true;
				}
				if (PeekType() == EToken::Number)
				{
					OutStep.Value = FYarnValue(static_cast<float>(FCString::Atoi(*Next().Text)));
					return true;
				}
				return SyntaxError(TEXT("set: ") + OutStep.Name + TEXT(" ="), TEXT("BOOL or NUMBER"), OutError);
			}

			case EToken::Saliency:
				OutStep.Kind = EStepKind::Saliency;
				if (PeekType() != EToken::Identifier)
				{
					return SyntaxError(TEXT("saliency:"), TEXT("IDENTIFIER"), OutError);
				}
				OutStep.Name = Next().Text;
				return true;

			case EToken::Node:
				OutStep.Kind = EStepKind::Node;
				if (PeekType() != EToken::Identifier)
				{
					return SyntaxError(TEXT("node:"), TEXT("IDENTIFIER"), OutError);
				}
				OutStep.Name = Next().Text;
				return true;

			default:
				OutError = FString::Printf(TEXT("unhandled step type '%s'"), *Keyword.Text);
				return false;
			}
		}
	};

	FString DescribeStep(const FStep* Step)
	{
		if (!Step)
		{
			return TEXT("no step");
		}
		switch (Step->Kind)
		{
		case EStepKind::Line:
			return Step->bAnyText ? FString(TEXT("line: *")) : FString::Printf(TEXT("line: `%s`"), *Step->Text);
		case EStepKind::Option:
			return FString::Printf(TEXT("option: `%s`%s"), *Step->Text, Step->bAvailable ? TEXT("") : TEXT(" [disabled]"));
		case EStepKind::Command:
			return FString::Printf(TEXT("command: `%s`"), *Step->Text);
		case EStepKind::Stop:
			return TEXT("stop");
		case EStepKind::Select:
			return FString::Printf(TEXT("select: %d"), Step->Index + 1);
		case EStepKind::Set:
			return FString::Printf(TEXT("set: %s = %s"), *Step->Name, *Step->Value.ConvertToString());
		case EStepKind::Saliency:
			return FString::Printf(TEXT("saliency: %s"), *Step->Name);
		case EStepKind::Node:
			return FString::Printf(TEXT("node: %s"), *Step->Name);
		}
		return TEXT("no step");
	}

	FString DescribeEvent(const FYarnTestPlanEvent& Event)
	{
		switch (Event.Kind)
		{
		case EYarnTestPlanEventKind::Line:
			return FString::Printf(TEXT("line \"%s\""), *Event.Line.Text.ToString());
		case EYarnTestPlanEventKind::Options:
		{
			TArray<FString> Texts;
			for (const FYarnOption& Option : Event.Options.Options)
			{
				Texts.Add(Option.Line.Text.ToString());
			}
			return FString::Printf(TEXT("options [%s]"), *FString::Join(Texts, TEXT(", ")));
		}
		case EYarnTestPlanEventKind::Command:
			return FString::Printf(TEXT("command \"%s\""), *Event.CommandText);
		case EYarnTestPlanEventKind::Stop:
			return TEXT("stop");
		}
		return TEXT("nothing");
	}

	bool ContainsExactly(const TArray<FString>& Values, const FString& Value)
	{
		return Values.ContainsByPredicate([&Value](const FString& Candidate)
		{
			return Candidate.Equals(Value, ESearchCase::CaseSensitive);
		});
	}

	TArray<TArray<FString>> ReadCsv(const FString& Content)
	{
		TArray<TArray<FString>> Records;
		TArray<FString> Record;
		FString Field;
		bool bQuoted = false;
		bool bHasContent = false;
		for (int32 Index = 0; Index < Content.Len(); ++Index)
		{
			const TCHAR C = Content[Index];
			if (bQuoted)
			{
				if (C == '"')
				{
					if (Index + 1 < Content.Len() && Content[Index + 1] == '"')
					{
						Field.AppendChar('"');
						++Index;
					}
					else
					{
						bQuoted = false;
					}
				}
				else
				{
					Field.AppendChar(C);
				}
				continue;
			}
			if (C == '"')
			{
				bQuoted = true;
				bHasContent = true;
			}
			else if (C == ',')
			{
				Record.Add(MoveTemp(Field));
				Field.Reset();
				bHasContent = true;
			}
			else if (C == '\n')
			{
				if (bHasContent)
				{
					Record.Add(MoveTemp(Field));
					Records.Add(MoveTemp(Record));
				}
				Record.Reset();
				Field.Reset();
				bHasContent = false;
			}
			else if (C != '\r')
			{
				Field.AppendChar(C);
				bHasContent = true;
			}
		}
		if (bHasContent)
		{
			Record.Add(MoveTemp(Field));
			Records.Add(MoveTemp(Record));
		}
		return Records;
	}

	FString GetFixtureDirectory()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("YarnSpinner"));
		return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Tests"), TEXT("TestPlans")) : FString();
	}

	bool LoadProject(const FString& CaseName, UYarnProject& OutProject, FString& OutPlan, FString& OutError)
	{
		const FString BasePath = FPaths::Combine(GetFixtureDirectory(), CaseName);

		TArray<uint8> ProgramBytes;
		if (!FFileHelper::LoadFileToArray(ProgramBytes, *(BasePath + TEXT(".yarnc"))) || ProgramBytes.Num() == 0)
		{
			OutError = FString::Printf(TEXT("missing or empty fixture %s.yarnc"), *BasePath);
			return false;
		}
		if (!FFileHelper::LoadFileToString(OutPlan, *(BasePath + TEXT(".testplan"))))
		{
			OutError = FString::Printf(TEXT("missing fixture %s.testplan"), *BasePath);
			return false;
		}
		FString LinesContent;
		if (!FFileHelper::LoadFileToString(LinesContent, *(BasePath + TEXT("-Lines.csv"))))
		{
			OutError = FString::Printf(TEXT("missing fixture %s-Lines.csv"), *BasePath);
			return false;
		}
		FString MetadataContent;
		if (!FFileHelper::LoadFileToString(MetadataContent, *(BasePath + TEXT("-Metadata.csv"))))
		{
			OutError = FString::Printf(TEXT("missing fixture %s-Metadata.csv"), *BasePath);
			return false;
		}

		FYarnProtobufParser Parser(ProgramBytes);
		if (!Parser.ParseProgram(OutProject.Program, OutError))
		{
			OutError = FString::Printf(TEXT("compiled program could not be parsed: %s"), *OutError);
			return false;
		}

		const TArray<TArray<FString>> LineRecords = ReadCsv(LinesContent);
		for (int32 Index = 1; Index < LineRecords.Num(); ++Index)
		{
			if (LineRecords[Index].Num() >= 2)
			{
				OutProject.BaseStringTable.Add(LineRecords[Index][0], LineRecords[Index][1]);
			}
		}

		const TArray<TArray<FString>> MetadataRecords = ReadCsv(MetadataContent);
		for (int32 Index = 1; Index < MetadataRecords.Num(); ++Index)
		{
			if (MetadataRecords[Index].Num() >= 4)
			{
				OutProject.LineMetadata.Add(MetadataRecords[Index][0], MetadataRecords[Index][3]);
			}
		}

		for (const TPair<FString, FYarnNode>& Pair : OutProject.Program.Nodes)
		{
			OutProject.NodeNames.Add(Pair.Key);
		}
		return true;
	}

	class FRunner
	{
	public:
		explicit FRunner(UYarnProject* InProject)
			: Project(InProject)
		{
		}

		bool Setup(FString& OutError)
		{
			if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
			{
				TArray<FString> Errors;
				WorldWrapper.AppendErrorMessages(Errors);
				OutError = FString::Printf(TEXT("could not create a test world: %s"), *FString::Join(Errors, TEXT("; ")));
				return false;
			}

			AActor* Actor = WorldWrapper.GetTestWorld()->SpawnActor<AActor>();
			if (!Actor)
			{
				OutError = TEXT("could not spawn an actor in the test world");
				return false;
			}

			Storage = NewObject<UYarnInMemoryVariableStorage>(Actor);
			Storage->RegisterComponent();

			Presenter = NewObject<UYarnTestPlanPresenter>(Actor);
			Presenter->RegisterComponent();

			UYarnBuiltinLineProvider* LineProvider = NewObject<UYarnBuiltinLineProvider>(Actor);
			LineProvider->SetLocaleCode(LocaleCode);

			Runner = NewObject<UYarnDialogueRunner>(Actor);
			Runner->YarnProject = Project;
			Runner->VariableStorage = TScriptInterface<IYarnVariableStorage>(Storage);
			Runner->DialoguePresenters.Add(Presenter);
			Runner->LineProvider = LineProvider;
			Runner->SaliencyStrategy = EYarnSaliencyStrategy::BestLeastRecentlyViewed;
			Runner->bContinueOnUnhandledCommand = false;
			Runner->OnUnhandledCommand.AddDynamic(Presenter, &UYarnTestPlanPresenter::HandleUnhandledCommand);
			Runner->RegisterComponent();

			Runner->AddFunction(TEXT("assert"), [this](const TArray<FYarnValue>& Parameters)
			{
				if (Parameters.Num() < 1 || !Parameters[0].ConvertToBool())
				{
					Fail(TEXT("assertion should pass"));
					Runner->ReportFunctionError(TEXT("assert: assertion should pass"));
				}
				return FYarnValue(true);
			}, 1);
			Runner->AddFunction(TEXT("add_three_operands"), [](const TArray<FYarnValue>& Parameters)
			{
				float Sum = 0.0f;
				for (const FYarnValue& Parameter : Parameters)
				{
					Sum += FMath::RoundHalfToEven(Parameter.ConvertToNumber());
				}
				return FYarnValue(Sum);
			}, 3);
			Runner->AddFunction(TEXT("set_objective_complete"), [](const TArray<FYarnValue>&) { return FYarnValue(true); }, 1);
			Runner->AddFunction(TEXT("is_objective_active"), [](const TArray<FYarnValue>&) { return FYarnValue(true); }, 1);
			Runner->AddFunction(TEXT("get_quest_status"), [](const TArray<FYarnValue>&) { return FYarnValue(FString(TEXT("InProgress"))); }, 1);
			Runner->AddFunction(TEXT("dummy_bool"), [](const TArray<FYarnValue>&) { return FYarnValue(true); }, 0);
			Runner->AddFunction(TEXT("dummy_number"), [](const TArray<FYarnValue>&) { return FYarnValue(1.0f); }, 0);
			Runner->AddFunction(TEXT("dummy_string"), [](const TArray<FYarnValue>&) { return FYarnValue(FString(TEXT("string"))); }, 0);
			return true;
		}

		FString Run(const TArray<FRun>& Runs)
		{
			for (int32 RunIndex = 0; RunIndex < Runs.Num() && Failure.IsEmpty(); ++RunIndex)
			{
				const FRun& Run = Runs[RunIndex];
				StepLabel = FString::Printf(TEXT("run %d, start"), RunIndex + 1);
				ExpectedOptions.Reset();
				if (!RequestStart(Run.StartNode))
				{
					break;
				}

				for (int32 StepIndex = 0; StepIndex < Run.Steps.Num() && Failure.IsEmpty(); ++StepIndex)
				{
					const FStep& Step = Run.Steps[StepIndex];
					StepLabel = FString::Printf(TEXT("run %d, step %d (%s)"), RunIndex + 1, StepIndex + 1, *DescribeStep(&Step));

					switch (Step.Kind)
					{
					case EStepKind::Line:
					case EStepKind::Command:
					case EStepKind::Stop:
					{
						FYarnTestPlanEvent Event;
						if (NextEvent(Step, Event))
						{
							CheckContent(Step, Event);
						}
						break;
					}
					case EStepKind::Option:
						ExpectedOptions.Add(&Step);
						break;
					case EStepKind::Select:
					{
						FYarnTestPlanEvent Event;
						if (NextEvent(Step, Event) && CheckOptions(Step, Event))
						{
							Presenter->OnOptionSelected(Step.Index);
						}
						ExpectedOptions.Reset();
						break;
					}
					case EStepKind::Node:
						RequestStart(Step.Name);
						break;
					case EStepKind::Set:
						if (!Project->Program.InitialValues.Contains(Step.Name))
						{
							Fail(FString::Printf(TEXT("variable %s is not valid in program"), *Step.Name));
						}
						else
						{
							IYarnVariableStorage::Execute_SetValue(Storage, Step.Name, Step.Value);
						}
						break;
					case EStepKind::Saliency:
						ApplySaliencyStrategy(Step.Name);
						break;
					}

					if (Step.Kind == EStepKind::Stop)
					{
						break;
					}
				}
			}
			return Failure;
		}

	private:
		UYarnProject* Project = nullptr;
		FTestWorldWrapper WorldWrapper;
		UYarnDialogueRunner* Runner = nullptr;
		UYarnTestPlanPresenter* Presenter = nullptr;
		UYarnInMemoryVariableStorage* Storage = nullptr;
		FString PendingStartNode;
		bool bNeedsStart = false;
		bool bStopped = true;
		TArray<const FStep*> ExpectedOptions;
		FString StepLabel;
		FString Failure;

		void Fail(const FString& Message)
		{
			if (Failure.IsEmpty())
			{
				Failure = FString::Printf(TEXT("%s: %s"), *StepLabel, *Message);
			}
		}

		bool RequestStart(const FString& NodeName)
		{
			if (!Project->HasNode(NodeName))
			{
				Fail(FString::Printf(TEXT("no node named '%s' has been loaded"), *NodeName));
				return false;
			}
			PendingStartNode = NodeName;
			bNeedsStart = true;
			return true;
		}

		void ApplySaliencyStrategy(const FString& Mode)
		{
			EYarnSaliencyStrategy Strategy;
			if (Mode.Equals(TEXT("first"), ESearchCase::CaseSensitive))
			{
				Strategy = EYarnSaliencyStrategy::First;
			}
			else if (Mode.Equals(TEXT("best"), ESearchCase::CaseSensitive))
			{
				Strategy = EYarnSaliencyStrategy::Best;
			}
			else if (Mode.Equals(TEXT("best_least_recently_seen"), ESearchCase::CaseSensitive))
			{
				Strategy = EYarnSaliencyStrategy::BestLeastRecentlyViewed;
			}
			else
			{
				Fail(FString::Printf(TEXT("unknown saliency strategy '%s'"), *Mode));
				return;
			}
			Runner->SetSaliencyStrategy(UYarnSaliencyStrategyFactory::CreateStrategy(
				Strategy, TScriptInterface<IYarnVariableStorage>(Storage), Runner));
		}

		bool NextEvent(const FStep& Expected, FYarnTestPlanEvent& OutEvent)
		{
			if (Presenter->Events.Num() == 0)
			{
				if (bNeedsStart)
				{
					bNeedsStart = false;
					bStopped = false;
					Presenter->ResetForRun();
					Runner->StartDialogue(PendingStartNode);
				}
				else if (bStopped)
				{
					Fail(FString::Printf(TEXT("expected %s, but the dialogue has already stopped"), *DescribeStep(&Expected)));
					return false;
				}
				else if (Presenter->HasPendingOptions())
				{
					Fail(TEXT("cannot continue running dialogue; still waiting on option selection"));
					return false;
				}
				else if (Presenter->HasPendingLine())
				{
					Presenter->OnLinePresentationComplete();
				}
				else if (Presenter->bCommandPending)
				{
					Presenter->bCommandPending = false;
					Runner->Continue();
				}
				else
				{
					Fail(FString::Printf(TEXT("expected %s, but the dialogue is not waiting to continue"), *DescribeStep(&Expected)));
					return false;
				}
			}

			if (Presenter->Events.Num() == 0)
			{
				Fail(FString::Printf(TEXT("expected %s, but the dialogue delivered nothing"), *DescribeStep(&Expected)));
				return false;
			}

			OutEvent = Presenter->Events[0];
			Presenter->Events.RemoveAt(0);
			if (OutEvent.Kind == EYarnTestPlanEventKind::Stop)
			{
				bStopped = true;
			}
			return true;
		}

		bool CheckHashtags(const FYarnLocalizedLine& Line, const TArray<FString>& Hashtags)
		{
			for (const FString& Hashtag : Hashtags)
			{
				if (!ContainsExactly(Line.Metadata, Hashtag) && !Hashtag.Equals(Line.RawLine.LineID, ESearchCase::CaseSensitive))
				{
					Fail(FString::Printf(TEXT("metadata for %s is expected to contain '%s' but was [%s]"),
						*Line.RawLine.LineID, *Hashtag, *FString::Join(Line.Metadata, TEXT(", "))));
					return false;
				}
			}
			return true;
		}

		void CheckContent(const FStep& Step, const FYarnTestPlanEvent& Event)
		{
			const bool bKindMatches =
				(Step.Kind == EStepKind::Line && Event.Kind == EYarnTestPlanEventKind::Line) ||
				(Step.Kind == EStepKind::Command && Event.Kind == EYarnTestPlanEventKind::Command) ||
				(Step.Kind == EStepKind::Stop && Event.Kind == EYarnTestPlanEventKind::Stop);
			if (!bKindMatches)
			{
				Fail(FString::Printf(TEXT("expected %s, not %s"), *DescribeStep(&Step), *DescribeEvent(Event)));
				return;
			}

			if (Step.Kind == EStepKind::Line)
			{
				if (!Event.Line.IsValid())
				{
					Fail(TEXT("the line provider found no text for the delivered line"));
					return;
				}
				const FString Text = Event.Line.Text.ToString();
				if (!Step.bAnyText && !Text.Equals(Step.Text, ESearchCase::CaseSensitive))
				{
					Fail(FString::Printf(TEXT("line text mismatch: expected `%s`, actual `%s`"), *Step.Text, *Text));
					return;
				}
				CheckHashtags(Event.Line, Step.Hashtags);
			}
			else if (Step.Kind == EStepKind::Command && !Event.CommandText.Equals(Step.Text, ESearchCase::CaseSensitive))
			{
				Fail(FString::Printf(TEXT("command text mismatch: expected `%s`, actual `%s`"), *Step.Text, *Event.CommandText));
			}
		}

		bool CheckOptions(const FStep& Step, const FYarnTestPlanEvent& Event)
		{
			if (Event.Kind != EYarnTestPlanEventKind::Options)
			{
				Fail(FString::Printf(TEXT("expected %s, not %s"), *DescribeStep(&Step), *DescribeEvent(Event)));
				return false;
			}

			const TArray<FYarnOption>& Options = Event.Options.Options;
			if (Options.Num() != ExpectedOptions.Num())
			{
				Fail(FString::Printf(TEXT("expected %d options, got %s"), ExpectedOptions.Num(), *DescribeEvent(Event)));
				return false;
			}

			bool bAnyAvailable = false;
			for (int32 Index = 0; Index < Options.Num(); ++Index)
			{
				const FYarnOption& Option = Options[Index];
				const FStep& Expected = *ExpectedOptions[Index];
				if (!Option.Line.IsValid())
				{
					Fail(FString::Printf(TEXT("the line provider found no text for option %d"), Index + 1));
					return false;
				}
				const FString Text = Option.Line.Text.ToString();
				if (!Text.Equals(Expected.Text, ESearchCase::CaseSensitive))
				{
					Fail(FString::Printf(TEXT("option %d text mismatch: expected `%s`, actual `%s`"), Index + 1, *Expected.Text, *Text));
					return false;
				}
				if (!CheckHashtags(Option.Line, Expected.Hashtags))
				{
					return false;
				}
				if (Option.bIsAvailable != Expected.bAvailable)
				{
					Fail(FString::Printf(TEXT("option \"%s\"'s availability was expected to be %s"),
						*Text, Expected.bAvailable ? TEXT("true") : TEXT("false")));
					return false;
				}
				bAnyAvailable |= Option.bIsAvailable;
			}

			if (Step.Index != YarnNoOptionSelected && (Step.Index < 0 || Step.Index >= Options.Num()))
			{
				Fail(FString::Printf(TEXT("%d is not a valid option ID (expected a number between 0 and %d)"), Step.Index, Options.Num() - 1));
				return false;
			}

			if (bAnyAvailable)
			{
				const int32 MatchingCount = Options.FilterByPredicate([&Step](const FYarnOption& Option)
				{
					return Option.OptionID == Step.Index;
				}).Num();
				if (MatchingCount != 1)
				{
					Fail(FString::Printf(TEXT("one option should have the ID that we want to select (%d), found %d"), Step.Index, MatchingCount));
					return false;
				}
			}
			else if (Step.Index != YarnNoOptionSelected)
			{
				Fail(FString::Printf(TEXT("no option is available, so the selected index should be %d, not %d"), YarnNoOptionSelected, Step.Index));
				return false;
			}
			return true;
		}
	};

	int32 CountReachableCommands(const TArray<FRun>& Runs)
	{
		int32 Count = 0;
		for (const FRun& Run : Runs)
		{
			for (const FStep& Step : Run.Steps)
			{
				if (Step.Kind == EStepKind::Stop)
				{
					break;
				}
				Count += Step.Kind == EStepKind::Command ? 1 : 0;
			}
		}
		return Count;
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FYarnTestPlanTest, "YarnSpinner.TestPlans",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void FYarnTestPlanTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	const FString Directory = YarnTestPlan::GetFixtureDirectory();
	if (Directory.IsEmpty())
	{
		return;
	}

	TArray<FString> PlanFiles;
	IFileManager::Get().FindFiles(PlanFiles, *FPaths::Combine(Directory, TEXT("*.testplan")), true, false);
	PlanFiles.Sort();
	for (const FString& PlanFile : PlanFiles)
	{
		const FString CaseName = FPaths::GetBaseFilename(PlanFile);
		OutBeautifiedNames.Add(CaseName);
		OutTestCommands.Add(CaseName);
	}
}

bool FYarnTestPlanTest::RunTest(const FString& Parameters)
{
	using namespace YarnTestPlan;

	TStrongObjectPtr<UYarnProject> Project(NewObject<UYarnProject>());
	FString PlanSource;
	FString Error;
	if (!LoadProject(Parameters, *Project, PlanSource, Error))
	{
		AddError(Error);
		return false;
	}

	TArray<FRun> Runs;
	FParser Parser;
	if (!Parser.Parse(PlanSource, Runs, Error))
	{
		AddError(FString::Printf(TEXT("syntax errors in test plan: %s"), *Error));
		return false;
	}

	if (!Project->HasNode(StartNodeName))
	{
		AddInfo(FString::Printf(TEXT("no %s node; compilation only"), StartNodeName));
		return true;
	}

	const int32 CommandCount = CountReachableCommands(Runs);
	if (CommandCount > 0)
	{
		AddExpectedErrorPlain(TEXT("Unhandled command"), EAutomationExpectedErrorFlags::Contains, CommandCount);
	}

	FRunner Runner(Project.Get());
	if (!Runner.Setup(Error))
	{
		AddError(Error);
		return false;
	}

	const FString Failure = Runner.Run(Runs);
	if (!Failure.IsEmpty())
	{
		AddError(Failure);
		return false;
	}
	return true;
}

#endif
