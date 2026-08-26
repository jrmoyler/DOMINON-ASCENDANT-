#include "Construction/DAConstructionComponent.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentManifest.h"
#include "World/DAWorldAsset.h"

#include "Cards/DACardInstance.h"
#include "City/DACityGridSubsystem.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "WorldAssetLifecycleTestObserver.h"

BEGIN_DEFINE_SPEC(FDAWorldAssetLifecycleSpec, "Dominion.Gameplay.WorldAssetLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAWorldAssetLifecycleSpec)

void FDAWorldAssetLifecycleSpec::Define()
{
    It("advances authored construction from foundation to operational after its definition cycles", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        FDABuiltManifestContent Built;
        TArray<FText> ContentErrors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, ContentErrors));
        TestTrue("Canonical content builds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Built, ContentErrors));
        UDA_CardDefinition* Definition = Built.FindDefinition(TEXT("synara.guardian_drone_cohort"));
        TestNotNull("Two-cycle authored fixture exists", Definition);
        if (!Definition) { return; }

        FCardInstance CardInstance;
        CardInstance.InstanceId = FGuid(1, 2, 3, 4);
        CardInstance.DefinitionId = Definition->DefinitionId;

        FDAWorldAssetRecord Record;
        Record.WorldAssetId = FGuid(5, 6, 7, 8);
        Record.CardInstanceId = CardInstance.InstanceId;
        Record.CardDefinitionId = CardInstance.DefinitionId;
        Record.CityId = FName("player_capital");
        Record.GridOrigin = FIntPoint(14, 22);
        Record.Rotation = 1;
        Record.ConstructionState = EDAConstructionState::Foundation;
        Record.StructuralIntegrity = 100.f;
        Record.OwnerCivilizationId = FName("synara");
        CardInstance.WorldAssetId = Record.WorldAssetId;

        FDACityGridSubsystem Grid;
        Grid.SetAllCellsClaimed(true);
        TestTrue("Grid reserves the record's authoritative asset identity", Grid.ReserveFootprint(FDAWorldAssetId(Record.WorldAssetId), Record.GridOrigin, FIntPoint(2, 2), EGridRotation::Ninety));

        UDAConstructionComponent* Construction = NewObject<UDAConstructionComponent>();
        UWorldAssetLifecycleTestObserver* Observer = NewObject<UWorldAssetLifecycleTestObserver>();
        Construction->OnStageChanged.AddDynamic(Observer, &UWorldAssetLifecycleTestObserver::HandleStageChanged);
        Construction->OnConstructionCompleted.AddDynamic(Observer, &UWorldAssetLifecycleTestObserver::HandleConstructionCompleted);
        TestTrue("Construction accepts the linked card instance and its definition", Construction->InitializeFromRecord(Record, &CardInstance, *Definition));

        TestTrue("First construction cycle advances", Construction->AdvanceCycle());
        TestTrue("First cycle has not completed construction", Record.ConstructionState != EDAConstructionState::Operational);
        TestEqual("Definition establishes the persisted required cycle count", Record.ConstructionCyclesRequired, 2);
        TestEqual("First cycle is persisted", Record.ConstructionCyclesCompleted, 1);
        TestEqual("First cycle emits a stage callback", Observer->StageChangeCount, 1);
        TestTrue("Stage callback reports the persisted stage", Observer->LastStage == Record.ConstructionState);
        TestEqual("First cycle does not emit a completion callback", Observer->CompletionCount, 0);

        ADAWorldAsset* ReconstructedActor = NewObject<ADAWorldAsset>();
        ReconstructedActor->InitializeFromRecord(Record);
        TestEqual("Actor reconstruction restores persisted progress", ReconstructedActor->GetWorldAssetRecord().ConstructionCyclesCompleted, 1);
        UDAConstructionComponent* ReboundConstruction = NewObject<UDAConstructionComponent>();
        ReboundConstruction->OnStageChanged.AddDynamic(Observer, &UWorldAssetLifecycleTestObserver::HandleStageChanged);
        ReboundConstruction->OnConstructionCompleted.AddDynamic(Observer, &UWorldAssetLifecycleTestObserver::HandleConstructionCompleted);
        TestTrue("Persistent record rebinds after actor reconstruction", ReboundConstruction->InitializeFromRecord(Record, &CardInstance, *Definition));

        TestTrue("Second construction cycle advances after record rebind", ReboundConstruction->AdvanceCycle());
        TestTrue("Definition construction cycle count reaches operational", Record.ConstructionState == EDAConstructionState::Operational);
        TestEqual("Completed cycle count survives reconstruction", Record.ConstructionCyclesCompleted, 2);
        TestEqual("Completion emits one additional stage callback", Observer->StageChangeCount, 2);
        TestEqual("Completion emits one completion callback", Observer->CompletionCount, 1);

        TestEqual("Reconstructed actor uses the persisted asset identity", ReconstructedActor->GetWorldAssetRecord().WorldAssetId, Record.WorldAssetId);
    });

    It("refuses to bind a world asset record without its linked card instance", [this]()
    {
        FDAVerticalSliceContentManifest Manifest;
        FDABuiltManifestContent Built;
        TArray<FText> ContentErrors;
        TestTrue("Canonical manifest parses", FDAContentManifestPipeline::LoadCanonical(Manifest, ContentErrors));
        TestTrue("Canonical content builds", FDAContentManifestPipeline::BuildRuntimeContent(Manifest, Built, ContentErrors));
        UDA_CardDefinition* Definition = Built.FindDefinition(TEXT("synara.guardian_drone_cohort"));
        TestNotNull("Two-cycle authored fixture exists", Definition);
        if (!Definition) { return; }

        FDAWorldAssetRecord Record;
        Record.WorldAssetId = FGuid(10, 11, 12, 13);
        Record.CardInstanceId = FGuid(14, 15, 16, 17);
        Record.CardDefinitionId = Definition->DefinitionId;
        Record.ConstructionState = EDAConstructionState::Foundation;

        UDAConstructionComponent* Construction = NewObject<UDAConstructionComponent>();
        TestFalse("Missing card instance cannot bind", Construction->InitializeFromRecord(Record, nullptr, *Definition));

        FCardInstance MismatchedCard;
        MismatchedCard.InstanceId = FGuid(18, 19, 20, 21);
        MismatchedCard.DefinitionId = Definition->DefinitionId;
        MismatchedCard.WorldAssetId = Record.WorldAssetId;
        TestFalse("Different card instance cannot bind", Construction->InitializeFromRecord(Record, &MismatchedCard, *Definition));
    });
}
