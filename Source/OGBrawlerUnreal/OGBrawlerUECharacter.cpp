// SPDX-License-Identifier: BUSL-1.1
// docs/OGBrawlerUECharacter-rationale.md · docs/OGBrawlerUECharacter-guards.md

#include "OGBrawlerUECharacter.h"
#include "OGSimulation/CompilerControl.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
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
#include "OGBrawlerUnreal/SharedIsometricCameraActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "OGBrawler/OGBrawlerLog.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "OGBrawler/DAttackRadialSimulation.h"
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"
#include "OGBrawler/BrawlerScoreboardVisualization.h"
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

namespace
{
	float g_camMouseSens = 1.f;
	float g_camStickSens = 1.f;
	bool g_camInvertMouseX = false;
	bool g_camInvertMouseY = false;
	bool g_camInvertStickY = false;
	float g_camPitchMinDeg = dAttackCameraBehaviour::kPitchMinDeg;
	float g_camPitchMaxDeg = dAttackCameraBehaviour::kPitchMaxDeg;
	float g_camPullEFoldDeg = dAttackCameraBehaviour::kPullYawDegreesPerEFold;
	float g_camPullMaxDegPerSec = dAttackCameraBehaviour::kPullMaxDegPerSec;
	float g_camPullHoldoffSec = dAttackCameraBehaviour::kVerticalHoldoffSeconds;
	float g_camPullDominanceRatio = dAttackCameraBehaviour::kDominanceRatio;

	FAutoConsoleVariableRef CVarCamMouseSens(TEXT("og.cam.mouseSens"), g_camMouseSens,
		TEXT("Solo camera: mouse look sensitivity multiplier (1 = the og-brawler default)"));
	FAutoConsoleVariableRef CVarCamStickSens(TEXT("og.cam.stickSens"), g_camStickSens,
		TEXT("Solo camera: look stick sensitivity multiplier (1 = the og-brawler default rates)"));
	FAutoConsoleVariableRef CVarCamInvertMouseX(TEXT("og.cam.invertMouseX"), g_camInvertMouseX,
		TEXT("Solo camera: 1 = mouse right turns the view left (the pre-orbit-camera direction)"));
	FAutoConsoleVariableRef CVarCamInvertMouseY(TEXT("og.cam.invertMouseY"), g_camInvertMouseY,
		TEXT("Solo camera: 1 = mouse up looks down"));
	FAutoConsoleVariableRef CVarCamInvertStickY(TEXT("og.cam.invertStickY"), g_camInvertStickY,
		TEXT("Solo camera: 1 = look stick up looks down"));
	FAutoConsoleVariableRef CVarCamPitchMin(TEXT("og.cam.pitchMin"), g_camPitchMinDeg,
		TEXT("Solo camera: smallest pitch in degrees below the horizon (sanitized into [1, 89])"));
	FAutoConsoleVariableRef CVarCamPitchMax(TEXT("og.cam.pitchMax"), g_camPitchMaxDeg,
		TEXT("Solo camera: largest pitch in degrees below the horizon (sanitized into [pitchMin, 89])"));
	FAutoConsoleVariableRef CVarCamPullEFoldDeg(TEXT("og.cam.pullEFoldDeg"), g_camPullEFoldDeg,
		TEXT("Solo camera: degrees of yaw per e-fold of the pull toward the iso pitch; smaller is stronger (raised to at least 1)"));
	FAutoConsoleVariableRef CVarCamPullMaxDegPerSec(TEXT("og.cam.pullMaxDegPerSec"), g_camPullMaxDegPerSec,
		TEXT("Solo camera: fastest the pull may move the pitch, in degrees per second (0 = no pull)"));
	FAutoConsoleVariableRef CVarCamPullHoldoffSec(TEXT("og.cam.pullHoldoffSec"), g_camPullHoldoffSec,
		TEXT("Solo camera: seconds the pull pauses after a deliberate vertical look (0 = never pauses, also not during vertical look)"));
	FAutoConsoleVariableRef CVarCamPullDominanceRatio(TEXT("og.cam.pullDominanceRatio"), g_camPullDominanceRatio,
		TEXT("Solo camera: weakens the pull on diagonal look; 0 = full pull on any diagonal, 2 = no pull past ~27 degrees off horizontal"));

	dAttackCameraBehaviour::OrbitCameraSettings orbitCameraSettingsFromCVars()
	{
		dAttackCameraBehaviour::OrbitCameraSettings settings;
		settings.mouseSens = g_camMouseSens;
		settings.stickSens = g_camStickSens;
		settings.invertMouseY = g_camInvertMouseY;
		settings.invertStickY = g_camInvertStickY;
		settings.pitchMinDeg = g_camPitchMinDeg;
		settings.pitchMaxDeg = g_camPitchMaxDeg;
		settings.pullYawDegreesPerEFold = g_camPullEFoldDeg;
		settings.pullMaxDegPerSec = g_camPullMaxDegPerSec;
		settings.verticalHoldoffSeconds = g_camPullHoldoffSec;
		settings.dominanceRatio = g_camPullDominanceRatio;
		return settings;
	}
}

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
	DVolumeCreationParams volumeCreationParams;
	volumeCreationParams.assets.push_back({ &m_volumeAssets[0], sphereTransform });

	DVolume<DShapeUImplementation> volume(volumeCreationParams);

	// ⛔G-02  docs/OGBrawlerUECharacter-guards.md
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCylinder"));
	// ⛔G-01  docs/OGBrawlerUECharacter-guards.md
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
	GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(true);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetCapsuleComponent()->bDynamicObstacle = true;
	RootComponent = GetCapsuleComponent();
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	// ⛔G-03  docs/OGBrawlerUECharacter-guards.md

	CapsulePhysicalMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("BrawlerCapsulePhysMat"));
	CapsulePhysicalMaterial->Friction = 0.f;
	// ⛔G-04  docs/OGBrawlerUECharacter-guards.md
	CapsulePhysicalMaterial->Restitution = 0.f;
	// ⛔G-17  docs/OGBrawlerUECharacter-guards.md

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = dAttackCameraBehaviour::kBoomLengthAtTargetPitch;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetUsingAbsoluteRotation(true);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	
	m_cameraAxis = CreateDefaultSubobject<USphereComponent>(TEXT("CameraAxis"));
	m_cameraAxis->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	m_cameraAxis->SetSimulatePhysics(true);
	m_cameraAxis->SetSphereRadius(30.f);
	m_cameraAxis->SetVisibility(true);
	m_cameraAxis->SetEnableGravity(false);

	SimmableUpdateComponent = CreateDefaultSubobject<USimmableUpdateComponent>(TEXT("USimmableUpdateComponent"));
	InputCollection = CreateDefaultSubobject<UOGBrawlerInputCollectionComponent>(TEXT("InputCollection"));

	OnCalculateCustomPhysics.BindUObject(this, &AOGBrawlerUECharacter::CustomPhysics);

	this->NetUpdateFrequency = 60.0f;
	this->MinNetUpdateFrequency = 60.0f;

	// ⛔G-05  docs/OGBrawlerUECharacter-guards.md
	this->SetReplicateMovement(false);

	this->bAlwaysRelevant = true;

	this->NetCullDistanceSquared = 10000.f* 10000.f;

	HumanoidMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HumanoidMesh"));
	HumanoidMesh->SetupAttachment(RootComponent);
	// ⛔G-06  docs/OGBrawlerUECharacter-guards.md
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
}

void AOGBrawlerUECharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	GetCapsuleComponent()->SetPhysMaterialOverride(CapsulePhysicalMaterial);
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

	// ⛔G-07  docs/OGBrawlerUECharacter-guards.md
	ApplyBrawlerColor();
}

void AOGBrawlerUECharacter::BeginPlay()
{
	Super::BeginPlay();

	InputCollection->addInputMappingContextForController(Controller);
}

namespace
{
	const FLinearColor kBrawlerPalette[] = {
		FLinearColor(0.90f, 0.20f, 0.20f),
		FLinearColor(0.20f, 0.45f, 0.95f),
		FLinearColor(0.95f, 0.75f, 0.10f),
		FLinearColor(0.20f, 0.75f, 0.35f),
		FLinearColor(0.70f, 0.30f, 0.85f),
		FLinearColor(0.15f, 0.80f, 0.80f),
		FLinearColor(0.95f, 0.50f, 0.15f),
		FLinearColor(0.95f, 0.45f, 0.70f),
		FLinearColor(0.55f, 0.75f, 0.20f),
		FLinearColor(0.45f, 0.35f, 0.75f),
	};
	constexpr int32 kBrawlerPaletteCount = UE_ARRAY_COUNT(kBrawlerPalette);

	static_assert(
		kBrawlerPaletteCount
			>= static_cast<int32>(brawlerScoreboardVisualization::kScoreboardMaxRows),
		"the brawler palette must carry at least one distinct tint per drawable scoreboard "
		"row, or two rows of the ring-out scoreboard can show the same swatch");

	int32 gNextBrawlerColorIndex = 0;
}

void AOGBrawlerUECharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ⛔G-08  docs/OGBrawlerUECharacter-guards.md
	DOREPLIFETIME(AOGBrawlerUECharacter, BrawlerColor);

	// ⛔G-09  docs/OGBrawlerUECharacter-guards.md
	DOREPLIFETIME(AOGBrawlerUECharacter, RingoutScore);

	// ⛔G-10  docs/OGBrawlerUECharacter-guards.md
	DOREPLIFETIME(AOGBrawlerUECharacter, SimCharacterIdValue);
}

void AOGBrawlerUECharacter::OnRep_BrawlerColor()
{
	ApplyBrawlerColor();
}

void AOGBrawlerUECharacter::OnRep_RingoutScore()
{
	// ⛔G-11  docs/OGBrawlerUECharacter-guards.md
	OGBLOG_G("[Warning][Ringout.score.client] id=%u score=%d",
		toStorageKey(GetSimCharacterId()), static_cast<int>(RingoutScore));
}

SimCharacterId AOGBrawlerUECharacter::GetSimCharacterId() const
{
	static_assert(std::is_same_v<decltype(SimCharacterIdValue), std::underlying_type_t<SimCharacterId>>,
		"SimCharacterIdValue is the replicated carrier of SimCharacterId and must be exactly its "
		"underlying byte, or the cast below narrows or widens the id on its way off the wire.");
	return static_cast<SimCharacterId>(SimCharacterIdValue);
}

void AOGBrawlerUECharacter::SetAuthoritativeSimCharacterId(SimCharacterId NewId)
{
	checkf(HasAuthority(),
		TEXT("SetAuthoritativeSimCharacterId called on a non-authority peer. The id is assigned only ")
		TEXT("by the authority's ASimulationManagerUImpl::allocateSimCharacterId; a client learns it ")
		TEXT("through replication of SimCharacterIdValue."));
	checkf(NewId != SimCharacterId::None,
		TEXT("SetAuthoritativeSimCharacterId called with SimCharacterId::None; 0 means 'no character' ")
		TEXT("and is never assigned."));
	checkf(SimCharacterIdValue == 0u,
		TEXT("SetAuthoritativeSimCharacterId: this character already holds id=%u and was handed id=%u. ")
		TEXT("A character's simulation id is assigned once and never changes (SimCharacterId G-01)."),
		static_cast<unsigned int>(SimCharacterIdValue), toStorageKey(NewId));
	SimCharacterIdValue = static_cast<uint8>(NewId);
}

void AOGBrawlerUECharacter::SetAuthoritativeRingoutScore(int32 NewScore)
{
	checkf(HasAuthority(),
		TEXT("SetAuthoritativeRingoutScore called on a non-authority peer (id=%u). The score ")
		TEXT("is pushed only from ASimulationManagerUImpl::OnPostPhysicsStep under the ")
		TEXT("authority manager's !runsPrediction() gate; a client learns it through OnRep_RingoutScore."),
		toStorageKey(GetSimCharacterId()));

	if (RingoutScore == NewScore)
		return;

	RingoutScore = NewScore;
	// ⛔G-12  docs/OGBrawlerUECharacter-guards.md

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

	// ⛔G-13  docs/OGBrawlerUECharacter-guards.md
	HumanoidColorMID->SetVectorParameterValue(TEXT("Color"), BrawlerColor);

	const int32 sectionCount = HumanoidMesh->GetNumSections();
	for (int32 section = 0; section < sectionCount; ++section)
	{
		HumanoidMesh->SetMaterial(section, HumanoidColorMID);
	}
}

void AOGBrawlerUECharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// ⛔G-14  docs/OGBrawlerUECharacter-guards.md
	if (HasAuthority())
	{
		BrawlerColor = kBrawlerPalette[gNextBrawlerColorIndex % kBrawlerPaletteCount];
		++gNextBrawlerColorIndex;

		// ⛔G-15  docs/OGBrawlerUECharacter-guards.md
		ApplyBrawlerColor();
	}

	InputCollection->addInputMappingContextForController(NewController);
}

void AOGBrawlerUECharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	InputCollection->addInputMappingContextForController(Controller);
}

void AOGBrawlerUECharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	InputCollection->initializeTranslator();

	// ⛔G-16  docs/OGBrawlerUECharacter-guards.md
	InputCollection->addInputMappingContextForController(Controller);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		InputCollection->setupBindings(EnhancedInputComponent);
		m_inputComponent = EnhancedInputComponent;

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

void AOGBrawlerUECharacter::seedCameraBoomAtIsoRotation()
{
	const dAttackCameraBehaviour::OrbitCameraSettings settings = orbitCameraSettingsFromCVars();
	const FRotator isoRotation = ASharedIsometricCameraActor::resolveIsoRotation();
	m_cameraState = dAttackCameraBehaviour::seedFromUERotation(isoRotation.Pitch, isoRotation.Yaw, settings);
	m_cameraBoomSeeded = true;
	applyCameraStateToBoom(dAttackCameraBehaviour::pitchDegFromUEPitchDegrees(isoRotation.Pitch, settings), settings);
}

void AOGBrawlerUECharacter::applyCameraStateToBoom(
	float targetPitchDeg, const dAttackCameraBehaviour::OrbitCameraSettings& settings)
{
	CameraBoom->SetWorldRotation(FRotator(-m_cameraState.pitchDeg, m_cameraState.yawDeg, 0.f));
	CameraBoom->TargetArmLength = dAttackCameraBehaviour::boomLength(m_cameraState.pitchDeg, targetPitchDeg, settings);
}

void AOGBrawlerUECharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	InputCollection->updateGameThreadCache();

	const glm::vec2 lookUnits = InputCollection->consumeLookStick();

	if (!m_cameraBoomSeeded && IsLocallyControlled())
	{
		seedCameraBoomAtIsoRotation();
	}

	if (m_cameraBoomSeeded)
	{
		const dAttackCameraBehaviour::OrbitCameraSettings settings = orbitCameraSettingsFromCVars();
		const float targetPitchDeg = dAttackCameraBehaviour::pitchDegFromUEPitchDegrees(
			ASharedIsometricCameraActor::resolveIsoRotation().Pitch, settings);
		const float mouseXSign = g_camInvertMouseX ? -1.f : 1.f;
		const glm::vec2 aimStick = InputCollection->getAimStick();
		// ⛔G-19  docs/OGBrawlerUECharacter-guards.md
		const dAttackCameraBehaviour::OrbitCameraInput input{ InputCollection->getBlockLook(),
			glm::vec2(mouseXSign * lookUnits.x, lookUnits.y), glm::vec2(aimStick.x, -aimStick.y), targetPitchDeg };
		m_cameraState = dAttackCameraBehaviour::integrate(DeltaSeconds, input, settings, m_cameraState);
		applyCameraStateToBoom(targetPitchDeg, settings);
	}

	{
		glm::vec3 worldAimDirection = InputCollection->buildAimDirection();

		glm::vec3 tmpAimInput = glm::vec3(1.f, 0.f, 0.f);
		if (InputCollection->hasInputComponent())
			tmpAimInput = worldAimDirection;

		glm::mat4 humanoidAimRotationMatrix;
		const glm::vec3 humanoidDefaultForward(0.f, 1.f, 0.f);
		dMathUtil::getRotationMatrix(humanoidDefaultForward, tmpAimInput, humanoidAimRotationMatrix);
		HumanoidMesh->SetWorldRotation(uglm::toFtransform(humanoidAimRotationMatrix).GetRotation());
	}

	if(!InputCollection->hasInputComponent())
		DrawDebugSphere(GetWorld(), GetCapsuleComponent()->GetComponentTransform().GetTranslation() + FVector(0.f, 0.f, 100.f), 10, 10, FColor::Green);

}

void AOGBrawlerUECharacter::Attack(const FInputActionValue& Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("attack!")));

}

OGSIM_OPTIMIZE_ON