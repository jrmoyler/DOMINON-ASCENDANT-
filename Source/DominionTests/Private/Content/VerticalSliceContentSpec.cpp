#include "Cards/DADeckRules.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentManifest.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Content/DAStarterDeckDefinition.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDAVerticalSliceContentSpec, "Dominion.Content.VerticalSlice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAVerticalSliceContentSpec)

namespace
{
    const TMap<EDAContentFaction, int32> ExpectedFactionCounts = {
        { EDAContentFaction::Synara, 15 },
        { EDAContentFaction::Forgeweave, 15 },
        { EDAContentFaction::EdenCircuit, 15 },
        { EDAContentFaction::Universal, 17 },
        { EDAContentFaction::Fusion, 1 },
        { EDAContentFaction::Special, 1 }
    };

    const TArray<FString> ExpectedDefinitionNames = {
        TEXT("Adaptive Habitat"), TEXT("Civic Autonomy Pods"), TEXT("Cognitive Operations Tower"),
        TEXT("Predictive Bureau"), TEXT("Autonomous Exchange"), TEXT("Algorithmic Market"),
        TEXT("Synthetic Fabrication Node"), TEXT("Swarm Foundry"), TEXT("Neural Relay"),
        TEXT("Orchestration Hub"), TEXT("Guardian Drone Cohort"), TEXT("Audit Sentinel"),
        TEXT("Agency Forum"), TEXT("Archon Mira Vey"), TEXT("The Thinking Spire"),
        TEXT("Worker Arcology"), TEXT("Forge Quarters"), TEXT("Production Directorate"),
        TEXT("Industrial Design Bureau"), TEXT("Industrial Exchange"), TEXT("Machine Parts Market"),
        TEXT("Infinite Foundry"), TEXT("Replication Forge"), TEXT("Freight Furnace"),
        TEXT("Smog Reclaimer"), TEXT("Forge Guard"), TEXT("Mechanist Crew"), TEXT("Workers' Canteen"),
        TEXT("Forge Lord Daxton Rhe"), TEXT("The Grand Forge"), TEXT("Garden Commune"),
        TEXT("Canopy Habitat"), TEXT("Ecological Design House"), TEXT("Restoration Bureau"),
        TEXT("Harvest Market"), TEXT("Seed Exchange"), TEXT("Regenerative Bioworks"),
        TEXT("Mycelium Works"), TEXT("Living Waterway"), TEXT("Pollinator Corridor"),
        TEXT("Ranger Circle"), TEXT("Symbiosis Keepers"), TEXT("Balance Grove"),
        TEXT("Caretaker Amara Venn"), TEXT("The Worldgarden"), TEXT("Compact Residence"),
        TEXT("Civic Apartments"), TEXT("Corner Exchange"), TEXT("District Market"),
        TEXT("Field Workshop"), TEXT("Utility Fabricator"), TEXT("Administrative Office"),
        TEXT("Research Annex"), TEXT("Public Clinic"), TEXT("Community Plaza"),
        TEXT("Microgrid Station"), TEXT("Water Reclaimer"), TEXT("Transit Stop"), TEXT("Warehouse"),
        TEXT("Founder Monument"), TEXT("Watch Post"), TEXT("Barrier Hub"),
        TEXT("Autonomous Factory"), TEXT("Founder Hall")
    };

    const TMap<FName, int32> ExpectedStarterQuantities = {
        { TEXT("synara.adaptive_habitat"), 3 }, { TEXT("synara.civic_autonomy_pods"), 3 },
        { TEXT("universal.compact_residence"), 3 }, { TEXT("universal.civic_apartments"), 3 },
        { TEXT("synara.autonomous_exchange"), 3 }, { TEXT("synara.algorithmic_market"), 2 },
        { TEXT("universal.corner_exchange"), 2 }, { TEXT("universal.district_market"), 2 },
        { TEXT("synara.cognitive_operations_tower"), 3 }, { TEXT("synara.predictive_bureau"), 2 },
        { TEXT("universal.research_annex"), 2 }, { TEXT("universal.administrative_office"), 1 },
        { TEXT("synara.synthetic_fabrication_node"), 3 }, { TEXT("synara.swarm_foundry"), 2 },
        { TEXT("universal.field_workshop"), 1 }, { TEXT("universal.utility_fabricator"), 1 },
        { TEXT("synara.neural_relay"), 3 }, { TEXT("synara.orchestration_hub"), 2 },
        { TEXT("universal.microgrid_station"), 2 }, { TEXT("universal.water_reclaimer"), 1 },
        { TEXT("universal.transit_stop"), 1 }, { TEXT("universal.warehouse"), 1 },
        { TEXT("synara.agency_forum"), 3 }, { TEXT("universal.community_plaza"), 1 },
        { TEXT("universal.public_clinic"), 1 }, { TEXT("universal.founder_monument"), 1 },
        { TEXT("synara.guardian_drone_cohort"), 2 }, { TEXT("synara.audit_sentinel"), 2 },
        { TEXT("universal.watch_post"), 1 }, { TEXT("universal.barrier_hub"), 1 },
        { TEXT("synara.archon_mira_vey"), 1 }, { TEXT("synara.the_thinking_spire"), 1 }
    };
}

void FDAVerticalSliceContentSpec::Define()
{
    It("loads the one canonical manifest with the frozen 15/15/15/17/1/1 names", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        TArray<FText> Errors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, Errors));
        TestEqual("No parse or schema errors", Errors.Num(), 0);
        TestEqual("Frozen definition count", Manifest.Definitions.Num(), 64);

        TMap<EDAContentFaction, int32> ActualCounts;
        TArray<FString> ActualNames;
        for (const FDAManifestCardDefinition& Definition : Manifest.Definitions)
        {
            ++ActualCounts.FindOrAdd(Definition.Faction);
            ActualNames.Add(Definition.DisplayName);
        }
        TestTrue("Exact faction counts", ActualCounts.OrderIndependentCompareEqual(ExpectedFactionCounts));
        TestTrue("Exact definition names and order", ActualNames == ExpectedDefinitionNames);
    });

    It("builds the exact v1.1 Section 22 starter composition as 60 deterministic owned instances", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        TArray<FText> Errors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, Errors));

        FDABuiltManifestContent First;
        FDABuiltManifestContent Second;
        TestTrue("First content factory succeeds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, First, Errors));
        TestTrue("Second content factory succeeds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Second, Errors));

        TMap<FName, int32> ActualQuantities;
        for (const TPair<FGuid, FCardInstance>& Pair : First.Collection.Instances)
        {
            ++ActualQuantities.FindOrAdd(Pair.Value.DefinitionId);
        }
        TestEqual("Starter collection instance count", First.Collection.Instances.Num(), 60);
        TestEqual("Starter deck instance count", First.Deck.GetInstanceIds().Num(), 60);
        TestTrue("Every literal Section 22 quantity matches", ActualQuantities.OrderIndependentCompareEqual(ExpectedStarterQuantities));
        TestTrue("Factory instance IDs are deterministic", First.Deck.GetInstanceIds() == Second.Deck.GetInstanceIds());
        TestFalse("Founder Hall is not in the starter deck", ActualQuantities.Contains(TEXT("special.founder_hall")));
        TestNotNull("Starter deck Data Asset source is built", First.DeckAsset);
        TestEqual("Starter deck asset has 32 quantity rows", First.DeckAsset->Entries.Num(), 32);
        TestEqual("Starter deck asset quantity sum", First.DeckAsset->GetInstanceCount(), 60);
        TestEqual("Fusion package path",
            FDAContentManifestPipeline::GetGeneratedPackageName(*First.FindDefinition(TEXT("fusion.autonomous_factory"))),
            FString(TEXT("/Game/DA/Cards/Fusion/DA_Card_AutonomousFactory")));
        TestEqual("Founder Hall package path",
            FDAContentManifestPipeline::GetGeneratedPackageName(*First.FindDefinition(TEXT("special.founder_hall"))),
            FString(TEXT("/Game/DA/Buildings/DA_FounderHall")));
        TSet<FString> GeneratedPackages;
        for (const UDA_CardDefinition* Definition : First.Definitions)
        {
            GeneratedPackages.Add(FDAContentManifestPipeline::GetGeneratedPackageName(*Definition));
        }
        TestEqual("Every definition has a unique generated package", GeneratedPackages.Num(), 64);
    });

    It("maps authored v0.8 values without filling unauthored per-card costs", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        FDABuiltManifestContent Built;
        TArray<FText> Errors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, Errors));
        TestTrue("Content factory succeeds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Built, Errors));

        const UDA_CardDefinition* Adaptive = Built.FindDefinition(TEXT("synara.adaptive_habitat"));
        const UDA_CardDefinition* AutonomousExchange = Built.FindDefinition(TEXT("synara.autonomous_exchange"));
        const UDA_CardDefinition* Guardian = Built.FindDefinition(TEXT("synara.guardian_drone_cohort"));
        const UDA_CardDefinition* AgencyForum = Built.FindDefinition(TEXT("synara.agency_forum"));
        const UDA_CardDefinition* FounderHall = Built.FindDefinition(TEXT("special.founder_hall"));
        const UDA_CardDefinition* AutonomousFactory = Built.FindDefinition(TEXT("fusion.autonomous_factory"));
        const UDA_CardDefinition* ThinkingSpire = Built.FindDefinition(TEXT("synara.the_thinking_spire"));
        const UDA_CardDefinition* GrandForge = Built.FindDefinition(TEXT("forgeweave.the_grand_forge"));
        const UDA_CardDefinition* Worldgarden = Built.FindDefinition(TEXT("eden.the_worldgarden"));
        const UDA_CardDefinition* Unaudited = Built.FindDefinition(TEXT("synara.predictive_bureau"));
        TestNotNull("Adaptive Habitat mapped", Adaptive);
        int32 IntValue = 0;
        float FloatValue = 0.f;
        TestTrue("Adaptive deploy capital authored", Adaptive->TryGetDeploymentCapital(IntValue));
        TestEqual("Adaptive deploy capital", IntValue, 8);
        TestTrue("Adaptive craft capital authored", Adaptive->TryGetCraftCapital(IntValue));
        TestEqual("Adaptive craft capital", IntValue, 4);
        TestTrue("Adaptive maintenance authored", Adaptive->TryGetMaintenanceCapitalPerCycle(FloatValue));
        TestEqual("Adaptive maintenance", FloatValue, 0.04f);
        TestTrue("Autonomous Exchange pressure authored", AutonomousExchange->TryGetSynaraDependencyPerCycle(FloatValue));
        TestEqual("Autonomous Exchange pressure", FloatValue, 0.10f);
        TestTrue("Guardian craft capital authored", Guardian->TryGetCraftCapital(IntValue));
        TestEqual("Guardian craft capital", IntValue, 8);
        TestTrue("Guardian craft insight authored", Guardian->TryGetCraftInsight(IntValue));
        TestEqual("Guardian craft insight", IntValue, 2);
        TestTrue("Guardian production throughput authored", Guardian->TryGetCraftProductionThroughput(IntValue));
        TestEqual("Guardian production throughput", IntValue, 5);
        FName FacilityId;
        TestTrue("Guardian facility authored", Guardian->TryGetRequiredCraftingFacilityId(FacilityId));
        TestEqual("Guardian facility", FacilityId, FName(TEXT("synara.synthetic_fabrication_node")));
        TestTrue("Guardian craft cycles authored", Guardian->TryGetConstructionCycles(IntValue));
        TestEqual("Guardian craft cycles", IntValue, 2);
        TestTrue("Agency Forum pressure authored", AgencyForum->TryGetSynaraDependencyPerCycle(FloatValue));
        TestEqual("Agency Forum pressure", FloatValue, -0.20f);
        TestTrue("Founder Hall capital output authored", FounderHall->TryGetBaseCapitalPerCycle(FloatValue));
        TestEqual("Founder Hall capital per cycle", FloatValue, 1.f);
        TestTrue("Founder Hall insight output authored", FounderHall->TryGetBaseInsightPerCycle(FloatValue));
        TestEqual("Founder Hall insight per cycle", FloatValue, 0.15f);
        TestTrue("Founder Hall influence output authored", FounderHall->TryGetBaseInfluencePerCycle(FloatValue));
        TestEqual("Founder Hall influence per cycle", FloatValue, 0.10f);
        TestTrue("Founder Hall housing authored", FounderHall->TryGetHousingCapacity(IntValue));
        TestEqual("Founder Hall temporary housing", IntValue, 24);
        TestTrue("Autonomous Factory deployment insight authored", AutonomousFactory->TryGetDeploymentInsight(IntValue));
        TestEqual("Autonomous Factory deployment insight", IntValue, 24);
        TestTrue("Autonomous Factory Synara pressure authored", AutonomousFactory->TryGetSynaraDependencyPerCycle(FloatValue));
        TestEqual("Autonomous Factory Synara pressure", FloatValue, 0.20f);
        TestTrue("Autonomous Factory Forgeweave pressure authored", AutonomousFactory->TryGetForgeweaveResourceHungerPerCycle(FloatValue));
        TestEqual("Autonomous Factory Forgeweave pressure", FloatValue, 0.15f);
        TestTrue("Autonomous Factory workforce modifier authored", AutonomousFactory->TryGetWorkforceRequirementModifier(FloatValue));
        TestEqual("Autonomous Factory workforce reduction", FloatValue, -0.80f);
        TestTrue("Autonomous Factory throughput modifier authored", AutonomousFactory->TryGetIndustrialThroughputModifier(FloatValue));
        TestEqual("Autonomous Factory throughput modifier", FloatValue, 0.25f);
        TestTrue("Autonomous Factory adjacent speed authored", AutonomousFactory->TryGetAdjacentIndustrialConstructionSpeedModifier(FloatValue));
        TestEqual("Autonomous Factory adjacent speed", FloatValue, 0.15f);
        TestTrue("Autonomous Factory construction timing authored", AutonomousFactory->TryGetConstructionCycles(IntValue));
        TestEqual("Autonomous Factory construction cycles", IntValue, 8);
        TestTrue("Autonomous Factory high Power demand authored", AutonomousFactory->TryGetUtilityPower(IntValue));
        TestEqual("Autonomous Factory high Power demand", IntValue, 24);
        TestTrue("Autonomous Factory high Data demand authored", AutonomousFactory->TryGetUtilityData(IntValue));
        TestEqual("Autonomous Factory high Data demand", IntValue, 24);
        TestTrue("Autonomous Factory crafting capital authored", AutonomousFactory->TryGetCraftCapital(IntValue));
        TestEqual("Autonomous Factory crafting capital", IntValue, 80);
        TestTrue("Autonomous Factory crafting insight authored", AutonomousFactory->TryGetCraftInsight(IntValue));
        TestEqual("Autonomous Factory crafting insight", IntValue, 0);
        TestTrue("Autonomous Factory crafting facility authored", AutonomousFactory->TryGetRequiredCraftingFacilityId(FacilityId));
        TestEqual("Autonomous Factory crafting facility", FacilityId, FName(TEXT("forgeweave.replication_forge")));
        TestTrue("Thinking Spire capital authored", ThinkingSpire->TryGetDeploymentCapital(IntValue));
        TestEqual("Thinking Spire capital", IntValue, 220);
        TestTrue("Thinking Spire insight authored", ThinkingSpire->TryGetDeploymentInsight(IntValue));
        TestEqual("Thinking Spire insight", IntValue, 30);
        TestTrue("Thinking Spire influence authored", ThinkingSpire->TryGetDeploymentInfluence(IntValue));
        TestEqual("Thinking Spire influence", IntValue, 25);
        TestTrue("Thinking Spire pressure authored", ThinkingSpire->TryGetSynaraDependencyPerCycle(FloatValue));
        TestEqual("Thinking Spire pressure", FloatValue, 0.35f);
        TestTrue("Thinking Spire cycles authored", ThinkingSpire->TryGetConstructionCycles(IntValue));
        TestEqual("Thinking Spire cycles", IntValue, 12);
        TestTrue("Grand Forge influence authored", GrandForge->TryGetDeploymentInfluence(IntValue));
        TestEqual("Grand Forge influence", IntValue, 20);
        TestTrue("Grand Forge capital authored", GrandForge->TryGetDeploymentCapital(IntValue));
        TestEqual("Grand Forge capital", IntValue, 240);
        TestTrue("Grand Forge insight authored", GrandForge->TryGetDeploymentInsight(IntValue));
        TestEqual("Grand Forge insight", IntValue, 20);
        TestTrue("Grand Forge cycles authored", GrandForge->TryGetConstructionCycles(IntValue));
        TestEqual("Grand Forge cycles", IntValue, 14);
        TestTrue("Worldgarden capital authored", Worldgarden->TryGetDeploymentCapital(IntValue));
        TestEqual("Worldgarden capital", IntValue, 200);
        TestTrue("Worldgarden insight authored", Worldgarden->TryGetDeploymentInsight(IntValue));
        TestEqual("Worldgarden insight", IntValue, 28);
        TestTrue("Worldgarden influence authored", Worldgarden->TryGetDeploymentInfluence(IntValue));
        TestEqual("Worldgarden influence", IntValue, 30);
        TestTrue("Worldgarden cycles authored", Worldgarden->TryGetConstructionCycles(IntValue));
        TestEqual("Worldgarden cycles", IntValue, 14);
        TestFalse("Predictive Bureau deployment cost is unavailable", Unaudited->TryGetDeploymentCapital(IntValue));
        TestFalse("Predictive Bureau construction cycles are unavailable", Unaudited->TryGetConstructionCycles(IntValue));
        FDACombatDefinition Combat;
        Combat.Armor = 77.f;
        TestFalse("Unauthored combat is unavailable", Unaudited->TryGetCombatDefinition(Combat));
        TestEqual("Unavailable combat does not overwrite output", Combat.Armor, 77.f);
        TestEqual("Canonical tags are explicitly empty", Unaudited->Tags.Num(), 0);
        TestEqual("Canonical upgrade branches are explicitly empty", Unaudited->UpgradeBranchIds.Num(), 0);
    });

    It("rejects every supplied gameplay value category when its authored mask omits it", [this]()
    {
        FDAVerticalSliceContentManifest Canonical;
        TArray<FText> LoadErrors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Canonical, LoadErrors));

        const TArray<TPair<EDAAuthoredCardValue, TFunction<void(FDAManifestCardDefinition&)>>> Mutations = {
            { EDAAuthoredCardValue::DeploymentCapital, [](auto& D) { D.DeploymentCapital = 1; } },
            { EDAAuthoredCardValue::DeploymentInsight, [](auto& D) { D.DeploymentInsight = 1; } },
            { EDAAuthoredCardValue::DeploymentInfluence, [](auto& D) { D.DeploymentInfluence = 1; } },
            { EDAAuthoredCardValue::CraftCapital, [](auto& D) { D.CraftCapital = 1; } },
            { EDAAuthoredCardValue::CraftInsight, [](auto& D) { D.CraftInsight = 1; } },
            { EDAAuthoredCardValue::CraftProductionThroughput, [](auto& D) { D.CraftProductionThroughput = 1; } },
            { EDAAuthoredCardValue::RequiredCraftingFacilityId, [](auto& D) { D.RequiredCraftingFacilityId = TEXT("test.facility"); } },
            { EDAAuthoredCardValue::MaintenanceCapitalPerCycle, [](auto& D) { D.MaintenanceCapitalPerCycle = 1.f; } },
            { EDAAuthoredCardValue::BaseCapitalPerCycle, [](auto& D) { D.BaseCapitalPerCycle = 1.f; } },
            { EDAAuthoredCardValue::BaseInsightPerCycle, [](auto& D) { D.BaseInsightPerCycle = 1.f; } },
            { EDAAuthoredCardValue::BaseInfluencePerCycle, [](auto& D) { D.BaseInfluencePerCycle = 1.f; } },
            { EDAAuthoredCardValue::SynaraDependencyPerCycle, [](auto& D) { D.SynaraDependencyPerCycle = 1.f; } },
            { EDAAuthoredCardValue::ForgeweaveResourceHungerPerCycle, [](auto& D) { D.ForgeweaveResourceHungerPerCycle = 1.f; } },
            { EDAAuthoredCardValue::WorkforceRequirementModifier, [](auto& D) { D.WorkforceRequirementModifier = 1.f; } },
            { EDAAuthoredCardValue::IndustrialThroughputModifier, [](auto& D) { D.IndustrialThroughputModifier = 1.f; } },
            { EDAAuthoredCardValue::AdjacentIndustrialConstructionSpeedModifier, [](auto& D) { D.AdjacentIndustrialConstructionSpeedModifier = 1.f; } },
            { EDAAuthoredCardValue::ConstructionCycles, [](auto& D) { D.ConstructionCycles = 1; } },
            { EDAAuthoredCardValue::UtilityPower, [](auto& D) { D.UtilityPower = 1; } },
            { EDAAuthoredCardValue::UtilityWater, [](auto& D) { D.UtilityWater = 1; } },
            { EDAAuthoredCardValue::UtilityData, [](auto& D) { D.UtilityData = 1; } },
            { EDAAuthoredCardValue::HousingCapacity, [](auto& D) { D.HousingCapacity = 1; } }
        };
        TestEqual("All authored gameplay-value categories have an unauthored mutation", Mutations.Num(), 21);

        for (const auto& Mutation : Mutations)
        {
            FDAVerticalSliceContentManifest Corrupt = Canonical;
            FDAManifestCardDefinition& Definition = Corrupt.Definitions[3];
            Mutation.Value(Definition);
            Definition.ProvidedValueMask |= DAAuthoredValueBit(Mutation.Key);
            TArray<FText> Errors;
            TestFalse("Unmasked supplied value rejected", FDAContentManifestPipeline::ValidateManifest(Corrupt, Errors));
        }

        FDAVerticalSliceContentManifest AuthoredZero = Canonical;
        FDAManifestCardDefinition& Definition = AuthoredZero.Definitions[3];
        Definition.UtilityPower = 0;
        Definition.ProvidedValueMask |= DAAuthoredValueBit(EDAAuthoredCardValue::UtilityPower);
        Definition.AuthoredValueMask |= DAAuthoredValueBit(EDAAuthoredCardValue::UtilityPower);
        TArray<FText> ZeroErrors;
        TestTrue("Explicit authored zero remains valid", FDAContentManifestPipeline::ValidateManifest(AuthoredZero, ZeroErrors));
        FDABuiltManifestContent ZeroBuilt;
        TestTrue("Explicit authored zero maps", FDAContentManifestPipeline::BuildRuntimeContent(AuthoredZero, ZeroBuilt, ZeroErrors));
        int32 UtilityPower = 99;
        TestTrue("Runtime exposes authored zero", ZeroBuilt.Definitions[3]->TryGetUtilityPower(UtilityPower));
        TestEqual("Authored zero remains zero", UtilityPower, 0);
    });

    It("uses stable DefinitionId primary IDs for manifest and generated objects", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        FDABuiltManifestContent Built;
        TArray<FText> Errors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, Errors));
        TestTrue("Content factory succeeds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Built, Errors));
        UDA_CardDefinition* Adaptive = Built.FindDefinition(TEXT("synara.adaptive_habitat"));
        TestEqual("Primary ID uses stable definition ID", Adaptive->GetPrimaryAssetId(),
            FPrimaryAssetId(UDA_CardDefinition::AssetType, TEXT("synara.adaptive_habitat")));

        UDA_CardDefinition* GeneratedNamedObject = DuplicateObject<UDA_CardDefinition>(Adaptive, GetTransientPackage(), TEXT("DA_Card_AdaptiveHabitat"));
        TestEqual("Generated object name cannot alter primary identity", GeneratedNamedObject->GetPrimaryAssetId(), Adaptive->GetPrimaryAssetId());
    });

    It("uses one fingerprint for LF CRLF and bare-CR checkouts", [this]()
    {
        FString LfJson;
        TestTrue("Canonical JSON reads", FFileHelper::LoadFileToString(LfJson, *FDAContentManifestPipeline::GetCanonicalManifestPath()));
        LfJson.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
        LfJson.ReplaceInline(TEXT("\r"), TEXT("\n"));
        FString CrlfJson = LfJson.Replace(TEXT("\n"), TEXT("\r\n"));
        FString CrJson = LfJson.Replace(TEXT("\n"), TEXT("\r"));

        FDAVerticalSliceContentManifest Lf;
        FDAVerticalSliceContentManifest Crlf;
        FDAVerticalSliceContentManifest Cr;
        TArray<FText> Errors;
        TestTrue("LF parses", FDAContentManifestPipeline::ParseJson(LfJson, Lf, Errors));
        TestTrue("CRLF parses", FDAContentManifestPipeline::ParseJson(CrlfJson, Crlf, Errors));
        TestTrue("Bare CR parses", FDAContentManifestPipeline::ParseJson(CrJson, Cr, Errors));
        TestEqual("CRLF fingerprint equals LF", Crlf.SourceFingerprint, Lf.SourceFingerprint);
        TestEqual("Bare CR fingerprint equals LF", Cr.SourceFingerprint, Lf.SourceFingerprint);
        TestEqual("SHA-1 fingerprint has 40 hex characters", Lf.SourceFingerprint.Len(), 40);
    });

    It("accepts only an exact fingerprinted generated cache and falls back for partial or stale caches", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        FDABuiltManifestContent Built;
        TArray<FText> Errors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, Errors));
        TestTrue("Content factory succeeds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Built, Errors));
        TestFalse("Manifest fingerprint exists", Manifest.SourceFingerprint.IsEmpty());
        TestEqual("Deck cache fingerprint", Built.DeckAsset->SourceManifestFingerprint, Manifest.SourceFingerprint);
        UObject* GeneratedOuter = NewObject<UObject>(GetTransientPackage());
        TArray<UDA_CardDefinition*> GeneratedDefinitions;
        for (UDA_CardDefinition* Definition : Built.Definitions)
        {
            const FString AssetName = FPackageName::GetLongPackageAssetName(FDAContentManifestPipeline::GetGeneratedPackageName(*Definition));
            GeneratedDefinitions.Add(DuplicateObject<UDA_CardDefinition>(Definition, GeneratedOuter, FName(*AssetName)));
        }
        UDA_DeckDefinition* GeneratedDeck = DuplicateObject<UDA_DeckDefinition>(Built.DeckAsset, GeneratedOuter, TEXT("DA_Deck_SynaraStarter60"));
        TestTrue("Exact generated cache validates", FDAContentManifestPipeline::ValidateGeneratedCache(Manifest, GeneratedDefinitions, GeneratedDeck, Errors));
        TestEqual("Fallback and generated stable IDs are identical", GeneratedDefinitions[0]->GetPrimaryAssetId(), Built.Definitions[0]->GetPrimaryAssetId());
        UDAContentRegistrySubsystem* ExactRegistry = NewObject<UDAContentRegistrySubsystem>();
        bool bUsedGeneratedCache = false;
        TestTrue("Exact cache registry rebuild succeeds", ExactRegistry->RebuildFromGeneratedCache(GeneratedDefinitions, GeneratedDeck, bUsedGeneratedCache, Errors));
        TestTrue("Exact cache is used", bUsedGeneratedCache);
        TestEqual("Exact generated registry count", ExactRegistry->GetRegisteredCardCount(), 64);

        TArray<UDA_CardDefinition*> Partial = GeneratedDefinitions;
        Partial.Pop();
        UDAContentRegistrySubsystem* PartialRegistry = NewObject<UDAContentRegistrySubsystem>();
        bUsedGeneratedCache = true;
        TestTrue("Partial cache rebuild succeeds via manifest fallback", PartialRegistry->RebuildFromGeneratedCache(Partial, GeneratedDeck, bUsedGeneratedCache, Errors));
        TestFalse("Partial cache is not used", bUsedGeneratedCache);
        TestEqual("Fallback restores all definitions", PartialRegistry->GetRegisteredCardCount(), 64);
        TestNotNull("Fallback restores Founder Hall", PartialRegistry->GetCardDefinition(TEXT("special.founder_hall")));

        GeneratedDefinitions[0]->DisplayName = FText::FromString(TEXT("Stale Habitat"));
        UDAContentRegistrySubsystem* StaleRegistry = NewObject<UDAContentRegistrySubsystem>();
        bUsedGeneratedCache = true;
        TestTrue("Stale cache rebuild succeeds via manifest fallback", StaleRegistry->RebuildFromGeneratedCache(GeneratedDefinitions, GeneratedDeck, bUsedGeneratedCache, Errors));
        TestFalse("Stale cache is not used", bUsedGeneratedCache);
        TestEqual("Fallback restores canonical name", StaleRegistry->GetCardDefinition(TEXT("synara.adaptive_habitat"))->DisplayName.ToString(), FString(TEXT("Adaptive Habitat")));

        GeneratedDefinitions[0]->DisplayName = FText::FromString(TEXT("Adaptive Habitat"));
        GeneratedDefinitions[0]->SourceManifestFingerprint = TEXT("stale-fingerprint");
        TArray<FText> FingerprintErrors;
        TestFalse("Stale fingerprint is rejected even when values match",
            FDAContentManifestPipeline::ValidateGeneratedCache(Manifest, GeneratedDefinitions, GeneratedDeck, FingerprintErrors));

        GeneratedDefinitions[0]->SourceManifestFingerprint = Manifest.SourceFingerprint;
        FDAVerticalSliceContentManifest StaleValueManifest = Manifest;
        FDAManifestCardDefinition* StaleWorldgarden = StaleValueManifest.Definitions.FindByPredicate([](const FDAManifestCardDefinition& Definition)
        {
            return Definition.DefinitionId == TEXT("eden.the_worldgarden");
        });
        TestNotNull("Worldgarden manifest fixture exists", StaleWorldgarden);
        if (StaleWorldgarden)
        {
            StaleWorldgarden->ConstructionCycles = 13;
            FDABuiltManifestContent StaleValueBuilt;
            TArray<FText> ValueErrors;
            TestTrue("Stale-value fixture builds through mapper",
                FDAContentManifestPipeline::BuildRuntimeContent(StaleValueManifest, StaleValueBuilt, ValueErrors));
            TestFalse("Stale authored value is rejected even when fingerprint matches",
                FDAContentManifestPipeline::ValidateGeneratedCache(Manifest, StaleValueBuilt.Definitions, GeneratedDeck, ValueErrors));
        }

        GeneratedDefinitions[0]->Tags.Add(FGameplayTag());
        TArray<FText> TagErrors;
        TestFalse("Invented generated tag rejects the cache",
            FDAContentManifestPipeline::ValidateGeneratedCache(Manifest, GeneratedDefinitions, GeneratedDeck, TagErrors));
        GeneratedDefinitions[0]->Tags.Reset();
        GeneratedDefinitions[0]->UpgradeBranchIds.Add(TEXT("upgrade.invented"));
        TArray<FText> UpgradeErrors;
        TestFalse("Invented generated upgrade branch rejects the cache",
            FDAContentManifestPipeline::ValidateGeneratedCache(Manifest, GeneratedDefinitions, GeneratedDeck, UpgradeErrors));
        GeneratedDefinitions[0]->UpgradeBranchIds.Reset();

        FDAVerticalSliceContentManifest InventedCombatManifest = Manifest;
        InventedCombatManifest.Definitions[0].bHasAuthoredCombat = true;
        InventedCombatManifest.Definitions[0].bCombatProvided = true;
        InventedCombatManifest.Definitions[0].Combat.StructuralIntegrity = 1.f;
        FDABuiltManifestContent InventedCombatBuilt;
        TArray<FText> CombatErrors;
        TestTrue("Authored combat fixture builds through mapper",
            FDAContentManifestPipeline::BuildRuntimeContent(InventedCombatManifest, InventedCombatBuilt, CombatErrors));
        TestFalse("Invented generated combat rejects the canonical cache",
            FDAContentManifestPipeline::ValidateGeneratedCache(Manifest, InventedCombatBuilt.Definitions, GeneratedDeck, CombatErrors));
    });

    It("strictly rejects missing wrong-type and unknown manifest keys at every object level", [this]()
    {
        FString Json;
        TestTrue("Canonical JSON reads", FFileHelper::LoadFileToString(Json, *FDAContentManifestPipeline::GetCanonicalManifestPath()));

        const TArray<TPair<FString, FString>> Mutations = {
            { TEXT("\"schemaVersion\": 1"), TEXT("\"schemaVersion\": \"1\"") },
            { TEXT("\"schemaVersion\": 1,"), TEXT("\"unknownTop\": 1,") },
            { TEXT("\"gameplayValues\":"), TEXT("\"unknownAuthority\": \"x\", \"gameplayValues\":") },
            { TEXT("\"total\": 64,"), TEXT("\"unknownCount\": 64, \"total\": 64,") },
            { TEXT("\"id\": \"synara.adaptive_habitat\""), TEXT("\"id\": \"Synara.AdaptiveHabitat\"") },
            { TEXT("\"displayName\": \"Adaptive Habitat\","), TEXT("") },
            { TEXT("\"cardType\": \"Residential\","), TEXT("\"unknownDefinition\": true, \"cardType\": \"Residential\",") },
            { TEXT("\"placeable\": true"), TEXT("\"placeable\": \"true\"") },
            { TEXT("/Script/DominionGameplay.DAGrayboxBuildingActor"), TEXT("/Game/UnresolvedPrefab") },
            { TEXT("\"tags\": []"), TEXT("\"combat\": {\"structuralIntegrity\": 1, \"armor\": 0, \"cyberIntegrity\": 0, \"capturable\": false, \"unknownCombat\": 1}, \"tags\": []") },
            { TEXT("\"authoredValues\": ["), TEXT("\"utilityPower\": 1, \"authoredValues\": [") },
            { TEXT("\"definitionId\": \"synara.adaptive_habitat\","), TEXT("\"unknownDeckKey\": 1, \"definitionId\": \"synara.adaptive_habitat\",") },
            { TEXT("\"quantity\": 3"), TEXT("\"quantity\": \"3\"") },
            { TEXT("\"quantity\": 3"), TEXT("\"quantity\": 4") }
        };

        for (const auto& Mutation : Mutations)
        {
            FString Corrupt = Json;
            TestTrue("Mutation target exists", Corrupt.ReplaceInline(*Mutation.Key, *Mutation.Value, ESearchCase::CaseSensitive) > 0);
            FDAVerticalSliceContentManifest Parsed;
            TArray<FText> Errors;
            TestFalse("Strict parser rejects schema mutation", FDAContentManifestPipeline::ParseJson(Corrupt, Parsed, Errors));
            TestTrue("Strict parser explains rejection", Errors.Num() > 0);
            if (Mutation.Value.Contains(TEXT("unknownCombat")) && !Parsed.Definitions.IsEmpty())
            {
                TestFalse("Unknown combat fields never establish authored combat", Parsed.Definitions[0].bHasAuthoredCombat);
            }
        }
    });

    It("passes the real definition registry and deck validators with no duplicate prefab cost copy or cache errors", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        FDABuiltManifestContent Built;
        TArray<FText> Errors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, Errors));
        TestTrue("Content factory succeeds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Built, Errors));

        UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
        TestTrue("Manifest definitions index", Registry->RebuildFromDefinitions(Built.Definitions, Errors));
        TestTrue("Registry validation has zero errors", Registry->ValidateRegistry(Errors));
        TestTrue("Starter deck obeys real deck rules", FDADeckRules::Validate(Built.Deck, *Registry, Errors));
        TestEqual("No validation errors", Errors.Num(), 0);

        for (const UDA_CardDefinition* Definition : Built.Definitions)
        {
            if (Definition->bPlaceable)
            {
                TestFalse(*FString::Printf(TEXT("%s has a prefab path"), *Definition->DefinitionId.ToString()), Definition->WorldPrefab.IsNull());
                TestNotNull(*FString::Printf(TEXT("%s prefab class resolves"), *Definition->DefinitionId.ToString()), Definition->WorldPrefab.LoadSynchronous());
            }
        }
    });

    It("rejects duplicate IDs missing prefabs invalid costs and Dominion cache eligibility", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        TArray<FText> LoadErrors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, LoadErrors));

        Manifest.Definitions[1].DefinitionId = Manifest.Definitions[0].DefinitionId;
        Manifest.Definitions[2].WorldPrefabClassPath.Reset();
        Manifest.Definitions[3].CraftCapital = -1;
        Manifest.Definitions[4].Rarity = EDARarity::Dominion;
        Manifest.Definitions[4].bRandomCacheEligible = true;

        TArray<FText> Errors;
        TestFalse("Corrupt manifest is rejected", FDAContentManifestPipeline::ValidateManifest(Manifest, Errors));
        TestTrue("Duplicate ID reported", Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("duplicate")); }));
        TestTrue("Missing prefab reported", Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("prefab")); }));
        TestTrue("Invalid cost reported", Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("negative cost")); }));
        TestTrue("Dominion cache violation reported", Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("cache eligible")); }));
    });

    It("routes a starter over-copy through the actual deck rules", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        FDABuiltManifestContent Built;
        TArray<FText> Errors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, Errors));

        TestTrue("Canonical content builds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Built, Errors));
        TArray<FGuid> MutatedInstanceIds = Built.Deck.GetInstanceIds();
        const int32 ReplacementIndex = MutatedInstanceIds.IndexOfByPredicate([&Built](const FGuid InstanceId)
        {
            const FCardInstance* Instance = Built.Collection.FindInstance(InstanceId);
            return Instance && Instance->DefinitionId == TEXT("synara.civic_autonomy_pods");
        });
        TestTrue("A replaceable starter instance exists", ReplacementIndex != INDEX_NONE);
        if (ReplacementIndex != INDEX_NONE)
        {
            MutatedInstanceIds[ReplacementIndex] = Built.Collection.AddInstance(
                TEXT("synara.adaptive_habitat"), EDAAcquisitionSource::StarterDeck);
            Built.Deck.SetInstanceIds(MutatedInstanceIds);
        }
        TestEqual("Mutated composition still contains 60 instances", Built.Deck.GetInstanceIds().Num(), 60);
        UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
        TestTrue("Manifest definitions index", Registry->RebuildFromDefinitions(Built.Definitions, Errors));
        TestFalse("Actual deck rules reject fourth Common copy", FDADeckRules::Validate(Built.Deck, *Registry, Errors));
        TestTrue("Copy-limit error reported", Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("allows at most")); }));
    });
}
