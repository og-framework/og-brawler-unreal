// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "SharedIsometricCameraActor.generated.h"

UCLASS()
class ASharedIsometricCameraActor : public AActor
{
    GENERATED_BODY()
public:
    ASharedIsometricCameraActor();
    virtual void Tick(float DeltaSeconds) override;

    // Set by AOGBrawlerPlayerController::refreshViewTarget (Task 4). Identifies
    // the world whose locally-controlled brawlers should be framed. Stored as
    // a weak ref to survive PC end-play without dangling. Any PC on the same
    // client may set this; all PCs on the same client set the same camera and
    // the same world, so last-write-wins is fine.
    void setOwningWorld(UWorld* world) { m_owningWorld = world; }

    // The iso camera's world rotation: the og.iso.pitch / og.iso.yaw CVars when
    // non-zero, else the UPROPERTY values of `source`. A null `source` reads the
    // class default object, so the solo third-person camera can start at the iso
    // angle when no iso actor exists (AOGBrawlerUECharacter::Tick). Tick passes
    // `this`, so both cameras resolve through this one function.
    static FRotator resolveIsoRotation(const ASharedIsometricCameraActor* source = nullptr);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
    USceneComponent* m_root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
    UCameraComponent* m_camera;

    UPROPERTY(EditAnywhere, Category=Iso) float m_yawDegrees       = 0.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_pitchDegrees     = -60.f;
    // Narrow FOV (telephoto) compresses perspective so the camera approximates
    // orthographic-style framing while still providing depth cues. Wider values
    // make stick-aim-to-screen-direction increasingly unintuitive at the edges.
    UPROPERTY(EditAnywhere, Category=Iso) float m_fovDegrees       = 20.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_minDistanceCm    = 2000.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_maxDistanceCm    = 25000.f;
    // Padding added to the brawler-AABB half-extent on screen (NOT added to
    // camera distance). Effectively the visual breathing room you get around
    // the outermost brawlers, in centimeters of world-space at the camera plane.
    // A value of 800 keeps a brawler's full body comfortably inside the frame
    // even during fast separation.
    UPROPERTY(EditAnywhere, Category=Iso) float m_marginCm         = 800.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_positionDeadzoneCm = 100.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_positionLerpSpeed = 6.f;

    TWeakObjectPtr<UWorld> m_owningWorld;
};
