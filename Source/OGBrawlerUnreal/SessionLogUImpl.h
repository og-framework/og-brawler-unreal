// SPDX-License-Identifier: BUSL-1.1
// docs/SessionLog-rationale.md · docs/SessionLog-guards.md

#pragma once

#include "CoreMinimal.h"

class AController;
class AGameModeBase;
class APlayerController;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogOGSession, Log, All);

namespace sessionLogUImpl
{

struct SessionLineFormats
{
	TCHAR joined[96];
	TCHAR left[96];
	TCHAR aboveTestedSize[96];
	TCHAR listening[96];
	TCHAR buildLabel[96];
};

// ⛔G-01  docs/SessionLog-guards.md
inline constexpr SessionLineFormats kSessionLineFormats{
	TEXT("OGBrawlerSession: joined players=%d tested=%d local=%d"),
	TEXT("OGBrawlerSession: left players=%d tested=%d"),
	TEXT("OGBrawlerSession: above tested size players=%d tested=%d"),
	TEXT("OGBrawlerSession: listening port=%d"),
	TEXT("OGBrawlerSession: build label=%s"),
};

inline constexpr TCHAR kSessionLinePrefix[] = TEXT("OGBrawlerSession: ");

constexpr bool startsWithSessionPrefix(const TCHAR* line)
{
	for (const TCHAR* prefix = kSessionLinePrefix; *prefix != TEXT('\0'); ++prefix, ++line)
	{
		if (*line != *prefix)
			return false;
	}
	return true;
}

static_assert(startsWithSessionPrefix(kSessionLineFormats.joined)
                  && startsWithSessionPrefix(kSessionLineFormats.left)
                  && startsWithSessionPrefix(kSessionLineFormats.aboveTestedSize)
                  && startsWithSessionPrefix(kSessionLineFormats.listening)
                  && startsWithSessionPrefix(kSessionLineFormats.buildLabel),
              "Was the backlog's 'stable, documented prefix' (task 13): every session line starts "
              "with 'OGBrawlerSession: ', which the host launcher's patterns match. "
              "See docs/SessionLog-guards.md G-01.");

constexpr bool startsWithWord(const TCHAR* text, const TCHAR* word)
{
	for (; *word != TEXT('\0'); ++word, ++text)
	{
		if (*text != *word)
			return false;
	}
	return true;
}

constexpr bool readsAsParsedLine(const TCHAR* format)
{
	const TCHAR* afterPrefix = format + (UE_ARRAY_COUNT(kSessionLinePrefix) - 1);
	return startsWithWord(afterPrefix, TEXT("joined")) || startsWithWord(afterPrefix, TEXT("left"))
	    || startsWithWord(afterPrefix, TEXT("listening"));
}

static_assert(readsAsParsedLine(kSessionLineFormats.joined) && readsAsParsedLine(kSessionLineFormats.left)
                  && readsAsParsedLine(kSessionLineFormats.listening)
                  && !readsAsParsedLine(kSessionLineFormats.aboveTestedSize)
                  && !readsAsParsedLine(kSessionLineFormats.buildLabel),
              "Was prose in docs/SessionLog-guards.md G-01 ('do not make the above tested size line start "
              "with joined or left') and the task 15 brief (the build label line must match neither host "
              "launcher pattern): only the joined, left and listening lines may start with the words the "
              "launcher's JoinLinePattern and ReadyLinePattern look for right after the prefix. "
              "See docs/SessionLog-rationale.md section 6.");

struct AboveTestedStep
{
	bool nowAbove = false;
	bool logCrossing = false;
};

constexpr AboveTestedStep aboveTestedAfter(bool wasAbove, int32 players, int32 tested)
{
	const bool nowAbove = players > tested;
	return { nowAbove, nowAbove && !wasAbove };
}

static_assert(aboveTestedAfter(false, 4, 3).logCrossing && aboveTestedAfter(false, 4, 3).nowAbove
                  && !aboveTestedAfter(true, 5, 3).logCrossing && aboveTestedAfter(true, 5, 3).nowAbove
                  && !aboveTestedAfter(true, 3, 3).logCrossing && !aboveTestedAfter(true, 3, 3).nowAbove
                  && !aboveTestedAfter(false, 3, 3).logCrossing && !aboveTestedAfter(false, 3, 3).nowAbove,
              "Was the backlog's 'one above tested size line per crossing' (task 13): the line is "
              "logged when players goes from <= tested to > tested, and re-armed only when it "
              "drops back to <= tested. See docs/SessionLog-rationale.md section 3.");

struct PlayerCounts
{
	int32 players = 0;
	int32 onConnection = 0;
};

PlayerCounts countPlayers(const UWorld& world, const APlayerController* connectionOf, const AController* exiting);

class SessionLog
{
public:
	void noteListening(const AGameModeBase& gameMode);

	void noteJoined(const AGameModeBase& gameMode, const APlayerController& joined);

	void noteLeft(const AGameModeBase& gameMode, const AController* exiting);

private:
	bool m_aboveTestedSize = false;
};

} // namespace sessionLogUImpl
