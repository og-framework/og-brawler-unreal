#pragma once

// SPDX-License-Identifier: BUSL-1.1

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPMS, Log, All);

class FProceduralMountainSideModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
