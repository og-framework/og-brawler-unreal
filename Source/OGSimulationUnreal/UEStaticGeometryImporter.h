#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/UEStaticGeometryImporter-rationale.md · docs/UEStaticGeometryImporter-guards.md

#include "OGSimulationUnreal.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "Engine/EngineTypes.h"
#include "OGSimulation/StaticGeometry.h"

class UWorld;

OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGStaticImport, Log, All);

struct OGSIMULATIONUNREAL_API UEStaticCategoryChannel
{
	uint32_t category = 0;
	ECollisionChannel channel = ECollisionChannel::ECC_WorldStatic;
};

struct OGSIMULATIONUNREAL_API UEStaticImportReport
{
	std::array<uint32_t, kStaticShapeTypeCount> shapeCountByType{};
	uint32_t componentsImported = 0;
	uint32_t instancesImported = 0;
	uint32_t landscapeUnsupported = 0;
	uint32_t skinnedUnsupported = 0;
	uint32_t movableSkipped = 0;
	uint32_t simulatingSkipped = 0;
	uint32_t noBodySetup = 0;
	uint32_t elementsCollisionDisabled = 0;
	uint32_t elementsUnmappedObjectType = 0;
	uint32_t elementsPhysicsOnly = 0;
	uint32_t convexWithoutChaosMesh = 0;
	uint32_t nonUniformSphereOrCapsule = 0;
	uint32_t duplicateKeys = 0;
	uint64_t checksum = 0;
	double importSeconds = 0.0;

	template <typename Shape>
	uint32_t countOf() const { return shapeCountByType[kStaticShapeIndex<Shape>]; }

	uint32_t shapeCount() const;
};

struct OGSIMULATIONUNREAL_API UEStaticImportResult
{
	StaticWorldDescription description;
	UEStaticImportReport report;
};

class OGSIMULATIONUNREAL_API UEStaticGeometryImporter
{
public:
	explicit UEStaticGeometryImporter(std::initializer_list<UEStaticCategoryChannel> categoryChannels);
	explicit UEStaticGeometryImporter(const std::vector<UEStaticCategoryChannel>& categoryChannels);

	UEStaticImportResult importWorld(UWorld& world) const;

	static uint64_t checksum(const StaticWorldDescription& description);
	static void logReport(const UEStaticImportReport& report, const TCHAR* label);

private:
	void addMapping(const UEStaticCategoryChannel& mapping);

	std::vector<UEStaticCategoryChannel> m_categoryChannels;
	std::array<CollisionCategories, ECollisionChannel::ECC_MAX> m_channelToCategory{};
};
