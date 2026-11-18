// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerSimNetLanesUImpl-rationale.md

#include "OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h"
#include "OGBrawlerUnreal/OGBrawlerSimulationManager.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGNetUnreal/OGNetSimLanes.h"
#include "OGSimulationUnreal/UESimNetLanes.h"
#include "Templates/UniquePtr.h"

TUniquePtr<IOGNetSimLanes> makeBrawlerSimNetLanes(UOGNetDriver& driver, bool bIsServer)
{
	static_assert(std::is_same_v<decltype(&makeBrawlerSimNetLanes), FOGNetSimLanesFactory>,
		"makeBrawlerSimNetLanes is the game's FOGNetSimLanesFactory: UOGNetDriver::SetSimLanesFactory takes it, and InitBase "
		"calls it once per og-net driver with the driver's role");
	return MakeUnique<UESimNetLanes<BrawlerSessionTraitsUImpl>>(&driver, bIsServer);
}
