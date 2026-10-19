// SPDX-License-Identifier: MPL-2.0
// docs/TickDriverBackendUImpl-rationale.md · docs/TickDriverBackendUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/TickDriverConcepts.h"
#include "OGSimulation/SimulationTickBackend.h"
#include "OGSimulation/PhysicsEngineTraits.h"
#include "OGSimulationUnreal/UEPresentControls.h"
#include "OGSimulationUnreal/UERenderTargetTable.h"
#include "OGSimulationUnreal/UEStaticGeometryImporter.h"
#include "OGSimulationUnreal/UETickDriverHost.h"
#include "Components/CapsuleComponent.h"
#include "Templates/Casts.h"

#include <cstdint>
#include <optional>

class AActor;
class UWorld;

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
concept TickDriverBackendComposition = PhysicsEngineTraits<EngineTraitsT> && EngineTraitsT::kHasWorldHost
	&& PhysicsEngineBinding<EngineBindingT, EngineTraitsT>
	&& TickDriverWorldHost<typename EngineTraitsT::template WorldHost<typename EngineBindingT::Simulatable>>
	&& requires(const typename GameBindingT::Character& character, unsigned int id)
	{
		EngineBindingT::checkCapsuleContract(character, id);
	};

template <typename WorldHostT>
struct TickDriverBackendWorldsUImpl
{
	SimulationEngineUImpl engine;
	WorldHostT            host{ engine };
};

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
class TickDriverBackendUImpl
{
public:
	using WorldHost   = typename EngineTraitsT::template WorldHost<typename EngineBindingT::Simulatable>;
	using Worlds      = TickDriverBackendWorldsUImpl<WorldHost>;
	using Simulatable = typename EngineBindingT::Simulatable;
	using Session     = typename GameBindingT::Session;
	using Component   = typename GameBindingT::Component;
	using Character   = typename GameBindingT::Character;

	TickDriverBackendUImpl(Session& owner, Worlds& worlds)
		: m_owner(owner)
		, m_worlds(worlds)
		, m_tickDriverGame(owner)
		, m_tickBackend(m_tickDriverGame, worlds.host, m_frameHost.engine())
	{
	}

	TickDriverBackendUImpl(const TickDriverBackendUImpl&) = delete;
	TickDriverBackendUImpl& operator=(const TickDriverBackendUImpl&) = delete;

	void buildWorld(UWorld& world, float dt, bool worldIsAuthority);
	void startStepping(UWorld& world, float dt, bool worldIsAuthority);
	void onEndPlayBegin();
	void onEndPlayWorld(UWorld&) {}
	void beforePresent_GameThread();
	void updateVisualization_GameThread();

	typename EngineTraitsT::VizQuery&        editVizQuery();
	const typename EngineTraitsT::VizReader& getVizReader() const;
	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner);

	bool createBodies_GameThread(Component& owner, Character& character, Simulatable& simulatable, unsigned int id);
	void onRegistered_GameThread(Component& owner, unsigned int id);
	void onUnregistered_GameThread(Component& owner, unsigned int id);

	struct Diagnostics
	{
		void sampleLatencyWindow_GameThread(bool) {}
		void logLatencyWindow_GameThread(bool) {}
	};
	Diagnostics editDiagnostics() { return {}; }

private:
	static uint64 slotKeyOf(const AActor* owner);

	template <typename SlotCallF>
	decltype(auto) callWithSlotRequest(AActor& owner, SlotCallF&& slotCall);

	void startDriverStepping(UWorld& world, float dt, bool worldIsAuthority);

	Session& m_owner;
	Worlds&  m_worlds;

	using TickBackend = SimulationTickBackend<GameBindingT, WorldHost, Simulatable, SimulationEngineUImpl>;

	UETickDriverHost    m_frameHost;
	GameBindingT        m_tickDriverGame;
	UERenderTargetTable m_renderTargets;
	TickBackend         m_tickBackend;

	static constexpr bool compositionContractsHold()
	{
		static_assert(__builtin_offsetof(Worlds, engine) < __builtin_offsetof(Worlds, host),
			"TickDriverBackendWorldsUImpl members construct in declaration order and destruct in REVERSE. The world host "
			"holds its engine (the clock, the frame number and the LogOGSimHost sink of its lines) by reference from its "
			"constructor on, so the engine must be built first and go last. Keep engine < host. The worlds, the runtime "
			"lease and the contact counter are the host's own members (og-simulation-jolt's two destruction-order rows), "
			"and were this struct's members and two static_asserts until og-tickdriver task 10. Was this struct's own "
			"file-scope static_assert until og-tickdriver task 15b made the struct a template over the world host.");
		static_assert(__builtin_offsetof(TickDriverBackendUImpl, m_frameHost) < __builtin_offsetof(TickDriverBackendUImpl, m_tickBackend)
		           && __builtin_offsetof(TickDriverBackendUImpl, m_tickDriverGame) < __builtin_offsetof(TickDriverBackendUImpl, m_tickBackend),
			"TickDriverBackendUImpl members destruct in REVERSE declaration order. The tick backend's driver and presenter "
			"hold the frame host's engine and the game binding, and the driver's step driver unregisters its resync "
			"callback from the manager's client clock when it goes, so the tick backend must go first. Keep m_frameHost "
			"and m_tickDriverGame < m_tickBackend; that its driver goes before its presenter and the presenter before the "
			"render source is og-simulation's SimulationTickBackend's own row; that all of them go while the manager is "
			"alive is the session's own row (GameBindingT::Session's m_manager < m_backend); and that the world host "
			"(m_backendWorlds) outlives the driver is asserted in startDriverStepping. Was the rows 'm_frameHost, "
			"m_tickDriverGame and m_presenter < m_tickDriver' and 'm_frameHost and m_renderSource < m_presenter' until "
			"og-tickdriver task 15d moved the presenter, the driver and the render source into the tick backend.");
		return true;
	}

	void assertCompositionContracts()
	{
		static_assert(compositionContractsHold());
	}
};

#define HasAuthority HasAuthority_is_not_the_peer_role_in_TickDriverBackendUImpl_use_worldIsAuthority_or_runsPrediction

OGSIM_OPTIMIZE_OFF

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::buildWorld(UWorld& world, float, bool worldIsAuthority)
{
	const UEStaticImportResult import = UEStaticGeometryImporter(EngineBindingT::categoryChannels()).importWorld(world);
	UEStaticGeometryImporter::logReport(import.report, worldIsAuthority ? TEXT("authority") : TEXT("client"));
	const typename EngineTraitsT::Statics statics = EngineTraitsT::splitStatics(import.description, import.physicsOnly);

	typename WorldHost::Config config;
	config.isAuthority      = worldIsAuthority;
	config.runInline        = UETickDriverHost::runInline();
	config.gravity          = glm::vec3(0.f, 0.f, GameBindingT::gravity(m_owner));
	config.contactCounts    = EngineBindingT::contactCountsEnabled();
	config.mappedCategories = EngineBindingT::mappedCategories();
	config.log              = &EngineTraitsT::logEngineMessage;
	m_tickBackend.build(config, statics.queryable, statics.physicsOnly, m_owner.m_physAdapter, m_owner.m_physReaderAdapter,
		m_owner.m_queryAdapter);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::startStepping(UWorld& world, float dt, bool worldIsAuthority)
{
	startDriverStepping(world, dt, worldIsAuthority);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onEndPlayBegin()
{
	m_frameHost.unregisterTickFunctions();
	// ⛔G-01  docs/TickDriverBackendUImpl-guards.md
	m_tickBackend.stop();
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::beforePresent_GameThread()
{
	m_tickBackend.present(UEPresentControls::fromConsoleVariables(EngineBindingT::kParkSnap), m_renderTargets);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::updateVisualization_GameThread()
{
	m_tickBackend.updateVisualization();
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
typename EngineTraitsT::VizQuery& TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::editVizQuery()
{
	return m_tickBackend.editVizQuery();
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
const typename EngineTraitsT::VizReader& TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::getVizReader() const
{
	return m_tickBackend.getVizReader();
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
uint64 TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::slotKeyOf(const AActor* owner)
{
	return static_cast<uint64>(reinterpret_cast<UPTRINT>(owner));
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
template <typename SlotCallF>
decltype(auto) TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::callWithSlotRequest(AActor& owner, SlotCallF&& slotCall)
{
	return GameBindingT::callWithSlotRequest(m_owner, owner, slotKeyOf(&owner), slotCall);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
std::optional<QueryVolumeId> TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner)
{
	return callWithSlotRequest(owner, [this, &descriptor](uint64_t key, const char* name, bool isAuthority,
		std::optional<uint32_t> replicatedSlot, auto&& publish)
	{
		return m_tickBackend.registerVizVolume(descriptor, key, name, isAuthority, replicatedSlot, publish);
	});
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
bool TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::createBodies_GameThread(Component& owner, Character& character,
	Simulatable& simulatable, unsigned int id)
{
	EngineBindingT::checkCapsuleContract(character, id);

	AActor* ownerActor = owner.GetOwner();
	const typename GameBindingT::StaticData& staticData = owner.getStaticData();
	return callWithSlotRequest(*ownerActor, [this, id, &simulatable, &staticData](uint64_t key, const char* name,
		bool isAuthority, std::optional<uint32_t> replicatedSlot, auto&& publish)
	{
		return m_tickBackend.acquireAndBind(key, name, isAuthority, replicatedSlot, publish, id, simulatable, staticData);
	});
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onRegistered_GameThread(Component& owner, unsigned int id)
{
	m_tickBackend.onRegistered(id, slotKeyOf(owner.GetOwner()));
	Character* const character = Cast<Character>(owner.GetOwner());
	m_renderTargets.registerTarget(id, character, character != nullptr ? character->GetCapsuleComponent() : nullptr,
		m_owner.runsPrediction());
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onUnregistered_GameThread(Component& owner, unsigned int id)
{
	const uint64 key = slotKeyOf(owner.GetOwner());
	m_tickBackend.forgetPresenterTarget(id, key);
	m_renderTargets.unregisterTarget(id);
	if (owner.GetOwner() != nullptr)
		m_tickBackend.releaseSlot(key, id);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::startDriverStepping(UWorld& world, float dt, bool worldIsAuthority)
{
	static_assert(__builtin_offsetof(Session, m_backendWorlds)
	                  < __builtin_offsetof(Session, m_backend),
		"The session's members (GameBindingT::Session) destruct in REVERSE declaration order. The tick driver built here "
		"(in m_backend) holds the world host (in m_backendWorlds) by pointer and locks it and steps its world until it "
		"stops, so the world host must outlive the driver. Keep m_backendWorlds < m_backend. Was implied by the owner's "
		"two rows until og-tickdriver task 10, when the driver's world host moved out of this class.");

	checkf(world.PersistentLevel != nullptr, TEXT("startDriverStepping: the world has no persistent level."));

	typename TickBackend::Driver& tickDriver = m_tickBackend.start(static_cast<double>(dt), worldIsAuthority,
		UETickDriverHost::requestedMaxCatchUpSteps());

	m_frameHost.begin(m_owner, *world.PersistentLevel, tickDriver);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
