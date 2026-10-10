// SPDX-License-Identifier: BUSL-1.1
// docs/OGBrawlerUECharacter-rationale.md · docs/OGBrawlerUECharacter-guards.md

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Logging/LogMacros.h"
#include "Runtime/Engine/Classes/Components/SphereComponent.h"

#include "DVolume/DVolumeAsset.h"
#include "OGBrawler/DAttackCircle.h"
#include "OGBrawler/DAttackCamera.h"

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
#include "OGBrawler/SimCharacterId.h"

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
class AOGBrawlerUECharacter : public APawn
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	class UCapsuleComponent* CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

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

	UPROPERTY()
	class UPhysicalMaterial* CapsulePhysicalMaterial = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_BrawlerColor)
	FLinearColor BrawlerColor = FLinearColor::White;

	UFUNCTION()
	void OnRep_BrawlerColor();

	UPROPERTY(ReplicatedUsing = OnRep_RingoutScore)
	int32 RingoutScore = 0;

	UFUNCTION()
	void OnRep_RingoutScore();

	UPROPERTY(Replicated)
	uint8 SimCharacterIdValue = 0;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* HumanoidColorMID = nullptr;

	void ApplyBrawlerColor();

	HumanoidVisualization m_humanoidViz;

	void RebuildHumanoidMesh();

	UEnhancedInputComponent* m_inputComponent = nullptr;

	std::vector<DVolumeAsset> m_volumeAssets;

public:
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

	AOGBrawlerUECharacter();

	void seedCameraBoomAtIsoRotation();

protected:

	dAttackCameraBehaviour::OrbitCameraState m_cameraState;

	bool m_cameraBoomSeeded = false;

	void applyCameraStateToBoom(float targetPitchDeg, const dAttackCameraBehaviour::OrbitCameraSettings& settings);

	void Attack(const FInputActionValue& Value);
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PostInitializeComponents() override;

	virtual void BeginPlay();

	virtual void PossessedBy(AController* NewController) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnRep_Controller() override;

public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE UOGBrawlerInputCollectionComponent* getInputCollection() const { return InputCollection; }
	FORCEINLINE class UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

	FORCEINLINE int32 GetRingoutScore() const { return RingoutScore; }

	FORCEINLINE FLinearColor GetBrawlerColor() const { return BrawlerColor; }

	SimCharacterId GetSimCharacterId() const;

	void SetAuthoritativeSimCharacterId(SimCharacterId NewId);

	void SetAuthoritativeRingoutScore(int32 NewScore);
};