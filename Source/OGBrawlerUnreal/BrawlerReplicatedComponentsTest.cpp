// SPDX-License-Identifier: BUSL-1.1

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/ActorComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBrawlerReplicatedComponentsTest,
    "OGBrawler.Replication.CharacterReplicatedComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBrawlerReplicatedComponentsTest::RunTest(const FString& Parameters)
{
    TArray<UObject*> subobjects;
    GetMutableDefault<AOGBrawlerUECharacter>()->GetDefaultSubobjects(subobjects);

    TArray<FString> replicated;
    for (const UObject* subobject : subobjects)
    {
        const UActorComponent* component = Cast<UActorComponent>(subobject);
        if (component != nullptr && component->GetIsReplicated())
        {
            replicated.Add(FString::Printf(TEXT("%s%s"),
                component->GetClass()->GetPrefixCPP(), *component->GetClass()->GetName()));
        }
    }
    replicated.Sort();

    const FString listed = FString::Printf(TEXT("{%s}"), *FString::Join(replicated, TEXT(", ")));
    AddInfo(FString::Printf(TEXT("[ReplicatedComponents] %s"), *listed));
    TestEqual(TEXT("the character default object's replicated components"),
        listed, FString(TEXT("{USimulatableNetSyncComponent}")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
