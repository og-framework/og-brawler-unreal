// SPDX-License-Identifier: BUSL-1.1
// docs/JoinScreen-rationale.md · docs/JoinScreen-guards.md

#include "OGBrawlerUnreal/JoinScreenUImpl.h"

#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"

DEFINE_LOG_CATEGORY(LogOGJoinScreen);

namespace
{

float GJoinScreenScale = brawlerJoinScreen::kJoinScreenDefaultScale;

static_assert(brawlerJoinScreen::kJoinScreenDefaultScale == 1.f,
              "Was prose: the CVar help string below says `Default 1.0`, and so does "
              "docs/JoinScreen-rationale.md section 6. Retune the sentences too.");
static_assert(brawlerJoinScreen::kJoinScreenMinScale == 0.5f
                  && brawlerJoinScreen::kJoinScreenMaxScale == 4.f,
              "Was prose: the CVar help string below says `CLAMPED to [0.5, 4]`, and "
              "docs/JoinScreen-rationale.md section 6 repeats the range.");
static_assert(brawlerJoinScreen::kJoinScreenReferenceCanvasHeight == 720.f,
              "Was prose: the CVar help string below says `1.0 is sized for a 720-pixel-high "
              "window`.");

static FAutoConsoleVariableRef CVarJoinScreenScale(
	TEXT("OGBrawler.JoinScreenScale"),
	GJoinScreenScale,
	TEXT("Size multiplier for the join screen (the front-end a client shows when it is ")
	TEXT("started without a server address). Default 1.0; CLAMPED to [0.5, 4] when read. ")
	TEXT("1.0 is sized for a 720-pixel-high window and grows with the window height; the ")
	TEXT("panel then shrinks to fit the window, so it is never cut off."),
	ECVF_Default);

const TCHAR* const kRecentAddressSection = TEXT("JoinScreen");
const TCHAR* const kRecentAddressKey     = TEXT("RecentAddresses");

FKey keyNamed(std::string_view keyName)
{
	const FKey key{ FName(*joinScreenUImpl::toFString(keyName)) };
	checkf(key.IsValid(),
		TEXT("brawlerJoinScreen::kLocalCoopKeyNames holds '%s', which is not an engine key name. ")
		TEXT("The local co-op hint shows this text and the player controller binds this key, so ")
		TEXT("both must be an EKeys name such as Tab or End. See og-brawler docs/BrawlerJoinScreen-guards.md."),
		*joinScreenUImpl::toFString(keyName));
	return key;
}

} // namespace

namespace joinScreenUImpl
{

float scale()
{
	return brawlerJoinScreen::clampJoinScreenScale(GJoinScreenScale);
}

FString toFString(std::string_view text)
{
	const FUTF8ToTCHAR converted(text.data(), static_cast<int32>(text.size()));
	return FString::ConstructFromPtrSize(converted.Get(), converted.Length());
}

std::string toUtf8(const FString& text)
{
	const FTCHARToUTF8 converted(*text, text.Len());
	return std::string(converted.Get(), static_cast<std::size_t>(converted.Length()));
}

bool editorLaunched()
{
	return GIsEditor || FParse::Param(FCommandLine::Get(), TEXT("PIEVIACONSOLE"));
}

FString commandLineAddress()
{
	const TCHAR* const commandLine = FCommandLine::Get();

	const std::basic_string_view<TCHAR> token =
		brawlerJoinScreen::commandLineMapOverrideToken(std::basic_string_view<TCHAR>(commandLine));
	if (!token.empty())
		return FString::ConstructFromPtrSize(token.data(), static_cast<int32>(token.size()));

#if !UE_BUILD_SHIPPING
	FString simulatedShippingAddress;
	if (FParse::Value(commandLine, TEXT("-OGFrontEndAddress="), simulatedShippingAddress))
		return simulatedShippingAddress;
#endif

	return FString();
}

std::string_view ownBuildLabel()
{
	return buildIdentityUImpl::buildLabel();
}

brawlerJoinScreen::RecentAddressList loadRecentAddresses()
{
	FString line;
	if (GConfig == nullptr
		|| !GConfig->GetString(kRecentAddressSection, kRecentAddressKey, line, GGameUserSettingsIni))
	{
		return brawlerJoinScreen::RecentAddressList{};
	}

	return brawlerJoinScreen::RecentAddressList::deserialize(toUtf8(line));
}

bool saveRecentAddresses(const brawlerJoinScreen::RecentAddressList& recents)
{
	// ⛔G-03  docs/JoinScreen-guards.md
	if (editorLaunched() || GConfig == nullptr)
		return false;

	GConfig->SetString(kRecentAddressSection, kRecentAddressKey,
		*toFString(recents.serialize()), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	return true;
}

FKey localCoopAddPlayerKey()
{
	return keyNamed(brawlerJoinScreen::kLocalCoopKeyNames.addPlayer);
}

FKey localCoopRemovePlayerKey()
{
	return keyNamed(brawlerJoinScreen::kLocalCoopKeyNames.removePlayer);
}

} // namespace joinScreenUImpl
