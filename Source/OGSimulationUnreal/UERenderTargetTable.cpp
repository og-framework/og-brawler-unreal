// SPDX-License-Identifier: MPL-2.0
// docs/UERenderTargetTable-rationale.md · docs/UERenderTargetTable-guards.md

#include "UERenderTargetTable.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "UGLMTypeConversion.h"

namespace
{
	glm::dvec3 lengthOf(const FVector& centimetres)
	{
		return glm::dvec3(centimetres.X, centimetres.Y, centimetres.Z) / UERenderTargetTable::kCentimetresPerLength;
	}
}

float UERenderTargetTable::lengthFromCentimetres(float centimetres)
{
	return static_cast<float>(static_cast<double>(centimetres) / kCentimetresPerLength);
}

void UERenderTargetTable::registerTarget(uint32_t id, AActor* actor, const USceneComponent* expectedRoot, bool runsPrediction)
{
	m_targets.Add(id, Target{ actor, actor != nullptr ? std::string(TCHAR_TO_UTF8(*actor->GetName())) : std::string() });
	if (actor == nullptr)
		return;

#if !UE_BUILD_SHIPPING
	checkf(actor->GetRootComponent() == expectedRoot,
		TEXT("UERenderTargetTable::registerTarget: id=%u's root component (%s) is not the component the game names as its ")
		TEXT("render root (%s). The render apply moves the actor's root and the render checks read its translation, while ")
		TEXT("the engine-hosted render path moved the actor and read that component. Name the actor's root component as ")
		TEXT("its render root, or make the named component the root."),
		id, actor->GetRootComponent() != nullptr ? *actor->GetRootComponent()->GetName() : TEXT("none"),
		expectedRoot != nullptr ? *expectedRoot->GetName() : TEXT("none"));
#endif

	UPrimitiveComponent* const root = Cast<UPrimitiveComponent>(actor->GetRootComponent());
	checkf(root != nullptr,
		TEXT("UERenderTargetTable::registerTarget: id=%u's root component is not a primitive component, so the ")
		TEXT("registration cannot switch its physics simulation off."), id);
	root->SetSimulatePhysics(false);

	TArray<UPrimitiveComponent*> primitives;
	actor->GetComponents<UPrimitiveComponent>(primitives);
	FString simulating;
	for (const UPrimitiveComponent* primitive : primitives)
	{
		if (primitive->IsSimulatingPhysics())
			simulating += (simulating.IsEmpty() ? TEXT("") : TEXT(",")) + primitive->GetName();
	}
	UE_LOG(LogOGSimHost, Log,
		TEXT("[SimHost.RenderTarget] role=%s id=%u capsuleSimulatesPhysics=%d simulatingComponents=[%s]"),
		runsPrediction ? TEXT("Client") : TEXT("Authority"), id, root->IsSimulatingPhysics() ? 1 : 0,
		*simulating);
}

void UERenderTargetTable::unregisterTarget(uint32_t id)
{
	m_targets.Remove(id);
}

std::optional<glm::dvec3> UERenderTargetTable::applyRootPose(uint32_t id, const glm::vec3& position,
	const std::optional<glm::quat>& rotation)
{
	AActor* const actor = actorOf(id);
	if (actor == nullptr)
		return std::nullopt;

	const FVector location = uglm::toFVector(position) * kCentimetresPerLength;
	const FQuat actorRotation = rotation.has_value() ? uglm::toFQuat(*rotation) : actor->GetActorQuat();
	// ⛔G-01  docs/UERenderTargetTable-guards.md
	actor->SetActorLocationAndRotation(location, actorRotation, /*bSweep*/ false, /*OutSweepHitResult*/ nullptr,
		ETeleportType::TeleportPhysics);
	return lengthOf(actor->GetActorLocation());
}

std::optional<glm::dvec3> UERenderTargetTable::readAnchor(uint32_t id) const
{
	const AActor* const actor = actorOf(id);
	if (actor == nullptr || actor->GetRootComponent() == nullptr)
		return std::nullopt;
	return lengthOf(actor->GetRootComponent()->GetComponentTransform().GetTranslation());
}

const char* UERenderTargetTable::name(uint32_t id) const
{
	const Target* const target = m_targets.Find(id);
	return target != nullptr ? target->name.c_str() : "";
}

AActor* UERenderTargetTable::actorOf(uint32_t id) const
{
	const Target* const target = m_targets.Find(id);
	return target != nullptr ? target->actor.Get() : nullptr;
}
