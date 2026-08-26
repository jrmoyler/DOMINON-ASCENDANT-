#include "Save/DASaveService.h"
#include "Damage/DAStructuralDamageComponent.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "World/DARegionalWorldState.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    class FRejectingTransactionPlatform final : public IDASaveTransactionPlatform
    {
    public:
        virtual bool Commit(
            const FString&,
            const FString&,
            const FString&,
            bool,
            FString& OutError) override
        {
            OutError = TEXT("Injected replacement failure.");
            return false;
        }

        virtual bool RecoverActive(
            const FString&,
            const FString&,
            const FString&,
            FString& OutError) override
        {
            OutError = TEXT("Recovery is not expected in the failed-replacement test.");
            return false;
        }
    };

    FDACampaignSnapshot MakeHistorySnapshot(const TCHAR* HistoryTag)
    {
        FDACampaignSnapshot Snapshot;
        Snapshot.HistoryTags.Add(FName(HistoryTag));
        return Snapshot;
    }

    FDAWorldCampaignState MakeInterruptedWorldState()
    {
        FDAWorldCampaignState State;
        State.bInitialized = true;
        State.CurrentWorldTick = 12;
        State.CurrentRegionId = TEXT("region.synara_frontier");
        State.Trade.LastProcessedWorldTick = 12;
        State.Forgeweave = FDAForgeweaveCityState::MakeVerticalSliceInitialState(1701, 12);

        FDARegionState Synara;
        Synara.RegionId = TEXT("region.synara_frontier");
        Synara.OwnerId = TEXT("civilization.synara");
        Synara.SettlementIds.Add(TEXT("settlement.arden_reservoir"));
        Synara.PersistentDelta.Revision = 3;
        FDARegionActorState Terminal;
        Terminal.ActorId = TEXT("actor.synara.transit_terminal");
        Terminal.DefinitionId = TEXT("asset.passage_terminal");
        Synara.PersistentDelta.LocalActors.Add(Terminal);
        State.Regions.Add(Synara);

        FDARegionState Ironheart;
        Ironheart.RegionId = TEXT("region.ironheart");
        Ironheart.OwnerId = TEXT("civilization.forgeweave");
        Ironheart.SettlementIds.Add(TEXT("settlement.ore_station_7"));
        State.Regions.Add(Ironheart);

        State.TravelHandoff.RequestId = TEXT("travel.save.interrupted");
        State.TravelHandoff.SourceRegionId = Synara.RegionId;
        State.TravelHandoff.DestinationRegionId = Ironheart.RegionId;
        State.TravelHandoff.DepartureWorldTick = 10;
        State.TravelHandoff.TravelWorldTicks = 2;
        State.TravelHandoff.Stage = EDATravelHandoffStage::DestinationLoaded;
        return State;
    }
}

BEGIN_DEFINE_SPEC(FDASaveRoundTripSpec, "Dominion.Core.Save.RoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

    FString TestSaveDirectory;
END_DEFINE_SPEC(FDASaveRoundTripSpec)

void FDASaveRoundTripSpec::Define()
{
    BeforeEach([this]()
    {
        TestSaveDirectory = FPaths::Combine(
            FPaths::ProjectIntermediateDir(),
            TEXT("DominionSaveTests"),
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
    });

    AfterEach([this]()
    {
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*TestSaveDirectory);
    });

    It("bootstraps the frozen wallet and opening hand through the production campaign owner", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;

        const FDACampaignSnapshot& Bootstrap = World->GetPersistentCampaign();
        TestTrue("Production bootstrap is initialized", Bootstrap.WorldState.bInitialized);
        TestEqual("Starter collection plus Founder Hall has 61 instances",
            Bootstrap.CollectionState.Instances.Num(), 61);
        TestEqual("Starter deck owns exactly 60 instances",
            Bootstrap.DeckState.GetInstanceIds().Num(), FDADeckState::RequiredDeckSize);
        TestEqual("Production opening hand draws exactly seven",
            Bootstrap.DeckState.GetHand().Num(), FDADeckState::OpeningHandSize);
        TestEqual("Opening draw leaves 53 cards",
            Bootstrap.DeckState.GetDrawPile().Num(), 53);
        TestEqual("Opening bootstrap has no reserve cards",
            Bootstrap.DeckState.GetReserveQueue().Num(), 0);
        TestEqual("Opening bootstrap has no deployed deck cards",
            Bootstrap.DeckState.GetDeployed().Num(), 0);
        TestEqual("Capital starts at the frozen value",
            Bootstrap.CitySimulationState.Wallet.Capital, 40.f);
        TestEqual("Insight starts at the frozen value",
            Bootstrap.CitySimulationState.Wallet.Insight, 12.f);
        TestEqual("Influence starts at the frozen value",
            Bootstrap.CitySimulationState.Wallet.Influence, 8.f);
        TestEqual("No unrelated Materials currency is invented",
            Bootstrap.OperationConflict.Resources.Materials, 0);
        TestEqual("Population starts at 24", Bootstrap.CitySimulationState.Population, 24);

        const FGuid* BarrierCardId = Bootstrap.DeckState.GetHand().FindByPredicate(
            [&Bootstrap](const FGuid InstanceId)
            {
                const FCardInstance* Card = Bootstrap.CollectionState.FindInstance(InstanceId);
                return Card != nullptr && Card->DefinitionId == TEXT("universal.barrier_hub");
            });
        TestNotNull("The canonical opening hand contains an immediately placeable card",
            BarrierCardId);
        if (BarrierCardId == nullptr) return;

        const FGuid PlacedCardId = *BarrierCardId;
        FGuid WorldAssetId;
        FString Error;
        TestTrue("Production placement consumes the opening-hand card",
            World->TryPlacePlayerWorldAsset(PlacedCardId, TEXT("player_capital"),
                FIntPoint(0, 0), FIntPoint(1, 1), EGridRotation::Zero,
                WorldAssetId, Error));
        const FDACampaignSnapshot& Placed = World->GetPersistentCampaign();
        TestFalse("Placed card cannot remain in hand",
            Placed.DeckState.GetHand().Contains(PlacedCardId));
        TestTrue("Placed card enters the persisted deployed zone",
            Placed.DeckState.GetDeployed().Contains(PlacedCardId));
        TestEqual("All deck zones still partition exactly 60 cards",
            Placed.DeckState.GetDrawPile().Num() + Placed.DeckState.GetHand().Num()
                + Placed.DeckState.GetReserveQueue().Num()
                + Placed.DeckState.GetDeployed().Num(),
            FDADeckState::RequiredDeckSize);
        const FCardInstance* PlacedCard = Placed.CollectionState.FindInstance(PlacedCardId);
        const FDAWorldAssetRecord* PlacedAsset = Placed.FindWorldAssetRecord(WorldAssetId);
        TestTrue("Placement creates the exact bidirectional card link",
            PlacedCard != nullptr && PlacedAsset != nullptr
                && PlacedCard->WorldAssetId == WorldAssetId
                && PlacedAsset->CardInstanceId == PlacedCardId);
    });

    It("round-trips the Adaptive Habitat card and authoritative world asset record", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;
        FDACampaignSnapshot Prepared = World->GetPersistentCampaign();
        FCardInstance* Card = nullptr;
        for (TPair<FGuid, FCardInstance>& Pair : Prepared.CollectionState.Instances)
        {
            if (Pair.Value.DefinitionId == TEXT("synara.adaptive_habitat")
                && Pair.Value.RecoveryState == EDARecoveryState::Available)
            {
                Card = &Pair.Value;
                break;
            }
        }
        TestNotNull("Authored starter collection contains Adaptive Habitat", Card);
        if (Card == nullptr) return;
        const FGuid CardId = Card->InstanceId;
        Card->MasteryXp = 725;
        Prepared.HistoryTags = {FName(TEXT("history.first_habitat"))};
        TestTrue("Mastery fixture restores through the production owner",
            World->RestorePersistentCampaign(Prepared));
        FGuid WorldAssetId;
        FString PlacementError;
        TestTrue("Adaptive Habitat places from canonical starter state",
            World->TryPlacePlayerWorldAsset(CardId, TEXT("player_capital"),
                FIntPoint(14, 22), FIntPoint(2, 2), EGridRotation::Ninety,
                WorldAssetId, PlacementError));
        const FDACampaignSnapshot Snapshot = World->GetPersistentCampaign();

        FDASaveService SaveService(TestSaveDirectory);
        const FDASaveResult SaveResult = SaveService.SaveCampaign(Snapshot, TEXT("adaptive-habitat"));
        TestTrue("Campaign save succeeds", SaveResult.IsSuccess());

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(TEXT("adaptive-habitat"));
        TestTrue("Campaign load has a value", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            return;
        }

        const FDACampaignSnapshot& Loaded = LoadResult.GetValue();
        TestEqual("Canonical collection plus Founder Hall is restored",
            Loaded.CollectionState.Instances.Num(), 61);
        TestEqual("Exact deck membership is restored",
            Loaded.DeckState.GetInstanceIds().Num(), FDADeckState::RequiredDeckSize);
        TestTrue("Loaded deck is rebound to its owning collection", Loaded.DeckState.GetCollection() == &Loaded.CollectionState);
        TestEqual("Founder Hall and Adaptive Habitat are restored", Loaded.WorldAssets.Num(), 2);
        if (Loaded.CollectionState.Instances.Num() != 61
            || Loaded.DeckState.GetInstanceIds().Num() != FDADeckState::RequiredDeckSize
            || Loaded.WorldAssets.Num() != 2)
        {
            return;
        }

        const FCardInstance* LoadedCard = Loaded.CollectionState.FindInstance(CardId);
        TestTrue("Card is addressable through the authoritative collection", LoadedCard != nullptr);
        if (LoadedCard == nullptr)
        {
            return;
        }
        const FDAWorldAssetRecord* LoadedAsset = Loaded.FindWorldAssetRecord(WorldAssetId);
        TestNotNull("Adaptive Habitat world asset is restored", LoadedAsset);
        if (LoadedAsset == nullptr) return;
        TestEqual("Card instance identity survives", LoadedCard->InstanceId, CardId);
        TestEqual("Card definition identity survives", LoadedCard->DefinitionId,
            FName(TEXT("synara.adaptive_habitat")));
        TestEqual("Card-to-world link survives", LoadedCard->WorldAssetId, WorldAssetId);
        TestEqual("Mastery survives", LoadedCard->MasteryXp, 725);
        TestTrue("Deck identity survives", Loaded.DeckState.GetInstanceIds().Contains(CardId));
        TestEqual("World asset identity survives", LoadedAsset->WorldAssetId, WorldAssetId);
        TestEqual("World asset card link survives", LoadedAsset->CardInstanceId, CardId);
        TestEqual("City placement survives", LoadedAsset->CityId,
            FName(TEXT("player_capital")));
        TestEqual("Grid placement survives", LoadedAsset->GridOrigin, FIntPoint(14, 22));
        TestEqual("Rotation survives", LoadedAsset->Rotation, static_cast<uint8>(1));
        TestTrue("Untimed authored placement remains immediately operational",
            LoadedAsset->ConstructionState == EDAConstructionState::Operational);
        TestEqual("Structural integrity survives", LoadedAsset->StructuralIntegrity, 100.f);
        TestEqual("Owner survives", LoadedAsset->OwnerCivilizationId,
            FName(TEXT("civilization.synara")));
        TestEqual("One history tag survives", Loaded.HistoryTags.Num(), 1);
        if (Loaded.HistoryTags.Num() == 1)
        {
            TestEqual("History value survives", Loaded.HistoryTags[0], FName(TEXT("history.first_habitat")));
        }
    });

    It("rejects an overlapping stale capture after damage and preserves the universal revision", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAWorldStateSubsystem* World = Fixture.GetSubsystem<UDAWorldStateSubsystem>();
        TestNotNull("Production campaign owner exists", World);
        if (World == nullptr) return;
        TestTrue("Two rival construction ticks establish a structural target",
            World->AdvanceWorldTicks(2));

        const FDACampaignSnapshot Base = World->GetPersistentCampaign();
        const FDAStructuralDamageRecord* BaseDamage =
            Base.OperationConflict.StructuralDamageRecords.IsEmpty()
                ? nullptr : &Base.OperationConflict.StructuralDamageRecords[0];
        TestNotNull("A canonical structural target exists", BaseDamage);
        if (BaseDamage == nullptr || BaseDamage->Modules.IsEmpty()) return;

        FDACampaignSnapshot StaleCaptureCandidate = Base;
        FDACaptureRecord& PendingCapture =
            StaleCaptureCandidate.OperationConflict.CaptureRecords.Emplace_GetRef();
        PendingCapture.WorldAssetId = BaseDamage->WorldAssetId;
        FString ValidationError;
        TestTrue("The overlapping capture candidate is independently valid",
            StaleCaptureCandidate.Validate(ValidationError));

        UDAStructuralDamageComponent* Damage =
            NewObject<UDAStructuralDamageComponent>();
        TestTrue("Damage binds to the production authority",
            Damage->InitializeFromCampaign(*World, BaseDamage->WorldAssetId));
        TestTrue("Damage commits first through canonical CAS",
            Damage->ApplyModuleDamage(BaseDamage->Modules[0].ModuleId, 1.f));
        const int64 AfterDamageRevision =
            World->GetPersistentCampaign().CampaignMutationRevision;
        TestEqual("Successful damage advances the universal revision",
            AfterDamageRevision, Base.CampaignMutationRevision + 1);
        TestFalse("The capture candidate from the same base is stale",
            World->TryCommitPersistentCampaign(StaleCaptureCandidate,
                Base.NarrativeState.MutationRevision,
                Base.LiveSignals.MutationRevision,
                Base.WorldState.CurrentWorldTick));

        FDACampaignSnapshot FreshCapture = World->GetPersistentCampaign();
        FDACaptureRecord& FreshPending =
            FreshCapture.OperationConflict.CaptureRecords.Emplace_GetRef();
        FreshPending.WorldAssetId = BaseDamage->WorldAssetId;
        const FDACampaignSnapshot& Authority = World->GetPersistentCampaign();
        TestTrue("A refreshed capture candidate commits after damage",
            World->TryCommitPersistentCampaign(FreshCapture,
                Authority.NarrativeState.MutationRevision,
                Authority.LiveSignals.MutationRevision,
                Authority.WorldState.CurrentWorldTick));
        TestEqual("Successful capture advances the same universal revision",
            World->GetPersistentCampaign().CampaignMutationRevision,
            AfterDamageRevision + 1);

        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Overlapping damage/capture authority saves",
            SaveService.SaveCampaign(World->GetPersistentCampaign(),
                TEXT("universal-revision")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded =
            SaveService.LoadCampaign(TEXT("universal-revision"));
        TestTrue("Overlapping damage/capture authority reloads", Loaded.HasValue());
        if (!Loaded.HasValue()) return;
        TestEqual("Universal campaign revision survives save/load",
            Loaded.GetValue().CampaignMutationRevision,
            World->GetPersistentCampaign().CampaignMutationRevision);
        TestNotNull("Committed damage survives save/load",
            Loaded.GetValue().OperationConflict.FindStructuralDamageRecord(
                BaseDamage->WorldAssetId));
        TestNotNull("Refreshed capture survives save/load",
            Loaded.GetValue().OperationConflict.FindCaptureRecord(
                BaseDamage->WorldAssetId));
    });

    It("round-trips an interrupted regional handoff through the canonical campaign save", [this]()
    {
        FDACampaignSnapshot Snapshot;
        Snapshot.WorldState = MakeInterruptedWorldState();

        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Interrupted campaign saves", SaveService.SaveCampaign(Snapshot, TEXT("interrupted-travel")).IsSuccess());

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(TEXT("interrupted-travel"));
        TestTrue("Interrupted campaign loads", LoadResult.HasValue());
        if (!LoadResult.HasValue())
        {
            return;
        }

        const FDAWorldCampaignState& Loaded = LoadResult.GetValue().WorldState;
        TestTrue("Regional authority remains initialized", Loaded.bInitialized);
        TestEqual("Strategic time survives", Loaded.CurrentWorldTick, 12LL);
        TestEqual("Source authority survives", Loaded.CurrentRegionId, FName(TEXT("region.synara_frontier")));
        TestEqual("Request identity survives", Loaded.TravelHandoff.RequestId, FName(TEXT("travel.save.interrupted")));
        TestEqual("Interrupted stage survives the save envelope", Loaded.TravelHandoff.Stage, EDATravelHandoffStage::DestinationLoaded);
        const FDARegionState* SourceRegion = Loaded.FindRegion(TEXT("region.synara_frontier"));
        TestNotNull("Source persistent delta survives", SourceRegion);
        if (SourceRegion != nullptr)
        {
            TestEqual("Source delta revision survives", SourceRegion->PersistentDelta.Revision, 3LL);
            TestEqual("Source actor reconstruction data survives", SourceRegion->PersistentDelta.LocalActors.Num(), 1);
        }
    });

    It("keeps the active campaign readable when final replacement fails", [this]()
    {
        const FString Slot = TEXT("failed-replacement");
        FDASaveService InitialService(TestSaveDirectory);
        TestTrue("Initial campaign save succeeds", InitialService.SaveCampaign(MakeHistorySnapshot(TEXT("history.before_failure")), Slot).IsSuccess());

        FDASaveService FailingService(TestSaveDirectory, MakeShared<FRejectingTransactionPlatform>());
        const FDASaveResult FailedSave = FailingService.SaveCampaign(MakeHistorySnapshot(TEXT("history.after_failure")), Slot);
        TestFalse("Injected final replacement fails", FailedSave.IsSuccess());
        TestTrue("Failure is reported at the atomic replacement boundary", FailedSave.Error.Code == EDASaveErrorCode::AtomicReplaceFailed);

        const FString ActivePath = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        TestTrue("Existing active slot remains present", FPlatformFileManager::Get().GetPlatformFile().FileExists(*ActivePath));

        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = InitialService.LoadCampaign(Slot);
        TestTrue("Existing active campaign still loads", LoadResult.HasValue());
        if (LoadResult.HasValue())
        {
            TestEqual("Existing active campaign keeps one history tag", LoadResult.GetValue().HistoryTags.Num(), 1);
        }
        if (LoadResult.HasValue() && LoadResult.GetValue().HistoryTags.Num() == 1)
        {
            TestEqual("Failed replacement does not expose new state", LoadResult.GetValue().HistoryTags[0], FName(TEXT("history.before_failure")));
        }
    });

    It("rejects duplicate stable conflict keys before writing a campaign", [this]()
    {
        FDACampaignSnapshot Snapshot;
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(30, 31, 32, 33);
        Asset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
        Asset.OwnerCivilizationId = TEXT("forgeweave");
        Asset.ConstructionState = EDAConstructionState::Disabled;
        Asset.StructuralIntegrity = 10.f;
        Snapshot.WorldAssets.Add(Asset);

        FDACaptureRecord First;
        First.WorldAssetId = Asset.WorldAssetId;
        FDACaptureRecord Duplicate;
        Duplicate.WorldAssetId = Asset.WorldAssetId;
        Snapshot.OperationConflict.CaptureRecords = { First, Duplicate };

        FDASaveService SaveService(TestSaveDirectory);
        const FDASaveResult Result = SaveService.SaveCampaign(Snapshot, TEXT("duplicate-conflict-key"));
        TestFalse("Duplicate WorldAssetId authority is rejected", Result.IsSuccess());
        TestTrue("Duplicate stable keys fail snapshot serialization validation", Result.Error.Code == EDASaveErrorCode::SerializationFailed);
    });

    It("rejects structural persistence whose global state contradicts total integrity", [this]()
    {
        FDACampaignSnapshot Snapshot;
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(34, 35, 36, 37);
        Asset.CardDefinitionId = TEXT("synara.synthetic_fabrication_node");
        Asset.OwnerCivilizationId = TEXT("synara");
        Asset.ConstructionState = EDAConstructionState::Disabled;
        Asset.StructuralIntegrity = 100.f;
        Snapshot.WorldAssets.Add(Asset);

        FDAStructuralDamageRecord Damage;
        Damage.WorldAssetId = Asset.WorldAssetId;
        Damage.CardDefinitionId = Asset.CardDefinitionId;
        Damage.Modules = { FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true) };
        Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);

        FDASaveService SaveService(TestSaveDirectory);
        const FDASaveResult Result = SaveService.SaveCampaign(Snapshot, TEXT("contradictory-structural-band"));
        TestFalse("A functional module record cannot make 100 percent global integrity Disabled", Result.IsSuccess());
        TestTrue("Contradictory integrity bands fail snapshot validation", Result.Error.Code == EDASaveErrorCode::SerializationFailed);
    });

    It("rejects structural persistence for definitions outside the canonical Core eligibility policy", [this]()
    {
        FDACampaignSnapshot Snapshot;
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(42, 43, 44, 45);
        Asset.CardDefinitionId = TEXT("synara.adaptive_habitat");
        Asset.OwnerCivilizationId = TEXT("synara");
        Asset.ConstructionState = EDAConstructionState::Operational;
        Asset.StructuralIntegrity = 100.f;
        Snapshot.WorldAssets.Add(Asset);

        FDAStructuralDamageRecord Damage;
        Damage.WorldAssetId = Asset.WorldAssetId;
        Damage.CardDefinitionId = Asset.CardDefinitionId;
        Damage.Modules = { FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true) };
        Snapshot.OperationConflict.StructuralDamageRecords.Add(Damage);

        FDASaveService SaveService(TestSaveDirectory);
        const FDASaveResult Result = SaveService.SaveCampaign(Snapshot, TEXT("non-eligible-structural-definition"));
        TestFalse("A non-eligible definition cannot save modular damage", Result.IsSuccess());
        TestTrue("Canonical eligibility fails snapshot serialization validation", Result.Error.Code == EDASaveErrorCode::SerializationFailed);
    });

    It("rejects a resolved capture whose durable outcome flags are incomplete", [this]()
    {
        FDACampaignSnapshot Snapshot;
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(38, 39, 40, 41);
        Asset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
        Asset.OwnerCivilizationId = TEXT("synara");
        Asset.ConstructionState = EDAConstructionState::Disabled;
        Asset.StructuralIntegrity = 10.f;
        Snapshot.WorldAssets.Add(Asset);

        FDACaptureRecord Capture;
        Capture.WorldAssetId = Asset.WorldAssetId;
        Capture.OriginalOwnerCivilizationId = TEXT("forgeweave");
        Capture.CapturingCivilizationId = TEXT("synara");
        Capture.CaptureProgressSeconds = 20.f;
        Capture.RequiredCaptureTimeSeconds = 20.f;
        Capture.bCaptureCompleted = true;
        Capture.bOutcomeResolved = true;
        Capture.Outcome = EDACaptureOutcome::Preserve;
        Capture.bRewardsGranted = true;
        Snapshot.OperationConflict.CaptureRecords.Add(Capture);

        FDASaveService SaveService(TestSaveDirectory);
        const FDASaveResult Result = SaveService.SaveCampaign(Snapshot, TEXT("incomplete-capture-outcome"));
        TestFalse("Preserve without its integration flag and history is rejected", Result.IsSuccess());
        TestTrue("Incomplete outcome invariants fail snapshot validation", Result.Error.Code == EDASaveErrorCode::SerializationFailed);
    });

    It("rejects a durable Salvage outcome without its matching structural ruin record", [this]()
    {
        FDACampaignSnapshot Snapshot;
        FDAWorldAssetRecord Asset;
        Asset.WorldAssetId = FGuid(46, 47, 48, 49);
        Asset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
        Asset.OwnerCivilizationId = TEXT("synara");
        Asset.ConstructionState = EDAConstructionState::Ruined;
        Asset.StructuralIntegrity = 0.f;
        Snapshot.WorldAssets.Add(Asset);

        FDACaptureRecord Capture;
        Capture.WorldAssetId = Asset.WorldAssetId;
        Capture.OriginalOwnerCivilizationId = TEXT("forgeweave");
        Capture.CapturingCivilizationId = TEXT("synara");
        Capture.CaptureProgressSeconds = 20.f;
        Capture.RequiredCaptureTimeSeconds = 20.f;
        Capture.bCaptureCompleted = true;
        Capture.bOutcomeResolved = true;
        Capture.Outcome = EDACaptureOutcome::Salvage;
        Capture.bOperationalUseRemoved = true;
        Capture.bRewardsGranted = true;
        Capture.History.Add(TEXT("capture.salvaged"));
        Snapshot.OperationConflict.CaptureRecords.Add(Capture);

        FDASaveService SaveService(TestSaveDirectory);
        const FDASaveResult Result = SaveService.SaveCampaign(Snapshot, TEXT("salvage-without-structural-ruin"));
        TestFalse("Salvage cannot persist without matching structural authority", Result.IsSuccess());
        TestTrue("Incomplete Salvage aggregate fails snapshot validation", Result.Error.Code == EDASaveErrorCode::SerializationFailed);
    });

    It("rotates the previous campaign to backup and recovers it when active is missing", [this]()
    {
        const FString Slot = TEXT("backup-recovery");
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("First campaign save succeeds", SaveService.SaveCampaign(MakeHistorySnapshot(TEXT("history.first")), Slot).IsSuccess());
        TestTrue("Second campaign save succeeds", SaveService.SaveCampaign(MakeHistorySnapshot(TEXT("history.second")), Slot).IsSuccess());

        const TResult<FDACampaignSnapshot, FDASaveError> CurrentLoad = SaveService.LoadCampaign(Slot);
        TestTrue("Second campaign is active", CurrentLoad.HasValue());
        if (CurrentLoad.HasValue())
        {
            TestEqual("Active slot keeps one history tag", CurrentLoad.GetValue().HistoryTags.Num(), 1);
        }
        if (CurrentLoad.HasValue() && CurrentLoad.GetValue().HistoryTags.Num() == 1)
        {
            TestEqual("Active slot contains second state", CurrentLoad.GetValue().HistoryTags[0], FName(TEXT("history.second")));
        }

        const FString ActivePath = FPaths::Combine(TestSaveDirectory, Slot + TEXT(".dasave"));
        const FString BackupPath = ActivePath + TEXT(".bak");
        FString BackupDocument;
        TestTrue("Previous active slot is retained as backup", FFileHelper::LoadFileToString(BackupDocument, *BackupPath));
        TestTrue("Backup contains first state", BackupDocument.Contains(TEXT("history.first")));
        TestFalse("Backup does not contain second state", BackupDocument.Contains(TEXT("history.second")));

        TestTrue("Interrupted-start fixture removes only active slot", FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*ActivePath));
        const TResult<FDACampaignSnapshot, FDASaveError> RecoveredLoad = SaveService.LoadCampaign(Slot);
        TestTrue("Missing active slot recovers from backup", RecoveredLoad.HasValue());
        if (RecoveredLoad.HasValue())
        {
            TestEqual("Recovered campaign keeps one history tag", RecoveredLoad.GetValue().HistoryTags.Num(), 1);
        }
        if (RecoveredLoad.HasValue() && RecoveredLoad.GetValue().HistoryTags.Num() == 1)
        {
            TestEqual("Recovered campaign is previous committed state", RecoveredLoad.GetValue().HistoryTags[0], FName(TEXT("history.first")));
        }
        TestTrue("Recovery republishes an active slot", FPlatformFileManager::Get().GetPlatformFile().FileExists(*ActivePath));
    });
}
