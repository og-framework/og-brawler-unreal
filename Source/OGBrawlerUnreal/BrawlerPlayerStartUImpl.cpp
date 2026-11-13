// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerPlayerStartUImpl-rationale.md

#include "OGBrawlerUnreal/BrawlerPlayerStartUImpl.h"

#include "OGBrawler/BrawlerRingoutSimulation.h"
#include "OGSimulationUnreal/UEFrameConversion.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"

#include "Engine/PlayerStartPIE.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"

AActor* brawlerPlayerStartUImpl::choosePlayerStart(AGameModeBase& gameMode, AController* player)
{
	UWorld* world = gameMode.GetWorld();
	if (world == nullptr)
		return nullptr;

	UClass* pawnClass = gameMode.GetDefaultPawnClassForController(player);
	const APawn* pawnToFit = pawnClass != nullptr ? pawnClass->GetDefaultObject<APawn>() : nullptr;
	const float killPlaneHeight = brawlerRingout::StaticData{}.killPlaneHeight;

	TArray<APlayerStart*> unoccupiedStarts;
	TArray<APlayerStart*> occupiedStarts;
	int32 rejectedCount = 0;
	for (TActorIterator<APlayerStart> it(world); it; ++it)
	{
		APlayerStart* playerStart = *it;
		if (playerStart->IsA<APlayerStartPIE>())
			return playerStart;

		FVector location = playerStart->GetActorLocation();
		if (!brawlerRingout::spawnPointClearsKillPlane(ueFrame::vectorToSim(location), killPlaneHeight))
		{
			++rejectedCount;
			continue;
		}

		const FRotator rotation = playerStart->GetActorRotation();
		if (!world->EncroachingBlockingGeometry(pawnToFit, location, rotation))
			unoccupiedStarts.Add(playerStart);
		else if (world->FindTeleportSpot(pawnToFit, location, rotation))
			occupiedStarts.Add(playerStart);
	}

	if (unoccupiedStarts.Num() > 0)
		return unoccupiedStarts[FMath::RandRange(0, unoccupiedStarts.Num() - 1)];
	if (occupiedStarts.Num() > 0)
		return occupiedStarts[FMath::RandRange(0, occupiedStarts.Num() - 1)];

	UE_LOG(LogOGMgmt, Warning,
		TEXT("[Ringout.playerStart] no APlayerStart clears the kill plane (%.4f m) plus the %.2f m margin ")
		TEXT("(%d rejected); the engine's own choice spawns this pawn"),
		killPlaneHeight, brawlerRingout::kSpawnPointKillPlaneMargin, rejectedCount);
	return nullptr;
}
