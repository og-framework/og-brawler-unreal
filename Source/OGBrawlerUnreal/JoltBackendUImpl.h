// SPDX-License-Identifier: BUSL-1.1
// docs/JoltBackendUImpl-rationale.md · docs/JoltBackendUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Async/Mutex.h"
#include "OGSimulation/StaticGeometry.h"
#include "OGSimulation/SimulationStepDriver.h"
#include "OGSimulation/RenderInterpolation.h"
#include "OGSimulation/RenderCorrectionSmoothing.h"
#include "OGBrawlerUnreal/SimulationFrameHostUImpl.h"
#include "OGBrawlerUnreal/BrawlerStepHooksUImpl.h"
#include "OGSimulationJolt/JoltContactCounter.h"
#include "OGBrawlerUnreal/PhysicsBackendUImpl.h"
#include "OGBrawlerUnreal/BrawlerCompositionUImpl.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

class ASimulationManagerUImpl;
class USimmableUpdateComponent;
class AOGBrawlerUECharacter;
class AActor;
class UWorld;

struct JoltRuntimeLeaseUImpl
{
	explicit JoltRuntimeLeaseUImpl(JoltRuntime::LogFn logger) : runtime(JoltRuntime::acquire(logger)) {}
	~JoltRuntimeLeaseUImpl() { JoltRuntime::release(); }

	JoltRuntimeLeaseUImpl(const JoltRuntimeLeaseUImpl&) = delete;
	JoltRuntimeLeaseUImpl& operator=(const JoltRuntimeLeaseUImpl&) = delete;

	JoltRuntime& runtime;
};

struct JoltBackendWorldsUImpl
{
	std::optional<JoltRuntimeLeaseUImpl>         m_joltRuntime;
	std::optional<JoltContactCounter>            m_joltContactCounter;
	std::optional<JoltWorld>                     m_joltWorld;
	std::optional<JoltWorld>                     m_shadowWorld;
	std::optional<JoltPhysicsBodyAdapter>        m_shadowBodyAdapter;
	std::optional<JoltPhysicsBodyReaderAdapter>  m_shadowReader;
	std::optional<JoltSpatialQueryAdapter>       m_shadowQuery;
	std::vector<SlotBodyTemplate>                m_joltSlotTemplate;
	std::unordered_map<const AActor*, uint32>    m_joltSlotByOwner;
	std::unordered_map<const AActor*, uint8>     m_joltSlotWaitsLoggedByOwner;
	UE::FMutex                                   m_joltWorldMutex;
	bool                                         m_joltStepsInline = true;
	uint8                                        m_joltRoleBit     = 0;
};

static_assert(__builtin_offsetof(JoltBackendWorldsUImpl, m_joltRuntime)
                  < __builtin_offsetof(JoltBackendWorldsUImpl, m_joltContactCounter)
           && __builtin_offsetof(JoltBackendWorldsUImpl, m_joltContactCounter)
                  < __builtin_offsetof(JoltBackendWorldsUImpl, m_joltWorld),
	"JoltBackendWorldsUImpl members destruct in REVERSE declaration order. The JoltRuntime lease must outlive the "
	"world (the last JoltRuntime::release unregisters Jolt's types and deletes its Factory), and the step world holds "
	"a raw pointer to the contact counter (its Jolt contact listener) until it is destroyed, so the counter must "
	"outlive the world. Keep m_joltRuntime < m_joltContactCounter < m_joltWorld; the world's adapters are the "
	"owner's, after its m_backendWorlds (ASimulationManagerUImpl's own row). Was design D's compile-time row "
	"'runtime < world < adapters' (task 18) and task 78's install rule; moved here by task 83.");

static_assert(__builtin_offsetof(JoltBackendWorldsUImpl, m_joltRuntime)
                  < __builtin_offsetof(JoltBackendWorldsUImpl, m_shadowWorld)
           && __builtin_offsetof(JoltBackendWorldsUImpl, m_shadowWorld)
                  < __builtin_offsetof(JoltBackendWorldsUImpl, m_shadowBodyAdapter)
           && __builtin_offsetof(JoltBackendWorldsUImpl, m_shadowBodyAdapter)
                  < __builtin_offsetof(JoltBackendWorldsUImpl, m_shadowReader)
           && __builtin_offsetof(JoltBackendWorldsUImpl, m_shadowBodyAdapter)
                  < __builtin_offsetof(JoltBackendWorldsUImpl, m_shadowQuery),
	"JoltBackendWorldsUImpl members destruct in REVERSE declaration order. The game-thread shadow world must outlive "
	"its own adapters (the reader holds the shadow body adapter's bind table), and the JoltRuntime lease must outlive "
	"the shadow world as it outlives the step world. Keep m_joltRuntime < m_shadowWorld < m_shadowBodyAdapter < "
	"{m_shadowReader, m_shadowQuery}. Was design D's 'runtime < world < adapters' row, applied to the shadow world "
	"(task 56); moved here by task 83.");

class JoltBackendUImpl
{
public:
	JoltBackendUImpl(ASimulationManagerUImpl& owner, JoltBackendWorldsUImpl& worlds) : m_owner(owner), m_worlds(worlds) {}

	JoltBackendUImpl(const JoltBackendUImpl&) = delete;
	JoltBackendUImpl& operator=(const JoltBackendUImpl&) = delete;

	void buildWorld(UWorld& world, float dt, bool worldIsAuthority);
	void startStepping(UWorld& world, float dt, bool worldIsAuthority);
	void onEndPlayBegin();
	void onEndPlayWorld(UWorld&) {}
	void beforePresent_GameThread();
	void updateVisualization_GameThread();

	physicsBackendUImpl::VizQuery&        editVizQuery();
	const physicsBackendUImpl::VizReader& getVizReader() const;
	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner);

	bool createBodies_GameThread(USimmableUpdateComponent& owner, AOGBrawlerUECharacter& character,
		SimulatableBrawler& simulatable, unsigned int id);
	void onRegistered_GameThread(USimmableUpdateComponent& owner, unsigned int id);
	void onUnregistered_GameThread(USimmableUpdateComponent& owner, unsigned int id);

	void sampleLatencyWindow_GameThread(bool) {}
	void logLatencyWindow_GameThread(bool) {}

private:
	friend class SimulationFrameHostUImpl;

	using RenderSnapshot = BrawlerCompositionUImpl::RenderSnapshot;

	void decideJoltSteppingMode(bool worldIsAuthority);
	void buildJoltWorld(UWorld& world, bool worldIsAuthority);
	std::optional<uint32> tryAcquireJoltSlot(AActor& owner);
	void releaseJoltSlot(const AActor& owner, unsigned int id);
	BodyId joltSlotRootBodyId(const JoltWorld& world, uint32 slot) const;
	uint32 boundJoltBodyCount() const;
	void startJoltStepping(UWorld& world, float dt, bool worldIsAuthority);
	void runJoltStep_Step(uint64 physicsStep, double stepDeadlineSeconds);
	bool isJoltWorldMutexHeldOnThisThread() const;
	bool isJoltStepRunningOnThisThread() const;

	void buildShadowWorld(const JoltWorldConfig& stepConfig, const StaticWorldDescription& queryableStatics,
		const StaticWorldDescription& physicsOnlyStatics, const std::vector<JPH::BodyID>& stepStaticBodies);
	void bindShadowSlot(uint32 slot, unsigned int id, const SimulatableBrawler& simulatable);
	void releaseShadowSlot(uint32 slot);
	void compareShadowBindTables_GameThread(const TCHAR* after, uint32 slot, unsigned int id, bool sameResults);

	ASimulationManagerUImpl& m_owner;
	JoltBackendWorldsUImpl&  m_worlds;

	using BrawlerStepDriver = SimulationStepDriver<BrawlerCompositionUImpl::Manager, JoltWorld, BrawlerStepHooksUImpl>;

	struct RenderTargetUImpl
	{
		TWeakObjectPtr<AOGBrawlerUECharacter> character;
		FVector                               appliedCm = FVector::ZeroVector;
		bool                                  applied   = false;
	};

	struct RenderApplyWindowUImpl
	{
		uint32 frames          = 0u;
		uint32 appliedFrames   = 0u;
		uint32 applied         = 0u;
		uint32 missing         = 0u;
		uint32 exact           = 0u;
		uint32 outOfTolerance  = 0u;
		uint32 held            = 0u;
		uint32 moved           = 0u;
		double maxErrorCm      = 0.0;
		double maxHeldDriftCm  = 0.0;
		double pathCm          = 0.0;
		uint64 firstAppliedStep = 0u;
		uint64 lastAppliedStep  = 0u;
		uint32 interpolated    = 0u;
		uint32 newestMode      = 0u;
		uint32 atNewest        = 0u;
		uint32 beyondOldest    = 0u;
		uint32 snaps           = 0u;
		uint32 maxNextBack     = 0u;
		uint32 shadowBodies    = 0u;
		uint32 shadowExact     = 0u;
		uint32 shadowOutOfTolerance = 0u;
		double shadowMaxErrorCm     = 0.0;
		double shadowMaxAngleRad    = 0.0;
		std::vector<float> frameDisplacementCm;
	};

	struct RenderCorrectionWindowUImpl
	{
		uint32 corrections       = 0u;
		uint32 smoothedBodies    = 0u;
		uint32 snappedBodies     = 0u;
		uint32 late              = 0u;
		uint32 pendingDrops      = 0u;
		uint32 finished          = 0u;
		uint64 finishedSteps     = 0u;
		uint32 maxFinishedSteps  = 0u;
		uint32 activeFrames      = 0u;
		double maxErrorCm        = 0.0;
		double maxAppliedCm      = 0.0;
		uint32 policySnaps       = 0u;
		uint32 rootFramedFrames  = 0u;
		double maxPolicySnapCm   = 0.0;
		double maxPolicySnapDeg  = 0.0;
		uint32 straddleSnaps     = 0u;
		uint64 resimsAtStart     = 0u;
		uint64 capturedAtStart   = 0u;
		uint64 withoutErrorAtStart  = 0u;
		uint64 tickMismatchAtStart  = 0u;
		uint64 ringDropsAtStart  = 0u;
	};

	using RenderCorrection         = RenderCorrectionT<RenderSnapshot::kMaxBodies>;
	using RenderCorrectionSmoothing = RenderCorrectionSmoother<RenderSnapshot::kMaxBodies>;
	using RenderCorrectionCapturing = RenderCorrectionCapture<RenderSnapshot::kMaxBodies>;

	void captureRenderCorrection_Step(uint64 physicsStep);
	void publishRenderSnapshot_Step(const TickOutcome& outcome, double stepDeadlineSeconds);
	bool buildRenderPoses_GameThread();
	void syncRenderPoses_GameThread();
	void applyRenderSnapshot_GameThread();
	void updateVisualizationAtRenderPoses_GameThread();
	void overwriteShadowPoses_GameThread();
	void checkRenderTargetsHeld_GameThread();
	void checkRenderApply_GameThread();
	void traceRenderFrame_GameThread() const;
	void logRenderApplyWindow_GameThread();
	void logRenderCorrectionWindow_GameThread();

	SnapshotChannel<RenderSnapshot, 4>                   m_renderSnapshots;
	std::unordered_map<unsigned int, RenderTargetUImpl>  m_renderTargetsById;
	uint8                                                m_joltRootDeclarationIndex = 0;
	RenderSnapshot                                       m_renderPoses;
	bool                                                 m_renderPosesValid = false;
	RenderInterpolationFrame                             m_renderFrame;
	RenderSnapshot                                       m_renderNext;
	RenderApplyWindowUImpl                               m_renderApplyWindow;

	float                                                m_renderCorrectionStepSeconds = 0.f;
	RenderCorrectionCapturing                            m_renderCorrectionCapture_Step;
	SpscRing<RenderCorrection, 16>                       m_renderCorrections;
	std::atomic<uint64>                                  m_renderCorrectionResims{0u};
	std::atomic<uint64>                                  m_renderCorrectionsCaptured{0u};
	std::atomic<uint64>                                  m_renderCorrectionsWithoutError{0u};
	std::atomic<uint64>                                  m_renderCorrectionTickMismatches{0u};
	RenderCorrectionSmoothing                            m_renderCorrectionSmoother;
	RenderCorrectionFrame                                m_renderCorrectionFrame;
	RenderCorrectionWindowUImpl                          m_renderCorrectionWindow;

	struct ShadowStateSlotUImpl
	{
		ShadowStateSlotUImpl();

		JoltStateSlot state;
		uint64        sequence = 0u;
	};

	struct ShadowWindowUImpl
	{
		uint32 frames               = 0u;
		uint32 restores             = 0u;
		uint32 unchanged            = 0u;
		uint32 refused              = 0u;
		uint32 hashEqual            = 0u;
		uint32 hashDiffer           = 0u;
		uint32 occupiedSlots        = 0u;
		uint64 vizQueryShadow       = 0u;
		uint64 vizQueryStep         = 0u;
		uint64 vizReaderShadow      = 0u;
		uint64 vizReaderStep        = 0u;
		uint64 shadowQueryAccessesAtStart         = 0u;
		uint64 stepQueryGameThreadAccessesAtStart = 0u;
		uint64 publishedAtStart     = 0u;
		uint64 missingAtStart       = 0u;
		double maxRestoreMs         = 0.0;
		uint32 lastRestoredTick     = 0u;
	};

	void publishShadowSlot_Step(const TickOutcome& outcome);
	void restoreShadowWorld_GameThread();
	void logShadowWindow_GameThread();

	SnapshotChannel<ShadowStateSlotUImpl, 3>             m_shadowSlots;
	std::atomic<uint64>                                  m_shadowSlotsPublished{0u};
	std::atomic<uint64>                                  m_shadowSlotsMissing{0u};
	uint64                                               m_shadowRestoredSequence = 0u;
	uint64                                               m_shadowRestoredFrame    = 0u;
	uint32                                               m_shadowBindCompares     = 0u;
	uint32                                               m_shadowBindMismatches   = 0u;
	mutable ShadowWindowUImpl                            m_shadowWindow;

	struct ContactStepUImpl
	{
		uint32                tick      = 0u;
		uint64                worldStep = 0u;
		JoltContactStepCounts counts;
	};

	static void onJoltStepWorldStepped_Step(void* manager);
	void recordContactStep_Step();
	void logContactSteps_Step(uint64 physicsStep, const TickOutcome& outcome);

	std::vector<ContactStepUImpl>                        m_contactSteps_Step;

	SimulationFrameHostUImpl             m_frameHost;
	std::optional<BrawlerStepHooksUImpl> m_stepHooks;
	std::optional<BrawlerStepDriver>     m_stepDriver;

	static constexpr bool compositionContractsHold()
	{
		static_assert(__builtin_offsetof(JoltBackendUImpl, m_frameHost)
		                  < __builtin_offsetof(JoltBackendUImpl, m_stepHooks)
		           && __builtin_offsetof(JoltBackendUImpl, m_stepHooks)
		                  < __builtin_offsetof(JoltBackendUImpl, m_stepDriver),
			"JoltBackendUImpl members destruct in REVERSE declaration order. The step driver unregisters its resync "
			"callback from the manager's client clock in its destructor and holds the hooks, and the hooks hold the frame "
			"host, so the driver must go first, then the hooks, then the frame host. Keep m_frameHost < m_stepHooks < "
			"m_stepDriver; that all three go while the manager is alive is ASimulationManagerUImpl's own row, "
			"m_manager < m_backend. Was design D's compile-time row 'manager < hooks < driver' (task 18); split by "
			"task 83.");
		return true;
	}

	void assertCompositionContracts()
	{
		static_assert(compositionContractsHold());
	}
};
