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
	GetCapsuleComponent()->SetPhysMaterialOverride(CapsulePhysicalMaterial);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->bUsePawnControlRotation = false;

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

void AOGBrawlerUECharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	InputCollection->updateGameThreadCache();

	const glm::vec2 lookStick = InputCollection->consumeLookStick();

	DPIDSettings settings(0.03f, 0.01f, 0.01f);
	const glm::vec3 aimStick3 = glm::vec3(InputCollection->getAimStick(), 0.f);
	dAttackCameraBehaviour::integrate(DeltaSeconds, DAttackCameraInput{ aimStick3, lookStick, InputCollection->getBlockLook(), 0.8f, settings }, m_cameraState);
	CameraBoom->SetRelativeRotation(uglm::toFRotator(m_cameraState.getCameraBoomTransform()));
	CameraBoom->TargetArmLength = m_cameraState.getCameraBoomLength();

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