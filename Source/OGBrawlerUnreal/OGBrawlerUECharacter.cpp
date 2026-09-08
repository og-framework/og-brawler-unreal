// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUECharacter.h"
#include "OGSimulation/CompilerControl.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "DVolume/DVolume.h"
#include "DVolume/DVolumeAsset.h"
#include "OGBrawlerUnreal/DShapeUImplementation.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGBrawlerUnreal/DAttackCircleUImplementation.h"
#include "OGBrawlerUnreal/HumanoidMeshBuilder.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "OGBrawler/DAttackRadialSimulation.h"
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"
#include "OGSimulation/DMathUtil.h"
#include "glm/mat4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/Declares.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackInput.h"
#include "Chaos/ChaosMarshallingManager.h"


DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AOGBrawlerUECharacter

OGSIM_OPTIMIZE_OFF

AOGBrawlerUECharacter::AOGBrawlerUECharacter()
	: m_attackTimer(0.f)
	, m_attackSegments(8)
	, m_innerRadius(100.f)
	, m_outerRadius(300.f)
	, m_currentAttackAngle(0.f)
	, m_offsetWithSegmentHalf(false)
	, m_forwardRangeMultiplier(1.f)
{

	glm::mat4x4 sphereTransform(glm::identity<glm::mat4x4>());
	m_volumeAssets.emplace_back(DSphereAsset{ 1.f });
	//m_volume
	//m_volumeAssets->emplace_back(DSphereAsset{ 1.f });
	DVolumeCreationParams volumeCreationParams;
	volumeCreationParams.assets.push_back({ &m_volumeAssets[0], sphereTransform });

	DVolume<DShapeUImplementation> volume(volumeCreationParams);

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// =====================================================================================
	// ⛔⛔ CMC RETIRED — locomotion is `brawlerMovementSimulation`; do not re-enable.
	//
	// [movement-sim task 15] Every tuning knob that used to live here (JumpZVelocity,
	// AirControl, the walk-speed cap, the analog-walk floor, the braking decelerations) is now
	// authored ONCE in `simulatableBrawler::StaticData::m_movementStaticData`
	// (`OGBrawler/SimulatableBrawlerTypes.h`), which is the sub-simulation's StaticData and
	// the only place any of them is spelled. `Move()`, its engine movement-input call, and the
	// per-Tick walk-speed-cap resync were all deleted in the same change.
	//
	// ⚠ THE THREE CALLS BELOW ARE BELT-AND-BRACES, NOT THE MECHANISM. What actually stops
	// the CMC is `brawlerMovementSimulation::PhysicsSetup::body.simulatePhysics = true`: the
	// physics factory's adopt-root pass simulates THIS character's capsule, the capsule IS the
	// CMC's `UpdatedComponent`, and UE 5.6's CMC early-returns on
	// `UpdatedComponent->IsSimulatingPhysics()` on both its sync and its async path. These
	// calls make the retirement legible at the class that owns the component, and stop the
	// component ticking for nothing.
	//
	// ⛔ RE-ENABLING THIS WITHOUT FLIPPING BOTH SIM FLAGS BACK GIVES TWO AUTHORITIES writing
	// one capsule. Flipping only `simulatePhysics` back gives none. See the paired comments on
	// `StaticData::drivesBody` and `PhysicsSetup::body` in `BrawlerMovementSimulation.h`.
	//
	// The CharacterMovementComponent.h include is KEPT ON PURPOSE — these three disable calls
	// need the complete type. It is not a leftover.
	// =====================================================================================
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCharacterMovement()->bAutoActivate = false;

	// ⭐ FRICTION 0 / RESTITUTION 0 ON THE CAPSULE, and the mechanism is named: a
	// `UPhysicalMaterial` default subobject assigned as the primitive's override, so it needs
	// no content asset and cannot be un-set by a Blueprint that forgets to inherit one.
	// RESTITUTION 0 IS LOAD-BEARING (revision 6): step 6' measures the solver's positional
	// push-out and adopts it, so a bouncy capsule would feed a rebound back into the movement
	// state. FRICTION 0 is recorded rather than relied on — the sim re-writes
	// {position, velocity} every tick, so a tangential friction impulse has nothing to
	// accumulate into. Mass is deliberately NOT asserted (revision 6: mass plays no part in
	// the movement path).
	CapsulePhysicalMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("BrawlerCapsulePhysMat"));
	CapsulePhysicalMaterial->Friction = 0.f;
	CapsulePhysicalMaterial->Restitution = 0.f;
	GetCapsuleComponent()->SetPhysMaterialOverride(CapsulePhysicalMaterial);

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 900.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = false; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
	
	// Create a PhysicsComponent

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	//a Camera settings
	m_cameraAxis = CreateDefaultSubobject<USphereComponent>(TEXT("CameraAxis"));
	m_cameraAxis->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	m_cameraAxis->SetSimulatePhysics(true);
	m_cameraAxis->SetSphereRadius(30.f);
	m_cameraAxis->SetVisibility(true);
	m_cameraAxis->SetEnableGravity(false);

	SimmableUpdateComponent = CreateDefaultSubobject<USimmableUpdateComponent>(TEXT("USimmableUpdateComponent"));
	InputCollection = CreateDefaultSubobject<UOGBrawlerInputCollectionComponent>(TEXT("InputCollection"));

	OnCalculateCustomPhysics.BindUObject(this, &AOGBrawlerUECharacter::CustomPhysics);

	//
	// Tracks the Chaos async-physics tick rate (60 Hz; Config/DefaultEngine.ini
	// AsyncFixedTimeStepSize). Replicating faster than the sim produces new states
	// just wastes bandwidth, so this moves in lockstep with the tick-rate flip (T15).
	this->NetUpdateFrequency = 60.0f; // Higher = more updates per second
	this->MinNetUpdateFrequency = 60.0f;

	// [movement-sim task 15] ⛔ ACTOR MOVEMENT REPLICATION OFF (R6). The capsule's pose is
	// carried by the simulation's own state wire and reproduced on every peer by
	// `brawlerMovementSimulation::integrate` step 5, then corrected through the resim path.
	// Leaving UE's actor-movement replication on would put a SECOND, unsynchronised copy of
	// the same pose on the wire and fight the correction with visible rubber-banding.
	this->SetReplicateMovement(false);

	this->bAlwaysRelevant = true; // Always relevant for network updates

	this->NetCullDistanceSquared = 10000.f* 10000.f; // Distance at which the character is culled from the network updates

	HumanoidMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HumanoidMesh"));
	HumanoidMesh->SetupAttachment(RootComponent);
	// [movement-sim task 15] THE CAPSULE HOVERS NOW, so the feet no longer sit at
	// `-CapsuleHalfHeight`. `brawlerMovementSimulation`'s hover servo holds the capsule
	// CENTRE at `rideHeight` above the surface, so the mesh must drop by the half-height AND
	// the ride height for the feet to touch the ground.
	// ⚠ THE 10.f MIRRORS `m_movementStaticData.rideHeight` (`SimulatableBrawlerTypes.h`, the
	// one place it is authored) exactly as the 42/96 capsule dimensions mirror
	// `InitCapsuleSize` above — the sub-simulation is engine-free and cannot read this file,
	// so the pairing is maintained by hand. CHANGE BOTH TOGETHER or the character walks with
	// its feet 10 cm into, or 10 cm above, the floor.
	static constexpr float kRideHeightMirror = 10.f;
	HumanoidMesh->SetRelativeLocation(FVector(
		0.f, 0.f, -(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + kRideHeightMirror)));
	HumanoidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HumanoidMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicShapeMat.Succeeded())
	{
		HumanoidBaseMaterial = BasicShapeMat.Object;
	}

	m_humanoidViz = humanoidVisualizationDefaults::buildDefault();

	GetMesh()->SetVisibility(false);
}

void AOGBrawlerUECharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	RebuildHumanoidMesh();
}

void AOGBrawlerUECharacter::RebuildHumanoidMesh()
{
	static constexpr int32 LatSegs = 12;
	static constexpr int32 LonSegs = 16;

	UMaterialInterface* defaultMat = HumanoidBaseMaterial
		? HumanoidBaseMaterial
		: UMaterial::GetDefaultMaterial(MD_Surface);

	auto createSection = [this, defaultMat](int32 section, const HumanoidMeshBuilder::SectionGeometry& geo)
	{
		HumanoidMesh->CreateMeshSection_LinearColor(
			section,
			geo.vertices,
			geo.triangles,
			geo.normals,
			geo.uvs,
			geo.vertexColors,
			geo.tangents,
			/*bCreateCollision=*/false);
		HumanoidMesh->SetMaterial(section, defaultMat);
	};

	{
		HumanoidMeshBuilder::SectionGeometry geo;
		HumanoidMeshBuilder::buildEllipsoidSection(m_humanoidViz.head, LatSegs, LonSegs, geo);
		createSection(0, geo);
	}
	{
		HumanoidMeshBuilder::SectionGeometry geo;
		HumanoidMeshBuilder::buildEllipsoidSection(m_humanoidViz.torso, LatSegs, LonSegs, geo);
		createSection(1, geo);
	}
	{
		HumanoidMeshBuilder::SectionGeometry geo;
		HumanoidMeshBuilder::buildEllipsoidSection(m_humanoidViz.legs, LatSegs, LonSegs, geo);
		createSection(2, geo);
	}

	// ⛔ MUST BE LAST. Every createSection above sets the BASE material, so a rebuild
	// silently reverts the brawler tint. This re-applies it. Do not move this above
	// the sections, and do not drop it when adding a fourth section.
	ApplyBrawlerColor();
}


void AOGBrawlerUECharacter::BeginPlay()
{
	Super::BeginPlay();

	// Belt-and-suspenders IMC-add — works for LP0 at game start (BeginPlay
	// deferred until after possession). For mid-game-spawned pawns (LP1+),
	// PossessedBy and OnRep_Controller cover the cases this site misses;
	// SetupPlayerInputComponent re-adds after the translator is initialized.
	InputCollection->addInputMappingContextForController(Controller);
}

// ---------------------------------------------------------------------------
// BRAWLER COLOUR — a distinct tint per brawler, server-assigned, replicated.
//
// ⛔ KEYED PER BRAWLER, NOT PER CONNECTION. This game supports couch co-op:
// AOGBrawlerPlayerController::JoinLocalPlayer calls CreatePlayer(ControllerId=-1),
// so several players share ONE connection. Anything keyed on the connection or its
// address gives every sibling on a couch the same colour. The counter below is
// bumped once per POSSESSION, which is per brawler.
//
// Stable for a whole run, deliberately NOT stable across runs: a
// leaver's slot is never reused, so a rejoining player gets the next colour rather
// than their old one.
// ---------------------------------------------------------------------------
namespace
{
	// Hand-picked so neighbours stay distinguishable for a colour-blind reader and
	// against the level's grey; 10 entries for a 6-brawler target, so the wrap below
	// is a safety net rather than an expected path.
	const FLinearColor kBrawlerPalette[] = {
		FLinearColor(0.90f, 0.20f, 0.20f), // red
		FLinearColor(0.20f, 0.45f, 0.95f), // blue
		FLinearColor(0.95f, 0.75f, 0.10f), // amber
		FLinearColor(0.20f, 0.75f, 0.35f), // green
		FLinearColor(0.70f, 0.30f, 0.85f), // violet
		FLinearColor(0.15f, 0.80f, 0.80f), // cyan
		FLinearColor(0.95f, 0.50f, 0.15f), // orange
		FLinearColor(0.95f, 0.45f, 0.70f), // pink
		FLinearColor(0.55f, 0.75f, 0.20f), // lime
		FLinearColor(0.45f, 0.35f, 0.75f), // indigo
	};
	constexpr int32 kBrawlerPaletteCount = UE_ARRAY_COUNT(kBrawlerPalette);

	// ⛔ SERVER-ONLY. Never read or written on a client — a client's count would
	// diverge from the authority's and hand two brawlers the same tint.
	int32 gNextBrawlerColorIndex = 0;
}

void AOGBrawlerUECharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ⛔ NOT COND_OwnerOnly — every client must see every brawler's colour.
	DOREPLIFETIME(AOGBrawlerUECharacter, BrawlerColor);
}

void AOGBrawlerUECharacter::OnRep_BrawlerColor()
{
	ApplyBrawlerColor();
}

void AOGBrawlerUECharacter::ApplyBrawlerColor()
{
	if (HumanoidMesh == nullptr) return;

	if (HumanoidColorMID == nullptr)
	{
		UMaterialInterface* baseMat = HumanoidBaseMaterial
			? HumanoidBaseMaterial
			: UMaterial::GetDefaultMaterial(MD_Surface);
		HumanoidColorMID = UMaterialInstanceDynamic::Create(baseMat, this);
		if (HumanoidColorMID == nullptr) return;
	}

	// `Color` is BasicShapeMaterial's vector parameter. A wrong name here fails
	// SILENTLY — the mesh renders in the base tint and nothing logs — so it is
	// named once, at this single write site, rather than being spread around.
	HumanoidColorMID->SetVectorParameterValue(TEXT("Color"), BrawlerColor);

	// RebuildHumanoidMesh() re-applies the BASE material to every section, so the
	// MID must be re-applied after any rebuild, not just once at possession.
	const int32 sectionCount = HumanoidMesh->GetNumSections();
	for (int32 section = 0; section < sectionCount; ++section)
	{
		HumanoidMesh->SetMaterial(section, HumanoidColorMID);
	}
}

void AOGBrawlerUECharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Authority only: PossessedBy does not run on clients, but HasAuthority() is
	// stated rather than assumed because the counter must never advance twice.
	if (HasAuthority())
	{
		BrawlerColor = kBrawlerPalette[gNextBrawlerColorIndex % kBrawlerPaletteCount];
		++gNextBrawlerColorIndex;

		// The listen-server's own pawn gets no OnRep, so apply here too.
		ApplyBrawlerColor();
	}

	// Server-side hook for mid-game-spawned pawns: fires after Controller is
	// wired, which BeginPlay's IMC-add misses because BeginPlay can run before
	// Possess on the server.
	InputCollection->addInputMappingContextForController(NewController);
}

void AOGBrawlerUECharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// Client-side mirror: in PIE-as-client / dedicated-server-client setups
	// PossessedBy fires only on the server. On the client, the Controller
	// pointer replicates in and OnRep_Controller fires here.
	InputCollection->addInputMappingContextForController(Controller);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AOGBrawlerUECharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Initialize translator here — SetupPlayerInputComponent runs before BeginPlay (during possession).
	InputCollection->initializeTranslator();

	// CRITICAL: for client-side mid-game-spawned pawns (LP1+ via CreatePlayer),
	// the IMC-add calls in OnRep_Controller / PossessedBy / BeginPlay all fire
	// BEFORE this initialize() because client-side possession/replication order
	// is different from LP0's game-start order. Those earlier addToSubsystem
	// calls are silent no-ops because m_inputTranslator's mapping context is
	// still null. We re-add here so the joining LP's Enhanced Input subsystem
	// actually receives the IMC and can match incoming gamepad events.
	InputCollection->addInputMappingContextForController(Controller);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		InputCollection->setupBindings(EnhancedInputComponent);
		m_inputComponent = EnhancedInputComponent;

		// [movement-sim task 15] The character's own Move binding is GONE with the CMC path.
		// The Move action now has exactly ONE consumer: `InputCollection::onMove`, which caches
		// the stick for `buildPlayerInput()` to relay to the simulation on the physics tick.
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AOGBrawlerUECharacter::PhysicsTick_Implementation(float SubstepDeltaTime)
{
}

void AOGBrawlerUECharacter::CustomPhysics(float DeltaTime, FBodyInstance* BodyInstance)
{
	PhysicsTick(DeltaTime);
}

void AOGBrawlerUECharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	InputCollection->updateGameThreadCache();

	// [movement-sim task 15] The per-frame walk-speed-cap resync is GONE with the CMC path.
	// ⭐ [movement-sim task 17] AND THE WINDOW IT OPENED IS CLOSED. This block used to warn that
	// `OGBrawler.MoveSpeed` had NO READER "until task 16" and that the console lever was inert.
	// Task 16 has landed: `readMovementStaticDataCVars()` (MovementSchemeCVar.cpp) reads the cvar
	// ONCE, when the manager builds its `StaticData`, into the movement sub-simulation's
	// `maxWalkSpeed`. Mid-session changes are still ignored — that is the design, and each sink
	// says so out loud — but the lever is no longer inert.

	const glm::vec2 lookStick = InputCollection->consumeLookStick();

	DPIDSettings settings(0.03f, 0.01f, 0.01f);
	const glm::vec3 aimStick3 = glm::vec3(InputCollection->getAimStick(), 0.f);
	dAttackCameraBehaviour::integrate(DeltaSeconds, DAttackCameraInput{ aimStick3, lookStick, InputCollection->getBlockLook(), 0.8f, settings }, m_cameraState);
	CameraBoom->SetRelativeRotation(uglm::toFRotator(m_cameraState.getCameraBoomTransform()));
	CameraBoom->TargetArmLength = m_cameraState.getCameraBoomLength();

	{ // rotate the character mesh
		glm::vec3 worldAimDirection = InputCollection->buildAimDirection();

		glm::vec3 tmpAimInput = glm::vec3(1.f, 0.f, 0.f);
		if (InputCollection->hasInputComponent())
			tmpAimInput = worldAimDirection;

		glm::mat4 aimRotationMatrix2;
		const glm::vec3 defaultLeft(0.f, 1.f, 0.f);
		dMathUtil::getRotationMatrix(defaultLeft, tmpAimInput, aimRotationMatrix2);
		FTransform fAimRotationMatrix = uglm::toFtransform(aimRotationMatrix2);
		GetMesh()->SetWorldRotation(fAimRotationMatrix.GetRotation());

		glm::mat4 humanoidAimRotationMatrix;
		const glm::vec3 humanoidDefaultForward(0.f, 1.f, 0.f);
		dMathUtil::getRotationMatrix(humanoidDefaultForward, tmpAimInput, humanoidAimRotationMatrix);
		HumanoidMesh->SetWorldRotation(uglm::toFtransform(humanoidAimRotationMatrix).GetRotation());
	}

	if(!InputCollection->hasInputComponent())
		DrawDebugSphere(GetWorld(), GetMesh()->GetComponentTransform().GetTranslation() + FVector(0.f, 0.f, 100.f), 10, 10, FColor::Green);

}

void AOGBrawlerUECharacter::Attack(const FInputActionValue& Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("attack!")));

}

// [og-netcode-v2-input-relay item 77] Closes the OGSIM_OPTIMIZE_OFF opened
// above (AOGBrawlerUECharacter ctor). The file had no closing pragma since its
// initial commit, so the whole rest of the TU compiled unoptimized in every
// build — closing the pair here, at true end-of-file, changes nothing (there
// was no code after this point either), it just makes the always-off scope an
// explicit pair instead of an implicit "off to EOF".
OGSIM_OPTIMIZE_ON