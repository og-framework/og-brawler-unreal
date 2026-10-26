// SPDX-License-Identifier: BUSL-1.1
// docs/OGBuildIdentity-rationale.md · docs/OGBuildIdentity-guards.md

#include "OGBrawlerUnreal/OGBrawlerLocalPlayer.h"

#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"

FString UOGBrawlerLocalPlayer::GetGameLoginOptions() const
{
	// ⛔G-02  docs/OGBuildIdentity-guards.md
	return buildIdentityUImpl::backendLoginOption() + TEXT("?") + buildIdentityUImpl::wireLoginOption();
}
