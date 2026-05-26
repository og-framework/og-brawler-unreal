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
#include "OGBrawlerUnreal/DAttackCircleUImplementation.h"
#include "OGBrawlerUnreal/HumanoidMeshBuilder.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "OGBrawler/DAttackRadialSimulation.h"
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

//namespace DAttackMovementCVars
//{
//	int movementAndAimMode = 0;
//	//1 aim relative movement
//	//2 movement relative strike
//	//3 movementRelativeStrikeJoyAimRelativeMoveMouse
//	static FAutoConsoleVariableRef movementAndAimModeCVar(
//		TEXT("DAttackMovementCVars.movementAndAimMode"),
//		movementAndAimMode,
//		TEXT("test)"),
//		ECVF_Default);
//
//	bool aimRelativeMovement = false;
//	static FAutoConsoleVariableRef aimRelativeMovementCVar(
//		TEXT("DAttackMovementCVars.aimRelativeMovement"),
//		aimRelativeMovement,
//		TEXT("test)"),
//		ECVF_Default);
//}
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
	GetCharacterMovement()->MaxWalkSpeed = 200.f;
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
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			m_inputTranslator.addToSubsystem(Subsystem, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AOGBrawlerUECharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Initialize translator here — SetupPlayerInputComponent runs before BeginPlay (during possession).
	m_inputTranslator.initialize(this, dInput::gameMapping::buildDefaultContext());

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		SimmableUpdateComponent->setInputComponent(EnhancedInputComponent, m_inputTranslator);
		m_inputComponent = EnhancedInputComponent;
		// Jumping
		UInputAction* JumpAction = m_inputTranslator.getAction(dInput::gameMapping::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		UInputAction* MoveAction = m_inputTranslator.getAction(dInput::gameMapping::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOGBrawlerUECharacter::Move);

		// Looking
		UInputAction* LookAction = m_inputTranslator.getAction(dInput::gameMapping::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOGBrawlerUECharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::None, this, &AOGBrawlerUECharacter::Look);
		UInputAction* BlockLookAction = m_inputTranslator.getAction(dInput::gameMapping::BlockLook);
		EnhancedInputComponent->BindAction(BlockLookAction, ETriggerEvent::Triggered, this, &AOGBrawlerUECharacter::BlockLook);
		EnhancedInputComponent->BindAction(BlockLookAction, ETriggerEvent::Completed, this, &AOGBrawlerUECharacter::BlockLook);

		UInputAction* AimAction = m_inputTranslator.getAction(dInput::gameMapping::Aim);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &AOGBrawlerUECharacter::setAimInput);
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

	FVector fCurrentCamForward = FollowCamera->GetForwardVector();
	glm::vec3 currentCamForward = uglm::toGLMVec3(fCurrentCamForward);
	SimmableUpdateComponent->setCameraForward(currentCamForward);

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (m_blockLook)
		{
			PlayerController->SetShowMouseCursor(false);
			SimmableUpdateComponent->setMouseAimDirection(glm::vec3(0.f, 0.f, 0.f));
		}
		else
		{
			PlayerController->SetShowMouseCursor(true);

			PlayerController->GetMousePosition(m_mouseAxis.x, m_mouseAxis.y);
			FVector worldLocation;
			FVector worldDirection;
			PlayerController->DeprojectMousePositionToWorld(worldLocation, worldDirection);

			//line intersection with plane
			const FVector planeNormal = FVector(0.f, 0.f, 1.f);
			const FVector planePoint = GetCapsuleComponent()->GetComponentTransform().GetTranslation();
			const FVector linePoint = worldLocation;
			const FVector lineDirection = worldDirection;
			const FVector planePointToLinePoint = linePoint - planePoint;
			const float dotProduct = FVector::DotProduct(planeNormal, lineDirection);
			const float distance = FVector::DotProduct(planeNormal, planePointToLinePoint) / dotProduct;
			const FVector intersectionPoint = linePoint - distance * lineDirection;

			SimmableUpdateComponent->setMouseAimDirection(glm::normalize(uglm::toGLMVec3(intersectionPoint - planePoint)));
		}
	}

	const glm::vec3 mouseAxis= glm::normalize(glm::vec3(m_mouseAxis.x, m_mouseAxis.y, 0.f));
	const glm::vec3 defaultForward(1.f, 0.f, 0.f);

	DPIDSettings settings(0.03f, 0.01f, 0.01f);
	dAttackCameraBehaviour::integrate(DeltaSeconds, DAttackCameraInput{ m_rStickAxis, m_mouseAxis, m_blockLook, 0.8f, settings }, m_cameraState);
	CameraBoom->SetRelativeRotation(uglm::toFRotator(m_cameraState.getCameraBoomTransform()));
	CameraBoom->TargetArmLength = m_cameraState.getCameraBoomLength();
	m_mouseAxis = glm::vec2(0.f, 0.f);

	{ // rotate the character mesh
		glm::vec3 worldAimDirection = SimmableUpdateComponent->buildAimDirection();

		glm::vec3 tmpAimInput = glm::vec3(1.f, 0.f, 0.f);
		if (m_inputComponent != nullptr)
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

	if(m_inputComponent == nullptr)
		DrawDebugSphere(GetWorld(), GetMesh()->GetComponentTransform().GetTranslation() + FVector(0.f, 0.f, 100.f), 10, 10, FColor::Green);

}

void AOGBrawlerUECharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		if(DAttackMovementCVars::movementAndAimMode.GetValueOnAnyThread() == 1)
		{
			glm::vec3 worldAimDirection = SimmableUpdateComponent->buildAimDirection();
			glm::mat4 aimRotationMatrix2;
			const glm::vec3 defaultLeft(0.f, 1.f, 0.f);
			dMathUtil::getRotationMatrix(defaultLeft, worldAimDirection, aimRotationMatrix2);
			FTransform fAimRotationMatrix = uglm::toFtransform(aimRotationMatrix2);
			
			const FRotator Rotation = fAimRotationMatrix.Rotator();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			// get forward vector
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

			// get right vector 
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);


			// add movement 
			AddMovementInput(ForwardDirection, -MovementVector.X);
			AddMovementInput(RightDirection, MovementVector.Y);
		}
		else
		{
			// find out which way is forward
//const FRotator Rotation = Controller->GetControlRotation();
			const FRotator Rotation = CameraBoom->GetRelativeRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			// get forward vector
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

			// get right vector 
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			// add movement 
			AddMovementInput(ForwardDirection, MovementVector.Y);
			AddMovementInput(RightDirection, MovementVector.X);
		}

	}
}

void AOGBrawlerUECharacter::Attack(const FInputActionValue& Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("attack!")));

}