// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerChaosBindingUImpl-rationale.md · docs/BrawlerChaosBindingUImpl-guards.md

#if OG_PHYSICS_BACKEND_CHAOS
#include "BrawlerChaosBindingUImpl.h"

#include "OGBrawler/CollisionCategoryConstants.h"

std::initializer_list<ChaosCategoryMapping> BrawlerChaosBindingUImpl::categoryChannels()
{
	static const std::initializer_list<ChaosCategoryMapping> channels{
		{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
		{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
		{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
		{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 },
		{ collisionCategory::world,        ECollisionChannel::ECC_WorldStatic       },
		{ collisionCategory::character,    ECollisionChannel::ECC_GameTraceChannel6 }
	};
	return channels;
}

#endif
