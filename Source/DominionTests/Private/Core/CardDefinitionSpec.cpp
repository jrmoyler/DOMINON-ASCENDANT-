#include "Content/DACardDefinition.h"
#include "Content/DAContentManifest.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDACardDefinitionSpec, "Dominion.Core.CardDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDACardDefinitionSpec)

void FDACardDefinitionSpec::Define()
{
    It("rejects a placeable card without a world prefab", [this]()
    {
        UDA_CardDefinition* Def = NewObject<UDA_CardDefinition>();
        Def->DefinitionId = FName("synara.adaptive_habitat");
        Def->CardType = EDACardType::Residential;
        Def->Footprint = FIntPoint(2, 2);
        Def->bPlaceable = true;

        TArray<FText> Errors;
        TestFalse("Invalid definition", Def->Validate(Errors));
        TestTrue("Missing prefab reported", Errors.Num() > 0);
    });

    It("rejects an authored non-positive construction-cycle count in a manifest fixture", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        TArray<FText> Errors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, Errors));
        FDAManifestCardDefinition* Guardian = Manifest.Definitions.FindByPredicate([](const FDAManifestCardDefinition& Definition)
        {
            return Definition.DefinitionId == TEXT("synara.guardian_drone_cohort");
        });
        TestNotNull("Authored-cycle fixture exists", Guardian);
        if (Guardian)
        {
            Guardian->ConstructionCycles = 0;
            TestFalse("Invalid construction cycle count", FDAContentManifestPipeline::ValidateManifest(Manifest, Errors));
        }
    });

    It("does not expose an unauthored construction default to runtime", [this]()
    {
        UDA_CardDefinition* Def = NewObject<UDA_CardDefinition>();
        int32 ConstructionCycles = 77;
        TestFalse("Unauthored construction cycles unavailable", Def->TryGetConstructionCycles(ConstructionCycles));
        TestEqual("Failed read does not publish a default", ConstructionCycles, 77);
    });

    It("does not expose absent combat defaults to runtime", [this]()
    {
        UDA_CardDefinition* Def = NewObject<UDA_CardDefinition>();
        FDACombatDefinition Combat;
        Combat.StructuralIntegrity = 77.f;
        TestFalse("Unauthored combat unavailable", Def->TryGetCombatDefinition(Combat));
        TestEqual("Failed read does not publish combat defaults", Combat.StructuralIntegrity, 77.f);
    });
}
