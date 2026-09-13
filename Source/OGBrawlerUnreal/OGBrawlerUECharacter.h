// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
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
class AOGBrawlerUECharacter : public APawn
{
	GENERATED_BODY()

	// =====================================================================================
	// [movement-sim task 19] ⭐ THE ROOT COLLISION CAPSULE — CREATED AND OWNED BY THIS CLASS.
	// Until this task the pawn derived from the engine's stock walking-pawn base, which created
	// this component, named it, made it the root, and handed its movement component the very
	// same component to drive. That movement component was retired in task 15 — the movement
	// sub-simulation owns locomotion — so the base class was doing exactly ONE useful thing for
	// us: a handful of constructor lines. They live in our own constructor now, and the base is
	// `APawn`.
	//
	// ⛔⛔ 42 / 96 IS A CONTRACT, NOT A TUNING VALUE.
	// `brawlerMovementSimulation::PhysicsSetup::body` ships `CapsuleGeometry{42.f, 96.f}` with
	// `isRoot`, and `ChaosPhysicsFactory`'s adopt-root branch `checkf`s the AUTHORED capsule
	// AGAINST the descriptor rather than resizing it. A different size in the constructor is an
	// immediate assert at the first character registration. Change both together or neither.
	// =====================================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	class UCapsuleComponent* CapsuleComponent;

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

	// ⛔ REPLICATED. Purely cosmetic: it never reaches the engine-free simulation
	// core, so it cannot affect determinism or the correction path.
	// ⚠ [ringout task 5] IT IS NO LONGER THE ONLY REPLICATED STATE ON THIS CLASS —
	// `RingoutScore` below is the second, and it is safe for exactly this reason.
	UPROPERTY(ReplicatedUsing = OnRep_BrawlerColor)
	FLinearColor BrawlerColor = FLinearColor::White;

	UFUNCTION()
	void OnRep_BrawlerColor();

	// =====================================================================================
	// [ringout task 5] ⭐ THE REPLICATED RING-OUT SCORE — THE SCOREBOARD'S FIRST INPUT.
	//
	// ⛔ COSMETIC ON THE CLIENT, AND THAT IS THE PROPERTY THAT MAKES REPLICATING IT SAFE.
	// Exactly the claim `BrawlerColor` above makes about itself: this value never reaches
	// the engine-free simulation core. It is not in `simulatableBrawler::State`, not in any
	// composite, has no `SerializableFields` specialization, rides no correction buffer and
	// is read by nothing that integrates, rewinds or reconciles. A client that receives a
	// wrong, stale or missing score draws a wrong NUMBER; it cannot desynchronise, cannot
	// change a prediction and cannot make a resim land differently. The authority-side
	// ledger it mirrors — `brawlerRingout::ScoreSystem`'s table — is likewise outside sim
	// state, which is what task 4 built it that way for.
	// ⇒ THE GREP THAT PROVES IT, in two parts, re-runnable, over the two engine-free core
	//   trees (`Plugins/OGBrawler/.../og-brawler/` and `Plugins/OGSimulation/.../og-simulation/`):
	//     1. The four identifiers that carry this value — `RingoutScore`,
	//        `OnRep_RingoutScore`, `GetRingoutScore`, `SetAuthoritativeRingoutScore` — have
	//        ZERO occurrences in either tree.
	//        ⚠ Grep for the bare word and you get three hits. They are task 4's TEST-CASE
	//        NAMES (`RingoutScore.TheAwardCostsTheCompositeNothing` and two more) quoted in
	//        comments — a substring collision with a different symbol, not a read. Match the
	//        identifier, not the word: `RingoutScore[^.A-Za-z0-9_]`.
	//     2. Structural, and it is the stronger half: those trees contain no engine code at
	//        all. Their only mentions of `AOGBrawlerUECharacter`, `UPROPERTY` and
	//        `DOREPLIFETIME` are in comments and markdown. There is nothing there that COULD
	//        read a `UPROPERTY` even if it named one.
	//   Both commands, with their output, are in `impl_notes_ringout_5.md` §3.
	//
	// ⛔ PLAIN `DOREPLIFETIME`, NEVER `COND_OwnerOnly` — see `GetLifetimeReplicatedProps`.
	//
	// ⚠ SIGNEDNESS: `brawlerRingout::ScoreSystem` holds the score as `uint32_t`. This is
	// `int32` because that is the integer width UE replicates, reflects and exposes without
	// qualification; the push casts once, at the single write site below. One point per
	// death makes either width unreachable overflow.
	UPROPERTY(ReplicatedUsing = OnRep_RingoutScore)
	int32 RingoutScore = 0;

	UFUNCTION()
	void OnRep_RingoutScore();

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
	/**
	 * Returns the root collision capsule.
	 * ⭐ DELIBERATELY THE SAME NAME the engine base class used to provide, so all seven
	 * pre-existing call sites across three files are unchanged by the migration. This is also the
	 * component the movement sub-simulation ADOPTS as its body (`PhysicsSetup::body.isRoot`),
	 * which is why its authored size is a contract rather than a tuning value — see the
	 * capsule member above.
	 **/
	FORCEINLINE class UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

	// =====================================================================================
	// [ringout task 5] THE SCORE'S TWO DOORS — one in, one out.
	// =====================================================================================

	/**
	 * The ring-out score as this peer last heard it. ⭐ THIS IS WHERE THE SCOREBOARD (task 6b)
	 * READS THE SCORE, on EVERY peer: on a client it is whatever `RingoutScore` last
	 * replicated in, on the authority it is whatever the push below last wrote. There is no
	 * second route and deliberately so — a HUD that reached into `ScoreSystem` on the
	 * authority and into this property on a client would be two code paths, one of which
	 * nobody ever looks at.
	 * ⚠ The scoreboard's OTHER two inputs are NOT here and must NOT be replicated again:
	 * dead and ticks-until-respawn are `brawlerRingout::State`, already on every peer via the
	 * correction wire. See `impl_notes_ringout_5.md` §4 for the exact accessors.
	 */
	FORCEINLINE int32 GetRingoutScore() const { return RingoutScore; }

	/**
	 * ⭐ [ringout task 10] THE TINT THIS BRAWLER'S MESH IS WEARING — the SCOREBOARD'S SWATCH.
	 *
	 * ⛔ PRESENT AND CORRECT ON EVERY PEER, which is the property that lets the board draw a
	 * real colour on a client instead of a column of white. `BrawlerColor` is assigned
	 * server-side in `PossessedBy` from `kBrawlerPalette` and registered with a PLAIN
	 * `DOREPLIFETIME` — deliberately NOT `COND_OwnerOnly`; the reason is written at the
	 * registration and it is couch co-op. It therefore reaches a client's scoreboard by
	 * exactly the same route `RingoutScore` above does, and column one works on a client for
	 * exactly the reason column two does.
	 *
	 * ⚠ THE SCOREBOARD DROPS THE ALPHA. `brawlerScoreboardVisualization::ScoreboardInk` is
	 * three channels; the swatch is opaque on purpose, because a translucent swatch is a
	 * different colour to the eye over a bright scene than over a dark one, and "which
	 * fighter is this" must not depend on what is behind the board.
	 *
	 * ⚠ IT IS COSMETIC, LIKE THE PROPERTY ITSELF. Nothing here reaches the engine-free
	 * simulation core; see the note at `BrawlerColor`'s declaration.
	 */
	FORCEINLINE FLinearColor GetBrawlerColor() const { return BrawlerColor; }

	/**
	 * ⭐ THE JOIN KEY — the id this character is known by INSIDE the simulation, on every peer.
	 *
	 * ⛔ IT IS NOT `AActor::GetUniqueID()` ON THIS PAWN, AND THAT DISTINCTION IS THE WHOLE
	 * REASON THIS ACCESSOR EXISTS. Every id in the simulation — `SimulationObjectStorage`'s
	 * key, `brawlerRingout::ScoreSystem`'s roster key, `SpawnSlotAllocator`'s table, the
	 * input-history rings, every `id=%u` in every `[Ringout.*]` log line — is the
	 * `USimmableUpdateComponent`'s `GetUniqueID()`, because that is what
	 * `tryRegisterWithNewFramework` passes to `tryRegister`. The pawn's own id is a DIFFERENT
	 * number, and a scoreboard that joined on it would silently match nothing.
	 *
	 * ⭐ THIS IS HOW THE SCOREBOARD (task 6b) JOINS ITS THREE INPUTS. Iterate
	 * `AOGBrawlerUECharacter` actors for the SCORE (`GetRingoutScore()`, replicated, present
	 * on every peer), key each row by this id, and read DEAD and RESPAWN-AT-TICK for that same
	 * id out of the simulation — see the note on `GetRingoutScore` and
	 * `impl_notes_ringout_5.md` §4.
	 *
	 * Returns 0 if the component is missing, which cannot happen on a constructed pawn.
	 */
	unsigned int GetSimCharacterId() const;

	/**
	 * ⛔ AUTHORITY ONLY, AND IT IS A NO-OP WHEN THE VALUE IS UNCHANGED.
	 * Called once per character per game-thread post-physics pass by
	 * `ASimulationManagerUImpl::OnPostPhysicsStep`. The unchanged-value early-out is the
	 * whole reason this is a method rather than a public field: replication marks a property
	 * dirty on ASSIGNMENT, not on change, so an unguarded per-pass write would put a
	 * never-changing score on the wire 60 times a second, for every character, forever.
	 */
	void SetAuthoritativeRingoutScore(int32 NewScore);
};