// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerPlayerStartUImpl-rationale.md

#pragma once

#include "CoreMinimal.h"

class AActor;
class AController;
class AGameModeBase;

namespace brawlerPlayerStartUImpl
{
AActor* choosePlayerStart(AGameModeBase& gameMode, AController* player);
}
