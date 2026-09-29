// SPDX-License-Identifier: BUSL-1.1
// docs/OGBuildIdentity-rationale.md · docs/OGBuildIdentity-guards.md

#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"

#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/NetworkVersion.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include "OGBrawlerUnreal/JoinScreenUImpl.h"

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

void applyNetworkVersion()
{
	const BuildIdentity& identity = buildIdentityUImpl::buildIdentity();
	const FString        label    = joinScreenUImpl::toFString(identity.label);

	if (buildIdentityUImpl::overridesNetworkVersion(identity.label))
		FNetworkVersion::SetProjectVersion(*(FNetworkVersion::GetProjectVersion() + TEXT("+") + label));

	UE_LOG(LogOGBuildIdentity, Display, TEXT("OGBuildIdentity: label=%s source=%s networkProjectVersion=%s"), *label,
		sourceName(identity.source), *FNetworkVersion::GetProjectVersion());
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

void registerNetworkVersionHook()
{
	// ⛔G-01  docs/OGBuildIdentity-guards.md
	FCoreDelegates::OnPostEngineInit.AddStatic(&applyNetworkVersion);
}

} // namespace buildIdentityUImpl
