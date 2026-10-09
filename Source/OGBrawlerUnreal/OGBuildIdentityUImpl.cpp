// SPDX-License-Identifier: BUSL-1.1
// docs/OGBuildIdentity-rationale.md · docs/OGBuildIdentity-guards.md

#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/NetworkVersion.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include <cinttypes>
#include <cstdio>

#include "OGBrawlerUnreal/JoinScreenUImpl.h"
#include "OGBrawlerUnreal/PhysicsBackendUImpl.h"

DEFINE_LOG_CATEGORY(LogOGBuildIdentity);

namespace
{

using buildIdentityUImpl::BuildIdentity;
using buildIdentityUImpl::BuildLabelSource;

const TCHAR* sourceName(BuildLabelSource source)
{
	switch (source)
	{
	case BuildLabelSource::None:
		return TEXT("none");
	case BuildLabelSource::CommandLine:
		return TEXT("command-line");
	case BuildLabelSource::BuildInfoFile:
		return TEXT("build_info");
	}
	return TEXT("none");
}

BuildIdentity devIdentity()
{
	return BuildIdentity{ std::string(brawlerJoinScreen::kDevBuildLabel), BuildLabelSource::None };
}

BuildIdentity resolveBuildIdentity()
{
	FString commandLineLabel;
	if (FParse::Value(FCommandLine::Get(), TEXT("-OGBuildLabel="), commandLineLabel))
	{
		std::string label = joinScreenUImpl::toUtf8(commandLineLabel);
		if (buildIdentityUImpl::isValidBuildLabel(label))
			return BuildIdentity{ std::move(label), BuildLabelSource::CommandLine };

		UE_LOG(LogOGBuildIdentity, Error,
			TEXT("OGBuildIdentity: -OGBuildLabel=%s is not a build label (1-%d characters of 0-9 A-Z a-z . _ -); using %s."),
			*commandLineLabel, static_cast<int32>(buildIdentityUImpl::kMaxBuildLabelLength),
			*joinScreenUImpl::toFString(brawlerJoinScreen::kDevBuildLabel));
		return devIdentity();
	}

	const FString path = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::RootDir()),
		joinScreenUImpl::toFString(buildIdentityUImpl::kBuildInfoFileName));

	TArray<uint8> bytes;
	if (!FFileHelper::LoadFileToArray(bytes, *path, FILEREAD_Silent))
		return devIdentity();

	const buildIdentityUImpl::BuildInfoLabel parsed = buildIdentityUImpl::labelFromBuildInfo(
		std::string_view(reinterpret_cast<const char*>(bytes.GetData()), static_cast<std::size_t>(bytes.Num())));
	if (parsed.status == buildIdentityUImpl::BuildInfoLabelStatus::Valid)
		return BuildIdentity{ std::string(parsed.label), BuildLabelSource::BuildInfoFile };

	UE_LOG(LogOGBuildIdentity, Error, TEXT("OGBuildIdentity: %s has %s label= line; using %s."), *path,
		parsed.status == buildIdentityUImpl::BuildInfoLabelStatus::Missing ? TEXT("no") : TEXT("an invalid"),
		*joinScreenUImpl::toFString(brawlerJoinScreen::kDevBuildLabel));
	return devIdentity();
}

struct BackendIdentity
{
	std::string fingerprint;
	std::string token;
};

void logJoltToBuildIdentity(const char* message)
{
	const FString line(UTF8_TO_TCHAR(message));
	if (line.StartsWith(TEXT("[Warning]")))
	{
		UE_LOG(LogOGBuildIdentity, Warning, TEXT("OGBuildIdentity: Jolt: %s"), *line);
	}
	else
	{
		UE_LOG(LogOGBuildIdentity, Log, TEXT("OGBuildIdentity: Jolt: %s"), *line);
	}
}

BackendIdentity resolveBackendIdentity()
{
	BackendIdentity identity{ "none", std::string(physicsBackendUImpl::kBackendName) };
	if (const std::optional<uint64_t> value = physicsBackendUImpl::computeDeterminismFingerprint(&logJoltToBuildIdentity))
	{
		char hex[17];
		std::snprintf(hex, sizeof(hex), "%016" PRIx64, *value);
		identity.fingerprint = hex;
		identity.token += ':';
		identity.token += hex;
	}
	return identity;
}

const BackendIdentity& backendIdentity()
{
	static const BackendIdentity identity = resolveBackendIdentity();
	return identity;
}

FString backendRefusalText(buildIdentityUImpl::BackendLoginVerdict verdict, const FString& serverToken,
	const FString& clientToken)
{
	using buildIdentityUImpl::BackendLoginVerdict;

	switch (verdict)
	{
	case BackendLoginVerdict::MissingFromClient:
		return FString::Printf(
			TEXT("Different build: the client reports no physics backend, this server runs %s."), *serverToken);
	case BackendLoginVerdict::DifferentBackend:
		return FString::Printf(TEXT("Different physics backend: this server runs %s, the client runs %s."),
			*serverToken, *clientToken);
	case BackendLoginVerdict::DifferentFingerprint:
	case BackendLoginVerdict::Match:
		break;
	}
	return FString();
}

void applyNetworkVersion()
{
	const BuildIdentity& identity = buildIdentityUImpl::buildIdentity();
	const FString        label    = joinScreenUImpl::toFString(identity.label);
	const FString        backend  = joinScreenUImpl::toFString(buildIdentityUImpl::backendToken());

	if (buildIdentityUImpl::overridesNetworkVersion(identity.label))
		FNetworkVersion::SetProjectVersion(*(FNetworkVersion::GetProjectVersion() + TEXT("+") + label));

	UE_LOG(LogOGBuildIdentity, Display,
		TEXT("OGBuildIdentity: label=%s source=%s backend=%s networkProjectVersion=%s"), *label,
		sourceName(identity.source), *backend, *FNetworkVersion::GetProjectVersion());
}

} // namespace

namespace buildIdentityUImpl
{

const BuildIdentity& buildIdentity()
{
	static const BuildIdentity identity = resolveBuildIdentity();
	return identity;
}

std::string_view buildLabel()
{
	return buildIdentity().label;
}

std::string_view backendFingerprint()
{
	return backendIdentity().fingerprint;
}

std::string_view backendToken()
{
	return backendIdentity().token;
}

FString backendLoginOption()
{
	return joinScreenUImpl::toFString(kBackendLoginOptionKey) + TEXT("=") + joinScreenUImpl::toFString(backendToken());
}

bool refuseMismatchedBackendLogin(const FString& loginOptions, FString& errorMessage)
{
	const FString clientToken = UGameplayStatics::ParseOption(loginOptions, joinScreenUImpl::toFString(kBackendLoginOptionKey));
	const BackendLoginVerdict verdict = classifyBackendLogin(backendToken(), joinScreenUImpl::toUtf8(clientToken));
	if (verdict == BackendLoginVerdict::DifferentFingerprint)
	{
		UE_LOG(LogOGBuildIdentity, Warning,
			TEXT("OGBuildIdentity: admitted a login with a different Jolt determinism fingerprint: this server runs %s, "
			     "the client runs %s."),
			*joinScreenUImpl::toFString(backendToken()), *clientToken);
	}
	if (!refusesBackendLogin(verdict))
		return false;

	errorMessage = backendRefusalText(verdict, joinScreenUImpl::toFString(backendToken()), clientToken);
	UE_LOG(LogOGBuildIdentity, Warning, TEXT("OGBuildIdentity: refused a login: %s"), *errorMessage);
	return true;
}

void registerNetworkVersionHook()
{
	// ⛔G-01  docs/OGBuildIdentity-guards.md
	FCoreDelegates::OnPostEngineInit.AddStatic(&applyNetworkVersion);
}

} // namespace buildIdentityUImpl
