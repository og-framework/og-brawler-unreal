#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogJoltPhysics, Log, All);

class JOLTPHYSICSMODULE_API FJoltPhysicsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
