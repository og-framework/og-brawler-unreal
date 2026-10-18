// SPDX-License-Identifier: MPL-2.0
// docs/JoltEngineTraitsUImpl-rationale.md · docs/JoltEngineTraitsUImpl-guards.md

#include "JoltEngineTraitsUImpl.h"

#include "OGSimulationUnreal/OGSimulationUnreal.h"

void JoltEngineTraitsUImpl::logEngineMessage(const char* message)
{
	const FString line(UTF8_TO_TCHAR(message));
	if (line.StartsWith(TEXT("[Warning]")))
	{
		UE_LOG(LogOGSimHost, Warning, TEXT("%s"), *line);
	}
	else
	{
		UE_LOG(LogOGSimHost, Log, TEXT("%s"), *line);
	}
}
