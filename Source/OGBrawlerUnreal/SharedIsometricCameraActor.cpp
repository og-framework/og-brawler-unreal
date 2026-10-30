// SPDX-License-Identifier: BUSL-1.1

#include "SharedIsometricCameraActor.h"
#include "OGBrawlerUECharacter.h"
#include "OGBrawlerSimulationManager.h"
#include "OGBrawler/DAttackCamera.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"

namespace
{
    // Backing storage for CVars — Tick reads these each frame as overrides when non-zero.
    // A value of 0 means "use the UPROPERTY default on the actor".
    static float g_fovDegrees       = 0.f;
    static float g_pitchDegrees     = 0.f;
    static float g_yawDegrees       = 0.f;
    static float g_minDistanceCm    = 0.f;
    static float g_maxDistanceCm    = 0.f;
    static float g_marginHCm        = 0.f;
    static float g_marginVCm        = 0.f;
    // Not a "0 = actor default" override: 0 is a real choice (horizontal).
    static int32 g_fovAxis          = static_cast<int32>(dAttackCameraBehaviour::kFovAxis);
    static float g_posDeadzoneCm    = 0.f;
    static float g_posStopCm        = 0.f;
    static float g_posLerp          = 0.f;
    // Not a "0 = actor default" override either: 0 switches the matching off.
    static int32 g_matchSolo        = 1;

    // The group the shared camera framed last, for the [Camera.sight] readout:
    // its half-extents on the camera plane (cm) and how many local brawlers it
    // held. Written by Tick; read only while g_sightCamera is still alive.
    static float g_sightGroupHalfH  = 0.f;
    static float g_sightGroupHalfV  = 0.f;
    static int32 g_sightGroupPlayers = 0;
    static TWeakObjectPtr<const ASharedIsometricCameraActor> g_sightCamera;

    // The inputs of the last printed [Camera.sight] line. The line is printed
    // again only when one of these changes, never because the group moved (that
    // changes the iso distance every tick).
    struct CameraSightInputs
    {
        float soloBoom;
        float soloFovDeg;
        dAttackCameraBehaviour::FovAxis soloFovAxis;
        float soloFovAtHorizonDeg;
        bool matchSolo;
        dAttackCameraBehaviour::IsoFramingSettings iso;
        float aspect;
    };

    bool sameSightInputs(const CameraSightInputs& a, const CameraSightInputs& b)
    {
        return a.soloBoom == b.soloBoom && a.soloFovDeg == b.soloFovDeg && a.soloFovAxis == b.soloFovAxis
            && a.soloFovAtHorizonDeg == b.soloFovAtHorizonDeg
            && a.matchSolo == b.matchSolo && a.iso.fovDeg == b.iso.fovDeg && a.iso.fovAxis == b.iso.fovAxis
            && a.iso.marginH == b.iso.marginH && a.iso.marginV == b.iso.marginV && a.iso.minDist == b.iso.minDist
            && a.iso.maxDist == b.iso.maxDist && a.aspect == b.aspect;
    }

    static bool g_sightPrinted = false;
    static CameraSightInputs g_lastSightInputs{};

    static FAutoConsoleVariableRef CVarFov(
        TEXT("og.iso.fov"),
        g_fovDegrees,
        TEXT("Shared iso cam: FOV in degrees on the og.iso.fovAxis axis (0 = use actor default; narrower = less perspective distortion; ignored while og.iso.matchSolo is 1)"));

    static FAutoConsoleVariableRef CVarPitch(
        TEXT("og.iso.pitch"),
        g_pitchDegrees,
        TEXT("Shared iso cam: pitch in degrees (0 = use actor default; -30 shallow, -45 classic iso, -60 steep, -90 top-down)"));

    static FAutoConsoleVariableRef CVarYaw(
        TEXT("og.iso.yaw"),
        g_yawDegrees,
        TEXT("Shared iso cam: yaw in degrees (0 = use actor default; 45 classic iso)"));

    static FAutoConsoleVariableRef CVarMinDistance(
        TEXT("og.iso.minDistance"),
        g_minDistanceCm,
        TEXT("Shared iso cam: minimum camera distance in cm (0 = use actor default; ignored while og.iso.matchSolo is 1)"));

    static FAutoConsoleVariableRef CVarMaxDistance(
        TEXT("og.iso.maxDistance"),
        g_maxDistanceCm,
        TEXT("Shared iso cam: maximum camera distance in cm (0 = use actor default)"));

    static FAutoConsoleVariableRef CVarMarginHCm(
        TEXT("og.iso.marginHCm"),
        g_marginHCm,
        TEXT("Shared iso cam: framing margin on the screen's horizontal axis, in world cm at the camera plane (0 = use actor default; ignored while og.iso.matchSolo is 1)"));

    static FAutoConsoleVariableRef CVarMarginVCm(
        TEXT("og.iso.marginVCm"),
        g_marginVCm,
        TEXT("Shared iso cam: framing margin on the screen's vertical axis, in world cm at the camera plane (0 = use actor default; ignored while og.iso.matchSolo is 1)"));

    static FAutoConsoleVariableRef CVarFovAxis(
        TEXT("og.iso.fovAxis"),
        g_fovAxis,
        TEXT("Shared iso cam: the screen axis og.iso.fov spans; 0 = horizontal at every aspect, 1 = the shorter axis (default); ignored while og.iso.matchSolo is 1"));

    static FAutoConsoleVariableRef CVarPosDeadzoneCm(
        TEXT("og.iso.posDeadzoneCm"),
        g_posDeadzoneCm,
        TEXT("Shared iso cam: position deadzone in cm, the error that starts the camera following (0 = use actor default)"));

    static FAutoConsoleVariableRef CVarPosStopCm(
        TEXT("og.iso.posStopCm"),
        g_posStopCm,
        TEXT("Shared iso cam: once following, the error in cm below which the camera holds again; capped at og.iso.posDeadzoneCm (0 = use actor default)"));

    static FAutoConsoleVariableRef CVarPosLerp(
        TEXT("og.iso.posLerp"),
        g_posLerp,
        TEXT("Shared iso cam: position lerp speed (0 = use actor default)"));

    static FAutoConsoleVariableRef CVarMatchSolo(
        TEXT("og.iso.matchSolo"),
        g_matchSolo,
        TEXT("Shared iso cam: 1 (default) = match the solo camera: its FOV and FOV axis (og.cam.fov / og.cam.fovAxis) and margins equal to ")
        TEXT("what the solo camera sees around its player at og.cam.boomTarget, so every outermost local player keeps the solo view's reach ")
        TEXT("(og.iso.fov, og.iso.fovAxis, og.iso.marginHCm / marginVCm and og.iso.minDistance are then ignored); 0 = the iso camera's own settings"));

    static FAutoConsoleCommand CmdPrintSight(
        TEXT("og.cam.printSight"),
        TEXT("Prints the [Camera.sight] line: the solo boom and FOV (at the target pitch, plus the horizon FOV), what the solo camera sees around its player (reach, m), og.iso.matchSolo, ")
        TEXT("the iso margins and FOV in use and the iso distance for the current group"),
        FConsoleCommandDelegate::CreateLambda([]() { ASharedIsometricCameraActor::reportCameraSight(true); }));
}

ASharedIsometricCameraActor::ASharedIsometricCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bFindCameraComponentWhenViewTarget = true;

    m_root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = m_root;

    m_camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    m_camera->SetupAttachment(m_root);
    m_camera->ProjectionMode = ECameraProjectionMode::Perspective;
    // Until the first Tick resolves the settings: the default FOV at 16:9, the
    // solo camera's while og.iso.matchSolo is on (its default), else this actor's.
    m_camera->FieldOfView = dAttackCameraBehaviour::horizontalFovDeg(
        matchesSoloCamera() ? dAttackCameraBehaviour::kFovDeg : m_fovDegrees, dAttackCameraBehaviour::kFovAxis, 16.f / 9.f);
    m_camera->bConstrainAspectRatio = false;
    // Render FieldOfView as the HORIZONTAL FOV at every aspect. Without this
    // override the LocalPlayer default applies, which is MaintainYFOV
    // (BaseEngine.ini [/Script/Engine.LocalPlayer]); CameraStackTypes.cpp
    // CalculateProjectionMatrixGivenViewRectangle then fixes the VERTICAL FOV
    // at the camera's 16:9 reference AspectRatio and the window aspect only
    // widens or narrows the horizontal, so off 16:9 the render would not match
    // the framing below (horizontalFovDeg / isoFramingDistance assume a
    // horizontal-FOV renderer).
    m_camera->bOverrideAspectRatioAxisConstraint = true;
    m_camera->AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;

    SetActorRotation(FRotator(m_pitchDegrees, m_yawDegrees, 0.f));
}

FRotator ASharedIsometricCameraActor::resolveIsoRotation(const ASharedIsometricCameraActor* source)
{
    if (source == nullptr)
        source = GetDefault<ASharedIsometricCameraActor>();

    // CVar overrides when non-zero, else the actor UPROPERTY.
    const float pitchDegrees = !FMath::IsNearlyZero(g_pitchDegrees) ? g_pitchDegrees : source->m_pitchDegrees;
    const float yawDegrees   = !FMath::IsNearlyZero(g_yawDegrees)   ? g_yawDegrees   : source->m_yawDegrees;
    return FRotator(pitchDegrees, yawDegrees, 0.f);
}

float ASharedIsometricCameraActor::viewportAspectRatio()
{
    float aspect = 1.f;
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint size = GEngine->GameViewport->Viewport->GetSizeXY();
        if (size.Y > 0)
            aspect = static_cast<float>(size.X) / static_cast<float>(size.Y);
    }
    return aspect;
}

dAttackCameraBehaviour::IsoFramingSettings ASharedIsometricCameraActor::resolveIsoFramingSettings(
    const ASharedIsometricCameraActor* source)
{
    if (source == nullptr)
        source = GetDefault<ASharedIsometricCameraActor>();

    // CVar overrides when > 0, else the actor UPROPERTY; the axis is read directly.
    dAttackCameraBehaviour::IsoFramingSettings settings;
    settings.fovDeg  = g_fovDegrees    > 0.f ? g_fovDegrees    : source->m_fovDegrees;
    settings.fovAxis = dAttackCameraBehaviour::fovAxisFromConsoleValue(g_fovAxis);
    settings.marginH = g_marginHCm     > 0.f ? g_marginHCm     : source->m_marginHCm;
    settings.marginV = g_marginVCm     > 0.f ? g_marginVCm     : source->m_marginVCm;
    settings.minDist = g_minDistanceCm > 0.f ? g_minDistanceCm : source->m_minDistanceCm;
    settings.maxDist = g_maxDistanceCm > 0.f ? g_maxDistanceCm : source->m_maxDistanceCm;
    return settings;
}

bool ASharedIsometricCameraActor::matchesSoloCamera()
{
    return g_matchSolo > 0;
}

void ASharedIsometricCameraActor::reportCameraSight(bool onDemand)
{
    // The per-tick calls print nothing in a process that never renders: the
    // headless harness clients run with -nullrhi, so this tag stays off their
    // default-path logs.
    if (!onDemand && !FApp::CanEverRender())
        return;

    // The solo settings come from the character's one accessor (the same call
    // its Tick makes), never from a second copy of the og.cam.* CVar logic.
    const dAttackCameraBehaviour::OrbitCameraSettings solo = AOGBrawlerUECharacter::orbitCameraSettingsFromCVars();
    const ASharedIsometricCameraActor* camera = g_sightCamera.Get();
    const CameraSightInputs inputs{ solo.boomLengthAtTargetPitch, solo.fovDeg, solo.fovAxis, solo.fovDegAtHorizon, matchesSoloCamera(),
        resolveIsoFramingSettings(camera), viewportAspectRatio() };
    if (!onDemand && g_sightPrinted && sameSightInputs(inputs, g_lastSightInputs))
        return;
    g_sightPrinted = true;
    g_lastSightInputs = inputs;

    // No live shared camera: the group is empty, and the iso distance printed
    // is the one it would use for a single player.
    const bool haveGroup = camera != nullptr;
    const float groupHalfH = haveGroup ? g_sightGroupHalfH : 0.f;
    const float groupHalfV = haveGroup ? g_sightGroupHalfV : 0.f;
    const int32 players = haveGroup ? g_sightGroupPlayers : 0;
    const dAttackCameraBehaviour::CameraSight sight = dAttackCameraBehaviour::cameraSight(
        solo, inputs.matchSolo, inputs.iso, inputs.aspect, groupHalfH, groupHalfV);
    const float isoHorizontalFovDeg =
        dAttackCameraBehaviour::horizontalFovDeg(sight.iso.fovDeg, sight.iso.fovAxis, inputs.aspect);
    constexpr float kCmToM = 0.01f;
    UE_LOG(LogOGBrawler, Warning,
        TEXT("[Camera.sight] trigger=%s boomM=%.3f soloFovHDeg=%.3f soloFovVDeg=%.3f soloFovHorizonHDeg=%.3f aspect=%.4f reachHM=%.3f reachVM=%.3f ")
        TEXT("matchSolo=%d isoFovHDeg=%.3f marginHM=%.3f marginVM=%.3f players=%d groupHalfHM=%.3f groupHalfVM=%.3f isoDistM=%.3f"),
        onDemand ? TEXT("command") : TEXT("change"),
        sight.soloBoom * kCmToM, sight.soloHorizontalFovDeg, sight.soloVerticalFovDeg, sight.soloHorizontalFovAtHorizonDeg, inputs.aspect,
        sight.soloReach.halfWidth * kCmToM, sight.soloReach.halfHeight * kCmToM,
        sight.matchSolo ? 1 : 0, isoHorizontalFovDeg, sight.iso.marginH * kCmToM, sight.iso.marginV * kCmToM,
        players, groupHalfH * kCmToM, groupHalfV * kCmToM, sight.isoDistance * kCmToM);
}

void ASharedIsometricCameraActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!m_owningWorld.IsValid())
        return;

    // Resolve effective parameters — CVar overrides when non-zero, else actor UPROPERTY.
    const float deadzone    = g_posDeadzoneCm    > 0.f ? g_posDeadzoneCm    : m_positionDeadzoneCm;
    const float stopCm      = g_posStopCm        > 0.f ? g_posStopCm        : m_positionStopCm;
    const float posLerp     = g_posLerp          > 0.f ? g_posLerp          : m_positionLerpSpeed;

    // Determine aspect ratio from the viewport; 1.0 if not available. Read every
    // tick, so a resized window re-frames and re-converts the FOV at once.
    const float aspect = viewportAspectRatio();

    // The framing settings: this actor's own (resolveIsoFramingSettings), or,
    // with og.iso.matchSolo 1, the solo camera's FOV and margins equal to the
    // solo camera's reach at its boom (og.cam.boomTarget), so one local player
    // is framed from the solo boom and every outermost player of a group keeps
    // that reach. With matchSolo 0 the own settings pass through unchanged
    // (DAttackCameraMatchSoloTest.cpp). Read every tick, so og.cam.boomTarget /
    // og.cam.fov edits move this camera too.
    const dAttackCameraBehaviour::IsoFramingSettings framing = dAttackCameraBehaviour::resolveIsoFraming(
        matchesSoloCamera(), resolveIsoFramingSettings(this), AOGBrawlerUECharacter::orbitCameraSettingsFromCVars(), aspect);

    // Keep the live FOV in sync with the resolved value so CVar / UPROPERTY edits
    // take effect. This camera renders FieldOfView as the HORIZONTAL FOV (the
    // MaintainXFOV override set in the constructor), so a shorter-axis FOV is
    // converted first.
    const float renderedFovDegrees = dAttackCameraBehaviour::horizontalFovDeg(framing.fovDeg, framing.fovAxis, aspect);
    if (m_camera && !FMath::IsNearlyEqual(m_camera->FieldOfView, renderedFovDegrees))
    {
        m_camera->FieldOfView = renderedFovDegrees;
    }

    // Apply rotation from resolved pitch / yaw so CVar edits take effect at runtime.
    const FRotator targetRotation = resolveIsoRotation(this);
    if (!GetActorRotation().Equals(targetRotation, 0.01f))
    {
        SetActorRotation(targetRotation);
    }

    // Collect world positions of all locally-controlled brawlers.
    TArray<FVector> positions;
    for (TActorIterator<AOGBrawlerUECharacter> it(m_owningWorld.Get()); it; ++it)
    {
        if (it->IsLocallyControlled())
            positions.Add(it->GetActorLocation());
    }

    g_sightCamera = this;
    if (positions.Num() == 0)
    {
        g_sightGroupHalfH = 0.f;
        g_sightGroupHalfV = 0.f;
        g_sightGroupPlayers = 0;
        reportCameraSight(false);
        return;
    }

    FBox box(positions);

    // Rotate the AABB into camera-local space (yaw + pitch + roll) so the
    // resulting extents along Y (camera-right) and Z (camera-up) give us the
    // actual on-screen horizontal and vertical sizes of the box.
    // UE camera convention: actor +X = forward, +Y = right, +Z = up.
    const FQuat invCameraRot = GetActorQuat().Inverse();
    FVector corners[8];
    box.GetVertices(corners);
    FBox cameraLocalBox(EForceInit::ForceInitToZero);
    for (const FVector& corner : corners)
    {
        cameraLocalBox += invCameraRot.RotateVector(corner);
    }
    const FVector cameraLocalExtent = cameraLocalBox.GetExtent();
    const float halfScreenH = cameraLocalExtent.Y; // horizontal half-extent on screen
    const float halfScreenV = cameraLocalExtent.Z; // vertical half-extent on screen
    g_sightGroupHalfH = halfScreenH;
    g_sightGroupHalfV = halfScreenV;
    g_sightGroupPlayers = positions.Num();
    reportCameraSight(false);

    // Distance that fits the box padded by the H / V margins at the FOV UE
    // renders, clamped to the framing's [minDist, maxDist]. The margins pad the
    // on-screen half-extents (cm of breathing room around the brawlers at the
    // camera plane), not the distance, so the visual buffer is the same at any
    // FOV / distance. Engine-free and unit-tested: DAttackCameraFramingTest.cpp
    // (with og.iso.fovAxis 0 and equal margins it is the pre-task-15 formula,
    // bit for bit; the render is not: pre-task-15 the camera had no aspect-axis
    // override, so it rendered a fixed vertical FOV and only 16:9 matched).
    const float targetDistance = dAttackCameraBehaviour::isoFramingDistance(halfScreenH, halfScreenV, aspect, framing);

    // Target camera position: place the camera at (boxCenter - forward * distance)
    // so the box's world center is on the camera's optical axis.
    const FVector targetCenter   = box.GetCenter();
    const FVector targetLocation = targetCenter - GetActorForwardVector() * targetDistance;

    // Smooth position with a hysteresis deadzone: start following past
    // `deadzone`, keep following until within `stopCm`, then hold. With
    // perspective, "zoom" is just the camera moving along its forward axis, so
    // this one follow state handles both lateral pan and depth changes. The
    // step is FMath::VInterpTo's, transcribed engine-free so it is unit-tested
    // (DAttackCameraFollowTest.cpp).
    const FVector currentLocation = GetActorLocation();
    const dAttackCameraBehaviour::FollowStep step = dAttackCameraBehaviour::followStep(
        glm::dvec3(currentLocation.X, currentLocation.Y, currentLocation.Z),
        glm::dvec3(targetLocation.X, targetLocation.Y, targetLocation.Z),
        m_followTracking, DeltaSeconds, posLerp, deadzone, stopCm);
    m_followTracking = step.tracking;
    if (step.tracking)
    {
        SetActorLocation(FVector(step.position.x, step.position.y, step.position.z));
    }
}
