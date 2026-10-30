// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "OGBrawler/DAttackCamera.h"
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

    // Width / height of the game viewport, or 1 when there is none (or it has
    // no height yet). Both cameras read it every tick to convert a field of
    // view authored on the shorter screen axis into the horizontal one both
    // cameras render (MaintainXFOV; dAttackCameraBehaviour::horizontalFovDeg).
    // It is the whole game viewport: split-screen local players get the
    // window's aspect, not their own view's.
    static float viewportAspectRatio();

    // The iso camera's own framing settings before og.iso.matchSolo is applied:
    // each og.iso.* CVar when > 0 (og.iso.fovAxis read directly), else the
    // UPROPERTY values of `source` (null = the class default object, as in
    // resolveIsoRotation). Tick and the [Camera.sight] readout both resolve
    // through this one function.
    static dAttackCameraBehaviour::IsoFramingSettings resolveIsoFramingSettings(
        const ASharedIsometricCameraActor* source = nullptr);

    // Whether og.iso.matchSolo is on (> 0, the default): the iso camera then
    // uses the solo camera's FOV and margins equal to the solo camera's reach
    // (dAttackCameraBehaviour::resolveIsoFraming, og-ground-aligned-attacks
    // task 17).
    static bool matchesSoloCamera();

    // Prints the [Camera.sight] line (LogOGBrawler, Warning): the solo boom and
    // FOV at the target pitch, the solo FOV at the horizon (og.cam.fovHorizon,
    // task 18), the solo reach, og.iso.matchSolo, the iso margins and FOV in use, and
    // the iso distance for the group the shared camera framed last. With
    // `onDemand` false (the per-tick calls from this actor and from the solo
    // camera) it prints only when an input changed since the last print (the
    // solo boom / FOV / FOV axis / horizon FOV, matchSolo, the iso settings, the viewport
    // aspect), and only in a process that renders, so a -nullrhi process (the
    // headless harness) never prints it. `onDemand` true is the console
    // command og.cam.printSight: it always prints.
    static void reportCameraSight(bool onDemand);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
    USceneComponent* m_root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
    UCameraComponent* m_camera;

    UPROPERTY(EditAnywhere, Category=Iso) float m_yawDegrees       = 0.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_pitchDegrees     = -60.f;
    // Used only while og.iso.matchSolo is 0: with matchSolo 1 (the default)
    // the FOV, FOV axis and margins come from the solo camera, and
    // m_minDistanceCm is ignored (the matched framing never comes closer than
    // the solo boom); m_maxDistanceCm still applies.
    // Narrow FOV (telephoto) compresses perspective so the camera approximates
    // orthographic-style framing while still providing depth cues. Wider values
    // make stick-aim-to-screen-direction increasingly unintuitive at the edges.
    // Degrees on the og.iso.fovAxis axis (default: the SHORTER screen axis), so
    // 11.33 is the old 20 degrees horizontal on 16:9 (20.0025, within 0.006
    // degrees); a wider screen shows more
    // to the sides, a portrait one more above and below.
    UPROPERTY(EditAnywhere, Category=Iso) float m_fovDegrees       = 11.33f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_minDistanceCm    = 2000.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_maxDistanceCm    = 25000.f;
    // Padding added to the brawler-AABB half-extent on screen (NOT added to
    // camera distance), separately for the screen's horizontal (H) and
    // vertical (V) axis. Effectively the visual breathing room you get around
    // the outermost brawlers, in centimeters of world-space at the camera plane.
    // Until og-ground-aligned-attacks task 15 one 800 cm margin padded both.
    UPROPERTY(EditAnywhere, Category=Iso) float m_marginHCm        = 400.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_marginVCm        = 300.f;
    // Follow hysteresis (dAttackCameraBehaviour::followStep): the camera starts
    // moving when its position is more than m_positionDeadzoneCm from the
    // framing target and keeps moving until it is within m_positionStopCm, then
    // holds. A stop threshold equal to the start (or above it) is the one
    // deadzone used before og-ground-aligned-attacks task 16, which made a
    // steadily moving target stop and go: at a steady follow the error before
    // each step is v / m_positionLerpSpeed for a target moving at speed v
    // (95 cm at 5.7 m/s), just inside the 100 cm start, so the camera stopped,
    // fell behind past 100 cm, jumped, and stopped again.
    UPROPERTY(EditAnywhere, Category=Iso) float m_positionDeadzoneCm = 100.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_positionStopCm   = 10.f;
    UPROPERTY(EditAnywhere, Category=Iso) float m_positionLerpSpeed = 6.f;

    // Whether the camera was following last tick (between the start and the
    // stop threshold), carried from tick to tick.
    bool m_followTracking = false;

    TWeakObjectPtr<UWorld> m_owningWorld;
};
