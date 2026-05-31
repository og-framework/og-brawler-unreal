// SPDX-License-Identifier: BUSL-1.1

#include "SharedIsometricCameraActor.h"
#include "OGBrawlerUECharacter.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

namespace
{
    // Backing storage for CVars — Tick reads these each frame as overrides when non-zero.
    // A value of 0 means "use the UPROPERTY default on the actor".
    static float g_fovDegrees       = 0.f;
    static float g_pitchDegrees     = 0.f;
    static float g_yawDegrees       = 0.f;
    static float g_minDistanceCm    = 0.f;
    static float g_maxDistanceCm    = 0.f;
    static float g_marginCm         = 0.f;
    static float g_posDeadzoneCm    = 0.f;
    static float g_posLerp          = 0.f;

    static FAutoConsoleVariableRef CVarFov(
        TEXT("og.iso.fov"),
        g_fovDegrees,
        TEXT("Shared iso cam: horizontal FOV in degrees (0 = use actor default; narrower = less perspective distortion)"));

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
        TEXT("Shared iso cam: minimum camera distance in cm (0 = use actor default)"));

    static FAutoConsoleVariableRef CVarMaxDistance(
        TEXT("og.iso.maxDistance"),
        g_maxDistanceCm,
        TEXT("Shared iso cam: maximum camera distance in cm (0 = use actor default)"));

    static FAutoConsoleVariableRef CVarMarginCm(
        TEXT("og.iso.marginCm"),
        g_marginCm,
        TEXT("Shared iso cam: framing margin in cm (0 = use actor default)"));

    static FAutoConsoleVariableRef CVarPosDeadzoneCm(
        TEXT("og.iso.posDeadzoneCm"),
        g_posDeadzoneCm,
        TEXT("Shared iso cam: position deadzone in cm (0 = use actor default)"));

    static FAutoConsoleVariableRef CVarPosLerp(
        TEXT("og.iso.posLerp"),
        g_posLerp,
        TEXT("Shared iso cam: position lerp speed (0 = use actor default)"));
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
    m_camera->FieldOfView = m_fovDegrees;
    m_camera->bConstrainAspectRatio = false;

    SetActorRotation(FRotator(m_pitchDegrees, m_yawDegrees, 0.f));
}

void ASharedIsometricCameraActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!m_owningWorld.IsValid())
        return;

    // Resolve effective parameters — CVar overrides when non-zero, else actor UPROPERTY.
    const float fovDegrees  = g_fovDegrees       > 0.f ? g_fovDegrees       : m_fovDegrees;
    const float pitchDegrees = !FMath::IsNearlyZero(g_pitchDegrees) ? g_pitchDegrees : m_pitchDegrees;
    const float yawDegrees   = !FMath::IsNearlyZero(g_yawDegrees)   ? g_yawDegrees   : m_yawDegrees;
    const float minDistance = g_minDistanceCm    > 0.f ? g_minDistanceCm    : m_minDistanceCm;
    const float maxDistance = g_maxDistanceCm    > 0.f ? g_maxDistanceCm    : m_maxDistanceCm;
    const float margin      = g_marginCm         > 0.f ? g_marginCm         : m_marginCm;
    const float deadzone    = g_posDeadzoneCm    > 0.f ? g_posDeadzoneCm    : m_positionDeadzoneCm;
    const float posLerp     = g_posLerp          > 0.f ? g_posLerp          : m_positionLerpSpeed;

    // Keep the live FOV in sync with the resolved value so CVar / UPROPERTY edits take effect.
    if (m_camera && !FMath::IsNearlyEqual(m_camera->FieldOfView, fovDegrees))
    {
        m_camera->FieldOfView = fovDegrees;
    }

    // Apply rotation from resolved pitch / yaw so CVar edits take effect at runtime.
    const FRotator targetRotation(pitchDegrees, yawDegrees, 0.f);
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

    if (positions.Num() == 0)
        return;

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

    // Determine aspect ratio from the viewport; fall back to 1.0 if not available.
    float aspect = 1.f;
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint size = GEngine->GameViewport->Viewport->GetSizeXY();
        if (size.Y > 0)
            aspect = static_cast<float>(size.X) / static_cast<float>(size.Y);
    }

    // Pad the on-screen half-extents BEFORE computing distance. Padding the
    // extent (cm of breathing room around brawlers at the camera plane) rather
    // than the distance keeps the visual buffer constant regardless of FOV /
    // distance — a 800cm extent pad always shows 800cm of empty space around
    // the outermost brawler, whereas a "+800cm distance" buffer only adds
    // ~280cm of visible space at FOV=20°.
    const float paddedHalfH = halfScreenH + margin;
    const float paddedHalfV = halfScreenV + margin;

    // Distance required to fit the padded box at the configured FOV.
    // Horizontal: d_h = paddedHalfH / tan(hFOV/2).
    // Vertical: tan(vFOV/2) = tan(hFOV/2) / aspect, so d_v = paddedHalfV * aspect / tan(hFOV/2).
    // Use max(d_h, d_v) so the limiting axis determines the camera distance.
    const float tanHalfFov = FMath::Tan(FMath::DegreesToRadians(fovDegrees * 0.5f));
    const float safeTan = FMath::Max(tanHalfFov, 0.001f);
    const float requiredDistance = FMath::Max(paddedHalfH, paddedHalfV * aspect) / safeTan;
    const float targetDistance = FMath::Clamp(requiredDistance, minDistance, maxDistance);

    // Target camera position: place the camera at (boxCenter - forward * distance)
    // so the box's world center is on the camera's optical axis.
    const FVector targetCenter   = box.GetCenter();
    const FVector targetLocation = targetCenter - GetActorForwardVector() * targetDistance;

    // Smooth position with deadzone. With perspective, "zoom" is just the camera
    // moving along its forward axis, so this single deadzone naturally handles
    // both lateral pan and depth changes.
    if (FVector::Dist(GetActorLocation(), targetLocation) > deadzone)
    {
        SetActorLocation(FMath::VInterpTo(GetActorLocation(), targetLocation, DeltaSeconds, posLerp));
    }
}
