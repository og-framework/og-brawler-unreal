// SPDX-License-Identifier: MPL-2.0

#include "JoltPhysicsModule.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>

DEFINE_LOG_CATEGORY(LogJoltPhysics);

// Jolt trace callback — forward to UE_LOG
static void JoltTraceImpl(const char* inFMT, ...)
{
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	FCStringAnsi::GetVarArgs(buffer, sizeof(buffer), inFMT, list);
	va_end(list);
	UE_LOG(LogJoltPhysics, Log, TEXT("%hs"), buffer);
}

#ifdef JPH_ENABLE_ASSERTS
// Jolt assert callback — forward to UE check
static bool JoltAssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
{
	UE_LOG(LogJoltPhysics, Error, TEXT("Jolt Assert Failed: %hs (%hs) at %hs:%u"), inExpression, inMessage ? inMessage : "", inFile, inLine);
	check(false);
	return true; // break in debugger
}
#endif

void FJoltPhysicsModule::StartupModule()
{
	JPH::RegisterDefaultAllocator();

	JPH::Trace = JoltTraceImpl;
#ifdef JPH_ENABLE_ASSERTS
	JPH::AssertFailed = JoltAssertFailedImpl;
#endif

	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	UE_LOG(LogJoltPhysics, Log, TEXT("JoltPhysicsModule initialized (Jolt v%d.%d.%d)"), JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH);
}

void FJoltPhysicsModule::ShutdownModule()
{
	JPH::UnregisterTypes();

	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;

	UE_LOG(LogJoltPhysics, Log, TEXT("JoltPhysicsModule shut down"));
}

IMPLEMENT_MODULE(FJoltPhysicsModule, JoltPhysicsModule);
