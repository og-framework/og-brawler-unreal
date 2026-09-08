// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Runtime/Engine/Classes/Components/SphereComponent.h"

#include "DVolume/DVolumeAsset.h"
#include "OGBrawler/DAttackCircle.h"
#include "OGBrawler/DAttackCamera.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

#include "Runtime/Engine/Classes/Components/SplineComponent.h"
#include "ProceduralMeshComponent/Public/ProceduralMeshComponent.h"
#include "OGBrawler/CharacterVisualizationData.h"

#include <functional>
#include <optional>
#include <vector>
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGBrawler/InputMapping/GameInputMapping.h"
#include "OGBrawler/DAttackRadialSimulation.h"

#include "OGBrawlerUECharacter.generated.h"


class USpringArmComponent;
class UCameraComponent;
class UMaterialInterface;
struct FInputActionValue;

class AOGBrawlerUECharacter;
class UDPhysicsComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

USTRUCT()
struct FTestStruct
{
	GENERATED_USTRUCT_BODY()

	float x;
};

UCLASS(config=Game)
class AOGBrawlerUECharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USimmableUpdateComponent* SimmableUpdateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UOGBrawlerInputCollectionComponent* InputCollection;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visualization", meta = (AllowPrivateAccess = "true"))
	UProceduralMeshComponent* HumanoidMesh;

	UPROPERTY()
	UMaterialInterface* HumanoidBaseMaterial = nullptr;

	// [movement-sim task 15] Friction 0 / restitution 0, assigned to the capsule as a
	// primitive phys-material OVERRIDE in the constructor. Held as a UPROPERTY so the
	// default subobject is rooted for the lifetime of the CDO/instance and cannot be
	// collected out from under `SetPhysMaterialOverride`. Restitution 0 is load-bearing:
	// step 6' of the movement sub-simulation adopts the solver's positional push-out, so a
	// bouncy capsule would feed a rebound straight back into the movement state.
	UPROPERTY()
	class UPhysicalMaterial* CapsulePhysicalMaterial = nullptr;

	// ⛔ REPLICATED, AND THE ONLY REPLICATED STATE ON THIS CLASS. Purely cosmetic:
	// it never reaches the engine-free simulation core, so it cannot affect
	// determinism or the correction path.
	UPROPERTY(ReplicatedUsing = OnRep_BrawlerColor)
	FLinearColor BrawlerColor = FLinearColor::White;

	UFUNCTION()
	void OnRep_BrawlerColor();

	// The one dynamic instance the tint is written to. Created lazily because
	// RebuildHumanoidMesh() re-applies the BASE material to every section.
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* HumanoidColorMID = nullptr;

	void ApplyBrawlerColor();

	HumanoidVisualization m_humanoidViz;

	void RebuildHumanoidMesh();

	UEnhancedInputComponent* m_inputComponent = nullptr;

	std::vector<DVolumeAsset> m_volumeAssets;


public:
	UPROPERTY(Category = "Weapon", VisibleAnywhere, BlueprintReadWrite)
	UPhysicsConstraintComponent* m_weaponConstraint;
	//UPROPERTY(Category = "Weapon", VisibleAnywhere, BlueprintReadWrite)
	//UStaticMeshComponent* m_weaponVis;
	//UPROPERTY(Category = "Weapon", VisibleAnywhere, BlueprintReadWrite)
	//TArray<USplineComponent*> splines;

	UPROPERTY(Category = "Weapon", VisibleAnywhere, BlueprintReadWrite)
	USphereComponent* m_cameraAxis;
	
	UPROPERTY(Category = "Weapon", EditAnywhere)
	uint32 m_attackSegments;

	UPROPERTY(Category = "Weapon", EditAnywhere)
	float m_innerRadius;

	UPROPERTY(Category = "Weapon", EditAnywhere)
	FTestStruct m_testStruct;

	UPROPERTY(Category = "Weapon", EditAnywhere)
	float m_outerRadius;

	UPROPERTY(Category = "Weapon", EditAnywhere)
	bool m_offsetWithSegmentHalf;

	UPROPERTY(Category = "Weapon", EditAnywhere)
	float m_forwardRangeMultiplier;

	std::optional<DAttackCircle> m_attackCircle;

	float m_currentAttackAngle;
	float m_attackTimer;

	// Event called every physics tick and sub-step.
	UFUNCTION(BlueprintNativeEvent)
	void PhysicsTick(float SubstepDeltaTime);
	virtual void PhysicsTick_Implementation(float SubstepDeltaTime);

	// Custom physics Delegate
	FCalculateCustomPhysics OnCalculateCustomPhysics;
	void CustomPhysics(float DeltaTime, FBodyInstance* BodyInstance);

	AOGBrawlerUECharacter();
	

protected:

	struct OGBrawlerUEPID
	{
		float error = 0.f;
		float prev_err = 0.f;
		float integral = 0.f ;
		float derivative = 0.f;
		float adjustment = 0.f;

		float max_adjustment;
		float P = 0.f;
		float I = 0.f;
		float D = 0.f;
	};

	DAttackCameraState m_cameraState;

	OGBrawlerUEPID m_camPid;

	void Attack(const FInputActionValue& Value);
	virtual void Tick(float DeltaSeconds) override;

	// [movement-sim task 15] The flinch-freeze predicate is DELETED. Its only caller was `Move()`,
	// the legacy CMC path, and the flinch freeze it implemented now lives in the simulation
	// itself: `brawlerMovementSimulation::integrate` step 1 gates on `machineFreezesMovement`,
	// which reads the machine sub-simulation's own state on the sim clock instead of a
	// game-thread viz snapshot. One authority, one clock.


protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PostInitializeComponents() override;

	// To add mapping context
	virtual void BeginPlay();

	virtual void PossessedBy(AController* NewController) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Client-side mirror of PossessedBy's IMC-add. Fires when the server-
	// authoritative Controller pointer replicates to this client-side pawn.
	// Required in PIE-as-client / dedicated-server-client setups where
	// PossessedBy is server-only.
	virtual void OnRep_Controller() override;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	/** Returns InputCollection subobject **/
	FORCEINLINE UOGBrawlerInputCollectionComponent* getInputCollection() const { return InputCollection; }
};