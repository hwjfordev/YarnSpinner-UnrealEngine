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

#include "YarnUnicodeNormalization.h"

#include "YarnSpinnerModule.h"

#if UE_ENABLE_ICU
THIRD_PARTY_INCLUDES_START
#include <unicode/unorm2.h>
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
	// Nothing below U+0300 can combine with what precedes it so text made
	// only ooff such characters is already normalised..
	constexpr uint32 FirstUnstableCodepoint = 0x300;

	bool CouldNeedNormalising(const FString& Text)
	{
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			if (static_cast<uint32>(Text[Index]) >= FirstUnstableCodepoint)
			{
				return true;
			}
		}
		return false;
	}
}

FString FYarnUnicodeNormalization::NFC(const FString& Text)
{
	if (Text.IsEmpty() || !CouldNeedNormalising(Text))
	{
		return Text;
	}

#if UE_ENABLE_ICU
	UErrorCode Status = U_ZERO_ERROR;
	const UNormalizer2* Normalizer = unorm2_getNFCInstance(&Status);
	if (U_FAILURE(Status) || !Normalizer)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Unicode normalisation is unavailable (%s); text will be used as written"),
			UTF8_TO_TCHAR(u_errorName(Status)));
		return Text;
	}

	const FTCHARToUTF16 Source(*Text, Text.Len());

	TArray<UChar> Buffer;
	Buffer.SetNumUninitialized(Source.Length() + 1);

	Status = U_ZERO_ERROR;
	int32 Length = unorm2_normalize(Normalizer,
		reinterpret_cast<const UChar*>(Source.Get()), Source.Length(),
		Buffer.GetData(), Buffer.Num(), &Status);

	if (Status == U_BUFFER_OVERFLOW_ERROR)
	{
		Buffer.SetNumUninitialized(Length + 1);
		Status = U_ZERO_ERROR;
		Length = unorm2_normalize(Normalizer,
			reinterpret_cast<const UChar*>(Source.Get()), Source.Length(),
			Buffer.GetData(), Buffer.Num(), &Status);
	}

	if (U_FAILURE(Status))
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Unicode normalisation failed (%s); text will be used as written"),
			UTF8_TO_TCHAR(u_errorName(Status)));
		return Text;
	}

	return FString(StringCast<TCHAR>(reinterpret_cast<const UTF16CHAR*>(Buffer.GetData()), Length));
#else
	return Text;
#endif
}
