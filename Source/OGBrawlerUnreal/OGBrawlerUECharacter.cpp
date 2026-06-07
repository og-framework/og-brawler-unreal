// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUECharacter.h"
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

#pragma optimize( "", off )

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

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 100.f; // bootstrap; Tick() resyncs from g_moveSpeed.
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

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
	this->NetUpdateFrequency = 100.0f; // Higher = more updates per second
	this->MinNetUpdateFrequency = 100.0f;

	this->bAlwaysRelevant = true; // Always relevant for network updates

	this->NetCullDistanceSquared = 10000.f* 10000.f; // Distance at which the character is culled from the network updates

	HumanoidMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HumanoidMesh"));
	HumanoidMesh->SetupAttachment(RootComponent);
	// Capsule center sits CapsuleHalfHeight above the floor; the visualization is
	// authored with feet at local Z=0, so push the mesh down by that amount.
	HumanoidMesh->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
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

void AOGBrawlerUECharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

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

		// CMC Move binding stays on the character — AOGBrawlerUECharacter::Move feeds
		// AddMovementInput for the legacy CharacterMovementComponent path (sim path is
		// handled by InputCollection::onMove). Two consumers, one action.
		UInputAction* MoveAction = InputCollection->getTranslator().getAction(dInput::gameMapping::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOGBrawlerUECharacter::Move);
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

	// Sync MaxWalkSpeed from the tweakable each frame so OGBrawler.MoveSpeed updates live.
	GetCharacterMovement()->MaxWalkSpeed = dAttackMachineSimulation::g_moveSpeed.load();

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

void AOGBrawlerUECharacter::Move(const FInputActionValue& Value)
{
	if (Controller == nullptr)
		return;

	// HoldGuard (left trigger axis / Left Shift) freezes CMC movement so the character roots
	// in place while in guard stance. The sim still sees the unchanged moveStick +
	// moveDirectionWorld because those flow via UOGBrawlerInputCollectionComponent::
	// buildPlayerInput() on the physics tick — that path does not consult getHoldGuard().
	// Net result: visible motion stops but the attack picker (integrate3) keeps reading
	// the player's intended direction.
	if (InputCollection->getHoldGuard())
		return;

	// Stick magnitude carries analog speed: buildMoveDirectionWorld() returns a unit
	// world direction (internal normalize), so the magnitude must be passed as the
	// AddMovementInput scalar — otherwise any deflection produces full-speed motion.
	// Deadzone-gate before computing the direction: buildMoveDirectionWorld itself
	// returns zero below the deadzone, but exiting early here skips the work and avoids
	// feeding a zero direction to AddMovementInput.
	const glm::vec2 stick = InputCollection->getMoveStick();
	const float stickMagnitude = glm::length(stick);
	const float moveDeadzone = dAttackMachineSimulation::g_moveStickDeadzone.load();
	if (stickMagnitude < moveDeadzone)
		return;

	glm::vec3 worldDir = InputCollection->buildMoveDirectionWorld();
	worldDir.z = 0.f;

	// TEMPORARILY DISABLED: freeze CMC movement when the player's intended move direction
	// is sharply away from where they're aiming (XY-projected angle ≥ 3π/4 ≈ 135°). Acts
	// like the HoldGuard freeze above — the sim's PlayerInput still receives the
	// unchanged moveDirectionWorld via buildPlayerInput() on the physics tick, so the
	// attack picker continues to read the player's intended direction.
	//const glm::vec3 aimDir = InputCollection->buildAimDirection();
	//const glm::vec3 aimDirXY(aimDir.x, aimDir.y, 0.f);
	//const float aimLenXY  = glm::length(aimDirXY);
	//const float moveLenXY = glm::length(worldDir);
	//if (aimLenXY > KINDA_SMALL_NUMBER && moveLenXY > KINDA_SMALL_NUMBER)
	//{
	//	const float dotXY = glm::clamp(
	//		glm::dot(aimDirXY / aimLenXY, worldDir / moveLenXY), -1.f, 1.f);
	//	const float angle = glm::acos(dotXY);
	//	if (angle >= 3.f * glm::pi<float>() / 4.f)
	//		return;
	//}

	AddMovementInput(uglm::toFVector(worldDir), FMath::Min(stickMagnitude, 1.f));
}

void AOGBrawlerUECharacter::Attack(const FInputActionValue& Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("attack!")));

}