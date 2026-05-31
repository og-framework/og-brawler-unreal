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
#include "OGSimulationUnreal/InputMappingUETranslator.h"
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visualization", meta = (AllowPrivateAccess = "true"))
	UProceduralMeshComponent* HumanoidMesh;

	UPROPERTY()
	UMaterialInterface* HumanoidBaseMaterial = nullptr;

	HumanoidVisualization m_humanoidViz;

	void RebuildHumanoidMesh();

	InputMappingUETranslator m_inputTranslator;

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

	void Move(const FInputActionValue& Value);
	glm::vec2 m_mouseAxis;
	void Look(const FInputActionValue& Value){
		FVector2D LookAxisVector = Value.Get<FVector2D>();
		m_mouseAxis.x = LookAxisVector.X; m_mouseAxis.y = LookAxisVector.Y;
	}
	glm::vec3 m_rStickAxis;
	void setAimInput(const FInputActionValue& Value) {
		FVector2D AimAxisVector = Value.Get<FVector2D>();
		m_rStickAxis.x = AimAxisVector.X; m_rStickAxis.y = AimAxisVector.Y; m_rStickAxis.z = 0.f;
	}
	void BlockLook(const FInputActionValue& Value) { m_blockLook = Value.Get<bool>(); }
	OGBrawlerUEPID m_camPid;
	bool m_blockLook = false;

	void Attack(const FInputActionValue& Value);
	virtual void Tick(float DeltaSeconds) override;


protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PostInitializeComponents() override;

	// To add mapping context
	virtual void BeginPlay();

	virtual void PossessedBy(AController* NewController) override;

	// Client-side mirror of PossessedBy's IMC-add. Fires when the server-
	// authoritative Controller pointer replicates to this client-side pawn.
	// Required in PIE-as-client / dedicated-server-client setups where
	// PossessedBy is server-only.
	virtual void OnRep_Controller() override;

private:
	// Adds m_inputTranslator's IMC to the controlling LP's Enhanced Input
	// subsystem. Idempotent. Called from BeginPlay, PossessedBy, OnRep_Controller,
	// and SetupPlayerInputComponent so the IMC lands regardless of which
	// possession-ordering path the engine takes for this pawn.
	void addInputMappingContextForController(class AController* InController);

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};