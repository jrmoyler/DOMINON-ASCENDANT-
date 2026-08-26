#include "Narrative/DAHistoryLedger.h"
#include "Narrative/DAPromiseLedger.h"
#include "Save/DASaveService.h"

#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/DASaveJsonFields.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

BEGIN_DEFINE_SPEC(FDAPromiseSpec, "Dominion.World.Narrative.Promise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
    FString TestSaveDirectory;
END_DEFINE_SPEC(FDAPromiseSpec)

namespace
{
    bool RewriteActionHistoryWithValidChecksum(const FString& Directory, const FString& Slot)
    {
        const FString Path = FPaths::Combine(Directory, Slot + TEXT(".dasave"));
        FString Json;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Json, *Path)) return false;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr) return false;
        TArray<TSharedPtr<FJsonValue>> EmptyHistory;
        (*Campaign)->SetArrayField(FDASaveJsonFields::HistoryTags, MoveTemp(EmptyHistory));
        const TSharedRef<FJsonObject> Material = MakeShared<FJsonObject>();
        Material->SetNumberField(FDASaveJsonFields::SchemaVersion, FDASaveService::CurrentSchemaVersion);
        Material->SetNumberField(FDASaveJsonFields::ContentVersion,
            FDASaveService::CurrentContentVersion);
        Material->SetNumberField(FDASaveJsonFields::BuildVersion,
            FDASaveService::CurrentBuildVersion);
        Material->SetObjectField(FDASaveJsonFields::Campaign, Campaign->ToSharedRef());
        FString MaterialJson;
        const TSharedRef<TJsonWriter<>> MaterialWriter = TJsonWriterFactory<>::Create(&MaterialJson);
        if (!FJsonSerializer::Serialize(Material, MaterialWriter)) return false;
        const FTCHARToUTF8 Utf8(*MaterialJson);
        Root->SetStringField(FDASaveJsonFields::Checksum,
            FString::Printf(TEXT("%08X"), FCrc::MemCrc32(Utf8.Get(), Utf8.Length())));
        Json.Reset();
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        return FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)
            && FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    FDAPromiseRecord MakePromise(
        const FGuid PromiseId,
        const FName DefinitionId,
        const FName ConflictTag,
        const FName FulfillmentTag)
    {
        FDAPromiseRecord Promise;
        Promise.PromiseId = PromiseId;
        Promise.PromiseDefinitionId = DefinitionId;
        Promise.PromiserId = TEXT("leader.amara_venn");
        Promise.ConflictActionTags.Add(ConflictTag);
        Promise.FulfillmentActionTags.Add(FulfillmentTag);
        Promise.CreatedWorldTick = 10;
        return Promise;
    }
}

void FDAPromiseSpec::Define()
{
    BeforeEach([this]()
    {
        TestSaveDirectory = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("DominionPromiseSaveTests"),
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
    });
    AfterEach([this]()
    {
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*TestSaveDirectory);
    });

    It("returns active broken promises for exact conflict tags in stable PromiseID order", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        const FDAPromiseRecord Later = MakePromise(
            FGuid(9, 0, 0, 0),
            TEXT("promise.preserve_eden_watershed"),
            TEXT("action.eden.watershed_damage"),
            TEXT("action.eden.watershed_restored"));
        const FDAPromiseRecord Earlier = MakePromise(
            FGuid(1, 0, 0, 0),
            TEXT("promise.avoid_military_escalation"),
            TEXT("action.forgeweave.military_escalation"),
            TEXT("action.forgeweave.peace_compact"));
        TestEqual("Later promise registers", Ledger.RegisterPromise(Later), EDAPromiseMutationResult::Applied);
        TestEqual("Earlier promise registers", Ledger.RegisterPromise(Earlier), EDAPromiseMutationResult::Applied);

        const TArray<FDAPromiseRecord> PrefixOnly = Ledger.GetBrokenPromises({TEXT("action.eden.watershed_damage.severe")});
        TestEqual("Prefix similarity is not a conflict", PrefixOnly.Num(), 0);

        const TArray<FDAPromiseRecord> Broken = Ledger.GetBrokenPromises({
            TEXT("action.forgeweave.military_escalation"),
            TEXT("action.eden.watershed_damage")});
        TestEqual("Both exact conflicts are exposed before commit", Broken.Num(), 2);
        if (Broken.Num() == 2)
        {
            TestEqual("Lowest stable PromiseID is first", Broken[0].PromiseId, Earlier.PromiseId);
            TestEqual("Highest stable PromiseID is second", Broken[1].PromiseId, Later.PromiseId);
        }
    });

    It("requires confirmation before a conflicting action commits", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        const FDAPromiseRecord Promise = MakePromise(
            FGuid(1, 2, 3, 4),
            TEXT("promise.protect_forgeweave_workers"),
            TEXT("action.forgeweave.workers_harmed"),
            TEXT("action.forgeweave.workers_protected"));
        TestEqual("Promise registers", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        const int64 RevisionBefore = Ledger.GetRevision();

        TestEqual(
            "Unconfirmed conflict is stopped at the commit guard",
            Ledger.CommitAction(
                FGuid(10, 20, 30, 40),
                {TEXT("action.forgeweave.workers_harmed")},
                15,
                RevisionBefore,
                false),
            EDAPromiseMutationResult::RequiresConfirmation);
        TestEqual("Rejected commit does not advance revision", Ledger.GetRevision(), RevisionBefore);
        TestFalse("Rejected commit writes no campaign history", FDAHistoryLedger::HasTag(Campaign, TEXT("action.forgeweave.workers_harmed")));
        TestEqual("Rejected commit leaves the promise active", Campaign.NarrativeState.PromiseRecords[0].State, EDAPromiseState::Active);
    });

    It("breaches a confirmed promise once and keeps the exact conflicting action tag", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        const FDAPromiseRecord Promise = MakePromise(
            FGuid(1, 2, 3, 4),
            TEXT("promise.preserve_eden_watershed"),
            TEXT("action.eden.watershed_damage"),
            TEXT("action.eden.watershed_restored"));
        TestEqual("Promise registers", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        const int64 RevisionBefore = Ledger.GetRevision();
        const FGuid ActionId(50, 60, 70, 80);

        TestEqual(
            "Confirmed conflict commits",
            Ledger.CommitAction(ActionId, {TEXT("action.eden.watershed_damage")}, 22, RevisionBefore, true),
            EDAPromiseMutationResult::Applied);
        const FDAPromiseRecord& Breached = Campaign.NarrativeState.PromiseRecords[0];
        TestEqual("Promise is breached", Breached.State, EDAPromiseState::Breached);
        TestEqual("Exact breach tag is retained", Breached.ResolutionActionTag, FName(TEXT("action.eden.watershed_damage")));
        TestEqual("Breach tick is retained", Breached.ResolvedWorldTick, 22LL);
        TestEqual("Promise retains the resolving ActionID", Breached.ResolutionActionId, ActionId);
        TestEqual("Action is canonical campaign history", Campaign.HistoryTags, TArray<FName>({TEXT("action.eden.watershed_damage")}));
        TestEqual("One semantic action is durable", Campaign.NarrativeState.ActionRecords.Num(), 1);
        if (Campaign.NarrativeState.ActionRecords.Num() == 1)
        {
            const FDANarrativeActionRecord& Action = Campaign.NarrativeState.ActionRecords[0];
            TestEqual("Action identity survives", Action.ActionId, ActionId);
            TestEqual("Normalized tags survive", Action.NormalizedActionTags, TArray<FName>({TEXT("action.eden.watershed_damage")}));
            TestEqual("Action tick survives", Action.WorldTick, 22LL);
            TestEqual("Breach result identity survives", Action.BreachedPromiseIds, TArray<FGuid>({Promise.PromiseId}));
        }

        const int64 RevisionAfter = Ledger.GetRevision();
        TestEqual(
            "Replaying the same stable ActionID is idempotent",
            Ledger.CommitAction(ActionId, {TEXT("action.eden.watershed_damage")}, 22, RevisionAfter, true),
            EDAPromiseMutationResult::AlreadyApplied);
        TestEqual("Replay writes no duplicate history", Campaign.HistoryTags.Num(), 1);
        TestEqual("Replay does not advance revision", Ledger.GetRevision(), RevisionAfter);
        TestEqual(
            "Reusing an ActionID with different tags is rejected",
            Ledger.CommitAction(ActionId, {TEXT("action.eden.watershed_restored")}, 22, RevisionAfter, true),
            EDAPromiseMutationResult::InvalidInput);
        TestEqual(
            "Reusing an ActionID with a different tick is rejected",
            Ledger.CommitAction(ActionId, {TEXT("action.eden.watershed_damage")}, 23, RevisionAfter, true),
            EDAPromiseMutationResult::InvalidInput);
        TestEqual("Conflicting replay remains atomic", Ledger.GetRevision(), RevisionAfter);
        TestEqual("Re-registering the immutable promise definition stays idempotent after breach", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::AlreadyApplied);
        TestEqual("Re-registration cannot reset breached state", Campaign.NarrativeState.PromiseRecords[0].State, EDAPromiseState::Breached);
    });

    It("validates promise and semantic action linkage in both directions", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        const FDAPromiseRecord Promise = MakePromise(FGuid(3, 1, 4, 1), TEXT("promise.bidirectional"),
            TEXT("action.bidirectional.conflict"), TEXT("action.bidirectional.fulfill"));
        TestEqual("Promise registers", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        const FGuid ActionId(5, 9, 2, 6);
        TestEqual("Promise resolves", Ledger.CommitAction(ActionId, {TEXT("action.bidirectional.fulfill")}, 20,
            Ledger.GetRevision(), false), EDAPromiseMutationResult::Applied);
        FString Error;
        TestTrue("Linked baseline validates", Campaign.Validate(Error));

        FDACampaignSnapshot MissingAction = Campaign;
        MissingAction.NarrativeState.ActionRecords.Reset();
        TestFalse("A nonlegacy resolved promise cannot lose its action record", MissingAction.Validate(Error));

        FDACampaignSnapshot MissingResult = Campaign;
        MissingResult.NarrativeState.ActionRecords[0].FulfilledPromiseIds.Reset();
        TestFalse("An action cannot omit its resolved PromiseID", MissingResult.Validate(Error));

        FDACampaignSnapshot ConflictingActionIdentity = Campaign;
        ConflictingActionIdentity.NarrativeState.PromiseRecords[0].ResolutionActionId = FGuid(8, 8, 8, 8);
        TestFalse("Promise and action identities must agree", ConflictingActionIdentity.Validate(Error));

        FDACampaignSnapshot MissingHistory = Campaign;
        MissingHistory.HistoryTags.Reset();
        TestFalse("Every nonlegacy action tag must exist in canonical campaign history", MissingHistory.Validate(Error));
        FDAPromiseLedger InvalidReplayLedger(MissingHistory);
        TestEqual("Identical replay cannot bless missing history", InvalidReplayLedger.CommitAction(ActionId,
            {TEXT("action.bidirectional.fulfill")}, 20, InvalidReplayLedger.GetRevision(), false),
            EDAPromiseMutationResult::InvalidInput);
    });

    It("requires persisted results to mirror CommitAction tag classification exactly", [this]()
    {
        FDACampaignSnapshot FulfilledCampaign;
        FDAPromiseLedger FulfilledLedger(FulfilledCampaign);
        FDAPromiseRecord FulfilledPromise = MakePromise(FGuid(12, 1, 1, 1), TEXT("promise.result.fulfilled"),
            TEXT("action.result.conflict.a"), TEXT("action.result.fulfill.z"));
        FulfilledPromise.ConflictActionTags.Add(TEXT("action.result.conflict.z"));
        FulfilledPromise.FulfillmentActionTags.Add(TEXT("action.result.fulfill.a"));
        TestEqual("Multi-tag fulfillment promise registers", FulfilledLedger.RegisterPromise(FulfilledPromise),
            EDAPromiseMutationResult::Applied);
        const FGuid FulfilledActionId(12, 2, 2, 2);
        const TArray<FName> FulfillmentTags = {
            TEXT("action.result.fulfill.z"), TEXT("action.result.unrelated"), TEXT("action.result.fulfill.a")};
        TestEqual("Canonical fulfillment commits", FulfilledLedger.CommitAction(FulfilledActionId, FulfillmentTags,
            20, FulfilledLedger.GetRevision(), false), EDAPromiseMutationResult::Applied);
        TestEqual("CommitAction records the lexical first exact fulfillment tag",
            FulfilledCampaign.NarrativeState.PromiseRecords[0].ResolutionActionTag,
            FName(TEXT("action.result.fulfill.a")));

        FString Error;
        FDACampaignSnapshot NoncanonicalFulfillment = FulfilledCampaign;
        NoncanonicalFulfillment.NarrativeState.PromiseRecords[0].ResolutionActionTag = TEXT("action.result.fulfill.z");
        TestFalse("A fulfilled promise cannot name a noncanonical matching tag", NoncanonicalFulfillment.Validate(Error));

        FDACampaignSnapshot FulfillmentWithConflict = FulfilledCampaign;
        FulfillmentWithConflict.NarrativeState.ActionRecords[0].NormalizedActionTags.Add(TEXT("action.result.conflict.a"));
        FulfillmentWithConflict.NarrativeState.ActionRecords[0].NormalizedActionTags.Sort(
            [](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        FulfillmentWithConflict.HistoryTags.Add(TEXT("action.result.conflict.a"));
        FulfillmentWithConflict.HistoryTags.Sort(
            [](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        TestFalse("A fulfilled result cannot carry any exact conflict tag", FulfillmentWithConflict.Validate(Error));
        FDAPromiseLedger InvalidFulfillmentReplay(FulfillmentWithConflict);
        TestEqual("Replay refuses a persisted simultaneous fulfillment/conflict match",
            InvalidFulfillmentReplay.CommitAction(FulfilledActionId,
                FulfillmentWithConflict.NarrativeState.ActionRecords[0].NormalizedActionTags,
                20, InvalidFulfillmentReplay.GetRevision(), true),
            EDAPromiseMutationResult::InvalidInput);

        FDACampaignSnapshot BreachedCampaign;
        FDAPromiseLedger BreachedLedger(BreachedCampaign);
        FDAPromiseRecord BreachedPromise = MakePromise(FGuid(13, 1, 1, 1), TEXT("promise.result.breached"),
            TEXT("action.breach.z"), TEXT("action.fulfill.a"));
        BreachedPromise.ConflictActionTags.Add(TEXT("action.breach.a"));
        BreachedPromise.FulfillmentActionTags.Add(TEXT("action.fulfill.z"));
        TestEqual("Multi-tag breach promise registers", BreachedLedger.RegisterPromise(BreachedPromise),
            EDAPromiseMutationResult::Applied);
        const FGuid BreachedActionId(13, 2, 2, 2);
        const TArray<FName> BreachTags = {TEXT("action.breach.z"), TEXT("action.breach.a")};
        TestEqual("Canonical breach commits", BreachedLedger.CommitAction(BreachedActionId, BreachTags,
            21, BreachedLedger.GetRevision(), true), EDAPromiseMutationResult::Applied);
        TestEqual("CommitAction records the lexical first exact conflict tag",
            BreachedCampaign.NarrativeState.PromiseRecords[0].ResolutionActionTag,
            FName(TEXT("action.breach.a")));

        FDACampaignSnapshot NoncanonicalBreach = BreachedCampaign;
        NoncanonicalBreach.NarrativeState.PromiseRecords[0].ResolutionActionTag = TEXT("action.breach.z");
        TestFalse("A breached promise cannot name a noncanonical matching tag", NoncanonicalBreach.Validate(Error));

        FDACampaignSnapshot BreachWithFulfillment = BreachedCampaign;
        BreachWithFulfillment.NarrativeState.ActionRecords[0].NormalizedActionTags.Add(TEXT("action.fulfill.a"));
        BreachWithFulfillment.NarrativeState.ActionRecords[0].NormalizedActionTags.Sort(
            [](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        BreachWithFulfillment.HistoryTags.Add(TEXT("action.fulfill.a"));
        BreachWithFulfillment.HistoryTags.Sort(
            [](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        TestFalse("A breached result cannot carry any exact fulfillment tag", BreachWithFulfillment.Validate(Error));
    });

    It("fulfills a promise without presenting it as broken", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        const FDAPromiseRecord Promise = MakePromise(
            FGuid(1, 2, 3, 4),
            TEXT("promise.deliver_industrial_components"),
            TEXT("action.forgeweave.components_withheld"),
            TEXT("action.forgeweave.components_delivered"));
        TestEqual("Promise registers", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        TestEqual("Fulfillment is not a broken promise", Ledger.GetBrokenPromises({TEXT("action.forgeweave.components_delivered")}).Num(), 0);

        const int64 RevisionBefore = Ledger.GetRevision();
        TestEqual(
            "Fulfillment commits without confirmation",
            Ledger.CommitAction(
                FGuid(5, 6, 7, 8),
                {TEXT("action.forgeweave.components_delivered")},
                18,
                RevisionBefore,
                false),
            EDAPromiseMutationResult::Applied);
        const FDAPromiseRecord& Fulfilled = Campaign.NarrativeState.PromiseRecords[0];
        TestEqual("Promise is fulfilled", Fulfilled.State, EDAPromiseState::Fulfilled);
        TestEqual("Exact fulfillment tag is retained", Fulfilled.ResolutionActionTag, FName(TEXT("action.forgeweave.components_delivered")));
    });

    It("preserves semantic ActionID replay guards across save and load", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        const FDAPromiseRecord Promise = MakePromise(FGuid(1, 8, 2, 8), TEXT("promise.saved_action"),
            TEXT("action.saved.conflict"), TEXT("action.saved.fulfill"));
        TestEqual("Promise registers", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        const FGuid ActionId(8, 6, 7, 5);
        TestEqual("Action commits", Ledger.CommitAction(ActionId, {TEXT("action.saved.fulfill")}, 30,
            Ledger.GetRevision(), false), EDAPromiseMutationResult::Applied);
        FDASaveService SaveService(TestSaveDirectory);
        TestTrue("Semantic action saves", SaveService.SaveCampaign(Campaign, TEXT("semantic-action")).IsSuccess());
        const TResult<FDACampaignSnapshot, FDASaveError> LoadResult = SaveService.LoadCampaign(TEXT("semantic-action"));
        TestTrue("Semantic action reloads", LoadResult.HasValue());
        if (!LoadResult.HasValue()) return;
        FDACampaignSnapshot Loaded = LoadResult.GetValue();
        FDAPromiseLedger ReloadedLedger(Loaded);
        const int64 Revision = ReloadedLedger.GetRevision();
        TestEqual("Identical saved replay is idempotent", ReloadedLedger.CommitAction(ActionId,
            {TEXT("action.saved.fulfill")}, 30, Revision, false), EDAPromiseMutationResult::AlreadyApplied);
        TestEqual("Different saved tag reuse is rejected", ReloadedLedger.CommitAction(ActionId,
            {TEXT("action.saved.conflict")}, 30, Revision, true), EDAPromiseMutationResult::InvalidInput);
        TestEqual("Different saved tick reuse is rejected", ReloadedLedger.CommitAction(ActionId,
            {TEXT("action.saved.fulfill")}, 31, Revision, false), EDAPromiseMutationResult::InvalidInput);
        TestEqual("Saved replay conflicts are atomic", ReloadedLedger.GetRevision(), Revision);

        TestTrue("Checksummed action history is removed",
            RewriteActionHistoryWithValidChecksum(TestSaveDirectory, TEXT("semantic-action")));
        const TResult<FDACampaignSnapshot, FDASaveError> Tampered = SaveService.LoadCampaign(TEXT("semantic-action"));
        TestFalse("Checksummed missing action history is rejected", Tampered.HasValue());
    });

    It("rejects stale or invalid commits atomically", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        const FDAPromiseRecord Promise = MakePromise(
            FGuid(1, 2, 3, 4),
            TEXT("promise.avoid_military_escalation"),
            TEXT("action.forgeweave.military_escalation"),
            TEXT("action.forgeweave.peace_compact"));
        TestEqual("Promise registers", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        const FDANarrativeCampaignState Before = Campaign.NarrativeState;

        TestEqual(
            "Stale expected revision is rejected",
            Ledger.CommitAction(
                FGuid(10, 20, 30, 40),
                {TEXT("action.forgeweave.military_escalation")},
                15,
                Ledger.GetRevision() - 1,
                true),
            EDAPromiseMutationResult::StaleRevision);
        TestEqual("Stale rejection writes no action records", Campaign.NarrativeState.ActionRecords.Num(), Before.ActionRecords.Num());
        TestEqual("Stale rejection writes no history", Campaign.HistoryTags.Num(), 0);

        TestEqual(
            "An invalid tag cannot partially mutate state",
            Ledger.CommitAction(FGuid(11, 22, 33, 44), {NAME_None}, 15, Ledger.GetRevision(), true),
            EDAPromiseMutationResult::InvalidInput);
        TestEqual("Invalid rejection leaves the promise active", Campaign.NarrativeState.PromiseRecords[0].State, EDAPromiseState::Active);
        TestEqual("Invalid rejection writes no action records", Campaign.NarrativeState.ActionRecords.Num(), Before.ActionRecords.Num());
        TestEqual("Invalid rejection writes no history", Campaign.HistoryTags.Num(), 0);
    });

    It("rejects an action that would fulfill and breach the same promise atomically", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        const FDAPromiseRecord Promise = MakePromise(
            FGuid(1, 2, 3, 4),
            TEXT("promise.preserve_eden_watershed"),
            TEXT("action.eden.watershed_damage"),
            TEXT("action.eden.watershed_restored"));
        TestEqual("Promise registers", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        const int64 RevisionBefore = Ledger.GetRevision();

        TestEqual(
            "Contradictory action is rejected",
            Ledger.CommitAction(
                FGuid(20, 30, 40, 50),
                {TEXT("action.eden.watershed_damage"), TEXT("action.eden.watershed_restored")},
                20,
                RevisionBefore,
                true),
            EDAPromiseMutationResult::InvalidInput);
        TestEqual("Contradictory action leaves the promise active", Campaign.NarrativeState.PromiseRecords[0].State, EDAPromiseState::Active);
        TestEqual("Contradictory action writes no history", Campaign.HistoryTags.Num(), 0);
        TestEqual("Contradictory action writes no semantic action", Campaign.NarrativeState.ActionRecords.Num(), 0);
        TestEqual("Contradictory action does not advance revision", Ledger.GetRevision(), RevisionBefore);
    });

    It("registers the same stable PromiseID idempotently but rejects a conflicting reuse", [this]()
    {
        FDACampaignSnapshot Campaign;
        FDAPromiseLedger Ledger(Campaign);
        FDAPromiseRecord Promise = MakePromise(
            FGuid(1, 2, 3, 4),
            TEXT("promise.protect_forgeweave_workers"),
            TEXT("action.forgeweave.workers_harmed"),
            TEXT("action.forgeweave.workers_protected"));
        TestEqual("First registration applies", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::Applied);
        const int64 RevisionAfterFirst = Ledger.GetRevision();
        TestEqual("Identical registration is idempotent", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::AlreadyApplied);
        TestEqual("Identical registration does not advance revision", Ledger.GetRevision(), RevisionAfterFirst);

        Promise.PromiseDefinitionId = TEXT("promise.reused_wrongly");
        TestEqual("Conflicting stable-ID reuse is rejected", Ledger.RegisterPromise(Promise), EDAPromiseMutationResult::InvalidInput);
        TestEqual("Conflicting reuse does not duplicate the record", Campaign.NarrativeState.PromiseRecords.Num(), 1);
    });
}
