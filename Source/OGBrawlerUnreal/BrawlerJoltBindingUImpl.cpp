// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerJoltBindingUImpl-rationale.md · docs/BrawlerJoltBindingUImpl-guards.md

#if !OG_PHYSICS_BACKEND_CHAOS
#include "BrawlerJoltBindingUImpl.h"

#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGSimulation/CompilerControl.h"
#include "OGBrawler/BrawlerMovementSimulation.h"
#include "OGBrawler/CollisionCategoryConstants.h"
#include "Components/CapsuleComponent.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarJoltContactCounts(
		TEXT("og.Jolt.ContactCounts"),
		0,
		TEXT("Jolt configuration only. 1 = count the step world's contacts with a passive Jolt contact listener and log one ")
		TEXT("[SimHost.Contacts] line per simulated step, forward and replayed, that touched a contact (LogOGSimHost, Verbose). ")
		TEXT("Read once, when the step world is built at the simulation manager's BeginPlay, so set it with ")
		TEXT("-ini:Engine:[ConsoleVariables]:og.Jolt.ContactCounts=1, not a later console command. 0 = nothing is installed."),
		ECVF_Default);
}

const std::vector<UEStaticCategoryChannel>& BrawlerJoltBindingUImpl::categoryChannels()
{
	static const std::vector<UEStaticCategoryChannel> channels{
		{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
		{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
		{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
		{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 },
		{ collisionCategory::world,        ECollisionChannel::ECC_WorldStatic       },
		{ collisionCategory::character,    ECollisionChannel::ECC_GameTraceChannel6 }
	};
	return channels;
}

uint32 BrawlerJoltBindingUImpl::mappedCategories()
{
	uint32 mapped = 0u;
	for (const UEStaticCategoryChannel& channel : categoryChannels())
		mapped |= 1u << channel.category;
	return mapped;
}

bool BrawlerJoltBindingUImpl::contactCountsEnabled()
{
	return CVarJoltContactCounts.GetValueOnGameThread() == 1;
}

OGSIM_OPTIMIZE_OFF

void BrawlerJoltBindingUImpl::checkCapsuleContract(const AOGBrawlerUECharacter& character, unsigned int id)
{
	const UCapsuleComponent* capsule = character.GetCapsuleComponent();
	const CapsuleGeometry* descriptorCapsule = std::get_if<CapsuleGeometry>(
		&brawlerMovementSimulation::PhysicsDeclaration::descriptor().shapes.front().geometry);
	checkf(descriptorCapsule != nullptr
			   && FMath::IsNearlyEqual(capsule->GetUnscaledCapsuleRadius(), descriptorCapsule->radius)
			   && FMath::IsNearlyEqual(capsule->GetUnscaledCapsuleHalfHeight(), descriptorCapsule->halfHeight),
		   TEXT("tryRegister: the pawn's capsule (radius %f, half-height %f) disagrees with the movement ")
		   TEXT("descriptor's CapsuleGeometry (radius %f, half-height %f), id=%u. The Jolt slot body is built ")
		   TEXT("from the descriptor, while the UE capsule is the aim plane and the camera anchor: change both ")
		   TEXT("together. Was the Chaos factory's adopt-root check (design D14; OGBrawlerUECharacter G-01)."),
		   capsule->GetUnscaledCapsuleRadius(), capsule->GetUnscaledCapsuleHalfHeight(),
		   descriptorCapsule != nullptr ? descriptorCapsule->radius : 0.f,
		   descriptorCapsule != nullptr ? descriptorCapsule->halfHeight : 0.f, id);
}

template class TickDriverBackendUImpl<BrawlerTickDriverGame, JoltEngineTraitsUImpl, BrawlerJoltBindingUImpl>;

OGSIM_OPTIMIZE_ON

#endif
