#include "Capture/DACaptureComponent.h"
#include "Damage/DAStructuralDamageComponent.h"
#include "Save/DASaveService.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    FDACaptureInteractionContext MakeInteraction(
        const EDACaptureAgentRole Role = EDACaptureAgentRole::Founder,
        const uint32 Seed = 1)
    {
        FDACaptureInteractionContext Context;
        Context.InteractionId = FGuid(Seed, Seed + 1, Seed + 2, Seed + 3);
        Context.CaptureActorId = FGuid(Seed + 4, Seed + 5, Seed + 6, Seed + 7);
        Context.AgentRole = Role;
        Context.bActorPresent = true;
        return Context;
    }

    struct FDACaptureFixture
    {
        FDACaptureFixture()
        {
            Campaign = NewObject<UDACampaignSaveGame>();
            AssetId = FGuid(11, 12, 13, 14);
            FDAWorldAssetRecord Asset;
            Asset.WorldAssetId = AssetId;
            Asset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
            Asset.OwnerCivilizationId = TEXT("forgeweave");
            Asset.ConstructionState = EDAConstructionState::Disabled;
            Asset.StructuralIntegrity = 10.f;

            Campaign->Snapshot.WorldAssets.Add(Asset);

            FDACaptureRecord Record;
            Record.WorldAssetId = AssetId;
            Record.StudyInsightReward = 40.f;
            Record.SalvageCapitalReward = 75.f;
            Record.SalvageMaterialReward = 6;
            Record.GiftInfluenceReward = 12.f;
            Record.GiftLoyaltyReward = 8.f;
            Record.AllowedGiftRecipients = {
                FDACaptureGiftRecipientRecord(TEXT("ironheart_workers"), EDAGiftRecipientRelationship::LocalAuthority),
                FDACaptureGiftRecipientRecord(TEXT("eden_circuit"), EDAGiftRecipientRelationship::Allied),
                FDACaptureGiftRecipientRecord(TEXT("forge_directorate"), EDAGiftRecipientRelationship::Hostile)
            };
            Campaign->Snapshot.OperationConflict.CaptureRecords.Add(Record);
            Campaign->Snapshot.OperationConflict.Resources.PostConflictLoyalty = 20.f;
            Component = NewObject<UDACaptureComponent>();
        }

        FDAWorldAssetRecord& Asset()
        {
            return *Campaign->Snapshot.FindWorldAssetRecord(AssetId);
        }

        FDACaptureRecord& Record()
        {
            return *Campaign->Snapshot.OperationConflict.FindCaptureRecord(AssetId);
        }

        FDAConflictResourceState& Resources()
        {
            return Campaign->Snapshot.OperationConflict.Resources;
        }

        void AddStructuralRecord()
        {
            FDAStructuralDamageRecord Damage;
            Damage.WorldAssetId = AssetId;
            Damage.CardDefinitionId = Asset().CardDefinitionId;
            Damage.Modules = {
                FDAStructureModuleHealthRecord(TEXT("power_core"), 100.f, false),
                FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true)
            };
            Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);
        }

        bool CompleteCapture()
        {
            const FDACaptureInteractionContext Context = MakeInteraction();
            return Component->InitializeFromCampaign(*Campaign, AssetId)
                && Component->BeginCapture(Context, TEXT("synara"))
                && Component->AdvanceCapture(UDACaptureComponent::BaseCaptureTimeSeconds, Context);
        }

        UDACampaignSaveGame* Campaign = nullptr;
        FGuid AssetId;
        UDACaptureComponent* Component = nullptr;
    };

    struct FDAConflictSaveDirectory
    {
        FDAConflictSaveDirectory()
            : Path(FPaths::Combine(
                FPaths::ProjectIntermediateDir(),
                TEXT("DominionConflictSaveTests"),
                FGuid::NewGuid().ToString(EGuidFormats::Digits)))
        {
        }

        ~FDAConflictSaveDirectory()
        {
            FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Path);
        }

        FString Path;
    };
}

BEGIN_DEFINE_SPEC(FDACaptureSpec, "Dominion.Gameplay.Capture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDACaptureSpec)

void FDACaptureSpec::Define()
{
    It("captures a Disabled Infinite Foundry at ten percent integrity after the uncontested base twenty seconds", [this]()
    {
        FDACaptureFixture Fixture;
        const FDACaptureInteractionContext Context = MakeInteraction();
        TestTrue("Capture records bind by stable WorldAssetId", Fixture.Component->InitializeFromCampaign(*Fixture.Campaign, Fixture.AssetId));
        TestTrue("An uncontested Founder can capture the boundary-integrity foundry", Fixture.Component->CanCapture(Context));
        TestTrue("Capture interaction begins", Fixture.Component->BeginCapture(Context, TEXT("synara")));
        TestEqual("Active interaction identity persists", Fixture.Record().ActiveInteractionId, Context.InteractionId);
        TestEqual("Active capture actor identity persists", Fixture.Record().ActiveCaptureActorId, Context.CaptureActorId);
        TestEqual("Active Founder role persists", static_cast<uint8>(Fixture.Record().ActiveCaptureRole), static_cast<uint8>(EDACaptureAgentRole::Founder));
        TestFalse("Capture does not complete early", Fixture.Component->AdvanceCapture(19.99f, Context));
        TestEqual("Progress retains elapsed authoritative time", Fixture.Record().CaptureProgressSeconds, 19.99f);
        TestTrue("Capture completes at the base twenty seconds", Fixture.Component->AdvanceCapture(0.01f, Context));
        TestTrue("Completed capture awaits its post-capture outcome", Fixture.Record().bCaptureCompleted);
        TestFalse("Capture completion alone does not duplicate ownership/outcome mutation", Fixture.Record().bOutcomeResolved);
    });

    It("applies the Engineer capture modifier at the exact thirteen-second boundary", [this]()
    {
        FDACaptureFixture Fixture;
        const FDACaptureInteractionContext Engineer = MakeInteraction(EDACaptureAgentRole::Engineer, 20);
        TestTrue("Capture records bind", Fixture.Component->InitializeFromCampaign(*Fixture.Campaign, Fixture.AssetId));
        TestTrue("Engineer interaction begins", Fixture.Component->BeginCapture(Engineer, TEXT("synara")));
        TestEqual("Engineer required time is thirteen seconds", Fixture.Record().RequiredCaptureTimeSeconds, 13.f);
        TestFalse("Engineer does not finish at 12.99 seconds", Fixture.Component->AdvanceCapture(12.99f, Engineer));
        TestTrue("Engineer finishes at thirteen seconds", Fixture.Component->AdvanceCapture(0.01f, Engineer));
    });

    It("enforces Disabled state integrity actor contest and security capture preconditions", [this]()
    {
        FDACaptureFixture Fixture;
        TestTrue("Capture records bind", Fixture.Component->InitializeFromCampaign(*Fixture.Campaign, Fixture.AssetId));

        FDACaptureInteractionContext Context = MakeInteraction();
        Context.bContested = true;
        TestFalse("Enemy contest blocks capture", Fixture.Component->CanCapture(Context));
        Context.bContested = false;
        Context.bActiveSecurity = true;
        TestFalse("Active security blocks capture", Fixture.Component->CanCapture(Context));
        Context.bActiveSecurity = false;
        Context.bActorPresent = false;
        TestFalse("An absent actor cannot capture", Fixture.Component->CanCapture(Context));
        Context.bActorPresent = true;
        Context.AgentRole = EDACaptureAgentRole::Other;
        TestFalse("An ineligible actor role cannot capture", Fixture.Component->CanCapture(Context));

        Context.AgentRole = EDACaptureAgentRole::Founder;
        Fixture.Asset().StructuralIntegrity = 9.99f;
        TestFalse("Integrity below ten percent blocks capture", Fixture.Component->CanCapture(Context));
        Fixture.Asset().StructuralIntegrity = 10.f;
        Fixture.Asset().ConstructionState = EDAConstructionState::Damaged;
        TestFalse("A non-Disabled facility cannot be captured", Fixture.Component->CanCapture(Context));
    });

    It("revalidates presence contest security and role identity on every capture advance", [this]()
    {
        {
            FDACaptureFixture Fixture;
            FDACaptureInteractionContext Context = MakeInteraction(EDACaptureAgentRole::Founder, 30);
            TestTrue("Presence fixture binds", Fixture.Component->InitializeFromCampaign(*Fixture.Campaign, Fixture.AssetId));
            TestTrue("Presence fixture begins", Fixture.Component->BeginCapture(Context, TEXT("synara")));
            TestFalse("Partial presence fixture does not complete", Fixture.Component->AdvanceCapture(5.f, Context));
            Context.bActorPresent = false;
            TestFalse("Absent active actor interrupts", Fixture.Component->AdvanceCapture(1.f, Context));
            TestFalse("Presence invalidation clears active state", Fixture.Record().bCaptureInProgress);
            TestEqual("Presence invalidation clears progress", Fixture.Record().CaptureProgressSeconds, 0.f);
        }
        {
            FDACaptureFixture Fixture;
            FDACaptureInteractionContext Context = MakeInteraction(EDACaptureAgentRole::Founder, 40);
            TestTrue("Contest fixture binds", Fixture.Component->InitializeFromCampaign(*Fixture.Campaign, Fixture.AssetId));
            TestTrue("Contest fixture begins", Fixture.Component->BeginCapture(Context, TEXT("synara")));
            Context.bContested = true;
            TestFalse("New contest interrupts", Fixture.Component->AdvanceCapture(1.f, Context));
            TestFalse("Contest invalidation clears active state", Fixture.Record().bCaptureInProgress);
        }
        {
            FDACaptureFixture Fixture;
            FDACaptureInteractionContext Context = MakeInteraction(EDACaptureAgentRole::Engineer, 50);
            TestTrue("Security fixture binds", Fixture.Component->InitializeFromCampaign(*Fixture.Campaign, Fixture.AssetId));
            TestTrue("Security fixture begins", Fixture.Component->BeginCapture(Context, TEXT("synara")));
            Context.bActiveSecurity = true;
            TestFalse("Reactivated security interrupts", Fixture.Component->AdvanceCapture(1.f, Context));
            TestFalse("Security invalidation clears active state", Fixture.Record().bCaptureInProgress);
        }
        {
            FDACaptureFixture Fixture;
            FDACaptureInteractionContext Context = MakeInteraction(EDACaptureAgentRole::Founder, 60);
            TestTrue("Identity fixture binds", Fixture.Component->InitializeFromCampaign(*Fixture.Campaign, Fixture.AssetId));
            TestTrue("Identity fixture begins", Fixture.Component->BeginCapture(Context, TEXT("synara")));
            Context.AgentRole = EDACaptureAgentRole::Engineer;
            TestFalse("Changed active role interrupts", Fixture.Component->AdvanceCapture(1.f, Context));
            TestFalse("Role invalidation clears active state", Fixture.Record().bCaptureInProgress);
            TestTrue("Role invalidation clears interaction identity", !Fixture.Record().ActiveInteractionId.IsValid());
        }
    });

    It("preserves the original definition and changes ownership without granting a resource reward", [this]()
    {
        FDACaptureFixture Fixture;
        TestTrue("Capture completes", Fixture.CompleteCapture());
        TestTrue("Preserve resolves", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Preserve, NAME_None));
        TestEqual("Preserve retains the original definition", Fixture.Asset().CardDefinitionId, FName(TEXT("forgeweave.infinite_foundry")));
        TestEqual("Preserve changes ownership to the captor", Fixture.Asset().OwnerCivilizationId, FName(TEXT("synara")));
        TestFalse("Preserve retains operational use", Fixture.Record().bOperationalUseRemoved);
        TestEqual("Preserve grants no Insight", Fixture.Resources().Insight, 0.f);
        TestEqual("Preserve grants no Capital", Fixture.Resources().Capital, 0.f);
        TestTrue("Preserve requires integration at reduced efficiency", Fixture.Record().bIntegrationRequired);
    });

    It("sanctions conversion as a persistent operation instead of instantly replacing the captured definition", [this]()
    {
        FDACaptureFixture Fixture;
        TestTrue("Capture completes", Fixture.CompleteCapture());
        TestTrue("Convert resolves", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Convert, NAME_None));
        TestTrue("A conversion operation is sanctioned", Fixture.Record().bConversionOperationSanctioned);
        TestEqual("The conversion operation retains its source definition until completed", Fixture.Asset().CardDefinitionId, FName(TEXT("forgeweave.infinite_foundry")));
        TestEqual("The captor owns the conversion operation", Fixture.Asset().OwnerCivilizationId, FName(TEXT("synara")));
        TestFalse("Convert does not remove future operational use", Fixture.Record().bOperationalUseRemoved);
    });

    It("removes operational use and grants only the authored Study reward exactly once", [this]()
    {
        FDACaptureFixture Fixture;
        TestTrue("Capture completes", Fixture.CompleteCapture());
        TestTrue("Study resolves", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Study, NAME_None));
        TestTrue("Study permanently removes operational use", Fixture.Record().bOperationalUseRemoved);
        TestEqual("Study leaves boundary-integrity facility Disabled", static_cast<uint8>(Fixture.Asset().ConstructionState), static_cast<uint8>(EDAConstructionState::Disabled));
        TestEqual("Study grants authored Insight", Fixture.Resources().Insight, 40.f);
        TestEqual("Study grants no Capital", Fixture.Resources().Capital, 0.f);
        TestEqual("Study writes one canonical campaign history tag", Fixture.Campaign->Snapshot.HistoryTags,
            TArray<FName>({TEXT("capture.studied")}));
        TestFalse("A resolved outcome cannot resolve twice", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Study, NAME_None));
        TestEqual("Rejected repeat grants no duplicate Insight", Fixture.Resources().Insight, 40.f);
        TestEqual("Rejected repeat grants no duplicate campaign history", Fixture.Campaign->Snapshot.HistoryTags.Num(), 1);
    });

    It("ruins salvaged infrastructure and grants only authored Capital and materials exactly once", [this]()
    {
        FDACaptureFixture Fixture;
        Fixture.AddStructuralRecord();
        TestTrue("Capture completes", Fixture.CompleteCapture());
        TestTrue("Salvage resolves", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Salvage, NAME_None));
        TestTrue("Salvage removes operational use", Fixture.Record().bOperationalUseRemoved);
        TestEqual("Salvage transitions the structure to Ruined", static_cast<uint8>(Fixture.Asset().ConstructionState), static_cast<uint8>(EDAConstructionState::Ruined));
        const FDAStructuralDamageRecord* SalvagedDamage = Fixture.Campaign->Snapshot.OperationConflict.FindStructuralDamageRecord(Fixture.AssetId);
        TestNotNull("Salvage resolves its matching structural record", SalvagedDamage);
        if (SalvagedDamage == nullptr)
        {
            return;
        }
        TestTrue("Salvage disables production", SalvagedDamage->bProductionDisabled);
        for (const FDAStructureModuleHealthRecord& Module : SalvagedDamage->Modules)
        {
            TestEqual("Every salvaged module is Ruined", static_cast<uint8>(Module.State), static_cast<uint8>(EDAStructureDamageState::Ruined));
        }
        TestEqual("Salvage grants authored Capital", Fixture.Resources().Capital, 75.f);
        TestEqual("Salvage grants authored materials", Fixture.Resources().Materials, 6);
        TestEqual("Salvage grants no Insight", Fixture.Resources().Insight, 0.f);
        TestFalse("A resolved salvage cannot repeat", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Salvage, NAME_None));
        TestEqual("Rejected repeat grants no duplicate Capital", Fixture.Resources().Capital, 75.f);
        TestEqual("Rejected repeat grants no duplicate materials", Fixture.Resources().Materials, 6);
    });

    It("gifts only to an authoritatively allowed allied or local recipient", [this]()
    {
        FDACaptureFixture Fixture;
        TestTrue("Capture completes", Fixture.CompleteCapture());
        TestFalse("A hostile configured recipient is rejected", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Gift, TEXT("forge_directorate")));
        TestFalse("An unrelated recipient is rejected", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Gift, TEXT("unrelated_faction")));
        TestEqual("Rejected Gift preserves enemy ownership", Fixture.Asset().OwnerCivilizationId, FName(TEXT("forgeweave")));
        TestEqual("Rejected Gift grants no Influence", Fixture.Resources().Influence, 0.f);
        TestEqual("Rejected Gift grants no Loyalty", Fixture.Resources().PostConflictLoyalty, 20.f);
        TestTrue("A local-authority Gift resolves", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Gift, TEXT("ironheart_workers")));
        TestEqual("Gift transfers ownership to the local authority", Fixture.Asset().OwnerCivilizationId, FName(TEXT("ironheart_workers")));
        TestEqual("Gift grants authored Influence", Fixture.Resources().Influence, 12.f);
        TestEqual("Gift grants authored Loyalty", Fixture.Resources().PostConflictLoyalty, 28.f);
    });

    It("re-resolves capture authority after world and conflict arrays grow", [this]()
    {
        FDACaptureFixture Fixture;
        TestTrue("Capture component binds by stable WorldAssetId", Fixture.Component->InitializeFromCampaign(*Fixture.Campaign, Fixture.AssetId));

        for (uint32 Index = 0; Index < 64; ++Index)
        {
            FDAWorldAssetRecord OtherAsset;
            OtherAsset.WorldAssetId = FGuid(5000 + Index, 6000 + Index, 7000 + Index, 8000 + Index);
            OtherAsset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
            OtherAsset.OwnerCivilizationId = TEXT("forgeweave");
            OtherAsset.ConstructionState = EDAConstructionState::Disabled;
            OtherAsset.StructuralIntegrity = 10.f;
            Fixture.Campaign->Snapshot.WorldAssets.Add(OtherAsset);

            FDACaptureRecord OtherCapture;
            OtherCapture.WorldAssetId = OtherAsset.WorldAssetId;
            Fixture.Campaign->Snapshot.OperationConflict.CaptureRecords.Add(OtherCapture);
        }

        const FDACaptureInteractionContext Context = MakeInteraction(EDACaptureAgentRole::Founder, 65);
        TestTrue("Array relocation does not invalidate capture start", Fixture.Component->BeginCapture(Context, TEXT("synara")));
        TestTrue("Array relocation does not invalidate capture completion", Fixture.Component->AdvanceCapture(20.f, Context));
        const FDACaptureRecord* Persisted = Fixture.Campaign->Snapshot.OperationConflict.FindCaptureRecord(Fixture.AssetId);
        TestNotNull("Original capture record is re-resolved", Persisted);
        TestTrue("Original capture record completes", Persisted != nullptr && Persisted->bCaptureCompleted);
    });

    It("round-trips Salvage as one capture structural and reward transaction", [this]()
    {
        FDAConflictSaveDirectory SaveDirectory;
        FDACaptureFixture Fixture;
        Fixture.AddStructuralRecord();
        TestTrue("Capture completes", Fixture.CompleteCapture());
        TestTrue("Salvage atomically resolves", Fixture.Component->ResolveOutcome(EDACaptureOutcome::Salvage, NAME_None));

        FDASaveService SaveService(SaveDirectory.Path);
        TestTrue("Combined Salvage aggregate saves", SaveService.SaveCampaign(Fixture.Campaign->Snapshot, TEXT("salvage-aggregate")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(TEXT("salvage-aggregate"));
        TestTrue("Combined Salvage aggregate reloads", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            return;
        }

        const FDACampaignSnapshot& Loaded = LoadResult.GetValue();
        const FDAWorldAssetRecord* Asset = Loaded.FindWorldAssetRecord(Fixture.AssetId);
        const FDACaptureRecord* Capture = Loaded.OperationConflict.FindCaptureRecord(Fixture.AssetId);
        const FDAStructuralDamageRecord* Damage = Loaded.OperationConflict.FindStructuralDamageRecord(Fixture.AssetId);
        TestNotNull("Salvaged asset reloads", Asset);
        TestNotNull("Resolved capture reloads", Capture);
        TestNotNull("Salvaged structural record reloads", Damage);
        if (Asset == nullptr || Capture == nullptr || Damage == nullptr)
        {
            return;
        }

        TestEqual("Salvage persists zero total integrity", Asset->StructuralIntegrity, 0.f);
        TestEqual("Salvage persists global ruin", static_cast<uint8>(Asset->ConstructionState), static_cast<uint8>(EDAConstructionState::Ruined));
        TestTrue("Salvage persists its exact-once outcome guard", Capture->bRewardsGranted && Capture->bOutcomeResolved);
        TestTrue("Salvage persists production disabled", Damage->bProductionDisabled);
        for (const FDAStructureModuleHealthRecord& Module : Damage->Modules)
        {
            TestEqual("Every reloaded module is Ruined", static_cast<uint8>(Module.State), static_cast<uint8>(EDAStructureDamageState::Ruined));
        }
        TestEqual("Salvage Capital reloads exactly once", Loaded.OperationConflict.Resources.Capital, 75.f);
        TestEqual("Salvage materials reload exactly once", Loaded.OperationConflict.Resources.Materials, 6);
    });

    It("accepts Forge Guard surrender at the inclusive sovereignty threshold and never repeats", [this]()
    {
        FDASurrenderContext Context;
        Context.SquadDefinitionId = TEXT("forgeweave.forge_guard");
        Context.MoraleState = EDAMoraleState::Breaking;
        Context.MilitarySovereignty = 25.f;

        FDACampaignSnapshot Campaign;
        FDAOperationConflictSnapshot& Conflict = Campaign.OperationConflict;
        FDASurrenderRecord Record;
        Record.SquadId = FGuid(21, 22, 23, 24);
        Context.SquadId = Record.SquadId;
        Conflict.SurrenderRecords.Add(Record);
        FDASurrenderRewardPolicy Rewards;
        Rewards.Influence = 5.f;
        Rewards.Loyalty = 7.f;
        Rewards.FutureSurrenderLikelihood = 10.f;
        Conflict.Resources.PostConflictLoyalty = 30.f;
        Conflict.Resources.FutureSurrenderLikelihood = 15.f;

        TestTrue("Sovereignty 25 is eligible", UDACaptureComponent::CanAcceptSurrender(Context, Conflict));
        FDASurrenderContext ForeignSquad = Context;
        ForeignSquad.SquadId = FGuid(90, 91, 92, 93);
        TestFalse("A different squad cannot mutate this aggregate", UDACaptureComponent::CanAcceptSurrender(ForeignSquad, Conflict));
        TestTrue("Eligible surrender is accepted", UDACaptureComponent::AcceptSurrender(Context, Rewards, Campaign));
        const FDASurrenderRecord* Accepted = Conflict.FindSurrenderRecord(Context.SquadId);
        TestNotNull("Accepted surrender remains in the authoritative aggregate", Accepted);
        TestTrue("Surrender acceptance persists on the squad record", Accepted != nullptr && Accepted->bAccepted);
        TestTrue("Acceptance writes canonical campaign history", Campaign.HistoryTags.Contains(TEXT("forge_guard_surrender_accepted")));
        TestEqual("Acceptance improves Influence", Conflict.Resources.Influence, 5.f);
        TestEqual("Acceptance improves post-conflict Loyalty", Conflict.Resources.PostConflictLoyalty, 37.f);
        TestEqual("Acceptance improves future surrender likelihood", Conflict.Resources.FutureSurrenderLikelihood, 25.f);
        TestFalse("An accepted surrender cannot repeat", UDACaptureComponent::AcceptSurrender(Context, Rewards, Campaign));
        TestEqual("Repeat acceptance grants no duplicate Influence", Conflict.Resources.Influence, 5.f);
        TestEqual("Repeat acceptance writes no duplicate history", Campaign.HistoryTags.Num(), 1);

        FDAOperationConflictSnapshot AboveBoundary;
        FDASurrenderRecord AboveBoundaryRecord;
        AboveBoundaryRecord.SquadId = FGuid(25, 26, 27, 28);
        AboveBoundary.SurrenderRecords.Add(AboveBoundaryRecord);
        Context.SquadId = AboveBoundaryRecord.SquadId;
        Context.MilitarySovereignty = 25.01f;
        TestFalse("Sovereignty above 25 is ineligible", UDACaptureComponent::CanAcceptSurrender(Context, AboveBoundary));
    });

    It("does not let transient or mismatched surrender aggregates grant authoritative rewards", [this]()
    {
        FDACampaignSnapshot AuthoritativeCampaign;
        FDAOperationConflictSnapshot& Authoritative = AuthoritativeCampaign.OperationConflict;
        FDASurrenderRecord Record;
        Record.SquadId = FGuid(70, 71, 72, 73);
        Authoritative.SurrenderRecords.Add(Record);

        FDASurrenderContext Context;
        Context.SquadId = Record.SquadId;
        Context.SquadDefinitionId = TEXT("forgeweave.forge_guard");
        Context.MoraleState = EDAMoraleState::Rout;
        Context.MilitarySovereignty = 0.f;
        FDASurrenderRewardPolicy Rewards;
        Rewards.Influence = 5.f;

        FDACampaignSnapshot TransientCopy = AuthoritativeCampaign;
        TestTrue("A complete transient aggregate can mutate only itself", UDACaptureComponent::AcceptSurrender(Context, Rewards, TransientCopy));
        TestEqual("Transient rewards do not reach authoritative resources", Authoritative.Resources.Influence, 0.f);
        TestFalse("Transient acceptance does not reach authoritative squad state", Authoritative.SurrenderRecords[0].bAccepted);

        FDACampaignSnapshot MismatchedCampaign;
        FDAOperationConflictSnapshot& Mismatched = MismatchedCampaign.OperationConflict;
        FDASurrenderRecord OtherRecord;
        OtherRecord.SquadId = FGuid(74, 75, 76, 77);
        Mismatched.SurrenderRecords.Add(OtherRecord);
        TestFalse("An aggregate without the stable SquadId is rejected", UDACaptureComponent::AcceptSurrender(Context, Rewards, MismatchedCampaign));
        TestEqual("Mismatched aggregate grants no Influence", Mismatched.Resources.Influence, 0.f);
    });

    It("round-trips an active Engineer identity and revalidates it after reconstruction", [this]()
    {
        FDAConflictSaveDirectory SaveDirectory;
        UDACampaignSaveGame* Campaign = NewObject<UDACampaignSaveGame>();
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(81, 82, 83, 84);
        Asset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
        Asset.OwnerCivilizationId = TEXT("forgeweave");
        Asset.ConstructionState = EDAConstructionState::Disabled;
        Asset.StructuralIntegrity = 10.f;
        Campaign->Snapshot.WorldAssets.Add(Asset);

        FDACaptureRecord Capture;
        Capture.WorldAssetId = Asset.WorldAssetId;
        Campaign->Snapshot.OperationConflict.CaptureRecords.Add(Capture);

        const FDACaptureInteractionContext Engineer = MakeInteraction(EDACaptureAgentRole::Engineer, 80);
        UDACaptureComponent* Component = NewObject<UDACaptureComponent>();
        TestTrue("Active-capture record binds", Component->InitializeFromCampaign(*Campaign, Asset.WorldAssetId));
        TestTrue("Engineer capture begins", Component->BeginCapture(Engineer, TEXT("synara")));
        TestFalse("Five seconds leaves Engineer capture active", Component->AdvanceCapture(5.f, Engineer));

        FDASaveService SaveService(SaveDirectory.Path);
        TestTrue("Active capture saves", SaveService.SaveCampaign(Campaign->Snapshot, TEXT("active-capture")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(TEXT("active-capture"));
        TestTrue("Active capture reloads", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            return;
        }

        UDACampaignSaveGame* LoadedCampaign = NewObject<UDACampaignSaveGame>();
        LoadedCampaign->Snapshot = LoadResult.GetValue();
        FDAWorldAssetRecord* LoadedAsset = LoadedCampaign->Snapshot.FindWorldAssetRecord(Asset.WorldAssetId);
        FDACaptureRecord* LoadedCapture = LoadedCampaign->Snapshot.OperationConflict.FindCaptureRecord(Asset.WorldAssetId);
        TestNotNull("Active asset reloads by stable id", LoadedAsset);
        TestNotNull("Active capture reloads by stable id", LoadedCapture);
        if (LoadedAsset == nullptr || LoadedCapture == nullptr)
        {
            return;
        }
        TestEqual("Active interaction id survives", LoadedCapture->ActiveInteractionId, Engineer.InteractionId);
        TestEqual("Active capture actor id survives", LoadedCapture->ActiveCaptureActorId, Engineer.CaptureActorId);
        TestEqual("Active Engineer role survives", static_cast<uint8>(LoadedCapture->ActiveCaptureRole), static_cast<uint8>(EDACaptureAgentRole::Engineer));
        TestEqual("Active progress survives", LoadedCapture->CaptureProgressSeconds, 5.f);

        UDACaptureComponent* Reconstructed = NewObject<UDACaptureComponent>();
        TestTrue("Active capture reconstructs", Reconstructed->InitializeFromCampaign(*LoadedCampaign, Asset.WorldAssetId));
        FDACaptureInteractionContext AbsentEngineer = Engineer;
        AbsentEngineer.bActorPresent = false;
        TestFalse("Reconstructed capture revalidates actor presence", Reconstructed->AdvanceCapture(1.f, AbsentEngineer));
        TestFalse("Invalid reconstructed interaction is reset", LoadedCapture->bCaptureInProgress);
        TestEqual("Invalid reconstructed interaction loses progress", LoadedCapture->CaptureProgressSeconds, 0.f);
    });

    It("round-trips damage capture rewards and surrender guards then reconstructs without replay", [this]()
    {
        FDAConflictSaveDirectory SaveDirectory;
        UDACampaignSaveGame* Campaign = NewObject<UDACampaignSaveGame>();

        FDAWorldAssetRecord Foundry;
        Foundry.WorldAssetId = FGuid(101, 102, 103, 104);
        Foundry.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
        Foundry.OwnerCivilizationId = TEXT("forgeweave");
        Foundry.ConstructionState = EDAConstructionState::Disabled;
        Foundry.StructuralIntegrity = 10.f;
        Campaign->Snapshot.WorldAssets.Add(Foundry);

        FDAWorldAssetRecord FabricationNode;
        FabricationNode.WorldAssetId = FGuid(105, 106, 107, 108);
        FabricationNode.CardDefinitionId = TEXT("synara.synthetic_fabrication_node");
        FabricationNode.OwnerCivilizationId = TEXT("synara");
        FabricationNode.ConstructionState = EDAConstructionState::Operational;
        FabricationNode.StructuralIntegrity = 100.f;
        Campaign->Snapshot.WorldAssets.Add(FabricationNode);

        FDACaptureRecord Capture;
        Capture.WorldAssetId = Foundry.WorldAssetId;
        Capture.StudyInsightReward = 40.f;
        Campaign->Snapshot.OperationConflict.CaptureRecords.Add(Capture);

        FDAStructuralDamageRecord Damage;
        Damage.WorldAssetId = FabricationNode.WorldAssetId;
        Damage.CardDefinitionId = FabricationNode.CardDefinitionId;
        Damage.Modules = { FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true) };
        Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);

        FDASurrenderRecord Surrender;
        Surrender.SquadId = FGuid(109, 110, 111, 112);
        Campaign->Snapshot.OperationConflict.SurrenderRecords.Add(Surrender);

        UDAStructuralDamageComponent* DamageComponent = NewObject<UDAStructuralDamageComponent>();
        TestTrue("Damage record binds before save", DamageComponent->InitializeFromCampaign(*Campaign, FabricationNode.WorldAssetId));
        TestTrue("Persistent Control Center is destroyed before save", DamageComponent->ApplyModuleDamage(TEXT("control_center"), 100.f));

        UDACaptureComponent* CaptureComponent = NewObject<UDACaptureComponent>();
        TestTrue("Capture record binds before save", CaptureComponent->InitializeFromCampaign(*Campaign, Foundry.WorldAssetId));
        const FDACaptureInteractionContext Interaction = MakeInteraction();
        TestTrue("Capture begins before save", CaptureComponent->BeginCapture(Interaction, TEXT("synara")));
        TestTrue("Capture completes before save", CaptureComponent->AdvanceCapture(20.f, Interaction));
        TestTrue("Study resolves before save", CaptureComponent->ResolveOutcome(EDACaptureOutcome::Study, NAME_None));

        FDASurrenderContext SurrenderContext;
        SurrenderContext.SquadId = Surrender.SquadId;
        SurrenderContext.SquadDefinitionId = TEXT("forgeweave.forge_guard");
        SurrenderContext.MoraleState = EDAMoraleState::Rout;
        SurrenderContext.MilitarySovereignty = 0.f;
        FDASurrenderRewardPolicy SurrenderRewards;
        SurrenderRewards.Influence = 5.f;
        SurrenderRewards.Loyalty = 7.f;
        SurrenderRewards.FutureSurrenderLikelihood = 10.f;
        TestTrue("Surrender resolves before save", UDACaptureComponent::AcceptSurrender(
            SurrenderContext,
            SurrenderRewards,
            Campaign->Snapshot));

        FDASaveService SaveService(SaveDirectory.Path);
        TestTrue("Conflict snapshot saves", SaveService.SaveCampaign(Campaign->Snapshot, TEXT("conflict-round-trip")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(TEXT("conflict-round-trip"));
        TestTrue("Conflict snapshot reloads", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            return;
        }

        UDACampaignSaveGame* LoadedCampaign = NewObject<UDACampaignSaveGame>();
        LoadedCampaign->Snapshot = LoadResult.GetValue();
        FDAWorldAssetRecord* LoadedFoundry = LoadedCampaign->Snapshot.FindWorldAssetRecord(Foundry.WorldAssetId);
        FDACaptureRecord* LoadedCapture = LoadedCampaign->Snapshot.OperationConflict.FindCaptureRecord(Foundry.WorldAssetId);
        FDAWorldAssetRecord* LoadedFabrication = LoadedCampaign->Snapshot.FindWorldAssetRecord(FabricationNode.WorldAssetId);
        FDAStructuralDamageRecord* LoadedDamage = LoadedCampaign->Snapshot.OperationConflict.FindStructuralDamageRecord(FabricationNode.WorldAssetId);
        FDASurrenderRecord* LoadedSurrender = LoadedCampaign->Snapshot.OperationConflict.FindSurrenderRecord(Surrender.SquadId);
        TestNotNull("Foundry reloads by stable id", LoadedFoundry);
        TestNotNull("Capture reloads by stable WorldAssetId", LoadedCapture);
        TestNotNull("Fabrication node reloads by stable id", LoadedFabrication);
        TestNotNull("Structural damage reloads by stable WorldAssetId", LoadedDamage);
        TestNotNull("Surrender reloads by stable SquadId", LoadedSurrender);
        if (LoadedFoundry == nullptr || LoadedCapture == nullptr || LoadedFabrication == nullptr || LoadedDamage == nullptr || LoadedSurrender == nullptr)
        {
            return;
        }

        TestEqual("Destroyed module health survives", LoadedDamage->Modules[0].CurrentHealth, 0.f);
        TestTrue("Functional production-disabled state survives", LoadedDamage->bProductionDisabled);
        TestEqual("Capture reward survives exactly", LoadedCampaign->Snapshot.OperationConflict.Resources.Insight, 40.f);
        TestEqual("Surrender Influence survives exactly", LoadedCampaign->Snapshot.OperationConflict.Resources.Influence, 5.f);
        TestTrue("Surrender history survives in the one campaign ledger", LoadedCampaign->Snapshot.HistoryTags.Contains(TEXT("forge_guard_surrender_accepted")));

        UDAStructuralDamageComponent* ReconstructedDamage = NewObject<UDAStructuralDamageComponent>();
        TestTrue("Damage component reconstructs from stable ids", ReconstructedDamage->InitializeFromCampaign(*LoadedCampaign, FabricationNode.WorldAssetId));
        TestFalse("Reconstructed disabled production remains disabled", ReconstructedDamage->IsProductionEnabled());
        TestEqual("Functional module loss does not corrupt global state after reload", static_cast<uint8>(LoadedFabrication->ConstructionState), static_cast<uint8>(EDAConstructionState::Operational));

        UDACaptureComponent* ReconstructedCapture = NewObject<UDACaptureComponent>();
        TestTrue("Capture component reconstructs from stable ids", ReconstructedCapture->InitializeFromCampaign(*LoadedCampaign, Foundry.WorldAssetId));
        TestFalse("Resolved Study cannot replay after reload", ReconstructedCapture->ResolveOutcome(EDACaptureOutcome::Study, NAME_None));
        TestEqual("Rejected replay does not duplicate Insight", LoadedCampaign->Snapshot.OperationConflict.Resources.Insight, 40.f);
        TestFalse("Accepted surrender cannot replay after reload", UDACaptureComponent::AcceptSurrender(
            SurrenderContext,
            SurrenderRewards,
            LoadedCampaign->Snapshot));
        TestEqual("Rejected surrender replay does not duplicate Influence", LoadedCampaign->Snapshot.OperationConflict.Resources.Influence, 5.f);
    });
}
