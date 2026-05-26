#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include "OGSimulation/InputMapping/InputActionDescriptor.h"
#include <unordered_map>

class UObject;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

class OGSIMULATIONUNREAL_API InputMappingUETranslator
{
public:
	void initialize(UObject* outer, const dInput::MappingContext& context);
	void addToSubsystem(UEnhancedInputLocalPlayerSubsystem* subsystem, int32_t priority = 0);
	UInputAction* getAction(const dInput::ActionDescriptor& descriptor) const;

	static struct FKey toUnrealKey(dInput::KeyId keyId);

private:
	UInputMappingContext* m_mappingContext = nullptr;
	std::unordered_map<const dInput::ActionDescriptor*, UInputAction*> m_actionMap;
};
