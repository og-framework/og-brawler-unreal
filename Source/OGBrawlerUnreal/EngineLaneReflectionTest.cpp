// SPDX-License-Identifier: BUSL-1.1
// docs/EngineLaneReflectionTest-rationale.md

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEngineLaneReflectionTest,
    "OGBrawler.Replication.EngineLaneMembers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace engineLaneReflectionTest
{
    const TCHAR* const kGameRoot = TEXT("/Game");
    const TCHAR* const kScriptPackages[] = {
        TEXT("/Script/OGBrawlerUnreal"),
        TEXT("/Script/OGSimulationUnreal"),
        TEXT("/Script/OGSimulationJoltUnreal"),
    };

    bool isUnderGame(const FString& packageName)
    {
        return packageName == kGameRoot || packageName.StartsWith(FString(kGameRoot) + TEXT("/"));
    }

    bool isInScope(const UClass* cls)
    {
        const FString name = cls->GetName();
        if (name.StartsWith(TEXT("SKEL_")) || name.StartsWith(TEXT("REINST_")))
        {
            return false;
        }
        const FString packageName = cls->GetOutermost()->GetName();
        for (const TCHAR* scriptPackage : kScriptPackages)
        {
            if (packageName == scriptPackage)
            {
                return true;
            }
        }
        return isUnderGame(packageName);
    }

    FString classKeyName(const UClass* cls)
    {
        return FString::Printf(TEXT("%s%s"), cls->GetPrefixCPP(), *cls->GetName());
    }

    FString propertyType(const FProperty* property)
    {
        FString extended;
        const FString type = property->GetCPPType(&extended);
        return type + extended;
    }

    FString rpcParameterTypes(const UFunction* function)
    {
        TArray<FString> types;
        for (TFieldIterator<FProperty> it(function); it && it->HasAnyPropertyFlags(CPF_Parm); ++it)
        {
            if (it->HasAnyPropertyFlags(CPF_ReturnParm))
            {
                continue;
            }
            FString type = propertyType(*it);
            if (it->HasAnyPropertyFlags(CPF_ConstParm))
            {
                type = TEXT("const ") + type;
            }
            if (it->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_OutParm))
            {
                type += TEXT("&");
            }
            types.Add(type);
        }
        return types.IsEmpty() ? FString(TEXT("-")) : FString::Join(types, TEXT(", "));
    }

    bool replicatesByDefault(const UClass* cls)
    {
        const UObject* defaultObject = cls->GetDefaultObject();
        if (const AActor* actor = Cast<AActor>(defaultObject))
        {
            return actor->GetIsReplicated();
        }
        if (const UActorComponent* component = Cast<UActorComponent>(defaultObject))
        {
            return component->GetIsReplicated();
        }
        return false;
    }
} // namespace engineLaneReflectionTest

bool FEngineLaneReflectionTest::RunTest(const FString& Parameters)
{
    using namespace engineLaneReflectionTest;

    IAssetRegistry& registry = IAssetRegistry::GetChecked();
    registry.ScanPathsSynchronous({ FString(kGameRoot) }, /*bForceRescan*/ false);
    TArray<FAssetData> blueprintAssets;
    registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), blueprintAssets, /*bSearchSubClasses*/ true);
    blueprintAssets.Sort([](const FAssetData& a, const FAssetData& b) { return a.PackageName.LexicalLess(b.PackageName); });

    int32 loadedBlueprints = 0;
    for (const FAssetData& asset : blueprintAssets)
    {
        const FString packageName = asset.PackageName.ToString();
        if (!isUnderGame(packageName))
        {
            continue;
        }
        const UBlueprint* blueprint = Cast<UBlueprint>(asset.GetAsset());
        const UClass* generated = blueprint != nullptr ? blueprint->GeneratedClass.Get() : nullptr;
        if (generated == nullptr)
        {
            AddError(FString::Printf(TEXT("[EngineLane.Blueprint] %s has no generated class after loading"), *packageName));
            continue;
        }
        ++loadedBlueprints;
        AddInfo(FString::Printf(TEXT("[EngineLane.Blueprint] %s %s"), *packageName, *classKeyName(generated)));
    }

    TArray<FString> lines;
    for (TObjectIterator<UClass> it; it; ++it)
    {
        const UClass* cls = *it;
        if (!isInScope(cls))
        {
            continue;
        }
        const FString name = classKeyName(cls);
        for (TFieldIterator<FProperty> property(cls, EFieldIteratorFlags::ExcludeSuper); property; ++property)
        {
            if (property->HasAnyPropertyFlags(CPF_Net))
            {
                lines.Add(FString::Printf(TEXT("Property %s %s %s"), *name, *property->GetName(), *propertyType(*property)));
            }
        }
        for (TFieldIterator<UFunction> function(cls, EFieldIteratorFlags::ExcludeSuper); function; ++function)
        {
            if (function->HasAnyFunctionFlags(FUNC_Net))
            {
                lines.Add(FString::Printf(TEXT("Rpc %s %s %s"), *name, *function->GetName(), *rpcParameterTypes(*function)));
            }
        }
        if (replicatesByDefault(cls))
        {
            lines.Add(FString::Printf(TEXT("ReplicatedClass %s * -"), *name));
        }
    }
    lines.Sort();

    for (const FString& line : lines)
    {
        AddInfo(FString::Printf(TEXT("[EngineLane.Reflected] %s"), *line));
    }
    AddInfo(FString::Printf(TEXT("[EngineLane.Summary] blueprints=%d reflected=%d"), loadedBlueprints, lines.Num()));

    const bool namesTheCharacter = lines.ContainsByPredicate(
        [](const FString& line) { return line.Contains(TEXT(" AOGBrawlerUECharacter ")); });
    TestTrue(TEXT("[EngineLane.SelfCheck] a reflected line names AOGBrawlerUECharacter"), namesTheCharacter);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
