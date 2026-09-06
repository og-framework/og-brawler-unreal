// SPDX-License-Identifier: MPL-2.0

#include "ChaosSpatialQueryAdapter.h"

#include "Runtime/Experimental/Chaos/Public/Chaos/ShapeInstanceFwd.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/ShapeInstance.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Runtime/Engine/Classes/PhysicsEngine/BodyInstance.h"
#include "Runtime/Engine/Public/Physics/PhysicsFiltering.h"
#include "Runtime/PhysicsCore/Public/Chaos/ChaosScene.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/Framework/PhysicsSolverBase.h"
#include "Runtime/Experimental/Chaos/Public/PBDRigidsSolver.h"
#include "Physics/Experimental/PhysInterface_Chaos.h"
#include "OGSimulation/CompilerControl.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

OGSIM_OPTIMIZE_OFF

DEFINE_LOG_CATEGORY(LogOGSpatialQuery);

namespace
{
// [task40-category-begin]
// ─────────────────────────────────────────────────────────────────────────────
// ⭐ [movement-sim T40] "IS THIS CATEGORY MAPPED?", ISOLATED AS PURE LOGIC.
//
// What this closes is not a wrong answer. It is an answer that cannot be told
// apart from a right one. Two of them, stacked, neither of which logged anything:
//
//   1. toEngineChannel(cat) returns ECollisionChannel(0) for a category the map has
//      never heard of — and ECC_WorldStatic IS ECollisionChannel(0). Task 39 mapped
//      world -> ECC_WorldStatic, which makes the collision LIVE rather than
//      theoretical: from that commit on, the return value alone cannot separate
//      "legitimately mapped to WorldStatic" from "never mapped at all".
//   2. toObjectQueryParams bounded its loop by `cat < m_toEngine.size()` and so
//      never even TESTED a bit past the table. A search naming only unmapped
//      categories therefore produced an EMPTY FCollisionObjectQueryParams. An empty
//      object query is well-formed and matches NOTHING, which is indistinguishable
//      from "the geometry genuinely is not there".
//      ⭐ That is the headline case, and it is the one that cost three PIE runs.
//
// Both are decided from a BITMASK OF EXPLICITLY MAPPED CATEGORIES and never from
// the table's size, because the ctor pads the table with ECollisionChannel(0): a
// gap BELOW the highest mapped category is in range and still unmapped, and it
// answers with a legitimate-looking channel. The decision is written as pure
// functions of integers so it can be proven with no engine, no world and no
// physics thread. The truth table below compiles into every target that builds
// this file, and impl/task40/ carries an out-of-tree harness that compiles THIS
// EXACT TEXT with cl.exe and re-runs the same rows at runtime.

// CollisionCategories::bits is a uint32_t (OGSimulation/QueryGeometry.h), so 0..31
// are the only category IDs a mask can even represent. ChaosPhysicsFactory.cpp:21
// spells the same 32 for the same reason.
constexpr uint32_t kCollisionCategoryCount = 32;

// Throttle bit for a category ID that is not representable in a CollisionCategories
// mask at all. Bit 32 cannot collide with any real category's bit.
constexpr std::uint64_t kOutOfRangeCategoryReportBit = std::uint64_t{1} << kCollisionCategoryCount;

constexpr bool isCategoryMapped(uint32_t mappedMask, uint32_t category)
{
	return category < kCollisionCategoryCount && ((mappedMask >> category) & 1u) != 0u;
}

// The requested categories the adapter has never heard of: the bits that contribute
// NOTHING to the object query and are dropped without a word.
constexpr uint32_t unmappedRequestedMask(uint32_t requestedMask, uint32_t mappedMask)
{
	return requestedMask & ~mappedMask;
}

// ⭐ THE HEADLINE CASE. A NON-EMPTY request that maps to nothing at all yields an
// EMPTY object query, which matches nothing and reads exactly like a clean miss.
// An EMPTY request is deliberately NOT this case: a volume that only ever sweeps by
// trace channel legitimately registers with searchCategories == {}, and making that
// noisy would be a false alarm on healthy code.
constexpr bool isSilentlyEmptyQuery(uint32_t requestedMask, uint32_t mappedMask)
{
	return requestedMask != 0u && (requestedMask & mappedMask) == 0u;
}

// Categories BELOW the highest mapped one that were never mapped. The ctor pads
// those slots with ECollisionChannel(0) == ECC_WorldStatic, so they answer
// toEngineChannel with a plausible channel forever. Knowable at CONSTRUCTION from
// the fixed initializer list, which is why the ctor reports it once and no caller
// has to.
constexpr uint32_t ambiguousGapMask(uint32_t mappedMask, uint32_t tableSize)
{
	const uint32_t below = (tableSize >= kCollisionCategoryCount)
		? ~0u
		: ((1u << tableSize) - 1u);
	return below & ~mappedMask;
}

// Which throttle bit a report about `category` claims.
constexpr std::uint64_t categoryReportBit(uint32_t category)
{
	return category < kCollisionCategoryCount
		? (std::uint64_t{1} << category)
		: kOutOfRangeCategoryReportBit;
}

// ── Truth table ──────────────────────────────────────────────────────────────
// The shipped map after task 43 is body,guard,queryRouting,projectile,world,character = 0..5.
//
// ⭐ [movement-sim T43] `character` (5) JOINED THE TABLE on 2026-09-05 — see the two
// m_queryAdapter.emplace tables in SimulationManagerUImpl.cpp for the channel (GTC6, user ruling
// #6) and for why an unmapped `character` was not merely undocumented but actively wrong.
// kPreT43Mask is kept BESIDE the shipped mask rather than deleted, and that is deliberate: the
// rows below that document what an unmapped category DOES to a query are the reason this block
// exists, and once `character` is mapped it can no longer play that part. Pairing each fixed row
// with its pre-fix value keeps both halves measurable and makes a silent regression impossible.
constexpr uint32_t kShippedMask = 0b111111u;   // categories 0..5
constexpr uint32_t kPreT43Mask  = 0b011111u;   // categories 0..4 — the map task 43 replaced

static_assert(isCategoryMapped(kShippedMask, 0));
static_assert(isCategoryMapped(kShippedMask, 4));       // world, added by task 39
static_assert(isCategoryMapped(kShippedMask, 5));       // character, added by task 43
static_assert(!isCategoryMapped(kPreT43Mask, 5));       // ...and it was NOT mapped before it
static_assert(!isCategoryMapped(kShippedMask, 31));
static_assert(!isCategoryMapped(kShippedMask, 32));     // not representable at all
static_assert(!isCategoryMapped(kShippedMask, 9999));   // no shift UB, no wrap

// ⭐ THE SILENT ROWS. If any of these went the other way the diagnostic would fire
// on every healthy registration, which is the failure mode that makes an instrument
// worse than none. They are the standing partner of the RED control in impl/task40/.
static_assert(unmappedRequestedMask(0b000011u, kShippedMask) == 0u);          // bodyAndGuard
static_assert(unmappedRequestedMask(0b001011u, kShippedMask) == 0u);          // bodyGuardProjectile
static_assert(unmappedRequestedMask(0b010000u, kShippedMask) == 0u);          // worldOnly
// ⭐ [movement-sim T43] worldAndCharacter: BOTH bits survive now. Before task 43 this row read
// `== 0b100000u` — `world` survived and `character` was dropped on the floor — and that former
// value is the kPreT43Mask row immediately below, kept as the RED half of the pair.
static_assert(unmappedRequestedMask(0b110000u, kShippedMask) == 0u);
static_assert(unmappedRequestedMask(0b110000u, kPreT43Mask)  == 0b100000u);

static_assert(!isSilentlyEmptyQuery(0b000011u, kShippedMask));   // healthy
static_assert(!isSilentlyEmptyQuery(0b110000u, kShippedMask));   // worldAndCharacter: now complete
// ⭐ [movement-sim T43] THE HEADLINE CASE, CLOSED. A character-only search used to produce EMPTY
// object query params: a NON-EMPTY request that matched nothing and read exactly like a clean
// miss. Both halves are pinned, so neither the fix nor the diagnosis can be lost.
static_assert(!isSilentlyEmptyQuery(0b100000u, kShippedMask));   // ⭐ character alone -> FINDS THINGS
static_assert( isSilentlyEmptyQuery(0b100000u, kPreT43Mask));    // ⭐ ...and used to be EMPTY
static_assert(!isSilentlyEmptyQuery(0u, kShippedMask));          // empty in, empty out: NOT a fault

// No gap in the shipped map (0..5 mapped, table size 6). THIS assertion is what
// makes the ctor's gap check silent today instead of an every-launch false alarm.
static_assert(ambiguousGapMask(kShippedMask, 6) == 0u);
// The pre-task-43 map had no gap either — `character` sat ABOVE the highest mapped
// category, which is exactly why the ctor's gap check never said a word about it and
// why task 40 needed a separate per-call diagnostic to surface it at all.
static_assert(ambiguousGapMask(kPreT43Mask, 5) == 0u);
// A table that mapped 1..4 but not 0 would leave category 0 answering ECC_WorldStatic.
static_assert(ambiguousGapMask(0b011110u, 5) == 0b000001u);
static_assert(ambiguousGapMask(0u, 0) == 0u);

static_assert(categoryReportBit(0) == 1u);
static_assert(categoryReportBit(5) == 32u);
static_assert(categoryReportBit(31) == (std::uint64_t{1} << 31));
static_assert(categoryReportBit(32) == kOutOfRangeCategoryReportBit);
static_assert(categoryReportBit(9999) == kOutOfRangeCategoryReportBit);

// One bit per category already reported, for the whole process. File-scope for the
// same reason gFilterCallCount is: a listen server constructs TWO adapters, and the
// second one's duplicate report carries no information.
// ⚠ This is the "no per-tick log spam" guarantee, and it is a HARD BOUND rather
// than a throttle: 33 claimable bits, each claimable once, so at most 33
// unmapped-category lines can ever be emitted by a process however its callers
// behave — including a future caller that puts toEngineChannel on a hot path.
std::atomic<std::uint64_t> gReportedUnmappedCategories{0};

// True exactly once per category, for the first caller to claim it.
bool claimUnmappedCategoryReport(uint32_t category)
{
	const std::uint64_t bit = categoryReportBit(category);
	return (gReportedUnmappedCategories.fetch_or(bit, std::memory_order_relaxed) & bit) == 0;
}
// [task40-category-end]

// The once-per-category line. Deliberately says BOTH failure modes on one line, the
// way the [SpatialQuery.Filter] line says raw= and kept= on one line: whichever of
// the two brought the reader here, the other one is the thing they have not thought
// of yet. ASCII only inside TEXT(), matching every other literal in this file.
void reportUnmappedCategory(uint32_t category, const TCHAR* site)
{
	if (!claimUnmappedCategoryReport(category))
		return;

	UE_LOG(LogOGSpatialQuery, Error,
		TEXT("[SpatialQuery.UnmappedCategory] site=%s category=%u -- this adapter has NO ")
		TEXT("mapping for that category. toEngineChannel answers ECollisionChannel(0), which ")
		TEXT("IS ECC_WorldStatic, so the answer is indistinguishable from a legitimate ")
		TEXT("mapping; toObjectQueryParams contributes nothing for it, so a search naming it ")
		TEXT("is silently narrowed and may match NOTHING. Fix it in the adapter's ")
		TEXT("construction table (SimulationManagerUImpl.cpp). Reported once per category ")
		TEXT("per process."),
		site, category);
}

// [task41-objectquery-begin]
// ──────────────────────────────────────────────────────────────────────────────
// ⭐ [movement-sim T41] THE ENGINE'S PRE-FILTER RULE, TRANSCRIBED AS PURE LOGIC.
//
// ⚠ THIS IS A TRANSCRIPTION OF UE 5.6 SOURCE. IT IS NOT A TEST OF THE ENGINE.
// It compiles two branches of Epic's CollisionQueryFilterCallback into constexpr
// functions and pins OUR READING of them with a truth table. It cannot fail if the
// engine changes, it cannot detect such a change, and it proves nothing about the
// engine's behaviour. What it does is make the rule this file's sweep() depends on
// reviewable in one screen, and make the row that cost three PIE runs impossible to
// read past. If the engine version moves, this block must be re-read against the new
// source by hand — nothing here will complain.
//
// The table is compiled into every target that builds this file, and impl/task41/
// carries an out-of-tree harness that extracts THIS EXACT TEXT and compiles it with
// cl.exe seven times: once verbatim (must compile) and once per poisoned row with a
// single expectation inverted (each must fail with C2607). That is the only sense in
// which this block is "tested" — it shows the rows discriminate, not that the engine
// agrees with them.
//
// The two branches, UE 5.6 (EpicGames/UnrealEngine @ tag 5.6):
//
//   (1) CalcQueryHitType, CollisionQueryFilterCallback.cpp:25-40 — the OBJECT-query
//       branch. Word0 says "object query"; the "channel" bits of Word3 do not carry a
//       channel at all but the multi/single flag (PhysicsInterfaceUtils.cpp:99,38
//       encodes TRACE_MULTI = Traits::IsMulti there, via SceneQuery.cpp:518). So:
//           if (bPreFilter) return MultiTrace ? Touch : Block;
//       with Epic's own comment: "In the case of an object query we actually want to
//       return all object types (or first in single case). So in PreFilter we have to
//       trick physx by not blocking in the multi case, and blocking in the single case."
//       ⛔ The object branch reads ONLY ShapeFilter.Word3 and QueryFilter.Word1. It
//       never looks at the shape's collision RESPONSE (Word1/Word2), which is read
//       only in the trace-channel branch at :44-72. The `channelResponse` parameter
//       below is therefore threaded through the object rows deliberately and IGNORED
//       by them — that is the whole content of the task-7 retraction: "every volume
//       registers all-block, so touches cannot arise" was never true of an object query.
//
//   (2) The bIgnoreTouches branch, same file :172-175, which runs AFTER (1) for both
//       query modes:  if (Result == Touch && bIgnoreTouches) Result = None;
//       A hit that becomes None is rejected before narrow phase and never appears in
//       the result array at all — it is not a post-filter, and nothing downstream of
//       the engine call can see that it happened.
//
// Composed: sweep() was Multi + object + bIgnoreTouches, which is (1) -> Touch and
// (2) -> None, for EVERY shape in the world. That is the RED row below.
enum class PreFilterHitType : unsigned char
{
	NoHit,  // rejected before narrow phase; never reaches the FHitResult array
	Touch,  // reported, and droppable by bIgnoreTouches
	Block,  // reported, and NOT droppable by bIgnoreTouches
};

// (1) `channelResponse` is what the trace-channel branch (:44-72) derives from the
// shape's Word1/Word2 response bits. The object branch is a pure function of the
// multi/single flag and ignores it — which is the point.
constexpr PreFilterHitType preFilterHitType(bool bObjectQuery,
                                            bool bMultiTrace,
                                            PreFilterHitType channelResponse)
{
	if (bObjectQuery)
		return bMultiTrace ? PreFilterHitType::Touch : PreFilterHitType::Block;
	return channelResponse;
}

// (2) True when the candidate survives the pre-filter and can reach narrow phase.
constexpr bool survivesPreFilter(PreFilterHitType hitType, bool bIgnoreTouches)
{
	if (hitType == PreFilterHitType::Touch && bIgnoreTouches)
		return false;  // :172-175 rewrites Touch -> None
	return hitType != PreFilterHitType::NoHit;
}

// ── Truth table: all 12 rows of preFilterHitType, all 6 of survivesPreFilter ────
// ⭐ OBJECT query, MULTI: Touch regardless of the registered response. The three rows
// that retract task 7's "all-block, so touches cannot arise" reasoning.
static_assert(preFilterHitType(true , true , PreFilterHitType::Block) == PreFilterHitType::Touch);
static_assert(preFilterHitType(true , true , PreFilterHitType::Touch) == PreFilterHitType::Touch);
static_assert(preFilterHitType(true , true , PreFilterHitType::NoHit) == PreFilterHitType::Touch);
// OBJECT query, SINGLE: Block regardless of the response. This is the game-thread
// control (SweepSingleByObjectType), and why it hit the floor 99/99.
static_assert(preFilterHitType(true , false, PreFilterHitType::Block) == PreFilterHitType::Block);
static_assert(preFilterHitType(true , false, PreFilterHitType::Touch) == PreFilterHitType::Block);
static_assert(preFilterHitType(true , false, PreFilterHitType::NoHit) == PreFilterHitType::Block);
// TRACE-CHANNEL query (empty searchCategories): the response decides, multi or single.
static_assert(preFilterHitType(false, true , PreFilterHitType::Block) == PreFilterHitType::Block);
static_assert(preFilterHitType(false, true , PreFilterHitType::Touch) == PreFilterHitType::Touch);
static_assert(preFilterHitType(false, true , PreFilterHitType::NoHit) == PreFilterHitType::NoHit);
static_assert(preFilterHitType(false, false, PreFilterHitType::Block) == PreFilterHitType::Block);
static_assert(preFilterHitType(false, false, PreFilterHitType::Touch) == PreFilterHitType::Touch);
static_assert(preFilterHitType(false, false, PreFilterHitType::NoHit) == PreFilterHitType::NoHit);

static_assert(!survivesPreFilter(PreFilterHitType::NoHit, false));
static_assert(!survivesPreFilter(PreFilterHitType::NoHit, true ));
static_assert( survivesPreFilter(PreFilterHitType::Touch, false));
static_assert(!survivesPreFilter(PreFilterHitType::Touch, true ));  // the only dropping row
static_assert( survivesPreFilter(PreFilterHitType::Block, false));
static_assert( survivesPreFilter(PreFilterHitType::Block, true ));

// ── The two rows that ARE this task ─────────────────────────────────────
// ⛔ RED — what sweep() shipped: object + multi + bIgnoreTouches. Every shape dropped.
static_assert(!survivesPreFilter(preFilterHitType(true, true, PreFilterHitType::Block), true));
// ✅ GREEN — what sweep() does now. The candidate reaches narrow phase, is converted
// with bBlockingHit = true (CollisionConversions.cpp:343-344), and the bBlockingHit
// loop in sweep() picks the nearest one.
static_assert( survivesPreFilter(preFilterHitType(true, true, PreFilterHitType::Block), false));
// And the flag is NOT inert everywhere: on a trace-channel volume it still means what
// it says, which is why the bBlockingHit loop, not this flag, is where the contract lives.
static_assert(!survivesPreFilter(preFilterHitType(false, true, PreFilterHitType::Touch), true));
static_assert( survivesPreFilter(preFilterHitType(false, true, PreFilterHitType::Touch), false));
// [task41-objectquery-end]
}  // namespace

ChaosSpatialQueryAdapter::ChaosSpatialQueryAdapter(UWorld* world, std::initializer_list<ChaosCategoryMapping> categoryMappings)
	: m_world(world)
{
	m_toDAttack.fill(kUnmapped);

	for (const auto& mapping : categoryMappings)
	{
		// Forward: DAttack category -> Chaos channel
		if (mapping.dattackCategory >= m_toEngine.size())
			m_toEngine.resize(mapping.dattackCategory + 1, static_cast<ECollisionChannel>(0));
		m_toEngine[mapping.dattackCategory] = mapping.chaosChannel;

		// ⭐ [movement-sim T40] The ONLY record of what was explicitly mapped. The
		// resize above pads every skipped slot with ECollisionChannel(0), so after this
		// loop m_toEngine cannot answer "was this mapped?" — only this mask can.
		if (mapping.dattackCategory < kCollisionCategoryCount)
		{
			m_mappedCategoryMask |= (1u << mapping.dattackCategory);
		}
		else
		{
			// A category ID no CollisionCategories mask can hold. It will occupy a
			// forward-table slot, cost the resize, and be unreachable from every
			// search mask in the tree. Construction-time, once, and impossible today.
			UE_LOG(LogOGSpatialQuery, Error,
				TEXT("[SpatialQuery.MapGap] mapped category=%u is beyond the %u categories a ")
				TEXT("CollisionCategories bitmask can represent -- no query can ever name it."),
				mapping.dattackCategory, kCollisionCategoryCount);
		}

		// Reverse: Chaos channel -> DAttack category
		const uint32_t channelIdx = static_cast<uint32_t>(mapping.chaosChannel);
		if (channelIdx < m_toDAttack.size())
			m_toDAttack[channelIdx] = mapping.dattackCategory;
	}

	// ⭐ [movement-sim T40] FAIL-LOUD AT CONSTRUCTION, which is the earliest moment
	// the question is answerable: the table is a fixed initializer list, so "which
	// categories are mapped" is knowable before anything queries anything.
	// What it catches is the AMBIGUOUS case specifically — a category BELOW the
	// highest mapped one that was never mapped. Those slots were padded with
	// ECollisionChannel(0) above, and ECC_WorldStatic IS ECollisionChannel(0), so
	// they will answer toEngineChannel with a perfectly plausible channel for the
	// life of the process and nothing downstream can tell.
	// This is NOT a check that every known category is mapped: `character` is
	// deliberately unmapped until task 13, and complaining about it here would fire
	// on every launch and train the reader to ignore the category. It fires only on
	// a gap, and the shipped table (0..4, size 5) has none — see the
	// ambiguousGapMask static_assert above, which is what pins that silence.
	const uint32_t gapMask =
		ambiguousGapMask(m_mappedCategoryMask, static_cast<uint32_t>(m_toEngine.size()));
	if (gapMask != 0u)
	{
		UE_LOG(LogOGSpatialQuery, Error,
			TEXT("[SpatialQuery.MapGap] mapped=0x%08X tableSize=%u gaps=0x%08X -- the gap ")
			TEXT("categories are INSIDE the forward table but were never mapped, so they were ")
			TEXT("padded with ECollisionChannel(0) == ECC_WorldStatic. toEngineChannel will ")
			TEXT("hand out a legitimate-looking WorldStatic channel for them and no caller can ")
			TEXT("tell the difference. Map them or stop mapping past them."),
			m_mappedCategoryMask, static_cast<uint32_t>(m_toEngine.size()), gapMask);
	}
}

QueryVolumeId ChaosSpatialQueryAdapter::registerVolume(
	const QueryVolumeDescriptor& descriptor,
	const FCollisionQueryParams& queryParams,
	const FActorInstanceHandle& owner)
{
	FCollisionShape uShape = std::visit([](const auto& geo) -> FCollisionShape {
		if constexpr (std::is_same_v<std::decay_t<decltype(geo)>, SphereGeometry>)
			return FCollisionShape::MakeSphere(geo.radius);
		else if constexpr (std::is_same_v<std::decay_t<decltype(geo)>, BoxGeometry>)
			return FCollisionShape::MakeBox(FVector(geo.halfExtents.x, geo.halfExtents.y, geo.halfExtents.z));
		else // CapsuleGeometry
			return FCollisionShape::MakeCapsule(geo.radius, geo.halfHeight);
	}, descriptor.geometry);

	FCollisionObjectQueryParams objectQueryParams = toObjectQueryParams(descriptor.searchCategories);
	ECollisionChannel collisionChannel = toEngineChannel(descriptor.traceCategory);

	// ⭐ [movement-sim T40] The ATTRIBUTABLE half of the unmapped-category diagnostic.
	// The two calls above already reported any unmapped category once per process, but
	// that line is anonymous: it names a number, not a caller. This one names the
	// volume this call is about to hand back and the descriptor's own masks, and it is
	// the ONLY place in the adapter that can, because searchCategories and
	// traceCategory stop being separable facts the moment they are folded into a
	// stored FCollisionObjectQueryParams. Setup-time, once per volume, silent when the
	// descriptor is fully mapped.
	// The id is m_nextVolumeId un-incremented on purpose: it is exactly the
	// QueryVolumeId this call returns below, so a log line and a caller's stored handle
	// name the same volume.
	diagnoseVolumeCategories(descriptor, QueryVolumeId{m_nextVolumeId});

	m_volumes.push_back(VolumeEntry{
		uShape,
		collisionChannel,
		queryParams,
		objectQueryParams,
		FCollisionResponseParams(ECR_Block),
		glm::mat4(1.f),
		descriptor.offsetTransform
	});

	return QueryVolumeId{m_nextVolumeId++};
}

ShapeId ChaosSpatialQueryAdapter::registerShape(FBodyInstanceAsyncPhysicsTickHandle body,
                                                unsigned int shapeIndex,
                                                std::optional<BodyId> parentBodyId)
{
	// Record the shape body -> root body mapping so overlap() can emit a meaningful
	// rootBodyId for hits on this shape. The shape body's key is the same Chaos particle
	// UniqueIdx that overlap() writes into SpatialQueryHit::bodyId (and that
	// ChaosPhysicsBodyAdapter::getBodyId hands back as a BodyId), so the two agree.
	// No parent supplied => no entry => overlap() falls back to rootBodyId == bodyId.
	//
	// parentBodyId is the IMMEDIATE parent — under multi-level hierarchies it may not
	// be the root. Walk the parent chain here to flatten to root at store-time so
	// overlap()'s lookup is always O(1). By the "register parents before children"
	// invariant (see header), every intermediate parent's own map entry ALSO already
	// points to root, so this walk terminates in at most one hop. The while-loop form
	// defends against out-of-order registration if that invariant ever slips.
	if (parentBodyId.has_value())
	{
		const uint32_t shapeBodyUniqueIdx =
			static_cast<uint32_t>(body.Proxy->GetParticle_LowLevel()->UniqueIdx().Idx);

		uint32_t rootIdxValue = parentBodyId->value;
		auto it = m_shapeBodyToRootBody.find(rootIdxValue);
		while (it != m_shapeBodyToRootBody.end())
		{
			rootIdxValue = it->second;
			it = m_shapeBodyToRootBody.find(rootIdxValue);
		}

		m_shapeBodyToRootBody[shapeBodyUniqueIdx] = rootIdxValue;
	}

	m_shapes.push_back(ShapeEntry{body, shapeIndex});
	return ShapeId{m_nextShapeId++};
}

SpatialQueryReport ChaosSpatialQueryAdapter::overlap(const std::vector<QueryVolumeId>& volumeIds)
{
	TArray<FHitResult> results;

	for (const auto& volumeId : volumeIds)
	{
		const auto& vol = m_volumes[volumeId.value];
		FTransform worldTransform;
		glm::mat4 worldMat = vol.parentTransform * vol.offsetTransform;
		uglm::toFtransform(worldMat, worldTransform);

		FPhysicsInterface::GeomSweepMulti(
			m_world,
			vol.uShape,
			FQuat::Identity,
			results,
			worldTransform.GetTranslation(),
			worldTransform.GetTranslation(),
			vol.collisionChannel,
			vol.queryParams,
			vol.responseParams,
			vol.objectQueryParams);
	}

	filterDisabledAndUnreadyHits(results);

	// Convert to engine-independent results — reverse-map Chaos channels to DAttack categories.
	SpatialQueryReport report;
	for (auto& hit : results)
	{
		SpatialQueryHit sqHit;
		sqHit.objectPosition = uglm::toGLMVec3(hit.HitObjectHandle.GetLocation());
		resolveHitIdentity(hit, sqHit.objectCategories, sqHit.bodyId, sqHit.rootBodyId);
		report.hits.push_back(sqHit);
	}
	return report;
}

SweepHit ChaosSpatialQueryAdapter::sweep(QueryVolumeId volumeId, const glm::mat4& transform, const glm::vec3& delta)
{
	// READ-ONLY in the volume table. `vol` is a const reference and the sweep pose comes
	// from the `transform` argument, NOT from vol.parentTransform — so this never calls
	// setVolumeParentTransform and never mutates m_volumes[volumeId]. A mover can run
	// several sub-sweeps per tick (wall-slide iterations) with no adapter state churn.
	const auto& vol = m_volumes[volumeId.value];

	FTransform worldTransform;
	glm::mat4 worldMat = transform * vol.offsetTransform;
	uglm::toFtransform(worldMat, worldTransform);

	const FVector start = worldTransform.GetTranslation();
	const FVector end = start + uglm::toFVector(delta);

	// Local copy: the stored queryParams are shared with overlap() and must keep its
	// semantics. A sweep additionally wants to see a volume that is ALREADY overlapping
	// at fraction 0 (bFindInitialOverlaps — that is what feeds SweepHit::startPenetrating
	// and the push-out). That is the ONLY thing it changes.
	//
	// ⛔ [movement-sim T41] DO NOT SET bIgnoreTouches HERE. It reads like the natural way
	// to spell the "nearest BLOCKING hit only" contract below, and on THIS call it is a
	// total blindfold. This is a MULTI, OBJECT-TYPE query, and in UE 5.6 the pre-filter
	// classifies every candidate shape of a multi object query as Touch — deliberately,
	// so that a multi object query returns ALL matching object types
	// (CollisionQueryFilterCallback.cpp:28-35, and the comment there is Epic's own).
	// bIgnoreTouches then rewrites every Touch to None on the very next branch (:172-175),
	// so NOTHING survives to narrow phase and NumHits == 0, for every shape in the world.
	// One rule, three measured observations, one binary (user PIE runs to 2026-09-05):
	//   • sweep(), Multi + object + the flag  -> 586 physics-thread [SpatialQuery.Filter]
	//     rows, all raw=0. The post-filter never saw a hit; it was never the cause.
	//   • overlap(), Multi + object, NO flag  -> hits (raw=2 on 14 physics-thread rows).
	//   • a game-thread SweepSingleByObjectType -> SINGLE, so the pre-filter answers Block
	//     rather than Touch, the flag is inert, and the same capsule hit the floor 99/99.
	// See the [task41-objectquery] transcription of both engine rules at the top of this
	// file. Note also that an object query never consults collision RESPONSE at all, so
	// "every volume registers all-block, therefore touches cannot arise" is NOT a reason
	// the flag is safe here — the object branch of CalcQueryHitType reads only
	// ShapeFilter.Word3 and QueryFilter.Word1 (:25-40); Word1/Word2 and our
	// FCollisionResponseParams(ECR_Block) are read only in the trace-channel branch (:44-72).
	//
	// ⭐ The "nearest BLOCKING hit only" contract is NOT weakened by dropping the flag,
	// because it is enforced AFTER conversion, not in the pre-filter: the engine marks
	// every object-type hit bBlockingHit = true at conversion time
	// (CollisionConversions.cpp:343-344), and the `if (!results[i].bBlockingHit) continue;`
	// loop below is what actually selects the minimum-Time BLOCKING result. That loop is
	// also what keeps the contract honest for a future TRACE-CHANNEL volume (empty
	// searchCategories), where Touch really does mean an overlap response — which is why
	// removing the flag is the right call for both query modes, not just for ours.
	FCollisionQueryParams localParams = vol.queryParams;
	localParams.bFindInitialOverlaps = true;

	// The identical call overlap() makes, with end != start.
	TArray<FHitResult> results;
	FPhysicsInterface::GeomSweepMulti(
		m_world,
		vol.uShape,
		FQuat::Identity,
		results,
		start,
		end,
		vol.collisionChannel,
		localParams,
		vol.responseParams,
		vol.objectQueryParams);

	filterDisabledAndUnreadyHits(results);

	// Nearest blocking hit only (v1): the minimum-Time blocking result. An initial
	// overlap comes back with Time == 0, so it naturally wins.
	int32 bestIndex = INDEX_NONE;
	for (int32 i = 0; i < results.Num(); ++i)
	{
		if (!results[i].bBlockingHit)
			continue;
		if (bestIndex == INDEX_NONE || results[i].Time < results[bestIndex].Time)
			bestIndex = i;
	}

	// Nothing blocking: the volume travelled the full delta. Defaults say exactly that
	// (blocked == false, fraction == 1).
	if (bestIndex == INDEX_NONE)
		return SweepHit{};

	FHitResult& hit = results[bestIndex];
	SweepHit sweepHit;
	sweepHit.blocked = true;
	sweepHit.fraction = static_cast<float>(hit.Time);
	sweepHit.normal = uglm::toGLMVec3(hit.ImpactNormal);
	sweepHit.impactPoint = uglm::toGLMVec3(hit.ImpactPoint);
	sweepHit.startPenetrating = hit.bStartPenetrating;
	sweepHit.penetrationDepth = static_cast<float>(hit.PenetrationDepth);
	resolveHitIdentity(hit, sweepHit.objectCategories, sweepHit.bodyId, sweepHit.rootBodyId);
	return sweepHit;
}

namespace
{
// [task38-classifier-begin]
// ─────────────────────────────────────────────────────────────────────────────
// ⭐ [movement-sim T38] THE CATEGORY DISTINCTION, ISOLATED AS PURE LOGIC.
//
// Until task 38 this post-filter keyed every hit on ONE fact — "does this proxy
// have an async-physics-tick view?" — and dropped the hit when the answer was no.
// That is right for exactly one of the three cases it has to serve:
//
//   (a) a DYNAMIC body MID-SPAWN. Chaos's FPBDRigidsSolver::RegisterObject has
//       already inserted the proxy into the EXTERNAL acceleration structure on the
//       game thread, but Proxy->Handle is still null on the physics side until
//       ProcessSinglePushedData_Internal runs on the next physics tick
//       (SceneQuery.cpp:454 documents that Chaos itself doesn't detect PT-context
//       queries reliably during this window). GetPhysicsThreadAPI() returns null
//       then and dereferencing it crashes at ~offset 0x48 — the crash commit
//       ce0c559 (2026-07-18) closed. Transient. DROP.
//   (b) any body whose shape is genuinely QUERY-DISABLED. DROP. This is the
//       filter's actual job: enableShape()/disableShape() write query-enabled
//       state through the PHYSICS-THREAD view, which the game thread's
//       acceleration structure cannot see, so a hit it returned may already be
//       disabled on our side.
//   (c) STATIC (or kinematic) WORLD GEOMETRY. It has no async-physics-tick rigid
//       view and never will: the engine's own doc comment on
//       UPrimitiveComponent::GetBodyInstanceAsyncPhysicsTickHandle says the handle
//       is "For use in the Async Physics Tick event". "No physics-thread view" is
//       therefore not a readiness signal about it at all, and its shape state is
//       exactly what the game-thread acceleration structure already used to
//       produce the hit. KEEP — dropping it would silently eat every static hit,
//       and this branch is what lets a floor hit exist at all once it arrives.
//
// ⛔ [movement-sim T41] CORRECTION TO TASK 38's ORIGIN STORY — READ THIS BEFORE
// CITING THIS BLOCK AS THE CAUSE OF ANYTHING. Task 38 was written believing that
// case (c) was why the physics-thread sweep saw a floor 0 times in 601 tries while
// a game-thread control on the same capsule, pose and vector saw it 99/99 (user PIE
// run 2026-09-05; the floor is StaticMeshActor_33). THAT ATTRIBUTION WAS WRONG, and
// it misled three investigations. The instrument below settled it: all 586
// physics-thread [SpatialQuery.Filter] rows read raw=0 — GeomSweepMulti returned
// NOTHING, so this filter was handed an empty array and could not have dropped
// anything. The real cause was sweep()'s own bIgnoreTouches on a MULTI OBJECT query,
// which made the ENGINE's pre-filter reject every shape before narrow phase; see the
// [task41-objectquery] block at the top of this file and the note in sweep().
// The task-38 classifier itself is CORRECT and stays: it is a necessary fix, because
// a static hit that now DOES arrive would otherwise be dropped here. It was simply
// never reached — the code below is downstream of a call that returned zero rows.
//
// ⭐ THE DISCRIMINATOR between (a) and (c) is whether the body simulates physics:
//     USceneComponent::IsSimulatingPhysics(FName BoneName = NAME_None) -> bool
//     "Returns whether the specified body is currently using physics simulation"
// — signature and doc text read from THIS REPO's own UHT reflection dump,
// Intermediate/Build/Win64/OGBrawlerUnreal/Inc/Engine/UHT/SceneComponent.gen.cpp
// lines 1351-1404 (the engine tree is out of bounds; in-repo UHT reflection is
// not). A body mid-spawn is ALREADY simulating — SetSimulatePhysics(true) returns
// on the game thread BEFORE the physics side catches up, which is precisely what
// opens the spawn window — while static world geometry never is.
//
// The decision is written as two pure functions of booleans so that it can be
// proven with no engine, no world and no physics thread. The truth table below is
// compiled into every target that builds this file, and impl/task38/ carries an
// out-of-tree harness that compiles THIS EXACT TEXT with cl.exe and runs the same
// table at runtime.
enum class ShapeStateSource : unsigned char
{
	Unreadable,     // no shape state can be consulted -> drop the hit
	PhysicsThread,  // consult the async-physics-tick view (what enable/disableShape write)
	GameThread,     // consult the external view (static geometry has no other)
};

constexpr ShapeStateSource selectShapeStateSource(bool hasProxy,
                                                  bool hasPhysicsThreadView,
                                                  bool bodySimulatesPhysics)
{
	if (!hasProxy)
		return ShapeStateSource::Unreadable;    // nothing to key on, no identity either
	if (hasPhysicsThreadView)
		return ShapeStateSource::PhysicsThread; // (b) is decided from the PT view
	if (bodySimulatesPhysics)
		return ShapeStateSource::Unreadable;    // (a) spawn window — drop, exactly as before
	return ShapeStateSource::GameThread;        // (c) static geometry — THE FIX
}

constexpr bool keepsHit(ShapeStateSource source, bool shapeQueryEnabled)
{
	return source != ShapeStateSource::Unreadable && shapeQueryEnabled;
}

// ── Truth table: all 8 rows of selectShapeStateSource, all 6 of keepsHit ──────
//                            hasProxy, hasPhysicsThreadView, bodySimulatesPhysics
static_assert(selectShapeStateSource(false, false, false) == ShapeStateSource::Unreadable);
static_assert(selectShapeStateSource(false, false, true ) == ShapeStateSource::Unreadable);
static_assert(selectShapeStateSource(false, true , false) == ShapeStateSource::Unreadable);
static_assert(selectShapeStateSource(false, true , true ) == ShapeStateSource::Unreadable);
static_assert(selectShapeStateSource(true , true , false) == ShapeStateSource::PhysicsThread);
static_assert(selectShapeStateSource(true , true , true ) == ShapeStateSource::PhysicsThread);
// (a) simulating, no PT view YET — the spawn-window drop this filter exists for.
static_assert(selectShapeStateSource(true , false, true ) == ShapeStateSource::Unreadable);
// (c) NOT simulating, no PT view EVER — static world geometry. THE ONE ROW THAT MOVED.
static_assert(selectShapeStateSource(true , false, false) == ShapeStateSource::GameThread);

static_assert(keepsHit(ShapeStateSource::Unreadable,    true ) == false);
static_assert(keepsHit(ShapeStateSource::Unreadable,    false) == false);
static_assert(keepsHit(ShapeStateSource::PhysicsThread, true ) == true );
static_assert(keepsHit(ShapeStateSource::PhysicsThread, false) == false);
static_assert(keepsHit(ShapeStateSource::GameThread,    true ) == true );
static_assert(keepsHit(ShapeStateSource::GameThread,    false) == false);
// [task38-classifier-end]

// Per-call accounting behind the [SpatialQuery.Filter] diagnostic line.
struct FilterTally
{
	int32 raw             = 0;
	int32 keptPtView      = 0;
	int32 keptGtView      = 0;   // in PT context this IS the task-38 static case
	int32 noComponent     = 0;
	int32 noBodyInstance  = 0;
	int32 noProxy         = 0;
	int32 spawnWindow     = 0;
	int32 disabledPtView  = 0;
	int32 disabledGtView  = 0;
};

// [task40-filterbudget-begin]
// ─────────────────────────────────────────────────────────────────────────────
// ⭐ [movement-sim T40] ONE LOG BUDGET PER THREAD CONTEXT, AS PURE LOGIC.
//
// Task 38 shipped this instrument with a SINGLE process-wide counter. On paper the
// quieting rule was right; in the user's 2026-09-05 PIE run it destroyed the run.
// The shipped game-thread callers (the radial-attack overlaps) start at level load
// and spent call=1..300 in three seconds, before the physics-thread subject was
// armed. Every one of the ~600 physics-thread sweeps landed past the budget, none
// of them dropped anything, so not one `ctx=PT` row was ever emitted — and the
// resulting "all 600 filter calls report ctx=GT" was then read as evidence that
// Chaos::IsInPhysicsThreadContext() is false inside the PT callback.
// ⛔ IT IS NOT EVIDENCE OF THAT. It is a LOGGING ARTIFACT. Whether PT-context
// filter calls happen is, as of this commit, still unknown — and now printable.
//
// The rule: a warm-up budget assumes the subject runs FIRST. A subject armed by a
// cvar, a command or a user action runs LAST by construction, so a shared warm-up
// budget is always spent by the noisiest caller, which is never the subject. One
// counter per context is the whole fix; `contextCallIndex` below is a per-context
// ordinal and shouldLogFilterCall can see nothing else.
enum FilterLogContext : std::size_t
{
	kFilterCtxGameThread    = 0,
	kFilterCtxPhysicsThread = 1,
	kFilterCtxCount         = 2,
};

constexpr std::uint64_t kFilterLogAlwaysCalls = 300;
constexpr std::uint64_t kFilterLogDropStride  = 64;

constexpr std::size_t filterLogContext(bool bPhysicsThread)
{
	return bPhysicsThread ? kFilterCtxPhysicsThread : kFilterCtxGameThread;
}

// ⭐ The one function that decides whether a row is printed, and the ONLY count it
// can read is the per-context one. A game-thread flood cannot advance the
// physics-thread index, so the PT budget is unspendable by GT traffic.
constexpr bool shouldLogFilterCall(std::uint64_t contextCallIndex, bool bDroppedSomething)
{
	return contextCallIndex <= kFilterLogAlwaysCalls ||
	       (bDroppedSomething && (contextCallIndex % kFilterLogDropStride) == 0);
}

static_assert(filterLogContext(true) != filterLogContext(false));
static_assert(filterLogContext(true) < kFilterCtxCount);
static_assert(filterLogContext(false) < kFilterCtxCount);

// Warm-up: unconditional, dropped or not.
static_assert(shouldLogFilterCall(1, false));
static_assert(shouldLogFilterCall(kFilterLogAlwaysCalls, false));
// Past the warm-up: only a dropping call, and only on the stride.
static_assert(!shouldLogFilterCall(kFilterLogAlwaysCalls + 1, false));
static_assert(!shouldLogFilterCall(kFilterLogAlwaysCalls + 1, true));   // 301 % 64 != 0
static_assert(shouldLogFilterCall(320, true));                          // 320 % 64 == 0
static_assert(!shouldLogFilterCall(320, false));
// ⚠ NO static_assert IS WRITTEN FOR THE PROPERTY THAT MATTERS — "a PT call can log
// while GT calls are plentiful" — and that is deliberate. It is not a property of
// this function, which cannot see the other context at all; it is a property of the
// WIRING at the call site (which counter is incremented, and which index is passed).
// A static_assert here could only restate a row above and would be a check that
// cannot fail. The wiring is measured instead, by the out-of-tree harness in
// impl/task40/, which drives 5000 GT calls and then a PT call through the shipped
// text of this block and a transcription of the call site, and whose RED control is
// the pre-T40 single shared counter.
// [task40-filterbudget-end]

// File-scope, not a member: the adapter must keep the copy/move properties it has
// (both composition roots hold it in a std::optional), and a listen server runs
// two instances whose calls should share one throttle budget rather than two.
// ⭐ [movement-sim T40] ONE ENTRY PER CONTEXT. Static storage duration, so both
// counters are zero-initialised before any adapter exists.
std::atomic<std::uint64_t> gFilterCallCount[kFilterCtxCount];
}  // namespace

void ChaosSpatialQueryAdapter::filterDisabledAndUnreadyHits(TArray<FHitResult>& results) const
{
	// Post-filter disabled shapes — the Chaos acceleration structure can only be
	// updated on the game thread, so we post-filter on the physics thread.
	const bool bPhysicsThread = Chaos::IsInPhysicsThreadContext();

	FilterTally tally;
	tally.raw = results.Num();

	// Generic over the two shapes-array types on purpose: the physics-thread and
	// game-thread arrays are DIFFERENT types with the same shape-level API, and
	// naming either one would mean reading engine headers. `auto` lets the compiler
	// name them. The Num() guard is new — indexing [0] of an empty array was
	// undefined behaviour before, and "no shapes" now reads as "not enabled".
	const auto isQueryEnabled = [](const auto& shapes)
	{
		return shapes.Num() > 0 && shapes[0]->GetQueryEnabled();
	};

	for (int32 i = results.Num() - 1; i >= 0; --i)
	{
		FHitResult& hit = results[i];

		// ⚠ [movement-sim T38] All three of these were UNGUARDED dereferences before
		// task 38, and the game-thread branch had no null check on the proxy at all
		// — which is why task 9's probe routed its game-thread control AROUND this
		// adapter rather than through it (Spike9Probe.cpp:639). Question 3.
		UPrimitiveComponent* component = hit.GetComponent();
		FBodyInstance* bodyInstance = component ? component->GetBodyInstance() : nullptr;
		auto* proxy = bodyInstance
			? bodyInstance->GetBodyInstanceAsyncPhysicsTickHandle().Proxy
			: nullptr;

		auto* ptApi = (proxy && bPhysicsThread) ? proxy->GetPhysicsThreadAPI() : nullptr;

		// Ask the body whether it simulates ONLY when the answer can change the
		// outcome: on the physics thread, for a proxy with no physics-thread view.
		// On the game thread the external view is always the right one to read,
		// which is what passing hasPhysicsThreadView = bodySimulatesPhysics = false
		// selects — so the game-thread branch keeps its pre-task-38 behaviour
		// exactly, minus the null dereference.
		const bool bSimulates = (bPhysicsThread && proxy && !ptApi)
			? component->IsSimulatingPhysics()
			: false;

		const ShapeStateSource source =
			selectShapeStateSource(proxy != nullptr, ptApi != nullptr, bSimulates);

		bool bQueryEnabled = false;
		switch (source)
		{
		case ShapeStateSource::PhysicsThread:
			bQueryEnabled = isQueryEnabled(ptApi->ShapesArray());
			break;
		case ShapeStateSource::GameThread:
			bQueryEnabled = isQueryEnabled(proxy->GetGameThreadAPI().ShapesArray());
			break;
		case ShapeStateSource::Unreadable:
			break;
		}

		if (keepsHit(source, bQueryEnabled))
		{
			if (source == ShapeStateSource::PhysicsThread)
				++tally.keptPtView;
			else
				++tally.keptGtView;
			continue;
		}

		if (!component)                                     ++tally.noComponent;
		else if (!bodyInstance)                             ++tally.noBodyInstance;
		else if (!proxy)                                    ++tally.noProxy;
		else if (source == ShapeStateSource::Unreadable)    ++tally.spawnWindow;
		else if (source == ShapeStateSource::PhysicsThread) ++tally.disabledPtView;
		else                                                ++tally.disabledGtView;

		results.RemoveAt(i);
	}

	// ⭐ [movement-sim T38] Question 1's instrument, and it is deliberately BOTH
	// counts on ONE line: raw= is the result count immediately before the filter,
	// kept= the count immediately after it. raw=0 says GeomSweepMulti returned
	// nothing and the filter is innocent; raw>0 with kept=0 says the filter ate the
	// hit, and the drop counters say which branch did it.
	// ⭐ [movement-sim T40] THE WIRING THAT MAKES THE BUDGET PER-CONTEXT. `call=` in the
	// line below is now a PER-CONTEXT ordinal, not a process-wide one: two counters,
	// each advanced only by its own context, and shouldLogFilterCall is handed the
	// index of the context it is deciding about and nothing else. A game-thread flood
	// can no longer spend the physics thread's budget, which is exactly what voided the
	// 2026-09-05 run.
	const std::size_t filterCtx = filterLogContext(bPhysicsThread);
	const std::uint64_t callIndex =
		gFilterCallCount[filterCtx].fetch_add(1, std::memory_order_relaxed) + 1;
	const bool bDroppedSomething = results.Num() != tally.raw;
	if (shouldLogFilterCall(callIndex, bDroppedSomething))
	{
		UE_LOG(LogOGSpatialQuery, Warning,
			TEXT("[SpatialQuery.Filter] call=%llu ctx=%s raw=%d kept=%d ")
			TEXT("keptPtView=%d keptGtView=%d dropNoComponent=%d dropNoBodyInstance=%d ")
			TEXT("dropNoProxy=%d dropSpawnWindow=%d dropDisabledPt=%d dropDisabledGt=%d"),
			static_cast<unsigned long long>(callIndex),
			bPhysicsThread ? TEXT("PT") : TEXT("GT"),
			tally.raw, results.Num(),
			tally.keptPtView, tally.keptGtView,
			tally.noComponent, tally.noBodyInstance,
			tally.noProxy, tally.spawnWindow,
			tally.disabledPtView, tally.disabledGtView);
	}
}

void ChaosSpatialQueryAdapter::resolveHitIdentity(FHitResult& hit,
                                                  CollisionCategories& outCategories,
                                                  BodyId& outBodyId,
                                                  BodyId& outRootBodyId) const
{
	// Reverse-map: extract Chaos channel from hit, convert to DAttack category.
	auto* bodyInstance = hit.GetComponent()->GetBodyInstance();
	auto handle = bodyInstance->GetBodyInstanceAsyncPhysicsTickHandle();

	// Generic over the two shapes-array types — same reason as in
	// filterDisabledAndUnreadyHits: they are different types with the same
	// shape-level API, and naming either means reading engine headers.
	const auto queryChannel = [](const auto& shapes)
	{
		return static_cast<ECollisionChannel>(
			GetCollisionChannel(shapes[0]->GetQueryData().Word3));
	};

	ECollisionChannel chaosChannel;
	if (Chaos::IsInPhysicsThreadContext())
	{
		// ⭐ [movement-sim T38] ANSWER TO QUESTION 4: yes, this site shared the
		// filter's category error. It read handle.Proxy->GetPhysicsThreadAPI()
		// ->ShapesArray()[0] with NO null guard, and that was safe only because the
		// filter dropped every hit with no physics-thread view — INCLUDING static
		// world geometry, which is the bug. Admitting a static hit past the filter
		// WITHOUT fixing this line would have converted a silent drop into a
		// physics-thread crash, so the two are one change.
		// The fallback source is the external (game-thread) view, and it is the
		// right one for this case specifically: a static body's shapes are fixed at
		// creation so there is no writer to race with, it is the very data the
		// acceleration structure used to produce the hit, and it is exactly what
		// the game-thread branch below reads for the same body.
		auto* ptApi = handle.Proxy->GetPhysicsThreadAPI();
		chaosChannel = ptApi
			? queryChannel(ptApi->ShapesArray())
			: queryChannel(handle.Proxy->GetGameThreadAPI().ShapesArray());
	}
	else
	{
		chaosChannel = queryChannel(handle.Proxy->GetGameThreadAPI().ShapesArray());
	}

	const uint32_t dattackCat = toDAttackCategory(chaosChannel);
	outCategories = (dattackCat != kUnmapped) ? CollisionCategories::single(dattackCat)
	                                          : CollisionCategories{};

	// Body ID from engine's native particle index (the SHAPE body actually hit).
	outBodyId = BodyId{static_cast<uint32_t>(handle.Proxy->GetParticle_LowLevel()->UniqueIdx().Idx)};

	// [hit-resolution T11] Root (actor-level) identity: look up the shape body's
	// registered parent. Character shapes (hurtbox, guard) map to the character
	// capsule's body id, so every shape on a character resolves to the SAME
	// rootBodyId — the id cross-character routing and actor-hit mergers key on.
	// A shape with no registered parent (standalone body) falls back to self-as-root.
	if (auto it = m_shapeBodyToRootBody.find(outBodyId.value); it != m_shapeBodyToRootBody.end())
		outRootBodyId = BodyId{it->second};
	else
		outRootBodyId = outBodyId;
}

void ChaosSpatialQueryAdapter::setVolumeParentTransform(QueryVolumeId volumeId, const glm::mat4& transform)
{
	m_volumes[volumeId.value].parentTransform = transform;
}

void ChaosSpatialQueryAdapter::enableShape(ShapeId shapeId)
{
	auto& entry = m_shapes[shapeId.value];
	entry.body.Proxy->GetPhysicsThreadAPI()->ShapesArray()[entry.shapeIndex]->SetQueryEnabled(true);
}

void ChaosSpatialQueryAdapter::disableShape(ShapeId shapeId)
{
	auto& entry = m_shapes[shapeId.value];
	entry.body.Proxy->GetPhysicsThreadAPI()->ShapesArray()[entry.shapeIndex]->SetQueryEnabled(false);
}

ECollisionChannel ChaosSpatialQueryAdapter::toEngineChannel(uint32_t dattackCategory) const
{
	// ⭐ [movement-sim T40] The RETURN VALUE below cannot be made honest without
	// changing what every caller receives, and that is a different task: 0 is a real
	// channel (ECC_WorldStatic), so there is no spare sentinel to hand back. What CAN
	// be made honest is the record. Report it, once per category per process, and keep
	// the answer bit-for-bit what it was — no caller's behaviour changes here, only the
	// log gains a line that says the answer is not to be trusted.
	// Hard-bounded to 33 lines per process by claimUnmappedCategoryReport, which is
	// what makes this safe on a public method the adapter does not control the callers
	// of. Today those callers are registerVolume and ChaosPhysicsFactory::applyDescriptor,
	// all three of them setup-time.
	if (!isCategoryMapped(m_mappedCategoryMask, dattackCategory))
		reportUnmappedCategory(dattackCategory, TEXT("toEngineChannel"));

	if (dattackCategory < m_toEngine.size())
		return m_toEngine[dattackCategory];
	return static_cast<ECollisionChannel>(0);
}

uint32_t ChaosSpatialQueryAdapter::toDAttackCategory(ECollisionChannel channel) const
{
	const uint32_t idx = static_cast<uint32_t>(channel);
	if (idx < m_toDAttack.size())
		return m_toDAttack[idx];
	return kUnmapped;
}

FCollisionObjectQueryParams ChaosSpatialQueryAdapter::toObjectQueryParams(CollisionCategories categories) const
{
	// ⭐ [movement-sim T40] Reported BEFORE the loop, because the loop structurally
	// cannot see the problem: it bounds itself by m_toEngine.size() and therefore never
	// even TESTS a requested bit past the table, let alone complains about one. That is
	// the mechanism behind the headline failure — a non-empty search that comes out as
	// EMPTY object query params, which is well-formed and matches nothing.
	// This is belt-and-braces to the attributable report in registerVolume, which is
	// today the sole caller: it makes the function loud on its own terms, so a future
	// second caller inherits the diagnostic instead of having to remember it. Iterating
	// all 32 representable categories rather than the table's size is the point — the
	// unmapped bits are precisely the ones the loop below skips.
	const uint32_t unmapped = unmappedRequestedMask(categories.bits, m_mappedCategoryMask);
	for (uint32_t cat = 0; cat < kCollisionCategoryCount; ++cat)
	{
		if ((unmapped & (1u << cat)) != 0u)
			reportUnmappedCategory(cat, TEXT("toObjectQueryParams"));
	}

	// ⚠ The loop below is UNCHANGED, on purpose. Task 40 makes an unmapped category
	// loud; it does not change what any query is built from. In particular a GAP
	// category (in range, never mapped) still contributes its padded
	// ECollisionChannel(0) here exactly as before. That is currently unreachable — the
	// shipped table has no gap and the ctor's static_assert-pinned check would say so
	// — and narrowing it would be a silent semantic change beyond this task's remit.
	FCollisionObjectQueryParams params;
	for (uint32_t cat = 0; cat < static_cast<uint32_t>(m_toEngine.size()); ++cat)
	{
		if (categories.contains(cat))
			params.AddObjectTypesToQuery(m_toEngine[cat]);
	}
	return params;
}

// ⭐ [movement-sim T40] The attributable registration-time diagnostic. See the
// declaration in the header for why registration is both the free site and the only
// site that can name a caller.
void ChaosSpatialQueryAdapter::diagnoseVolumeCategories(const QueryVolumeDescriptor& descriptor,
                                                        QueryVolumeId volumeId) const
{
	const uint32_t requested = descriptor.searchCategories.bits;
	const uint32_t unmapped = unmappedRequestedMask(requested, m_mappedCategoryMask);

	// ⭐ THE HEADLINE CASE. Non-empty in, nothing mapped, so the stored
	// FCollisionObjectQueryParams is EMPTY and every overlap()/sweep() on this volume
	// will report a clean miss forever. This is the shape that survived three PIE runs.
	if (isSilentlyEmptyQuery(requested, m_mappedCategoryMask))
	{
		UE_LOG(LogOGSpatialQuery, Error,
			TEXT("[SpatialQuery.EmptyObjectQuery] volume=%u searchCategories=0x%08X ")
			TEXT("mapped=0x%08X -- NOT ONE requested category is mapped, so this volume was ")
			TEXT("registered with EMPTY object query params. Every overlap() and sweep() on ")
			TEXT("it will match NOTHING, and an empty object query is well-formed: the result ")
			TEXT("is indistinguishable from 'the geometry is genuinely not there'. Map the ")
			TEXT("categories in the adapter's construction table (SimulationManagerUImpl.cpp) ")
			TEXT("or stop searching them."),
			volumeId.value, requested, m_mappedCategoryMask);
	}
	// Partial: some requested categories are mapped and some are not. The query still
	// finds things, which is exactly why this one needs saying out loud -- a volume
	// that finds SOME of what it asked for looks healthy.
	else if (unmapped != 0u)
	{
		UE_LOG(LogOGSpatialQuery, Error,
			TEXT("[SpatialQuery.PartialObjectQuery] volume=%u searchCategories=0x%08X ")
			TEXT("unmapped=0x%08X mapped=0x%08X -- the unmapped bits are SILENTLY DROPPED ")
			TEXT("from this volume's object query. It will find the rest and miss those, and ")
			TEXT("a partial result reads as a healthy one."),
			volumeId.value, requested, unmapped, m_mappedCategoryMask);
	}

	// The trace channel is a separate fact from the search mask and fails separately:
	// an unmapped traceCategory does not narrow the query, it MISDIRECTS it onto
	// WorldStatic.
	if (!isCategoryMapped(m_mappedCategoryMask, descriptor.traceCategory))
	{
		UE_LOG(LogOGSpatialQuery, Error,
			TEXT("[SpatialQuery.UnmappedTraceCategory] volume=%u traceCategory=%u ")
			TEXT("mapped=0x%08X -- toEngineChannel answered ECollisionChannel(0), which IS ")
			TEXT("ECC_WorldStatic. This volume will trace on the WorldStatic channel by ")
			TEXT("accident, and the return value cannot tell you that happened."),
			volumeId.value, descriptor.traceCategory, m_mappedCategoryMask);
	}
}

// [og-netcode-v2-input-relay item 77] Closes the OGSIM_OPTIMIZE_OFF opened
// above. The file had no closing pragma since its initial commit, so the
// whole rest of the TU compiled unoptimized in every build — closing the pair
// here, at true end-of-file, changes nothing (there was no code after this
// point either), it just makes the always-off scope an explicit pair instead
// of an implicit "off to EOF".
OGSIM_OPTIMIZE_ON
