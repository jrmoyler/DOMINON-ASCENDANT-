#include "Narrative/DAFoundryShortageRuntime.h"

#include "Diplomacy/DADiplomacySystem.h"
#include "Misc/Crc.h"
#include "Narrative/DARegionalCampaignCoordinatorSubsystem.h"
#include "Save/DACampaignSaveGame.h"

namespace
{
    constexpr TCHAR MachineComponents[] = TEXT("good.machine_components");
    constexpr TCHAR FoundryEvent[] = TEXT("event.foundry_shortage");
    FName ResolutionName(const EDAFoundryShortageResolution Resolution)
    {
        switch (Resolution)
        {
        case EDAFoundryShortageResolution::IndustrialSupport: return TEXT("industrial_support");
        case EDAFoundryShortageResolution::EdenRestriction: return TEXT("eden_restriction");
        case EDAFoundryShortageResolution::BrokeredCompact: return TEXT("brokered_compact");
        case EDAFoundryShortageResolution::MarketExploitation: return TEXT("market_exploitation");
        case EDAFoundryShortageResolution::Collapse: return TEXT("collapse");
        default: return NAME_None;
        }
    }
    FGuid DeterministicGuid(const FString& Key)
    {
        return FGuid(FCrc::StrCrc32(*Key), FCrc::StrCrc32(*(Key + TEXT("|b"))),
            FCrc::StrCrc32(*(Key + TEXT("|c"))), FCrc::StrCrc32(*(Key + TEXT("|d"))));
    }
    void AddHistoryTag(FDACampaignSnapshot& Campaign, const FName Tag)
    {
        Campaign.HistoryTags.AddUnique(Tag);
        Campaign.HistoryTags.Sort([](const FName A, const FName B){ return A.LexicalLess(B); });
    }
    bool AppendMarket(FDACampaignSnapshot& Campaign, const FName MutationId,
        const double Modifier, const int64 WorldTick)
    {
        if (Campaign.WorldState.Trade.MarketPriceModifiers.ContainsByPredicate(
            [MutationId](const FDAMarketPriceModifierRecord& Row){ return Row.MutationId == MutationId; })) return true;
        FDAMarketPriceModifierRecord& Record = Campaign.WorldState.Trade.MarketPriceModifiers.Emplace_GetRef();
        Record.MutationId = MutationId; Record.GoodId = MachineComponents; Record.SourceEventId = FoundryEvent;
        Record.Modifier = Modifier; Record.WorldTick = WorldTick; return true;
    }
    EDAFoundryShortageStage StageForId(const FName StageId)
    {
        return StageId == TEXT("shortage_warning") ? EDAFoundryShortageStage::ShortageWarning
            : StageId == TEXT("market_spike") ? EDAFoundryShortageStage::MarketSpike
            : StageId == TEXT("ecological_dispute") ? EDAFoundryShortageStage::EcologicalDispute
            : StageId == TEXT("emergency_overdrive") ? EDAFoundryShortageStage::EmergencyOverdrive
            : EDAFoundryShortageStage::Inactive;
    }
}

bool FDAFoundryShortageRuntime::ProcessWorldTick(const FDARegionalCrisisManifest& Manifest,
    const int64 WorldTick, FDACampaignSnapshot& Campaign, UDADiplomacySystem& Diplomacy,
    FString& OutError)
{
    TArray<FText> Errors;
    const FDARegionalWorldEventEntry* Event = Manifest.FindEvent(FoundryEvent);
    if (!FDARegionalCrisisPipeline::Validate(Manifest, Errors) || WorldTick < 0
        || WorldTick != Campaign.WorldState.CurrentWorldTick || Event == nullptr)
    { OutError = TEXT("Foundry Shortage requires canonical content and the post-simulation candidate World Tick."); return false; }
    FDARegionalCrisisCampaignState& State = Campaign.RegionalCrisis;
    if (!State.bTriggered)
    {
        if (!FDARegionalCampaignCoordinatorRuntime::EvaluateEventTrigger(*Event, Campaign))
        { OutError.Reset(); return true; }
        const FDARegionalCrisisStageDefinition* Initial = Event->Stages.FindByPredicate(
            [Event](const FDARegionalCrisisStageDefinition& Stage){ return Stage.StageId == Event->InitialStageId; });
        if (Initial == nullptr || StageForId(Initial->StageId) == EDAFoundryShortageStage::Inactive)
        { OutError = TEXT("Foundry Shortage has no supported authored initial stage."); return false; }
        State.bTriggered = true; State.TriggerWorldTick = WorldTick;
        State.FoundryStage = StageForId(Initial->StageId);
        State.WarningEmissionCount = 1; State.LastTransitionWorldTick = WorldTick;
        State.ManifestFingerprint = Manifest.Fingerprint;
        AppendMarket(Campaign, FName(*FString::Printf(TEXT("market.foundry.warning.%lld"), static_cast<long long>(WorldTick))), Initial->PriceModifier, WorldTick);
        AddHistoryTag(Campaign, TEXT("foundry_shortage_warning"));
        OutError.Reset(); return true;
    }
    if (State.Resolution != EDAFoundryShortageResolution::None)
    {
        const FDAFoundryShortageResolutionRecord* ResolutionRecord = State.ResolutionRecords.IsEmpty()
            ? nullptr : &State.ResolutionRecords[0];
        if (ResolutionRecord == nullptr || State.RecoveryWorldTicks < 4) { OutError = TEXT("Resolved shortage lacks recovery authority."); return false; }
        const int64 Elapsed = WorldTick - State.ResolvedWorldTick;
        const double Modifier = Elapsed >= State.RecoveryWorldTicks ? 0.0
            : ResolutionRecord->MarketModifierAfter * static_cast<double>(State.RecoveryWorldTicks - Elapsed)
                / static_cast<double>(State.RecoveryWorldTicks);
        if (!FMath::IsNearlyEqual(Campaign.WorldState.Trade.GetMarketPriceModifier(MachineComponents), Modifier, 0.000001))
        {
            AppendMarket(Campaign, FName(*FString::Printf(TEXT("market.foundry.recovery.%lld"), static_cast<long long>(WorldTick))), Modifier, WorldTick);
            State.LastTransitionWorldTick = WorldTick;
        }
        OutError.Reset(); return true;
    }
    const int64 Elapsed = WorldTick - State.TriggerWorldTick;
    int64 TotalDuration = 0;
    for (const FDARegionalCrisisStageDefinition& Stage : Event->Stages)
        TotalDuration += Stage.DurationWorldTicks;
    if (Elapsed >= TotalDuration)
    {
        const FGuid CollapseAction = DeterministicGuid(FString::Printf(TEXT("foundry.collapse.%lld"), static_cast<long long>(State.TriggerWorldTick)));
        return Resolve(Manifest, CollapseAction, EDAFoundryShortageResolution::Collapse,
            WorldTick, Campaign, Diplomacy, OutError) != EDAFoundryShortageActionResult::Rejected;
    }
    const FDARegionalCrisisStageDefinition* DesiredDefinition = nullptr;
    int64 StageEnd = 0;
    for (const FDARegionalCrisisStageDefinition& Stage : Event->Stages)
    {
        StageEnd += Stage.DurationWorldTicks;
        if (Elapsed < StageEnd) { DesiredDefinition = &Stage; break; }
    }
    const EDAFoundryShortageStage Desired = DesiredDefinition == nullptr
        ? EDAFoundryShortageStage::Inactive : StageForId(DesiredDefinition->StageId);
    if (Desired == EDAFoundryShortageStage::Inactive)
    { OutError = TEXT("Foundry Shortage authored stage cannot map to persisted crisis state."); return false; }
    if (Desired != State.FoundryStage)
    {
        State.FoundryStage = Desired; State.LastTransitionWorldTick = WorldTick;
        AppendMarket(Campaign, FName(*FString::Printf(TEXT("market.foundry.stage.%d.%lld"),
            static_cast<int32>(Desired), static_cast<long long>(WorldTick))), DesiredDefinition->PriceModifier, WorldTick);
    }
    OutError.Reset(); return true;
}

EDAFoundryShortageActionResult FDAFoundryShortageRuntime::Resolve(
    const FDARegionalCrisisManifest& Manifest, const FGuid ActionId,
    const EDAFoundryShortageResolution Resolution, const int64 WorldTick,
    FDACampaignSnapshot& Campaign, UDADiplomacySystem& Diplomacy, FString& OutError)
{
    TArray<FText> Errors;
    const FDAFoundryShortageResolutionRecord* Existing = Campaign.RegionalCrisis.ResolutionRecords.FindByPredicate(
        [ActionId](const FDAFoundryShortageResolutionRecord& Row){ return Row.ActionId == ActionId; });
    if (Existing != nullptr)
    {
        if (Existing->Resolution == Resolution) { OutError.Reset(); return EDAFoundryShortageActionResult::AlreadyApplied; }
        OutError = TEXT("Foundry Shortage action ID was already used for a different resolution."); return EDAFoundryShortageActionResult::Rejected;
    }
    const FDARegionalWorldEventEntry* Event = Manifest.FindEvent(FoundryEvent);
    const FName AuthoredName = ResolutionName(Resolution);
    const FDARegionalCrisisResolutionDefinition* Authored = Event == nullptr ? nullptr : Event->FindResolution(AuthoredName);
    if (!ActionId.IsValid() || !FDARegionalCrisisPipeline::Validate(Manifest, Errors) || Authored == nullptr
        || !Campaign.RegionalCrisis.bTriggered || Campaign.RegionalCrisis.Resolution != EDAFoundryShortageResolution::None
        || WorldTick != Campaign.WorldState.CurrentWorldTick)
    { OutError = TEXT("Foundry Shortage resolution requires a fresh action, active event, and committed canonical clock."); return EDAFoundryShortageActionResult::Rejected; }
    FDACampaignSnapshot Candidate = Campaign;
    FDATradeRouteState* Route = Candidate.WorldState.Trade.FindRoute(TEXT("route.eden_ironheart_relief"));
    if (Route == nullptr || (Authored->TradeRouteCapacityDelta < 0 && Route->CapacityPerWorldTick < -Authored->TradeRouteCapacityDelta)
        || (Authored->TradeRouteCapacityDelta > 0 && Route->CapacityPerWorldTick > MAX_int64 - Authored->TradeRouteCapacityDelta))
    { OutError = TEXT("Foundry Shortage authored trade effect cannot be applied safely."); return EDAFoundryShortageActionResult::Rejected; }
    FDAFoundryShortageResolutionRecord Record;
    Record.ActionId = ActionId; Record.Resolution = Resolution; Record.ManifestFingerprint = Manifest.Fingerprint; Record.WorldTick = WorldTick;
    Record.JointCrisisHistoryRevisionAtResolution = Campaign.ConquestState.FindMutationRevision(
        TEXT("conquest.alliance.joint_crisis_success"));
    Record.TradeCapacityBefore = Route->CapacityPerWorldTick; Record.EcologyBefore = Candidate.WorldState.Ecology.Balance;
    Record.ResourceHungerBefore = Candidate.WorldState.Forgeweave.ResourceHunger;
    Record.CapitalBefore = Record.CapitalAfter = Candidate.LiveSignals.Capital;
    Record.InsightBefore = Record.InsightAfter = Candidate.LiveSignals.Insight;
    Record.InfluenceBefore = Record.InfluenceAfter = Candidate.LiveSignals.Influence;
    Route->CapacityPerWorldTick += Authored->TradeRouteCapacityDelta;
    if (Route->ReservedCapacityThisTick > Route->CapacityPerWorldTick)
    { OutError = TEXT("Foundry Shortage trade effect would invalidate reserved route capacity."); return EDAFoundryShortageActionResult::Rejected; }
    const FString ActionText = ActionId.ToString(EGuidFormats::Digits);
    if (!Candidate.WorldState.Ecology.ApplyReason(FName(*(TEXT("ecology.foundry.") + ActionText)),
        AuthoredName, Authored->EcologyDelta, WorldTick)
        || !Diplomacy.ApplyReason(Candidate, Authored->RelationshipId, Authored->RelationshipMetric,
            AuthoredName, Authored->RelationshipDelta, WorldTick, FName(*(TEXT("diplomacy.foundry.") + ActionText))))
    { OutError = TEXT("Foundry Shortage ecology/diplomacy effects rejected the atomic transaction."); return EDAFoundryShortageActionResult::Rejected; }
    Candidate.WorldState.Forgeweave.ResourceHunger = FMath::Clamp(
        Candidate.WorldState.Forgeweave.ResourceHunger + Authored->ResourceHungerDelta, 0.f, 100.f);
    AppendMarket(Candidate, FName(*(TEXT("market.foundry.resolve.") + ActionText)), Authored->MarketModifier, WorldTick);
    auto& State = Candidate.RegionalCrisis;
    State.FoundryStage = EDAFoundryShortageStage::Resolved; State.Resolution = Resolution;
    State.ResolvedWorldTick = WorldTick; State.LastTransitionWorldTick = WorldTick;
    State.RecoveryWorldTicks = Authored->RecoveryWorldTicks; State.CitizenOutcomes = Authored->CitizenOutcomes;
    for (const FName Tag : Authored->HistoryTags) AddHistoryTag(Candidate, Tag);
    for (const TPair<FName, FName>& Outcome : Authored->CitizenOutcomes) AddHistoryTag(Candidate, Outcome.Value);
    Record.TradeCapacityAfter = Route->CapacityPerWorldTick; Record.EcologyAfter = Candidate.WorldState.Ecology.Balance;
    Record.ResourceHungerAfter = Candidate.WorldState.Forgeweave.ResourceHunger; Record.MarketModifierAfter = Authored->MarketModifier;
    State.ResolutionRecords.Add(Record);
    FString ValidationError;
    if (!Candidate.Validate(ValidationError))
    { OutError = TEXT("Foundry Shortage transaction failed snapshot validation: ") + ValidationError; return EDAFoundryShortageActionResult::Rejected; }
    Campaign = MoveTemp(Candidate); OutError.Reset(); return EDAFoundryShortageActionResult::Applied;
}

FName FDAFoundryShortageRuntime::EvaluateDialogueVariant(const FName ConditionId,
    const FDACampaignSnapshot& Campaign)
{
    if (Campaign.RegionalCrisis.Resolution == EDAFoundryShortageResolution::None) return NAME_None;
    if (ConditionId == TEXT("dialogue.daxton.foundry_followup"))
        return Campaign.RegionalCrisis.CitizenOutcomes.FindRef(TEXT("citizen.forgeweave.mara_kest"));
    if (ConditionId == TEXT("dialogue.amara.foundry_followup"))
        return Campaign.RegionalCrisis.CitizenOutcomes.FindRef(TEXT("citizen.eden.ori_sen"));
    return NAME_None;
}

bool FDAFoundryShortageRuntime::EvaluateDialogueCondition(const FName ConditionId,
    const FDACampaignSnapshot& Campaign)
{
    return !EvaluateDialogueVariant(ConditionId, Campaign).IsNone();
}
