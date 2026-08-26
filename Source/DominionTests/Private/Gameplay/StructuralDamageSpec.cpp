#include "Damage/DAStructuralDamageComponent.h"
#include "Save/DACampaignSaveGame.h"

#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDAStructuralDamageSpec, "Dominion.Gameplay.StructuralDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAStructuralDamageSpec)

void FDAStructuralDamageSpec::Define()
{
    It("disables a Synthetic Fabrication Node when its Control Center is destroyed without ruining the structure", [this]()
    {
        UDACampaignSaveGame* Campaign = NewObject<UDACampaignSaveGame>();
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(1, 2, 3, 4);
        Asset.CardDefinitionId = TEXT("synara.synthetic_fabrication_node");
        Asset.ConstructionState = EDAConstructionState::Operational;
        Asset.StructuralIntegrity = 100.f;

        FDAStructuralDamageRecord Damage;
        Damage.WorldAssetId = Asset.WorldAssetId;
        Damage.CardDefinitionId = Asset.CardDefinitionId;
        Damage.Modules = {
            FDAStructureModuleHealthRecord(TEXT("power_core"), 100.f, false),
            FDAStructureModuleHealthRecord(TEXT("fabrication_line"), 100.f, false),
            FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true)
        };

        Campaign->Snapshot.WorldAssets.Add(Asset);
        Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);

        UDAStructuralDamageComponent* Component = NewObject<UDAStructuralDamageComponent>();
        TestTrue("Persistent structural records bind through their lifetime owner", Component->InitializeFromCampaign(*Campaign, Asset.WorldAssetId));
        TestTrue("Control Center takes its full health in damage", Component->ApplyModuleDamage(TEXT("control_center"), 100.f));

        const FDAStructureModuleHealthRecord* ControlCenter = Component->FindModule(TEXT("control_center"));
        TestNotNull("Control Center remains in the persistent module record", ControlCenter);
        if (ControlCenter != nullptr)
        {
            TestEqual("Destroyed Control Center is Disabled", static_cast<uint8>(ControlCenter->State), static_cast<uint8>(EDAStructureDamageState::Disabled));
        }
        const FDAWorldAssetRecord* PersistentAsset = Campaign->Snapshot.FindWorldAssetRecord(Asset.WorldAssetId);
        const FDAStructuralDamageRecord* PersistentDamage = Campaign->Snapshot.OperationConflict.FindStructuralDamageRecord(Asset.WorldAssetId);
        TestNotNull("Asset remains addressable after mutation", PersistentAsset);
        TestNotNull("Damage remains addressable after mutation", PersistentDamage);
        if (PersistentAsset == nullptr || PersistentDamage == nullptr)
        {
            return;
        }
        TestEqual("Module loss does not override the 100 percent integrity band", static_cast<uint8>(PersistentAsset->ConstructionState), static_cast<uint8>(EDAConstructionState::Operational));
        TestEqual("Surgical module damage does not erase total integrity", PersistentAsset->StructuralIntegrity, 100.f);
        TestTrue("Functional production-disabled state is persisted separately", PersistentDamage->bProductionDisabled);
        TestFalse("Disabled production cannot run", Component->IsProductionEnabled());
        TestFalse("Control Center loss alone is not a ruin", Component->IsRuined());

        TestTrue("Total integrity accepts structural damage", Component->ApplyStructuralDamage(100.f));
        TestEqual("Zero total integrity is Ruined", static_cast<uint8>(PersistentAsset->ConstructionState), static_cast<uint8>(EDAConstructionState::Ruined));
        TestTrue("Zero total integrity reports a ruin", Component->IsRuined());
    });

    It("uses the canonical Operational Damaged Disabled and Ruined integrity bands", [this]()
    {
        UDACampaignSaveGame* Campaign = NewObject<UDACampaignSaveGame>();
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(5, 6, 7, 8);
        Asset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
        Asset.ConstructionState = EDAConstructionState::Operational;
        Asset.StructuralIntegrity = 100.f;

        FDAStructuralDamageRecord Damage;
        Damage.WorldAssetId = Asset.WorldAssetId;
        Damage.CardDefinitionId = Asset.CardDefinitionId;
        Damage.Modules = { FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true) };

        Campaign->Snapshot.WorldAssets.Add(Asset);
        Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);

        UDAStructuralDamageComponent* Component = NewObject<UDAStructuralDamageComponent>();
        TestTrue("Foundry record binds", Component->InitializeFromCampaign(*Campaign, Asset.WorldAssetId));
        TestTrue("Damage reaches 51 percent", Component->ApplyStructuralDamage(49.f));
        TestEqual("51 percent remains Operational", static_cast<uint8>(Campaign->Snapshot.WorldAssets[0].ConstructionState), static_cast<uint8>(EDAConstructionState::Operational));
        TestTrue("Damage reaches 50 percent", Component->ApplyStructuralDamage(1.f));
        TestEqual("50 percent becomes Damaged", static_cast<uint8>(Campaign->Snapshot.WorldAssets[0].ConstructionState), static_cast<uint8>(EDAConstructionState::Damaged));
        TestTrue("Damage reaches 25 percent", Component->ApplyStructuralDamage(25.f));
        TestEqual("25 percent becomes Disabled", static_cast<uint8>(Campaign->Snapshot.WorldAssets[0].ConstructionState), static_cast<uint8>(EDAConstructionState::Disabled));
        TestTrue("Damage reaches zero percent", Component->ApplyStructuralDamage(25.f));
        TestEqual("Zero percent becomes Ruined", static_cast<uint8>(Campaign->Snapshot.WorldAssets[0].ConstructionState), static_cast<uint8>(EDAConstructionState::Ruined));
    });

    It("allows full modular destruction for exactly the frozen eight structure definitions", [this]()
    {
        const TArray<FName> Expected = {
            TEXT("synara.synthetic_fabrication_node"),
            TEXT("synara.swarm_foundry"),
            TEXT("forgeweave.infinite_foundry"),
            TEXT("forgeweave.replication_forge"),
            TEXT("forgeweave.smog_reclaimer"),
            TEXT("forgeweave.freight_furnace"),
            TEXT("forgeweave.grand_forge"),
            TEXT("fusion.autonomous_factory")
        };

        const TArray<FName>& Eligible = FDAStructuralDamagePolicy::GetFullModularDestructionDefinitions();
        TestEqual("The frozen set contains exactly eight definitions", Eligible.Num(), 8);
        for (const FName DefinitionId : Expected)
        {
            TestTrue("Each frozen structure is eligible", Eligible.Contains(DefinitionId));
            TestTrue("Core eligibility query agrees with the frozen set", FDAStructuralDamagePolicy::SupportsFullModularDestruction(DefinitionId));
        }
        TestFalse("An ordinary building cannot receive full modular destruction", FDAStructuralDamagePolicy::SupportsFullModularDestruction(TEXT("synara.adaptive_habitat")));
    });

    It("rejects gameplay reconstruction for a definition outside the Core structural policy", [this]()
    {
        UDACampaignSaveGame* Campaign = NewObject<UDACampaignSaveGame>();
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(13, 14, 15, 16);
        Asset.CardDefinitionId = TEXT("synara.adaptive_habitat");
        Asset.OwnerCivilizationId = TEXT("synara");
        Asset.ConstructionState = EDAConstructionState::Operational;
        Asset.StructuralIntegrity = 100.f;
        Campaign->Snapshot.WorldAssets.Add(Asset);

        FDAStructuralDamageRecord Damage;
        Damage.WorldAssetId = Asset.WorldAssetId;
        Damage.CardDefinitionId = Asset.CardDefinitionId;
        Damage.Modules = { FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true) };
        Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);

        UDAStructuralDamageComponent* Component = NewObject<UDAStructuralDamageComponent>();
        TestFalse("Non-eligible records cannot reconstruct", Component->InitializeFromCampaign(*Campaign, Asset.WorldAssetId));
    });

    It("re-resolves structural records after authoritative arrays grow", [this]()
    {
        UDACampaignSaveGame* Campaign = NewObject<UDACampaignSaveGame>();
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(9, 10, 11, 12);
        Asset.CardDefinitionId = TEXT("synara.synthetic_fabrication_node");
        Asset.OwnerCivilizationId = TEXT("synara");
        Asset.ConstructionState = EDAConstructionState::Operational;
        Asset.StructuralIntegrity = 100.f;
        Campaign->Snapshot.WorldAssets.Add(Asset);

        FDAStructuralDamageRecord Damage;
        Damage.WorldAssetId = Asset.WorldAssetId;
        Damage.CardDefinitionId = Asset.CardDefinitionId;
        Damage.Modules = { FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true) };
        Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);

        UDAStructuralDamageComponent* Component = NewObject<UDAStructuralDamageComponent>();
        TestTrue("Component binds by stable id", Component->InitializeFromCampaign(*Campaign, Asset.WorldAssetId));
        for (uint32 Index = 0; Index < 64; ++Index)
        {
            FDAWorldAssetRecord OtherAsset;
            OtherAsset.WorldAssetId = FGuid(1000 + Index, 2000 + Index, 3000 + Index, 4000 + Index);
            OtherAsset.CardDefinitionId = TEXT("synara.synthetic_fabrication_node");
            Campaign->Snapshot.WorldAssets.Add(OtherAsset);

            FDAStructuralDamageRecord OtherDamage;
            OtherDamage.WorldAssetId = OtherAsset.WorldAssetId;
            OtherDamage.CardDefinitionId = OtherAsset.CardDefinitionId;
            OtherDamage.Modules = { FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true) };
            Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(OtherDamage);
        }

        TestTrue("Array relocation does not invalidate stable binding", Component->ApplyModuleDamage(TEXT("control_center"), 100.f));
        const FDAStructuralDamageRecord* Persisted = Campaign->Snapshot.OperationConflict.FindStructuralDamageRecord(Asset.WorldAssetId);
        TestNotNull("Original damage record is re-resolved", Persisted);
        if (Persisted != nullptr)
        {
            TestEqual("Original module receives the damage", Persisted->Modules[0].CurrentHealth, 0.f);
        }
    });
}
