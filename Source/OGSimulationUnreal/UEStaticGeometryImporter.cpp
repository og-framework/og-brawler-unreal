// SPDX-License-Identifier: MPL-2.0
// docs/UEStaticGeometryImporter-rationale.md · docs/UEStaticGeometryImporter-guards.md

#include "UEStaticGeometryImporter.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

#include "Chaos/Convex.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/TaperedCapsuleElem.h"
#include "PhysicsSettingsCore.h"
#include "UGLMTypeConversion.h"

DEFINE_LOG_CATEGORY(LogOGStaticImport);

namespace
{
	constexpr uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ull;
	constexpr uint64_t kFnvPrime = 0x00000100000001b3ull;
	constexpr uint32_t kCategoryBitCount = 32;

	struct Fnv1a64
	{
		uint64_t state = kFnvOffsetBasis;

		void byte(uint8_t value)
		{
			state ^= value;
			state *= kFnvPrime;
		}

		void u32(uint32_t value)
		{
			for (int shift = 0; shift < 32; shift += 8)
			{
				byte(static_cast<uint8_t>(value >> shift));
			}
		}

		void u64(uint64_t value)
		{
			for (int shift = 0; shift < 64; shift += 8)
			{
				byte(static_cast<uint8_t>(value >> shift));
			}
		}

		void f32(float value) { u32(std::bit_cast<uint32_t>(value)); }

		void vec3(const glm::vec3& value)
		{
			f32(value.x);
			f32(value.y);
			f32(value.z);
		}

		void text(const FString& value)
		{
			const FTCHARToUTF8 utf8(*value);
			for (int32 i = 0; i < utf8.Length(); ++i)
			{
				byte(static_cast<uint8_t>(utf8.Get()[i]));
			}
			byte(0);
		}
	};

	static_assert(std::is_same_v<decltype(StaticBox::halfExtents), glm::vec3>
		&& std::is_same_v<decltype(StaticSphere::radius), float>
		&& std::is_same_v<decltype(StaticCapsuleZ::totalHalfHeight), float>
		&& std::is_same_v<decltype(StaticConvexHull::points), std::vector<glm::vec3>>
		&& std::is_same_v<decltype(StaticTriangleMesh::indices), std::vector<uint32_t>>
		&& std::is_same_v<decltype(StaticShapeDescriptor::friction), float>
		&& std::is_same_v<decltype(StaticShapeDescriptor::localToWorld), glm::mat4>,
		"UEStaticGeometryImporter: the checksum hashes these fields as 32-bit floats and indices; a type change must update ShapeHasher");

	struct ShapeHasher
	{
		Fnv1a64& hash;

		void operator()(const StaticBox& box) const { hash.vec3(box.halfExtents); }
		void operator()(const StaticSphere& sphere) const { hash.f32(sphere.radius); }

		void operator()(const StaticCapsuleZ& capsule) const
		{
			hash.f32(capsule.radius);
			hash.f32(capsule.totalHalfHeight);
		}

		void operator()(const StaticConvexHull& hull) const
		{
			hash.u64(hull.points.size());
			for (const glm::vec3& point : hull.points)
			{
				hash.vec3(point);
			}
		}

		void operator()(const StaticTriangleMesh& mesh) const
		{
			hash.u64(mesh.vertices.size());
			for (const glm::vec3& vertex : mesh.vertices)
			{
				hash.vec3(vertex);
			}
			hash.u64(mesh.indices.size());
			for (uint32_t index : mesh.indices)
			{
				hash.u32(index);
			}
		}
	};

	void hashDescriptor(Fnv1a64& hash, const StaticShapeDescriptor& descriptor)
	{
		hash.u64(descriptor.stableKey);
		hash.u32(static_cast<uint32_t>(descriptor.shape.index()));
		std::visit(ShapeHasher{hash}, descriptor.shape);
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				hash.f32(descriptor.localToWorld[column][row]);
			}
		}
		hash.u32(descriptor.categories.bits);
		hash.u32(descriptor.blockingCategories.bits);
		hash.f32(descriptor.friction);
		hash.f32(descriptor.restitution);
	}

	uint64_t descriptorHash(const StaticShapeDescriptor& descriptor)
	{
		Fnv1a64 hash;
		hashDescriptor(hash, descriptor);
		return hash.state;
	}

	uint64_t stableKeyFor(const FString& actorPath, const FString& componentName, uint32_t instanceIndex, uint32_t elementIndex)
	{
		Fnv1a64 hash;
		hash.text(actorPath);
		hash.text(componentName);
		hash.u32(instanceIndex);
		hash.u32(elementIndex);
		return hash.state;
	}

	template <typename>
	inline constexpr bool kAlwaysFalse = false;

	template <typename Shape>
	constexpr const TCHAR* shapeTypeName()
	{
		if constexpr (std::is_same_v<Shape, StaticBox>) { return TEXT("box"); }
		else if constexpr (std::is_same_v<Shape, StaticSphere>) { return TEXT("sphere"); }
		else if constexpr (std::is_same_v<Shape, StaticCapsuleZ>) { return TEXT("capsule"); }
		else if constexpr (std::is_same_v<Shape, StaticConvexHull>) { return TEXT("convex"); }
		else if constexpr (std::is_same_v<Shape, StaticTriangleMesh>) { return TEXT("trimesh"); }
		else
		{
			static_assert(kAlwaysFalse<Shape>, "UEStaticGeometryImporter: a new StaticShape alternative needs a log name and an import path");
			return nullptr;
		}
	}

	template <std::size_t... Index>
	constexpr std::array<const TCHAR*, sizeof...(Index)> makeShapeTypeNames(std::index_sequence<Index...>)
	{
		return { shapeTypeName<std::variant_alternative_t<Index, StaticShape>>()... };
	}

	constexpr std::array<const TCHAR*, kStaticShapeTypeCount> kShapeTypeNames =
		makeShapeTypeNames(std::make_index_sequence<kStaticShapeTypeCount>{});

	bool isLandscapeCollision(const UPrimitiveComponent& primitive)
	{
		static const FName landscapeCollisionClass(TEXT("LandscapeHeightfieldCollisionComponent"));
		for (const UClass* cls = primitive.GetClass(); cls != nullptr; cls = cls->GetSuperClass())
		{
			if (cls->GetFName() == landscapeCollisionClass)
			{
				return true;
			}
		}
		return false;
	}

	bool isUniform(const FVector& scale)
	{
		const FVector absScale = scale.GetAbs();
		return FMath::IsNearlyEqual(absScale.X, absScale.Y) && FMath::IsNearlyEqual(absScale.Y, absScale.Z);
	}

	FVector convexNetScale(const FVector& scale)
	{
		FVector net = scale;
		for (int32 axis = 0; axis < 3; ++axis)
		{
			if (FMath::Abs(net[axis]) < UE_KINDA_SMALL_NUMBER)
			{
				net[axis] = UE_KINDA_SMALL_NUMBER;
			}
		}
		return net;
	}

	glm::vec3 scaledPoint(const FVector& point, const FVector& scale)
	{
		return glm::vec3(static_cast<float>(point.X * scale.X), static_cast<float>(point.Y * scale.Y), static_cast<float>(point.Z * scale.Z));
	}

	struct ComponentContext
	{
		const FString& actorPath;
		FString componentName;
		ECollisionEnabled::Type componentCollision = ECollisionEnabled::NoCollision;
		CollisionCategories categories;
		CollisionCategories blockingCategories;
		float friction = 0.f;
		float restitution = 0.f;
		bool reportedNonUniform = false;
	};

	struct WorldImport
	{
		const std::vector<UEStaticCategoryChannel>& categoryChannels;
		const std::array<CollisionCategories, ECollisionChannel::ECC_MAX>& channelToCategory;
		UEStaticImportReport& report;
		std::vector<StaticShapeDescriptor>& shapes;
		std::vector<uint8_t>& physicsOnly;

		void emit(ComponentContext& component, StaticShape shape, const FTransform& shapeToBody, const FTransform& bodyToWorld,
			ECollisionEnabled::Type elementCollision, uint32_t instanceIndex, uint32_t elementIndex)
		{
			const ECollisionEnabled::Type collision = CollisionEnabledIntersection(component.componentCollision, elementCollision);
			if (collision == ECollisionEnabled::NoCollision)
			{
				++report.elementsCollisionDisabled;
				return;
			}
			if (component.categories.bits == 0)
			{
				++report.elementsUnmappedObjectType;
				return;
			}
			const bool isPhysicsOnly = !CollisionEnabledHasQuery(collision);
			if (isPhysicsOnly)
			{
				++report.elementsPhysicsOnly;
			}

			StaticShapeDescriptor descriptor;
			descriptor.shape = std::move(shape);
			descriptor.localToWorld = uglm::toGLMMat4(shapeToBody * bodyToWorld);
			descriptor.categories = component.categories;
			// ⛔G-04  docs/UEStaticGeometryImporter-guards.md
			descriptor.blockingCategories = CollisionEnabledHasPhysics(collision) ? component.blockingCategories : CollisionCategories{};
			descriptor.friction = component.friction;
			descriptor.restitution = component.restitution;
			descriptor.stableKey = stableKeyFor(component.actorPath, component.componentName, instanceIndex, elementIndex);
			++report.shapeCountByType[descriptor.shape.index()];
			UE_LOG(LogOGStaticImport, Verbose,
				TEXT("[StaticImport] shape key=0x%016llx type=%s %s.%s instance=%u element=%u categories=0x%08x blocking=0x%08x at=(%.1f,%.1f,%.1f) friction=%.3f restitution=%.3f"),
				static_cast<unsigned long long>(descriptor.stableKey), kShapeTypeNames[descriptor.shape.index()], *component.actorPath,
				*component.componentName, instanceIndex, elementIndex, descriptor.categories.bits, descriptor.blockingCategories.bits,
				descriptor.localToWorld[3][0], descriptor.localToWorld[3][1], descriptor.localToWorld[3][2], descriptor.friction, descriptor.restitution);
			shapes.push_back(std::move(descriptor));
			physicsOnly.push_back(isPhysicsOnly ? 1u : 0u);
		}

		void warnNonUniform(ComponentContext& component, const FVector& scale)
		{
			++report.nonUniformSphereOrCapsule;
			if (!component.reportedNonUniform)
			{
				component.reportedNonUniform = true;
				UE_LOG(LogOGStaticImport, Warning,
					TEXT("[StaticImport] non-uniform scale %s on a sphere or capsule of %s.%s: approximated as UE does (sphere radius x min |scale|; capsule radius x max(|X|,|Y|), length x |Z|)"),
					*scale.ToString(), *component.actorPath, *component.componentName);
			}
		}

		void importBody(ComponentContext& component, const UBodySetup& bodySetup, const FTransform& componentToWorld, uint32_t instanceIndex)
		{
			const FVector scale = componentToWorld.GetScale3D().IsNearlyZero() ? FVector(0.1) : componentToWorld.GetScale3D();
			const FTransform bodyToWorld(componentToWorld.GetRotation(), componentToWorld.GetTranslation());
			const FKAggregateGeom& geometry = bodySetup.AggGeom;

			ECollisionTraceFlag traceFlag = bodySetup.GetCollisionTraceFlag();
			if (traceFlag == CTF_UseDefault)
			{
				traceFlag = UPhysicsSettingsCore::Get()->DefaultShapeComplexity;
			}
			// ⛔G-02  docs/UEStaticGeometryImporter-guards.md
			const bool useSimpleGeometry = traceFlag != CTF_UseComplexAsSimple || bodySetup.TriMeshGeometries.Num() == 0;

			uint32_t elementIndex = 0;
			if (useSimpleGeometry)
			{
				for (const FKSphereElem& element : geometry.SphereElems)
				{
					const FKSphereElem scaled = element.GetFinalScaled(scale, FTransform::Identity);
					if (!isUniform(scale))
					{
						warnNonUniform(component, scale);
					}
					const float radius = static_cast<float>(FMath::Max<double>(scaled.Radius, UE_KINDA_SMALL_NUMBER));
					emit(component, StaticSphere{radius}, FTransform(scaled.Center), bodyToWorld, element.GetCollisionEnabled(), instanceIndex, elementIndex++);
				}
				for (const FKBoxElem& element : geometry.BoxElems)
				{
					const FKBoxElem scaled = element.GetFinalScaled(scale, FTransform::Identity);
					const glm::vec3 halfExtents(
						static_cast<float>(FMath::Max<double>(scaled.X * 0.5, UE_KINDA_SMALL_NUMBER)),
						static_cast<float>(FMath::Max<double>(scaled.Y * 0.5, UE_KINDA_SMALL_NUMBER)),
						static_cast<float>(FMath::Max<double>(scaled.Z * 0.5, UE_KINDA_SMALL_NUMBER)));
					emit(component, StaticBox{halfExtents}, scaled.GetTransform(), bodyToWorld, element.GetCollisionEnabled(), instanceIndex, elementIndex++);
				}
				for (const FKSphylElem& element : geometry.SphylElems)
				{
					const FKSphylElem scaled = element.GetFinalScaled(scale, FTransform::Identity);
					if (!isUniform(scale))
					{
						warnNonUniform(component, scale);
					}
					const float radius = static_cast<float>(FMath::Max<double>(scaled.Radius, UE_KINDA_SMALL_NUMBER));
					const float cylinderHalfLength = static_cast<float>(FMath::Max<double>(scaled.Length * 0.5, UE_KINDA_SMALL_NUMBER));
					emit(component, StaticCapsuleZ{radius, cylinderHalfLength + radius}, scaled.GetTransform(), bodyToWorld,
						element.GetCollisionEnabled(), instanceIndex, elementIndex++);
				}
				for (const FKTaperedCapsuleElem& element : geometry.TaperedCapsuleElems)
				{
					const FKTaperedCapsuleElem scaled = element.GetFinalScaled(scale, FTransform::Identity);
					if (!isUniform(scale))
					{
						warnNonUniform(component, scale);
					}
					const double radius0 = FMath::Max<double>(scaled.Radius0, UE_KINDA_SMALL_NUMBER);
					const double radius1 = FMath::Max<double>(scaled.Radius1, UE_KINDA_SMALL_NUMBER);
					const float meanRadius = static_cast<float>(0.5 * (radius0 + radius1));
					const float cylinderHalfLength = static_cast<float>(FMath::Max<double>(scaled.Length * 0.5, UE_KINDA_SMALL_NUMBER));
					emit(component, StaticCapsuleZ{meanRadius, cylinderHalfLength + meanRadius}, scaled.GetTransform(), bodyToWorld,
						element.GetCollisionEnabled(), instanceIndex, elementIndex++);
				}

				const FVector netScale = convexNetScale(scale);
				for (const FKConvexElem& element : geometry.ConvexElems)
				{
					const uint32_t thisIndex = elementIndex++;
					const Chaos::FConvexPtr& convex = element.GetChaosConvexMesh();
					if (!convex)
					{
						++report.convexWithoutChaosMesh;
						continue;
					}
					StaticConvexHull hull;
					hull.points.reserve(static_cast<std::size_t>(convex->NumVertices()));
					for (int32 vertex = 0; vertex < convex->NumVertices(); ++vertex)
					{
						hull.points.push_back(scaledPoint(FVector(convex->GetVertex(vertex)), netScale));
					}
					emit(component, std::move(hull), FTransform::Identity, bodyToWorld, element.GetCollisionEnabled(), instanceIndex, thisIndex);
				}
				return;
			}

			elementIndex = static_cast<uint32_t>(geometry.SphereElems.Num() + geometry.BoxElems.Num() + geometry.SphylElems.Num()
				+ geometry.TaperedCapsuleElems.Num() + geometry.ConvexElems.Num());
			const bool mirrored = scale.X * scale.Y * scale.Z < 0.0;
			for (const Chaos::FTriangleMeshImplicitObjectPtr& triangleMesh : bodySetup.TriMeshGeometries)
			{
				const uint32_t thisIndex = elementIndex++;
				if (!triangleMesh)
				{
					continue;
				}
				StaticTriangleMesh mesh;
				const auto& particles = triangleMesh->Particles();
				mesh.vertices.reserve(particles.Size());
				for (uint32 vertex = 0; vertex < particles.Size(); ++vertex)
				{
					mesh.vertices.push_back(scaledPoint(FVector(particles.GetX(static_cast<int32>(vertex))), scale));
				}
				const auto appendTriangles = [&mesh, mirrored](const auto& triangles)
				{
					mesh.indices.reserve(static_cast<std::size_t>(triangles.Num()) * 3);
					for (const auto& triangle : triangles)
					{
						mesh.indices.push_back(static_cast<uint32_t>(triangle[0]));
						mesh.indices.push_back(static_cast<uint32_t>(mirrored ? triangle[2] : triangle[1]));
						mesh.indices.push_back(static_cast<uint32_t>(mirrored ? triangle[1] : triangle[2]));
					}
				};
				const Chaos::FTrimeshIndexBuffer& elements = triangleMesh->Elements();
				if (elements.RequiresLargeIndices())
				{
					appendTriangles(elements.GetLargeIndexBuffer());
				}
				else
				{
					appendTriangles(elements.GetSmallIndexBuffer());
				}
				emit(component, std::move(mesh), FTransform::Identity, bodyToWorld, ECollisionEnabled::QueryAndPhysics, instanceIndex, thisIndex);
			}
		}

		void skip(uint32_t& counter, const TCHAR* reason, const FString& actorPath, const UPrimitiveComponent& primitive)
		{
			++counter;
			UE_LOG(LogOGStaticImport, Verbose, TEXT("[StaticImport] skip %s: %s.%s (%s)"), reason, *actorPath, *primitive.GetName(), *primitive.GetClass()->GetName());
		}

		void importComponent(const FString& actorPath, UPrimitiveComponent& primitive)
		{
			if (!primitive.IsRegistered() || primitive.GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				return;
			}
			if (isLandscapeCollision(primitive))
			{
				skip(report.landscapeUnsupported, TEXT("landscape"), actorPath, primitive);
				return;
			}
			if (primitive.IsSimulatingPhysics())
			{
				skip(report.simulatingSkipped, TEXT("simulating"), actorPath, primitive);
				return;
			}
			if (primitive.Mobility == EComponentMobility::Movable)
			{
				skip(report.movableSkipped, TEXT("movable"), actorPath, primitive);
				return;
			}
			if (primitive.IsA<USkinnedMeshComponent>())
			{
				skip(report.skinnedUnsupported, TEXT("skinned"), actorPath, primitive);
				return;
			}
			const UBodySetup* bodySetup = primitive.GetBodySetup();
			if (bodySetup == nullptr)
			{
				skip(report.noBodySetup, TEXT("noBodySetup"), actorPath, primitive);
				return;
			}

			ComponentContext component{actorPath};
			component.componentName = primitive.GetName();
			component.componentCollision = primitive.GetCollisionEnabled();
			component.categories = channelToCategory[primitive.GetCollisionObjectType()];
			for (const UEStaticCategoryChannel& mapping : categoryChannels)
			{
				if (primitive.GetCollisionResponseToChannel(mapping.channel) == ECR_Block)
				{
					component.blockingCategories |= CollisionCategories::single(mapping.category);
				}
			}
			if (const UPhysicalMaterial* material = primitive.BodyInstance.GetSimplePhysicalMaterial())
			{
				component.friction = material->Friction;
				component.restitution = material->Restitution;
			}

			++report.componentsImported;
			if (const UInstancedStaticMeshComponent* instanced = Cast<UInstancedStaticMeshComponent>(&primitive))
			{
				const int32 instanceCount = instanced->GetInstanceCount();
				for (int32 instance = 0; instance < instanceCount; ++instance)
				{
					FTransform instanceToWorld;
					if (instanced->GetInstanceTransform(instance, instanceToWorld, /*bWorldSpace*/ true))
					{
						++report.instancesImported;
						importBody(component, *bodySetup, instanceToWorld, static_cast<uint32_t>(instance));
					}
				}
				return;
			}
			importBody(component, *bodySetup, primitive.GetComponentTransform(), 0);
		}
	};
} // namespace

uint32_t UEStaticImportReport::shapeCount() const
{
	uint32_t total = 0;
	for (uint32_t count : shapeCountByType)
	{
		total += count;
	}
	return total;
}

UEStaticGeometryImporter::UEStaticGeometryImporter(std::initializer_list<UEStaticCategoryChannel> categoryChannels)
{
	for (const UEStaticCategoryChannel& mapping : categoryChannels)
	{
		addMapping(mapping);
	}
}

UEStaticGeometryImporter::UEStaticGeometryImporter(const std::vector<UEStaticCategoryChannel>& categoryChannels)
{
	for (const UEStaticCategoryChannel& mapping : categoryChannels)
	{
		addMapping(mapping);
	}
}

void UEStaticGeometryImporter::addMapping(const UEStaticCategoryChannel& mapping)
{
	const int32 channel = static_cast<int32>(mapping.channel);
	if (mapping.category >= kCategoryBitCount || channel < 0 || channel >= ECollisionChannel::ECC_MAX)
	{
		UE_LOG(LogOGStaticImport, Error, TEXT("[StaticImport] mapping rejected: category %u, channel %d (category must be < %u, channel < %d)"),
			mapping.category, channel, kCategoryBitCount, static_cast<int32>(ECollisionChannel::ECC_MAX));
		return;
	}
	for (const UEStaticCategoryChannel& existing : m_categoryChannels)
	{
		if (existing.category == mapping.category || existing.channel == mapping.channel)
		{
			UE_LOG(LogOGStaticImport, Error, TEXT("[StaticImport] mapping rejected: category %u, channel %d repeats a category or channel already mapped"),
				mapping.category, channel);
			return;
		}
	}
	m_categoryChannels.push_back(mapping);
	m_channelToCategory[channel] = CollisionCategories::single(mapping.category);
}

UEStaticImportResult UEStaticGeometryImporter::importWorld(UWorld& world) const
{
	const double startSeconds = FPlatformTime::Seconds();
	UEStaticImportResult result;
	WorldImport walk{m_categoryChannels, m_channelToCategory, result.report, result.description.shapes, result.physicsOnly};

	for (TActorIterator<AActor> it(&world); it; ++it)
	{
		AActor* actor = *it;
		// ⛔G-01  docs/UEStaticGeometryImporter-guards.md
		const FString actorPath = UWorld::RemovePIEPrefix(actor->GetPathName());
		actor->ForEachComponent<UPrimitiveComponent>(false, [&walk, &actorPath](UPrimitiveComponent* primitive)
		{
			walk.importComponent(actorPath, *primitive);
		});
	}

	std::vector<StaticShapeDescriptor>& shapes = result.description.shapes;
	std::vector<std::pair<uint64_t, std::size_t>> order;
	order.reserve(shapes.size());
	for (std::size_t i = 0; i < shapes.size(); ++i)
	{
		order.emplace_back(descriptorHash(shapes[i]), i);
	}
	// ⛔G-03  docs/UEStaticGeometryImporter-guards.md
	std::sort(order.begin(), order.end(), [&shapes](const auto& a, const auto& b)
	{
		const uint64_t keyA = shapes[a.second].stableKey;
		const uint64_t keyB = shapes[b.second].stableKey;
		return keyA != keyB ? keyA < keyB : a.first < b.first;
	});
	std::vector<StaticShapeDescriptor> sorted;
	std::vector<uint8_t> sortedPhysicsOnly;
	sorted.reserve(shapes.size());
	sortedPhysicsOnly.reserve(shapes.size());
	for (const auto& entry : order)
	{
		sorted.push_back(std::move(shapes[entry.second]));
		sortedPhysicsOnly.push_back(result.physicsOnly[entry.second]);
	}
	shapes = std::move(sorted);
	result.physicsOnly = std::move(sortedPhysicsOnly);

	for (std::size_t i = 1; i < shapes.size(); ++i)
	{
		if (shapes[i].stableKey == shapes[i - 1].stableKey)
		{
			++result.report.duplicateKeys;
		}
	}
	result.report.checksum = checksum(result.description);
	result.report.importSeconds = FPlatformTime::Seconds() - startSeconds;
	return result;
}

uint64_t UEStaticGeometryImporter::checksum(const StaticWorldDescription& description)
{
	Fnv1a64 hash;
	hash.u64(description.shapes.size());
	for (const StaticShapeDescriptor& descriptor : description.shapes)
	{
		hashDescriptor(hash, descriptor);
	}
	return hash.state;
}

void UEStaticGeometryImporter::logReport(const UEStaticImportReport& report, const TCHAR* label)
{
	FString perType;
	for (std::size_t type = 0; type < kStaticShapeTypeCount; ++type)
	{
		perType += FString::Printf(TEXT(" %s=%u"), kShapeTypeNames[type], report.shapeCountByType[type]);
	}
	UE_LOG(LogOGStaticImport, Log,
		TEXT("[StaticImport] %s shapes=%u%s components=%u instances=%u checksum=0x%016llx seconds=%.4f"),
		label, report.shapeCount(), *perType, report.componentsImported, report.instancesImported,
		static_cast<unsigned long long>(report.checksum), report.importSeconds);
	UE_LOG(LogOGStaticImport, Log,
		TEXT("[StaticImport] %s skipped: movable=%u simulating=%u noBodySetup=%u elemCollisionOff=%u elemUnmappedObjectType=%u; elemPhysicsOnly=%u convexWithoutChaosMesh=%u nonUniformSphereOrCapsule=%u"),
		label, report.movableSkipped, report.simulatingSkipped, report.noBodySetup, report.elementsCollisionDisabled,
		report.elementsUnmappedObjectType, report.elementsPhysicsOnly, report.convexWithoutChaosMesh, report.nonUniformSphereOrCapsule);
	if (report.landscapeUnsupported > 0 || report.skinnedUnsupported > 0)
	{
		UE_LOG(LogOGStaticImport, Warning, TEXT("[StaticImport] %s Unsupported: landscape=%u skinned=%u components not imported"),
			label, report.landscapeUnsupported, report.skinnedUnsupported);
	}
	if (report.duplicateKeys > 0)
	{
		UE_LOG(LogOGStaticImport, Error, TEXT("[StaticImport] %s %u duplicate stableKey(s): peers may order these statics differently"),
			label, report.duplicateKeys);
	}
}

namespace
{
	bool parseChannel(const FString& text, ECollisionChannel& outChannel)
	{
		const UEnum* channelEnum = StaticEnum<ECollisionChannel>();
		int64 value = channelEnum->GetValueByNameString(text);
		if (value == INDEX_NONE)
		{
			value = channelEnum->GetValueByNameString(TEXT("ECC_") + text);
		}
		if (value == INDEX_NONE || value < 0 || value >= ECollisionChannel::ECC_MAX)
		{
			return false;
		}
		outChannel = static_cast<ECollisionChannel>(value);
		return true;
	}

	void runImportCommand(const TArray<FString>& args, UWorld* world)
	{
		if (world == nullptr)
		{
			UE_LOG(LogOGStaticImport, Error, TEXT("[StaticImport] no world"));
			return;
		}
		int32 runs = 2;
		std::vector<UEStaticCategoryChannel> categoryChannels;
		for (const FString& arg : args)
		{
			if (arg.IsNumeric())
			{
				runs = FMath::Clamp(FCString::Atoi(*arg), 1, 16);
				continue;
			}
			FString categoryText;
			FString channelText;
			ECollisionChannel channel = ECollisionChannel::ECC_WorldStatic;
			if (!arg.Split(TEXT(":"), &categoryText, &channelText) || !categoryText.IsNumeric() || !parseChannel(channelText, channel))
			{
				UE_LOG(LogOGStaticImport, Error, TEXT("[StaticImport] bad argument '%s': expected <runs> or <category>:<channel>, e.g. 4:WorldStatic"), *arg);
				return;
			}
			categoryChannels.push_back({static_cast<uint32_t>(FCString::Atoi(*categoryText)), channel});
		}
		FString mappingText;
		if (categoryChannels.empty())
		{
			for (uint32_t channel = 0; channel < kCategoryBitCount; ++channel)
			{
				categoryChannels.push_back({channel, static_cast<ECollisionChannel>(channel)});
			}
			mappingText = TEXT("identity(category=channel)");
		}
		else
		{
			for (const UEStaticCategoryChannel& mapping : categoryChannels)
			{
				mappingText += FString::Printf(TEXT("%s%u:%s"), mappingText.IsEmpty() ? TEXT("") : TEXT(","), mapping.category,
					*StaticEnum<ECollisionChannel>()->GetNameStringByValue(mapping.channel));
			}
		}

		const UEStaticGeometryImporter importer(categoryChannels);
		uint64_t firstChecksum = 0;
		uint32_t firstShapeCount = 0;
		bool identical = true;
		for (int32 run = 0; run < runs; ++run)
		{
			const UEStaticImportResult result = importer.importWorld(*world);
			UEStaticGeometryImporter::logReport(result.report, *FString::Printf(TEXT("run=%d/%d"), run + 1, runs));
			if (run == 0)
			{
				firstChecksum = result.report.checksum;
				firstShapeCount = result.report.shapeCount();
			}
			else
			{
				identical = identical && result.report.checksum == firstChecksum && result.report.shapeCount() == firstShapeCount;
			}
		}
		UE_LOG(LogOGStaticImport, Log, TEXT("[StaticImport] repeat runs=%d identical=%s checksum=0x%016llx world=%s mapping=%s"),
			runs, identical ? TEXT("yes") : TEXT("NO"), static_cast<unsigned long long>(firstChecksum),
			*UWorld::RemovePIEPrefix(world->GetPathName()), *mappingText);
		if (!identical)
		{
			UE_LOG(LogOGStaticImport, Error, TEXT("[StaticImport] repeated imports of one world differ"));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GImportStaticGeometryCommand(
		TEXT("og.sim.ImportStaticGeometry"),
		TEXT("Imports the world's static collision into a StaticWorldDescription and logs per-type counts and a checksum. ")
		TEXT("Args: [runs (default 2)] [category:channel ...] (default: identity, category N = ECollisionChannel N)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&runImportCommand));
} // namespace
