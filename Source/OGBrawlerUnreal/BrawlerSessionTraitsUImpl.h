// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerSessionTraitsUImpl-rationale.md · docs/BrawlerSessionTraitsUImpl-guards.md

#pragma once

#include "OGBrawler/SimulatableBrawlerTypes.h"

class SimulatableBrawler;
class USimmableUpdateComponent;
class AOGBrawlerUECharacter;
class ASimulationManagerUImpl;
class UOGBrawlerInputCollectionComponent;
class BrawlerSessionGameUImpl;

struct BrawlerSessionTraitsUImpl
{
	using Simulatable       = SimulatableBrawler;
	using PlayerInput       = simulatableBrawler::PlayerInput;
	using State             = simulatableBrawler::State;
	using Component         = USimmableUpdateComponent;
	using Character         = AOGBrawlerUECharacter;
	using SessionOwner      = ASimulationManagerUImpl;
	using Game              = BrawlerSessionGameUImpl;
	using InputSampleSource = const UOGBrawlerInputCollectionComponent*;
};
