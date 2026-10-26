// SPDX-License-Identifier: BUSL-1.1
// docs/OGBuildIdentity-rationale.md · docs/OGBuildIdentity-guards.md

#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>

#include "OGBrawler/BrawlerJoinScreen.h"
#include "OGSimulation/SimulationFrame.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOGBuildIdentity, Log, All);

namespace buildIdentityUImpl
{

inline constexpr std::string_view kBuildInfoFileName   = "build_info.txt";
inline constexpr std::string_view kBuildInfoLabelKey   = "label=";
inline constexpr std::size_t      kMaxBuildLabelLength = 64;

enum class BuildLabelSource : uint8
{
	None,
	CommandLine,
	BuildInfoFile,
};

enum class BuildInfoLabelStatus : uint8
{
	Valid,
	Missing,
	Invalid,
};

struct BuildInfoLabel
{
	BuildInfoLabelStatus status = BuildInfoLabelStatus::Missing;
	std::string_view     label;
};

constexpr bool isBuildLabelCharacter(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_'
	    || c == '.';
}

constexpr bool isValidBuildLabel(std::string_view label)
{
	if (label.empty() || label.size() > kMaxBuildLabelLength)
		return false;
	for (const char c : label)
	{
		if (!isBuildLabelCharacter(c))
			return false;
	}
	return true;
}

constexpr BuildInfoLabel labelFromBuildInfo(std::string_view text)
{
	constexpr std::string_view utf8ByteOrderMark = "\xEF\xBB\xBF";
	if (text.substr(0, utf8ByteOrderMark.size()) == utf8ByteOrderMark)
		text.remove_prefix(utf8ByteOrderMark.size());

	while (!text.empty())
	{
		const std::size_t lineEnd = text.find('\n');
		std::string_view  line    = text.substr(0, lineEnd);
		text = (lineEnd == std::string_view::npos) ? std::string_view{} : text.substr(lineEnd + 1);

		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);

		if (line.substr(0, kBuildInfoLabelKey.size()) != kBuildInfoLabelKey)
			continue;

		const std::string_view label = line.substr(kBuildInfoLabelKey.size());
		return isValidBuildLabel(label) ? BuildInfoLabel{ BuildInfoLabelStatus::Valid, label }
		                                : BuildInfoLabel{ BuildInfoLabelStatus::Invalid, label };
	}
	return BuildInfoLabel{ BuildInfoLabelStatus::Missing, {} };
}

constexpr bool overridesNetworkVersion(std::string_view label)
{
	return label != brawlerJoinScreen::kDevBuildLabel;
}

static_assert(labelFromBuildInfo("label=20260929-101500-abc1234\nsha=abc1234\ndirty=false\ncreated=2026-09-29T10:15:00Z\n")
                      .label == "20260929-101500-abc1234"
                  && labelFromBuildInfo("\xEF\xBB\xBFsha=abc1234\r\nlabel=20260929-101500-abc1234-dirty\r\n").label
                      == "20260929-101500-abc1234-dirty"
                  && labelFromBuildInfo("label=A").status == BuildInfoLabelStatus::Valid,
              "Was the task 14 build_info.txt format (og-tools Publish-OgSteamBuild: UTF-8 without BOM, LF, "
              "label= on the first line) plus the task 14 note that a hand-edited file may carry CR and a "
              "BOM. See docs/OGBuildIdentity-rationale.md section 2.");
static_assert(labelFromBuildInfo("sha=abc1234\ndirty=false\n").status == BuildInfoLabelStatus::Missing
                  && labelFromBuildInfo("").status == BuildInfoLabelStatus::Missing
                  && labelFromBuildInfo("label=\n").status == BuildInfoLabelStatus::Invalid
                  && labelFromBuildInfo("label=two words\n").status == BuildInfoLabelStatus::Invalid
                  && labelFromBuildInfo(" label=x\n").status == BuildInfoLabelStatus::Missing,
              "Was the label rule: a label is 1-64 characters of [0-9A-Za-z._-], so it can never contain the "
              "space that separates the server's session-log fields. See docs/OGBuildIdentity-rationale.md "
              "section 2.");
static_assert(!overridesNetworkVersion(brawlerJoinScreen::kDevBuildLabel) && overridesNetworkVersion("A"),
              "Was the task 15 rule 'absent build_info.txt => label dev and engine default behaviour "
              "unchanged': the dev label must never touch the network version, so the editor and PIE "
              "keep the engine's version. See docs/OGBuildIdentity-rationale.md section 3.");

inline constexpr std::string_view kBackendLoginOptionKey = "OGBackend";

enum class BackendLoginVerdict : uint8
{
	Match,
	MissingFromClient,
	DifferentBackend,
	DifferentFingerprint,
};

constexpr std::string_view backendNameOf(std::string_view token)
{
	return token.substr(0, token.find(':'));
}

constexpr BackendLoginVerdict classifyBackendLogin(std::string_view serverToken, std::string_view clientToken)
{
	if (clientToken.empty())
		return BackendLoginVerdict::MissingFromClient;
	if (clientToken == serverToken)
		return BackendLoginVerdict::Match;
	return backendNameOf(clientToken) == backendNameOf(serverToken) ? BackendLoginVerdict::DifferentFingerprint
	                                                                 : BackendLoginVerdict::DifferentBackend;
}

constexpr bool refusesBackendLogin(BackendLoginVerdict verdict)
{
	return verdict == BackendLoginVerdict::MissingFromClient || verdict == BackendLoginVerdict::DifferentBackend;
}

static_assert(classifyBackendLogin("chaos", "chaos") == BackendLoginVerdict::Match
                  && classifyBackendLogin("jolt:00000000000000aa", "jolt:00000000000000aa") == BackendLoginVerdict::Match
                  && classifyBackendLogin("jolt:00000000000000aa", "chaos") == BackendLoginVerdict::DifferentBackend
                  && classifyBackendLogin("chaos", "jolt:00000000000000aa") == BackendLoginVerdict::DifferentBackend
                  && classifyBackendLogin("jolt:00000000000000aa", "jolt:00000000000000ab")
                      == BackendLoginVerdict::DifferentFingerprint
                  && classifyBackendLogin("jolt:00000000000000aa", "") == BackendLoginVerdict::MissingFromClient,
              "Was the task 18 classification of a login's backend token (chaos, or jolt:<determinism fingerprint>) "
              "against the server's. See docs/OGBuildIdentity-rationale.md section 7.");
static_assert(refusesBackendLogin(BackendLoginVerdict::DifferentBackend)
                  && refusesBackendLogin(BackendLoginVerdict::MissingFromClient)
                  && !refusesBackendLogin(BackendLoginVerdict::DifferentFingerprint)
                  && !refusesBackendLogin(BackendLoginVerdict::Match),
              "Was the user's 'backend only' ruling (2026-10-09): a different physics backend or a missing token is "
              "refused; a different Jolt determinism fingerprint is admitted with a Warning, because an Android arm64 "
              "client always differs from a Win64 server and the netcode corrects rather than runs in lockstep. See "
              "docs/OGBuildIdentity-rationale.md section 7.");

inline constexpr std::string_view kWireLoginOptionKey = "OGWire";

struct WireToken
{
	char        text[16] = {};
	std::size_t length   = 0;

	constexpr std::string_view view() const { return std::string_view(text, length); }
};

constexpr WireToken wireTokenFor(uint8 correctionVersion, uint8 ringVersion, uint8 bundleVersion)
{
	WireToken  token;
	const auto append = [&token](char prefix, uint8 version)
	{
		token.text[token.length++] = prefix;
		if (version >= 100)
			token.text[token.length++] = static_cast<char>('0' + version / 100);
		if (version >= 10)
			token.text[token.length++] = static_cast<char>('0' + version / 10 % 10);
		token.text[token.length++] = static_cast<char>('0' + version % 10);
	};
	append('c', correctionVersion);
	token.text[token.length++] = '.';
	append('r', ringVersion);
	token.text[token.length++] = '.';
	append('b', bundleVersion);
	return token;
}

enum class WireLoginVerdict : uint8
{
	Match,
	Missing,
	Different,
};

constexpr WireLoginVerdict classifyWireLogin(std::string_view serverToken, std::string_view clientToken)
{
	if (clientToken.empty())
		return WireLoginVerdict::Missing;
	return clientToken == serverToken ? WireLoginVerdict::Match : WireLoginVerdict::Different;
}

constexpr bool refusesWireLogin(WireLoginVerdict verdict)
{
	return verdict != WireLoginVerdict::Match;
}

constexpr std::string_view unitsNameOf(float unitsPerMetre)
{
	return unitsPerMetre == 100.f ? "cm" : "m";
}

constexpr std::string_view axesNameOf(AxisFrame axisFrame)
{
	return axisFrame == AxisFrame::UeZUpLeftHanded ? "ZupLH" : "YupRH";
}

static_assert(wireTokenFor(6, 2, 2).view() == "c6.r2.b2" && wireTokenFor(8, 3, 3).view() == "c8.r3.b3"
                  && wireTokenFor(10, 2, 3).view() == "c10.r2.b3" && wireTokenFor(255, 100, 0).view() == "c255.r100.b0",
              "og-sim-metres-yup task 15 (design section 5): the wire token is c<correction>.r<ring>.b<bundle>, each "
              "version in decimal without leading zeros. See docs/OGBuildIdentity-rationale.md section 8.");
static_assert(classifyWireLogin("c6.r2.b2", "c6.r2.b2") == WireLoginVerdict::Match
                  && classifyWireLogin("c8.r3.b3", "c7.r2.b2") == WireLoginVerdict::Different
                  && classifyWireLogin("c6.r2.b2", "c6.r2.b3") == WireLoginVerdict::Different
                  && classifyWireLogin("c6.r2.b2", "c0.r0.b0") == WireLoginVerdict::Different
                  && classifyWireLogin("c6.r2.b2", "") == WireLoginVerdict::Missing,
              "og-sim-metres-yup task 15: a login's OGWire token is a match only when it equals the server's exactly; "
              "an absent or empty one is Missing. See docs/OGBuildIdentity-rationale.md section 8.");
static_assert(refusesWireLogin(WireLoginVerdict::Missing) && refusesWireLogin(WireLoginVerdict::Different)
                  && !refusesWireLogin(WireLoginVerdict::Match),
              "Was the user's ruling Q2 (og-sim-metres-yup, 2026-10-25): a login lacking OGWire, or with a different "
              "token, is refused, because peers of different wire versions cannot play together. See "
              "docs/OGBuildIdentity-rationale.md section 8.");
static_assert(unitsNameOf(100.f) == "cm" && unitsNameOf(1.f) == "m" && axesNameOf(AxisFrame::UeZUpLeftHanded) == "ZupLH"
                  && axesNameOf(AxisFrame::YUpRightHanded) == "YupRH",
              "og-sim-metres-yup task 15 (Q9): the start line's units= and axes= values, which the workspace tool "
              "capture_units.py reads as each capture's length unit. See docs/OGBuildIdentity-rationale.md section 8.");

struct BuildIdentity
{
	std::string      label;
	BuildLabelSource source = BuildLabelSource::None;
};

const BuildIdentity& buildIdentity();

std::string_view buildLabel();

std::string_view backendFingerprint();

std::string_view backendToken();

FString backendLoginOption();

std::string_view wireToken();

FString wireLoginOption();

bool refuseMismatchedBackendLogin(const FString& loginOptions, FString& errorMessage);

void registerNetworkVersionHook();

} // namespace buildIdentityUImpl
