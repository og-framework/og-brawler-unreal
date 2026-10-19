// SPDX-License-Identifier: BUSL-1.1
// docs/OGBuildIdentity-rationale.md

#include "OGBrawlerUnreal/OGBrawlerLocalPlayer.h"

#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"

FString UOGBrawlerLocalPlayer::GetGameLoginOptions() const
{
	return buildIdentityUImpl::backendLoginOption();
}
