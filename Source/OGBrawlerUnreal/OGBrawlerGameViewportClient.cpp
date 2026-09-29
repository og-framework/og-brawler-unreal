// SPDX-License-Identifier: BUSL-1.1
// docs/JoinScreen-rationale.md · docs/JoinScreen-guards.md

#include "OGBrawlerUnreal/OGBrawlerGameViewportClient.h"

#include "Engine/Console.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"

#include "OGBrawlerUnreal/OGBrawlerJoinSessionSubsystem.h"

UOGBrawlerJoinSessionSubsystem* UOGBrawlerGameViewportClient::joinSessionTakingInput() const
{
	// ⛔G-05  docs/JoinScreen-guards.md
	if (ViewportConsole != nullptr && ViewportConsole->ConsoleActive())
		return nullptr;

	return UOGBrawlerJoinSessionSubsystem::forShownFrontEnd(GetWorld());
}

bool UOGBrawlerGameViewportClient::InputKey(const FInputKeyEventArgs& eventArgs)
{
	if (eventArgs.Event == IE_Pressed || eventArgs.Event == IE_Repeat)
	{
		if (UOGBrawlerJoinSessionSubsystem* session = joinSessionTakingInput())
		{
			const bool controlDown = eventArgs.Viewport != nullptr
				&& (eventArgs.Viewport->KeyState(EKeys::LeftControl) || eventArgs.Viewport->KeyState(EKeys::RightControl));

			if (session->handleKey(eventArgs.Key, controlDown, *GetWorld()))
				return true;
		}
	}

	return Super::InputKey(eventArgs);
}

bool UOGBrawlerGameViewportClient::InputChar(FViewport* viewport, int32 controllerId, TCHAR character)
{
	if (UOGBrawlerJoinSessionSubsystem* session = joinSessionTakingInput())
	{
		if (session->handleCharacter(character))
			return true;
	}

	return Super::InputChar(viewport, controllerId, character);
}
