#include "Save/DACampaignSaveGame.h"

#include "Content/DAContentManifest.h"
#include "Misc/Crc.h"

bool UDACampaignSaveGame::TryCommitPersistentCampaign(
    const FDACampaignSnapshot& Candidate,
    const int64 ExpectedNarrativeRevision,
    const int64 ExpectedSignalRevision,
    const int64 ExpectedWorldTick)
{
    FString Error;
    if (Snapshot.CampaignMutationRevision == MAX_int64
        || Candidate.CampaignMutationRevision != Snapshot.CampaignMutationRevision
        || Snapshot.NarrativeState.MutationRevision != ExpectedNarrativeRevision
        || Snapshot.LiveSignals.MutationRevision != ExpectedSignalRevision
        || Snapshot.WorldState.CurrentWorldTick != ExpectedWorldTick
        || Candidate.WorldState.CurrentWorldTick != ExpectedWorldTick
        || !Candidate.Validate(Error))
    {
        return false;
    }
    FDACampaignSnapshot Committed = Candidate;
    Committed.CampaignMutationRevision = Candidate.CampaignMutationRevision + 1;
    Snapshot = MoveTemp(Committed);
    return true;
}

namespace
{
    struct FFirstHourUnlockSpec
    {
        FName QuestId;
        EDAQuestContentUnlockType Type;
        FName DefinitionId;
        FName ContentId;
        int32 Quantity;
    };

    FGuid DeterministicRewardGuid(const FName ActionId, const int32 Index)
    {
        const FString Key = ActionId.ToString() + TEXT("|") + FString::FromInt(Index);
        return FGuid(FCrc::StrCrc32(*Key), FCrc::StrCrc32(*(Key + TEXT("|b"))),
            FCrc::StrCrc32(*(Key + TEXT("|c"))), FCrc::StrCrc32(*(Key + TEXT("|d"))));
    }

    const TMap<FName, FFirstHourUnlockSpec>& FirstHourUnlockSpecs()
    {
        static const TMap<FName, FFirstHourUnlockSpec> Specs = {
            {TEXT("reward.wake_the_hall.city_mode"), {TEXT("quest.wake_the_hall"), EDAQuestContentUnlockType::CityMode, NAME_None, TEXT("mode.city"), 1}},
            {TEXT("reward.wake_the_hall.adaptive_habitat"), {TEXT("quest.wake_the_hall"), EDAQuestContentUnlockType::CardInstance, TEXT("synara.adaptive_habitat"), NAME_None, 1}},
            {TEXT("reward.a_place_to_stay.microgrid_blueprint"), {TEXT("quest.a_place_to_stay"), EDAQuestContentUnlockType::Blueprint, TEXT("universal.microgrid_station"), NAME_None, 1}},
            {TEXT("reward.a_place_to_stay.water_blueprint"), {TEXT("quest.a_place_to_stay"), EDAQuestContentUnlockType::Blueprint, TEXT("universal.water_reclaimer"), NAME_None, 1}},
            {TEXT("reward.power_water_people.utility_systems"), {TEXT("quest.power_water_people"), EDAQuestContentUnlockType::UtilitySystems, NAME_None, TEXT("system.utilities"), 1}},
            {TEXT("reward.nia_needs_a_job.operator_xp"), {TEXT("quest.nia_needs_a_job"), EDAQuestContentUnlockType::OperatorXp, NAME_None, TEXT("citizen.synara.nia_vale"), 1}},
            {TEXT("reward.nia_needs_a_job.corner_exchange"), {TEXT("quest.nia_needs_a_job"), EDAQuestContentUnlockType::CardInstance, TEXT("universal.corner_exchange"), NAME_None, 1}},
            {TEXT("reward.replacement_model.insight"), {TEXT("quest.replacement_model"), EDAQuestContentUnlockType::InsightReward, NAME_None, TEXT("reward.insight.replacement_model"), 1}},
            {TEXT("reward.replacement_model.intelligence_auditor"), {TEXT("quest.replacement_model"), EDAQuestContentUnlockType::IntelligenceAuditorPath, NAME_None, TEXT("class.synara.intelligence_auditor"), 1}},
            {TEXT("reward.signal_in_foundation.axiom_fragment"), {TEXT("quest.signal_in_foundation"), EDAQuestContentUnlockType::AxiomArchiveFragment, NAME_None, TEXT("archive.axiom.fragment.01"), 1}},
            {TEXT("reward.iron_at_border.forgeweave_contact"), {TEXT("quest.iron_at_border"), EDAQuestContentUnlockType::DiplomacyContact, NAME_None, TEXT("civilization.forgeweave"), 1}},
            {TEXT("reward.basin_speaks.eden_trade_access"), {TEXT("quest.basin_speaks"), EDAQuestContentUnlockType::EdenTradeAccess, NAME_None, TEXT("trade.eden.access"), 1}}
        };
        return Specs;
    }

    bool ValidateFirstHourUnlockRecord(const FDAQuestContentUnlockRecord& Unlock,
        const FDANarrativeCampaignState& NarrativeState, const FDACollectionState& CollectionState,
        TSet<FGuid>& GrantedCardIds, FString& OutError)
    {
        const FFirstHourUnlockSpec* Spec = FirstHourUnlockSpecs().Find(Unlock.ActionId);
        const FDAQuestSaveState* Quest = NarrativeState.FindQuestState(Unlock.QuestId);
        if (Spec == nullptr || Quest == nullptr || Quest->ProgressState != EDAQuestProgressState::Completed
            || Unlock.QuestId != Spec->QuestId || Unlock.Type != Spec->Type
            || Unlock.DefinitionId != Spec->DefinitionId || Unlock.ContentId != Spec->ContentId
            || Unlock.Quantity != Spec->Quantity
            || Unlock.SourceFingerprint != FDAFirstHourFrozenPolicy::ManifestFingerprint()
            || Unlock.WorldTick != Quest->LastTransitionWorldTick)
        {
            OutError = TEXT("First-hour unlock does not match its exact authored action, quest, payload, fingerprint, and completion tick.");
            return false;
        }
        if (Unlock.Type != EDAQuestContentUnlockType::CardInstance)
        {
            if (!Unlock.GrantedCardInstanceIds.IsEmpty())
            {
                OutError = TEXT("Only card unlocks may reference collection instances.");
                return false;
            }
            return true;
        }
        if (Unlock.GrantedCardInstanceIds.Num() != Unlock.Quantity)
        {
            OutError = TEXT("Card unlock quantity must equal its concrete collection grants.");
            return false;
        }
        for (int32 Index = 0; Index < Unlock.GrantedCardInstanceIds.Num(); ++Index)
        {
            const FGuid InstanceId = Unlock.GrantedCardInstanceIds[Index];
            const FCardInstance* Instance = CollectionState.FindInstance(InstanceId);
            if (InstanceId != DeterministicRewardGuid(Unlock.ActionId, Index) || GrantedCardIds.Contains(InstanceId)
                || Instance == nullptr || Instance->InstanceId != InstanceId || Instance->DefinitionId != Unlock.DefinitionId
                || Instance->AcquisitionSource != EDAAcquisitionSource::QuestReward
                || Instance->AcquisitionWorldTick != Unlock.WorldTick)
            {
                OutError = TEXT("Quest card reward must resolve its unique deterministic collection instance and acquisition audit.");
                return false;
            }
            GrantedCardIds.Add(InstanceId);
        }
        return true;
    }

    struct FFirstHourBindingSpec
    {
        FName DefinitionId;
        FName EarliestCompletedNodeId;
        bool bRequireOperational;
    };

    const TMap<FString, FFirstHourBindingSpec>& FirstHourBindingSpecs()
    {
        static const TMap<FString, FFirstHourBindingSpec> Specs = {
            {TEXT("quest.wake_the_hall|founder_hall"), {TEXT("special.founder_hall"), NAME_None, false}},
            {TEXT("quest.a_place_to_stay|adaptive_habitat"), {TEXT("synara.adaptive_habitat"), TEXT("place_habitat"), false}},
            {TEXT("quest.power_water_people|adaptive_habitat"), {TEXT("synara.adaptive_habitat"), NAME_None, true}},
            {TEXT("quest.power_water_people|microgrid_station"), {TEXT("universal.microgrid_station"), TEXT("microgrid_operational"), true}},
            {TEXT("quest.power_water_people|water_reclaimer"), {TEXT("universal.water_reclaimer"), TEXT("water_operational"), true}},
            {TEXT("quest.nia_needs_a_job|cognitive_operations_tower"), {TEXT("synara.cognitive_operations_tower"), TEXT("tower_operational"), true}},
            {TEXT("quest.replacement_model|cognitive_operations_tower"), {TEXT("synara.cognitive_operations_tower"), NAME_None, true}},
            {TEXT("quest.replacement_model|autonomous_exchange"), {TEXT("synara.autonomous_exchange"), NAME_None, false}},
            {TEXT("quest.agency_has_a_price|agency_forum"), {TEXT("synara.agency_forum"), TEXT("build_forum"), true}},
            {TEXT("quest.signal_in_foundation|founder_hall"), {TEXT("special.founder_hall"), NAME_None, true}}
        };
        return Specs;
    }

    bool ValidateFirstHourBindingRecord(const FDAQuestObjectiveAssetBindingRecord& Binding,
        const FDANarrativeCampaignState& NarrativeState, const TArray<FDAWorldAssetRecord>& WorldAssets,
        FString& OutError)
    {
        const FString Key = Binding.QuestId.ToString() + TEXT("|") + Binding.BindingId.ToString();
        const FFirstHourBindingSpec* Spec = FirstHourBindingSpecs().Find(Key);
        const FDAQuestSaveState* Quest = NarrativeState.FindQuestState(Binding.QuestId);
        const FDAWorldAssetRecord* Asset = WorldAssets.FindByPredicate(
            [&Binding](const FDAWorldAssetRecord& Candidate) { return Candidate.WorldAssetId == Binding.WorldAssetId; });
        const FDAQuestNodeTransitionRecord* BindingTransition = Quest == nullptr || Spec == nullptr
            || Spec->EarliestCompletedNodeId.IsNone() ? nullptr
            : Quest->NodeTransitionRecords.FindByPredicate([Spec](const FDAQuestNodeTransitionRecord& Transition)
                { return Transition.CompletedNodeId == Spec->EarliestCompletedNodeId; });
        const bool bReachedBindingPath = Spec != nullptr && (Spec->EarliestCompletedNodeId.IsNone()
            || BindingTransition != nullptr);
        const int64 ExpectedBindWorldTick = Quest == nullptr ? INDEX_NONE
            : Spec != nullptr && Spec->EarliestCompletedNodeId.IsNone()
                ? Quest->StartedWorldTick : BindingTransition != nullptr ? BindingTransition->WorldTick : INDEX_NONE;
        const bool bOperationalRequiredNow = Spec != nullptr && (Spec->bRequireOperational
            || (Binding.QuestId == TEXT("quest.a_place_to_stay") && Quest != nullptr
                && (Quest->CompletedNodeIds.Contains(TEXT("habitat_operational")) || Quest->ProgressState == EDAQuestProgressState::Completed)));
        if (Spec == nullptr || Quest == nullptr || Asset == nullptr || !bReachedBindingPath
            || Binding.DefinitionId != Spec->DefinitionId || Asset->CardDefinitionId != Spec->DefinitionId
            || Binding.QuestDefinitionFingerprint != Quest->DefinitionManifest.DefinitionFingerprint
            || Binding.BindWorldTick != ExpectedBindWorldTick
            || Asset->CityId != TEXT("player_capital") || Asset->OwnerCivilizationId != TEXT("civilization.synara")
            || (bOperationalRequiredNow && Asset->ConstructionState != EDAConstructionState::Operational))
        {
            OutError = TEXT("First-hour objective binding must match its authored quest path and canonical player-city WorldAsset semantics.");
            return false;
        }
        return true;
    }

    bool RectanglesOverlap(
        const FDAWorldAssetRecord& LeftAsset,
        const FDAForgeweaveBuildingState& Left,
        const FDAWorldAssetRecord& RightAsset,
        const FDAForgeweaveBuildingState& Right)
    {
        return LeftAsset.GridOrigin.X < RightAsset.GridOrigin.X + Right.Footprint.X
            && LeftAsset.GridOrigin.X + Left.Footprint.X > RightAsset.GridOrigin.X
            && LeftAsset.GridOrigin.Y < RightAsset.GridOrigin.Y + Right.Footprint.Y
            && LeftAsset.GridOrigin.Y + Left.Footprint.Y > RightAsset.GridOrigin.Y;
    }

    FTransform MakeForgeweaveConstructionTransform(const FDAWorldAssetRecord& Asset)
    {
        return FTransform(
            FRotator::ZeroRotator,
            FDAForgeweaveCityState::GridCellToWorld(Asset.GridOrigin),
            FVector::OneVector);
    }

    FTransform MakeForgeweaveDefenseTransform()
    {
        return FTransform(
            FRotator::ZeroRotator,
            FDAForgeweaveCityState::GridCellToWorld(FIntPoint(16, 16)),
            FVector::OneVector);
    }

}

namespace
{
    bool AppendSynaraReason(TArray<FDASynaraValueReason>& Ledger, const FName ActionId,
        const FName SubjectId, const double Baseline, const double Delta, const double Minimum,
        const double Maximum, const int64 WorldTick, double& InOutValue)
    {
        if (ActionId.IsNone() || !FMath::IsFinite(Delta) || Delta == 0.0 || WorldTick < 0
            || Ledger.ContainsByPredicate([ActionId](const FDASynaraValueReason& Reason) { return Reason.ActionId == ActionId; })) return false;
        FDASynaraValueReason Reason; Reason.ActionId = ActionId; Reason.SubjectId = SubjectId;
        Reason.Baseline = Baseline; Reason.Delta = Delta; Reason.Result = FMath::Clamp(Baseline + Delta, Minimum, Maximum);
        Reason.WorldTick = WorldTick; InOutValue = Reason.Result; Ledger.Add(Reason);
        return true;
    }
}

bool FDASynaraCampaignState::ApplyDependencyReason(const FName ActionId, const double Delta, const int64 WorldTick)
{ return AppendSynaraReason(DependencyReasons, ActionId, NAME_None, Dependency, Delta, 0.0, 100.0, WorldTick, Dependency); }

bool FDASynaraCampaignState::ApplyFactionSupportReason(const FName ActionId, const FName FactionId,
    const double Delta, const int64 WorldTick)
{
    double* Value = FactionSupport.Find(FactionId); return Value != nullptr
        && AppendSynaraReason(FactionSupportReasons, ActionId, FactionId, *Value, Delta, 0.0, 100.0, WorldTick, *Value);
}

bool FDASynaraCampaignState::ApplyCitizenRelationshipReason(const FName ActionId, const FName CitizenId,
    const double Delta, const int64 WorldTick)
{
    double* Value = CitizenRelationships.Find(CitizenId); return Value != nullptr
        && AppendSynaraReason(CitizenRelationshipReasons, ActionId, CitizenId, *Value, Delta, -100.0, 100.0, WorldTick, *Value);
}

bool FDASynaraCampaignState::ApplyPolicyReason(const FName ActionId, const FName AuthorityId,
    const int32 Result, const int64 WorldTick)
{
    int32 Baseline = 0; int32 Maximum = 0;
    if (AuthorityId == TEXT("CapitalEfficiency")) { Baseline = static_cast<int32>(CapitalEfficiency); Maximum = 2; }
    else if (AuthorityId == TEXT("AgencyPetition")) { Baseline = static_cast<int32>(AgencyPetition); Maximum = 4; }
    else if (AuthorityId == TEXT("IronBorder")) { Baseline = static_cast<int32>(IronBorder); Maximum = 4; }
    else if (AuthorityId == TEXT("ForgeweaveTrust")) { Baseline = static_cast<int32>(ForgeweaveTrust); Maximum = 2; }
    else if (AuthorityId == TEXT("ForgeweaveRespect")) { Baseline = static_cast<int32>(ForgeweaveRespect); Maximum = 2; }
    else if (AuthorityId == TEXT("ForgeweaveDependence")) { Baseline = static_cast<int32>(ForgeweaveDependence); Maximum = 2; }
    else return false;
    if (ActionId.IsNone() || Result < 0 || Result > Maximum || WorldTick < 0
        || PolicyReasons.ContainsByPredicate([ActionId, AuthorityId](const FDASynaraPolicyReason& Reason)
            { return Reason.ActionId == ActionId && Reason.AuthorityId == AuthorityId; })) return false;
    FDASynaraPolicyReason Reason; Reason.ActionId = ActionId; Reason.AuthorityId = AuthorityId;
    Reason.Baseline = Baseline; Reason.Result = Result; Reason.WorldTick = WorldTick; PolicyReasons.Add(Reason);
    if (AuthorityId == TEXT("CapitalEfficiency")) CapitalEfficiency = static_cast<EDASynaraCapitalEfficiencyState>(Result);
    else if (AuthorityId == TEXT("AgencyPetition")) AgencyPetition = static_cast<EDAAgencyPetitionResolution>(Result);
    else if (AuthorityId == TEXT("IronBorder")) IronBorder = static_cast<EDAIronBorderResolution>(Result);
    else if (AuthorityId == TEXT("ForgeweaveTrust")) ForgeweaveTrust = static_cast<EDADiplomaticTrend>(Result);
    else if (AuthorityId == TEXT("ForgeweaveRespect")) ForgeweaveRespect = static_cast<EDADiplomaticTrend>(Result);
    else ForgeweaveDependence = static_cast<EDADiplomaticTrend>(Result);
    return true;
}

bool FDASynaraCampaignState::Validate(FString& OutError) const
{
    static const TSet<FName> Factions = {TEXT("faction.synara.ascendants"), TEXT("faction.synara.human_agency"),
        TEXT("faction.synara.synthetic_rights"), TEXT("faction.synara.moderates")};
    if (!FMath::IsFinite(Dependency) || Dependency < 0.0 || Dependency > 100.0 || FactionSupport.Num() != 4
        || CitizenRelationships.Num() != 1 || !CitizenRelationships.Contains(TEXT("citizen.synara.nia_vale"))
        || CitizenEmployment.Num() > 1
        || static_cast<uint8>(CapitalEfficiency) > static_cast<uint8>(EDASynaraCapitalEfficiencyState::Increased)
        || static_cast<uint8>(AgencyPetition) > static_cast<uint8>(EDAAgencyPetitionResolution::Rejected)
        || static_cast<uint8>(IronBorder) > static_cast<uint8>(EDAIronBorderResolution::Deferred)
        || static_cast<uint8>(ForgeweaveTrust) > static_cast<uint8>(EDADiplomaticTrend::Decreased)
        || static_cast<uint8>(ForgeweaveRespect) > static_cast<uint8>(EDADiplomaticTrend::Decreased)
        || static_cast<uint8>(ForgeweaveDependence) > static_cast<uint8>(EDADiplomaticTrend::Decreased))
    { OutError = TEXT("Synara campaign authority has invalid canonical aggregates."); return false; }
    for (const TPair<FName, double>& Value : FactionSupport)
        if (!Factions.Contains(Value.Key) || !FMath::IsFinite(Value.Value) || Value.Value < 0.0 || Value.Value > 100.0)
        { OutError = TEXT("Synara faction support authority is invalid."); return false; }
    for (const TPair<FName, double>& Value : CitizenRelationships)
        if (!FMath::IsFinite(Value.Value) || Value.Value < -100.0 || Value.Value > 100.0)
        { OutError = TEXT("Synara citizen relationship authority is invalid."); return false; }
    auto ValidateLedger = [&OutError](const TArray<FDASynaraValueReason>& Ledger, const double Minimum, const double Maximum,
        const TMap<FName, double>& Initial, const TMap<FName, double>& Aggregates)
    {
        TSet<FName> Actions;
        TMap<FName, double> Latest = Initial;
        TMap<FName, int64> LatestTicks;
        for (const FDASynaraValueReason& Reason : Ledger)
        {
            if (Reason.ActionId.IsNone() || Actions.Contains(Reason.ActionId) || Reason.WorldTick < 0
                || !FMath::IsFinite(Reason.Baseline) || !FMath::IsFinite(Reason.Delta) || Reason.Delta == 0.0 || !FMath::IsFinite(Reason.Result)
                || Reason.Result != FMath::Clamp(Reason.Baseline + Reason.Delta, Minimum, Maximum)
                || !Latest.Contains(Reason.SubjectId) || Latest[Reason.SubjectId] != Reason.Baseline
                // Ledger order plus the exact baseline/result chain orders multiple deterministic
                // mutations inside one World Tick; only time reversal is invalid.
                || (LatestTicks.Contains(Reason.SubjectId) && Reason.WorldTick < LatestTicks[Reason.SubjectId]))
            { OutError = TEXT("Synara authority reason ledger does not form an exact historical chain."); return false; }
            Actions.Add(Reason.ActionId); Latest.Add(Reason.SubjectId, Reason.Result); LatestTicks.Add(Reason.SubjectId, Reason.WorldTick);
        }
        if (!Latest.OrderIndependentCompareEqual(Aggregates))
        { OutError = TEXT("Synara aggregate must equal the latest durable reason result."); return false; }
        return true;
    };
    const TMap<FName, double> InitialDependency = {{NAME_None, 0.0}};
    const TMap<FName, double> AggregateDependency = {{NAME_None, Dependency}};
    TMap<FName, double> InitialFactions; for (const FName Faction : Factions) InitialFactions.Add(Faction, 25.0);
    const TMap<FName, double> InitialRelationships = {{TEXT("citizen.synara.nia_vale"), 0.0}};
    if (!ValidateLedger(DependencyReasons, 0.0, 100.0, InitialDependency, AggregateDependency)
        || !ValidateLedger(FactionSupportReasons, 0.0, 100.0, InitialFactions, FactionSupport)
        || !ValidateLedger(CitizenRelationshipReasons, -100.0, 100.0, InitialRelationships, CitizenRelationships)) return false;
    const TMap<FName, int32> PolicyMaximums = {{TEXT("CapitalEfficiency"), 2}, {TEXT("AgencyPetition"), 4},
        {TEXT("IronBorder"), 4}, {TEXT("ForgeweaveTrust"), 2}, {TEXT("ForgeweaveRespect"), 2}, {TEXT("ForgeweaveDependence"), 2}};
    TMap<FName, int32> LatestPolicies; for (const TPair<FName, int32>& Pair : PolicyMaximums) LatestPolicies.Add(Pair.Key, 0);
    TMap<FName, int64> LatestPolicyTicks;
    TSet<FString> PolicyKeys;
    for (const FDASynaraPolicyReason& Reason : PolicyReasons)
    {
        const int32* Maximum = PolicyMaximums.Find(Reason.AuthorityId);
        const FString Key = Reason.ActionId.ToString() + TEXT("|") + Reason.AuthorityId.ToString();
        if (Reason.ActionId.IsNone() || Maximum == nullptr || PolicyKeys.Contains(Key) || Reason.WorldTick < 0
            || Reason.Result < 0 || Reason.Result > *Maximum || LatestPolicies[Reason.AuthorityId] != Reason.Baseline
            || (LatestPolicyTicks.Contains(Reason.AuthorityId) && Reason.WorldTick <= LatestPolicyTicks[Reason.AuthorityId]))
        { OutError = TEXT("Synara policy reason ledger does not form an exact historical chain."); return false; }
        LatestPolicies[Reason.AuthorityId] = Reason.Result; LatestPolicyTicks.Add(Reason.AuthorityId, Reason.WorldTick); PolicyKeys.Add(Key);
    }
    const TMap<FName, int32> AggregatePolicies = {{TEXT("CapitalEfficiency"), static_cast<int32>(CapitalEfficiency)},
        {TEXT("AgencyPetition"), static_cast<int32>(AgencyPetition)}, {TEXT("IronBorder"), static_cast<int32>(IronBorder)},
        {TEXT("ForgeweaveTrust"), static_cast<int32>(ForgeweaveTrust)}, {TEXT("ForgeweaveRespect"), static_cast<int32>(ForgeweaveRespect)},
        {TEXT("ForgeweaveDependence"), static_cast<int32>(ForgeweaveDependence)}};
    if (!LatestPolicies.OrderIndependentCompareEqual(AggregatePolicies))
    { OutError = TEXT("Synara policy aggregates must equal the latest durable policy reason."); return false; }
    if (CitizenEmployment.Num() > 1)
    { OutError = TEXT("Nia can have only one canonical employment record."); return false; }
    for (const FDASynaraCitizenEmployment& Employment : CitizenEmployment)
        if (Employment.CitizenId != TEXT("citizen.synara.nia_vale") || Employment.CityId != TEXT("player_capital")
            || Employment.JobId.IsNone() || !Employment.FacilityWorldAssetId.IsValid())
        { OutError = TEXT("Synara employment must identify canonical citizen, city, job and facility."); return false; }
    return true;
}

const FDACampaignCitizenSignal* FDACampaignLiveSignalState::FindCitizen(const FName CitizenId) const
{
    return Citizens.FindByPredicate([CitizenId](const FDACampaignCitizenSignal& Signal)
        { return Signal.CitizenId == CitizenId; });
}

EDACampaignUtilitySupply FDACampaignLiveSignalState::ResolveUtility(
    const EDACampaignUtilityKind Utility, const FGuid WorldAssetId) const
{
    const FDACampaignUtilitySignal* Signal = UtilitySignals.FindByPredicate(
        [Utility, WorldAssetId](const FDACampaignUtilitySignal& Candidate)
        { return Candidate.Utility == Utility && Candidate.WorldAssetId == WorldAssetId; });
    return Signal == nullptr ? EDACampaignUtilitySupply::Offline : Signal->Supply;
}

bool FDACampaignLiveSignalState::Validate(FString& OutError) const
{
    if (Population < 0 || !FMath::IsFinite(Capital) || !FMath::IsFinite(Insight)
        || !FMath::IsFinite(Influence) || ResolvedDevelopmentCycles < 0 || MutationRevision < 0)
    { OutError = TEXT("Campaign live city/economy signals are invalid."); return false; }
    TSet<FName> CitizenIds;
    for (const FDACampaignCitizenSignal& Citizen : Citizens)
        if (Citizen.CitizenId.IsNone() || Citizen.CityId.IsNone() || CitizenIds.Contains(Citizen.CitizenId))
        { OutError = TEXT("Campaign citizen signals require stable unique citizen and city IDs."); return false; }
        else CitizenIds.Add(Citizen.CitizenId);
    TSet<FString> OpeningKeys;
    for (const FDACampaignJobOpeningSignal& Opening : JobOpenings)
    {
        const FString Key = Opening.JobId.ToString() + TEXT("|") + Opening.FacilityWorldAssetId.ToString();
        if (Opening.JobId.IsNone() || Opening.CityId.IsNone() || !Opening.FacilityWorldAssetId.IsValid()
            || Opening.OpenPositions < 0 || OpeningKeys.Contains(Key))
        { OutError = TEXT("Campaign job opening signals require stable facility identity."); return false; }
        OpeningKeys.Add(Key);
    }
    TSet<FName> AssignedCitizens;
    for (const FDACampaignJobAssignmentSignal& Assignment : JobAssignments)
        if (!CitizenIds.Contains(Assignment.CitizenId) || Assignment.JobId.IsNone()
            || !Assignment.FacilityWorldAssetId.IsValid() || AssignedCitizens.Contains(Assignment.CitizenId)
            || !JobOpenings.ContainsByPredicate([&Assignment](const FDACampaignJobOpeningSignal& Opening)
                { return Opening.JobId == Assignment.JobId
                    && Opening.FacilityWorldAssetId == Assignment.FacilityWorldAssetId; }))
        { OutError = TEXT("Campaign job assignments must resolve one persisted citizen and opening."); return false; }
        else AssignedCitizens.Add(Assignment.CitizenId);
    TSet<FString> UtilityKeys;
    for (const FDACampaignUtilitySignal& Signal : UtilitySignals)
    {
        const FString Key = Signal.WorldAssetId.ToString() + TEXT("|") + FString::FromInt(static_cast<int32>(Signal.Utility));
        if (!Signal.WorldAssetId.IsValid() || UtilityKeys.Contains(Key)
            || static_cast<uint8>(Signal.Supply) > static_cast<uint8>(EDACampaignUtilitySupply::Offline))
        { OutError = TEXT("Campaign utility signals require unique asset/network identities."); return false; }
        UtilityKeys.Add(Key);
    }
    return true;
}

FDAWorldAssetRecord* FDACampaignSnapshot::FindWorldAssetRecord(const FGuid WorldAssetId)
{
    return WorldAssets.FindByPredicate(
        [WorldAssetId](const FDAWorldAssetRecord& Record)
        {
            return Record.WorldAssetId == WorldAssetId;
        });
}

const FDAWorldAssetRecord* FDACampaignSnapshot::FindWorldAssetRecord(const FGuid WorldAssetId) const
{
    return WorldAssets.FindByPredicate(
        [WorldAssetId](const FDAWorldAssetRecord& Record)
        {
            return Record.WorldAssetId == WorldAssetId;
        });
}

FDACityGridClaimState* FDACampaignSnapshot::FindCityGridClaims(const FName CityId)
{
    return CityGridClaims.FindByPredicate([CityId](const FDACityGridClaimState& Row)
        { return Row.CityId == CityId; });
}

const FDACityGridClaimState* FDACampaignSnapshot::FindCityGridClaims(const FName CityId) const
{
    return CityGridClaims.FindByPredicate([CityId](const FDACityGridClaimState& Row)
        { return Row.CityId == CityId; });
}

bool FDARegionalCrisisCampaignState::Validate(const FDAWorldCampaignState& WorldState,
    const FDACampaignLiveSignalState&, const TArray<FName>& HistoryTags,
    FString& OutError) const
{
    constexpr const TCHAR* FrozenFingerprint = TEXT("1bc31247330a8bc0af7103aaa8b70b51d8cd5d7a");
    if (!bTriggered)
    {
        if (TriggerWorldTick != 0 || FoundryStage != EDAFoundryShortageStage::Inactive
            || WarningEmissionCount != 0 || LastTransitionWorldTick != 0
            || Resolution != EDAFoundryShortageResolution::None || ResolvedWorldTick != 0
            || RecoveryWorldTicks != 0 || !ManifestFingerprint.IsEmpty()
            || !CitizenOutcomes.IsEmpty() || !ResolutionRecords.IsEmpty())
        {
            OutError = TEXT("Inactive Foundry Shortage state must contain only canonical defaults.");
            return false;
        }
        return true;
    }
    if (ManifestFingerprint != FrozenFingerprint || TriggerWorldTick < 0
        || LastTransitionWorldTick < TriggerWorldTick || LastTransitionWorldTick > WorldState.CurrentWorldTick
        || WarningEmissionCount != 1
        || static_cast<uint8>(FoundryStage) > static_cast<uint8>(EDAFoundryShortageStage::Resolved)
        || FoundryStage == EDAFoundryShortageStage::Inactive)
    {
        OutError = TEXT("Foundry Shortage trigger, warning, stage, or fingerprint authority is invalid.");
        return false;
    }
    const bool bResolved = Resolution != EDAFoundryShortageResolution::None;
    if (bResolved != (FoundryStage == EDAFoundryShortageStage::Resolved)
        || (bResolved && (ResolvedWorldTick < TriggerWorldTick || ResolvedWorldTick > WorldState.CurrentWorldTick
            || RecoveryWorldTicks < 4 || RecoveryWorldTicks > 8 || ResolutionRecords.Num() != 1
            || CitizenOutcomes.Num() != 3))
        || (!bResolved && (ResolvedWorldTick != 0 || RecoveryWorldTicks != 0
            || !ResolutionRecords.IsEmpty() || !CitizenOutcomes.IsEmpty())))
    {
        OutError = TEXT("Foundry Shortage resolution authority is incomplete or contradictory.");
        return false;
    }
    if (!bResolved)
    {
        const int64 Elapsed = WorldState.CurrentWorldTick - TriggerWorldTick;
        const EDAFoundryShortageStage ExpectedStage = Elapsed < 2
            ? EDAFoundryShortageStage::ShortageWarning
            : Elapsed < 3 ? EDAFoundryShortageStage::MarketSpike
            : Elapsed < 4 ? EDAFoundryShortageStage::EcologicalDispute
            : EDAFoundryShortageStage::EmergencyOverdrive;
        const int64 ExpectedTransitionTick = ExpectedStage == EDAFoundryShortageStage::ShortageWarning
            ? TriggerWorldTick : ExpectedStage == EDAFoundryShortageStage::MarketSpike
                ? TriggerWorldTick + 2 : ExpectedStage == EDAFoundryShortageStage::EcologicalDispute
                    ? TriggerWorldTick + 3 : TriggerWorldTick + 4;
        const double ExpectedModifier = ExpectedStage == EDAFoundryShortageStage::ShortageWarning
            ? 0.20 : ExpectedStage == EDAFoundryShortageStage::EmergencyOverdrive ? 0.60 : 0.35;
        if (Elapsed < 0 || Elapsed >= 6 || FoundryStage != ExpectedStage
            || LastTransitionWorldTick != ExpectedTransitionTick
            || WorldState.Trade.GetMarketPriceModifier(TEXT("good.machine_components")) != ExpectedModifier
            || !HistoryTags.Contains(TEXT("foundry_shortage_warning")))
        {
            OutError = TEXT("Unresolved Foundry Shortage stage, market modifier, warning, and World Tick must match the authored timeline.");
            return false;
        }
        return true;
    }
    static const TSet<FName> CitizenIds = {TEXT("citizen.neutral.tal_arden"),
        TEXT("citizen.forgeweave.mara_kest"), TEXT("citizen.eden.ori_sen")};
    for (const TPair<FName, FName>& Outcome : CitizenOutcomes)
        if (!CitizenIds.Contains(Outcome.Key) || Outcome.Value.IsNone())
        { OutError = TEXT("Foundry Shortage citizen outcomes require canonical Tal, Mara, and Ori identities."); return false; }
    const FDAFoundryShortageResolutionRecord& Record = ResolutionRecords[0];
    struct FAuthoredResolution
    {
        int32 RecoveryTicks;
        FName SourceTag;
        int64 TradeDelta;
        double EcologyDelta;
        EDADiplomaticMetric Metric;
        float RelationshipDelta;
        float HungerDelta;
        double MarketModifier;
        TArray<FName> HistoryTags;
        TMap<FName, FName> Outcomes;
    };
    FAuthoredResolution Authored{};
    switch (Resolution)
    {
    case EDAFoundryShortageResolution::IndustrialSupport:
        Authored = {4, TEXT("industrial_support"), 20, -8.0, EDADiplomaticMetric::Grievance, 8.f, -30.f, 0.20,
            {TEXT("foundry_shortage_industrial_support")},
            {{TEXT("citizen.neutral.tal_arden"), TEXT("tal_forgeweave_pumps_installed")},
             {TEXT("citizen.forgeweave.mara_kest"), TEXT("mara_component_supply_secured")},
             {TEXT("citizen.eden.ori_sen"), TEXT("ori_watershed_warning_overruled")}}};
        break;
    case EDAFoundryShortageResolution::EdenRestriction:
        Authored = {4, TEXT("eden_restriction"), -10, 12.0, EDADiplomaticMetric::Trust, 10.f, -10.f, 0.20,
            {TEXT("foundry_shortage_eden_restriction")},
            {{TEXT("citizen.neutral.tal_arden"), TEXT("tal_eden_filtration_installed")},
             {TEXT("citizen.forgeweave.mara_kest"), TEXT("mara_emergency_shift_constrained")},
             {TEXT("citizen.eden.ori_sen"), TEXT("ori_watershed_boundary_protected")}}};
        break;
    case EDAFoundryShortageResolution::BrokeredCompact:
        Authored = {6, TEXT("brokered_compact"), 10, 5.0, EDADiplomaticMetric::Compatibility, 12.f, -20.f, 0.20,
            {TEXT("foundry_shortage_brokered_compact")},
            {{TEXT("citizen.neutral.tal_arden"), TEXT("tal_shared_reservoir_plan")},
             {TEXT("citizen.forgeweave.mara_kest"), TEXT("mara_audited_transition")},
             {TEXT("citizen.eden.ori_sen"), TEXT("ori_engineered_mitigation")}}};
        break;
    case EDAFoundryShortageResolution::MarketExploitation:
        Authored = {8, TEXT("market_exploitation"), 25, -15.0, EDADiplomaticMetric::Grievance, 15.f, 5.f, 0.60,
            {TEXT("foundry_shortage_market_exploitation")},
            {{TEXT("citizen.neutral.tal_arden"), TEXT("tal_shortage_endured")},
             {TEXT("citizen.forgeweave.mara_kest"), TEXT("mara_numbers_suppressed")},
             {TEXT("citizen.eden.ori_sen"), TEXT("ori_warning_ignored")}}};
        break;
    case EDAFoundryShortageResolution::Collapse:
        Authored = {8, TEXT("collapse"), -10, -20.0, EDADiplomaticMetric::Grievance, 20.f, 10.f, 0.60,
            {TEXT("foundry_shortage_ignored"), TEXT("foundry_shortage_collapse")},
            {{TEXT("citizen.neutral.tal_arden"), TEXT("tal_reservoir_failed")},
             {TEXT("citizen.forgeweave.mara_kest"), TEXT("mara_empty_shift_recorded")},
             {TEXT("citizen.eden.ori_sen"), TEXT("ori_watershed_damaged")}}};
        break;
    default:
        OutError = TEXT("Foundry Shortage resolution is outside the authored set.");
        return false;
    }
    const FString ActionText = Record.ActionId.ToString(EGuidFormats::Digits);
    const FName MarketMutationId(*(TEXT("market.foundry.resolve.") + ActionText));
    const FName EcologyMutationId(*(TEXT("ecology.foundry.") + ActionText));
    const FName DiplomacyMutationId(*(TEXT("diplomacy.foundry.") + ActionText));
    const FDAMarketPriceModifierRecord* MarketRecord = WorldState.Trade.MarketPriceModifiers.FindByPredicate(
        [MarketMutationId](const FDAMarketPriceModifierRecord& Candidate)
        { return Candidate.MutationId == MarketMutationId; });
    const FDAEcologyReason* EcologyReason = WorldState.Ecology.ReasonLedger.FindByPredicate(
        [EcologyMutationId](const FDAEcologyReason& Candidate)
        { return Candidate.MutationId == EcologyMutationId; });
    const FDADiplomaticRelationship* Relationship =
        WorldState.Diplomacy.FindRelationship(TEXT("relationship.synara.eden"));
    const FDADiplomaticReason* DiplomaticReason = Relationship == nullptr ? nullptr
        : Relationship->ReasonLedger.FindByPredicate([DiplomacyMutationId](const FDADiplomaticReason& Candidate)
            { return Candidate.MutationId == DiplomacyMutationId; });
    int64 ExpectedTradeCapacity = 0;
    const bool bTradeDeltaSafe = Authored.TradeDelta >= 0
        ? Record.TradeCapacityBefore <= MAX_int64 - Authored.TradeDelta
        : Record.TradeCapacityBefore >= -Authored.TradeDelta;
    if (bTradeDeltaSafe) ExpectedTradeCapacity = Record.TradeCapacityBefore + Authored.TradeDelta;
    bool bOutcomesMatch = CitizenOutcomes.Num() == Authored.Outcomes.Num();
    for (const TPair<FName, FName>& ExpectedOutcome : Authored.Outcomes)
    {
        const FName* ActualOutcome = CitizenOutcomes.Find(ExpectedOutcome.Key);
        bOutcomesMatch &= ActualOutcome != nullptr && *ActualOutcome == ExpectedOutcome.Value
            && HistoryTags.Contains(ExpectedOutcome.Value);
    }
    if (!Record.ActionId.IsValid() || Record.Resolution != Resolution
        || Record.ManifestFingerprint != ManifestFingerprint || Record.WorldTick != ResolvedWorldTick
        || RecoveryWorldTicks != Authored.RecoveryTicks
        || !bTradeDeltaSafe || Record.TradeCapacityAfter != ExpectedTradeCapacity
        || Record.EcologyAfter != FMath::Clamp(Record.EcologyBefore + Authored.EcologyDelta, 0.0, 100.0)
        || Record.ResourceHungerAfter != FMath::Clamp(Record.ResourceHungerBefore + Authored.HungerDelta, 0.f, 100.f)
        || Record.MarketModifierAfter != Authored.MarketModifier
        || !bOutcomesMatch
        || Authored.HistoryTags.ContainsByPredicate([&HistoryTags](const FName Tag)
            { return !HistoryTags.Contains(Tag); })
        || MarketRecord == nullptr || MarketRecord->GoodId != TEXT("good.machine_components")
        || MarketRecord->SourceEventId != TEXT("event.foundry_shortage")
        || MarketRecord->Modifier != Authored.MarketModifier || MarketRecord->WorldTick != ResolvedWorldTick
        || EcologyReason == nullptr || EcologyReason->SourceTag != Authored.SourceTag
        || EcologyReason->Baseline != Record.EcologyBefore || EcologyReason->Result != Record.EcologyAfter
        || EcologyReason->Delta != Authored.EcologyDelta || EcologyReason->WorldTick != ResolvedWorldTick
        || DiplomaticReason == nullptr || DiplomaticReason->SourceTag != Authored.SourceTag
        || DiplomaticReason->Metric != Authored.Metric
        || DiplomaticReason->Magnitude != Authored.RelationshipDelta
        || DiplomaticReason->WorldTick != ResolvedWorldTick
        || Record.CapitalBefore != Record.CapitalAfter
        || Record.InsightBefore != Record.InsightAfter
        || Record.InfluenceBefore != Record.InfluenceAfter)
    {
        OutError = TEXT("Foundry Shortage resolution record does not reconcile with atomic live authorities.");
        return false;
    }
    if (!HistoryTags.ContainsByPredicate([](const FName Tag)
        { return Tag.ToString().StartsWith(TEXT("foundry_shortage_")); }))
    { OutError = TEXT("Foundry Shortage resolution is missing its durable history outcome tag."); return false; }
    return true;
}

namespace
{
    bool IsCompletedConquestQuest(const FDACampaignSnapshot& Campaign, const FName QuestId)
    {
        const FDAQuestSaveState* Quest = Campaign.NarrativeState.FindQuestState(QuestId);
        return Quest != nullptr && Quest->ProgressState == EDAQuestProgressState::Completed;
    }

    bool MatchesConquestGuid(const FGuid Guid, const FName SourceId)
    {
        return Guid.IsValid() && Guid.ToString(EGuidFormats::Digits) == SourceId.ToString();
    }

    bool HasFulfilledConquestContract(const FDACampaignSnapshot& Campaign, const FName ContractId)
    {
        const FDATradeContractState* Contract = Campaign.WorldState.Trade.Contracts.FindByPredicate(
            [ContractId](const FDATradeContractState& Row){ return Row.ContractId == ContractId; });
        return Contract != nullptr && Contract->RelationshipId == TEXT("relationship.synara.forgeweave")
            && (Contract->SourceRegionId == TEXT("region.ironheart")
                || Contract->DestinationRegionId == TEXT("region.ironheart"))
            && Contract->bCompleted && Contract->SuccessfulDeliveryCount > 0
            && Campaign.WorldState.Trade.Deliveries.ContainsByPredicate([Contract](const FDATradeDeliveryRecord& Delivery)
                { return Delivery.ContractId == Contract->ContractId && Delivery.Quantity > 0; });
    }

    const FDAFoundryShortageResolutionRecord* FindConquestCrisisRecord(
        const FDACampaignSnapshot& Campaign, const FName SourceId)
    {
        return Campaign.RegionalCrisis.ResolutionRecords.FindByPredicate(
            [&Campaign, SourceId](const FDAFoundryShortageResolutionRecord& Record)
            {
                return MatchesConquestGuid(Record.ActionId, SourceId)
                    && Record.Resolution == Campaign.RegionalCrisis.Resolution;
            });
    }

    bool CrisisRecordPrecedesMutation(const FDAFoundryShortageResolutionRecord& Record,
        const FDAConquestMeterMutation& Mutation, const int64 MutationRevision)
    {
        return Record.WorldTick < Mutation.WorldTick
            || (Record.WorldTick == Mutation.WorldTick
                && (Record.JointCrisisHistoryRevisionAtResolution == 0
                    || MutationRevision > Record.JointCrisisHistoryRevisionAtResolution));
    }

    double AllianceCrisisBaseContributionBeforeMutation(
        const FDACampaignSnapshot& Campaign, const FDAConquestMeterMutation& Mutation,
        const int64 MutationRevision)
    {
        const FDAFoundryShortageResolutionRecord* Record =
            Campaign.RegionalCrisis.ResolutionRecords.FindByPredicate(
                [&Campaign](const FDAFoundryShortageResolutionRecord& Row)
                {
                    return Row.ActionId.IsValid() && Row.Resolution == Campaign.RegionalCrisis.Resolution;
                });
        if (Record == nullptr || !CrisisRecordPrecedesMutation(*Record, Mutation, MutationRevision)
            || Record->Resolution == EDAFoundryShortageResolution::None
            || Record->Resolution == EDAFoundryShortageResolution::Collapse) return 0.0;
        return Record->Resolution == EDAFoundryShortageResolution::BrokeredCompact ? 25.0 : 16.25;
    }

    const FDADiplomaticReason* FindConquestDiplomaticReason(
        const FDADiplomaticRelationship& Relationship, const FName MutationId)
    {
        return Relationship.ReasonLedger.FindByPredicate(
            [MutationId](const FDADiplomaticReason& Reason)
            {
                return Reason.MutationId == MutationId;
            });
    }

    bool IsDependenceThresholdReason(const FDADiplomaticRelationship& Relationship,
        const FDADiplomaticReason& Target)
    {
        float Dependence = 0.f;
        for (const FDADiplomaticReason& Reason : Relationship.ReasonLedger)
        {
            if (Reason.Metric != EDADiplomaticMetric::Dependence) continue;
            const float Before = Dependence;
            Dependence += Reason.Magnitude;
            if (Before < 25.f && Dependence >= 25.f) return Reason.MutationId == Target.MutationId;
        }
        return false;
    }

    double AllianceReasonDelta(const FDADiplomaticRelationship& Relationship,
        const FDADiplomaticReason& Target)
    {
        double RawTotal = 0.0;
        double ClampedTotal = 0.0;
        for (const FDADiplomaticReason& Reason : Relationship.ReasonLedger)
        {
            if (Reason.Metric != Target.Metric) continue;
            const double Before = ClampedTotal;
            RawTotal += Reason.Magnitude;
            ClampedTotal = FMath::Clamp(RawTotal, 0.0, 100.0);
            if (Reason.MutationId == Target.MutationId) return (ClampedTotal - Before) / 4.0;
        }
        return 0.0;
    }

    bool ValidateConquestMutationSources(const FDACampaignSnapshot& Campaign, FString& OutError)
    {
        TSet<FString> ConsumedSources;
        double AllianceCrisisContribution = 0.0;
        bool bJointCrisisHistoryConsumed = false;
        const FDAFoundryShortageResolutionRecord* CanonicalCrisisRecord =
            Campaign.RegionalCrisis.ResolutionRecords.FindByPredicate(
                [&Campaign](const FDAFoundryShortageResolutionRecord& Record)
                {
                    return Record.ActionId.IsValid()
                        && Record.Resolution == Campaign.RegionalCrisis.Resolution;
                });
        const FDAConquestMeterMutation* JointHistory = Campaign.ConquestState.FindMutation(
            TEXT("conquest.alliance.joint_crisis_success"));
        const int64 JointHistoryRevision = Campaign.ConquestState.FindMutationRevision(
            TEXT("conquest.alliance.joint_crisis_success"));
        if (CanonicalCrisisRecord != nullptr
            && (CanonicalCrisisRecord->JointCrisisHistoryRevisionAtResolution < 0
                || CanonicalCrisisRecord->JointCrisisHistoryRevisionAtResolution
                    > Campaign.ConquestState.MutationRevision
                || (CanonicalCrisisRecord->JointCrisisHistoryRevisionAtResolution > 0
                    && (JointHistory == nullptr
                        || CanonicalCrisisRecord->JointCrisisHistoryRevisionAtResolution
                            != JointHistoryRevision
                        || JointHistory->WorldTick > CanonicalCrisisRecord->WorldTick))
                || (CanonicalCrisisRecord->JointCrisisHistoryRevisionAtResolution == 0
                    && JointHistory != nullptr
                    && JointHistory->WorldTick < CanonicalCrisisRecord->WorldTick)))
        {
            OutError = TEXT("Foundry crisis causal proof must name the exact prior joint-history conquest revision.");
            return false;
        }
        for (const FDAConquestMeterMutation& Mutation : Campaign.ConquestState.Mutations)
        {
            const int64 MutationRevision = Campaign.ConquestState.FindMutationRevision(Mutation.MutationId);
            bool bSourceValid = false;
            bool bAllianceCrisisSource = false;
            FName ExpectedMutationId;
            if (Mutation.SourceAuthority == TEXT("world.region.control_zone"))
            {
                const FDARegionState* Ironheart = Campaign.WorldState.FindRegion(TEXT("region.ironheart"));
                ExpectedMutationId = FName(*(TEXT("conquest.force.zone.") + Mutation.SourceId.ToString()));
                bSourceValid = Ironheart != nullptr
                    && Mutation.SourceId.ToString().StartsWith(TEXT("control_zone.synara."))
                    && Ironheart->PersistentDelta.StateTags.Contains(Mutation.SourceId)
                    && Mutation.Route == EDAForgeweaveRoute::Force
                    && Mutation.Meter == EDAConquestMeter::MilitarySovereignty && Mutation.Delta == -15.0;
            }
            else if (Mutation.SourceAuthority == TEXT("conflict.capture"))
            {
                ExpectedMutationId = FName(*(TEXT("conquest.force.capture.") + Mutation.SourceId.ToString()));
                const FDACaptureRecord* Capture = Campaign.OperationConflict.CaptureRecords.FindByPredicate(
                    [&Mutation](const FDACaptureRecord& Row){ return MatchesConquestGuid(Row.WorldAssetId, Mutation.SourceId); });
                const FDAWorldAssetRecord* Asset = Capture == nullptr ? nullptr
                    : Campaign.FindWorldAssetRecord(Capture->WorldAssetId);
                bSourceValid = Capture != nullptr && Asset != nullptr && Capture->bCaptureCompleted
                    && Capture->bOutcomeResolved
                    && Capture->OriginalOwnerCivilizationId == TEXT("civilization.forgeweave")
                    && Capture->CapturingCivilizationId == TEXT("civilization.synara")
                    && (Asset->CardDefinitionId == TEXT("forgeweave.heavy_carrier")
                        || Asset->CardDefinitionId == TEXT("forgeweave.command_bastion")
                        || Asset->CardDefinitionId == TEXT("forgeweave.grand_forge"))
                    && Mutation.Route == EDAForgeweaveRoute::Force
                    && Mutation.Meter == EDAConquestMeter::MilitarySovereignty && Mutation.Delta == -20.0;
            }
            else if (Mutation.SourceAuthority == TEXT("conflict.structural_damage"))
            {
                ExpectedMutationId = FName(*(TEXT("conquest.force.structure.") + Mutation.SourceId.ToString()));
                const FDAStructuralDamageRecord* Damage = Campaign.OperationConflict.StructuralDamageRecords.FindByPredicate(
                    [&Mutation](const FDAStructuralDamageRecord& Row){ return MatchesConquestGuid(Row.WorldAssetId, Mutation.SourceId); });
                bSourceValid = Damage != nullptr && Damage->bProductionDisabled
                    && (Damage->CardDefinitionId == TEXT("forgeweave.city_shield")
                        || Damage->CardDefinitionId == TEXT("forgeweave.command_core"))
                    && Mutation.Route == EDAForgeweaveRoute::Force
                    && Mutation.Meter == EDAConquestMeter::MilitarySovereignty && Mutation.Delta == -25.0;
            }
            else if (Mutation.SourceAuthority == TEXT("trade.contract"))
            {
                ExpectedMutationId = FName(*(TEXT("conquest.economic.contract.") + Mutation.SourceId.ToString()));
                bSourceValid = HasFulfilledConquestContract(Campaign, Mutation.SourceId)
                    && Mutation.Route == EDAForgeweaveRoute::Economic
                    && Mutation.Meter == EDAConquestMeter::EconomicAutonomy && Mutation.Delta == -15.0;
            }
            else if (Mutation.SourceAuthority == TEXT("world.region.owner"))
            {
                ExpectedMutationId = TEXT("conquest.economic.freight_share");
                const FDARegionState* Region = Campaign.WorldState.FindRegion(Mutation.SourceId);
                bSourceValid = Region != nullptr && Mutation.SourceId == TEXT("region.freight_corridor")
                    && Region->OwnerId == TEXT("civilization.synara")
                    && Mutation.Route == EDAForgeweaveRoute::Economic
                    && Mutation.Meter == EDAConquestMeter::EconomicAutonomy && Mutation.Delta == -15.0;
            }
            else if (Mutation.SourceAuthority == TEXT("diplomacy.reason"))
            {
                const FDADiplomaticRelationship* Relationship =
                    Campaign.WorldState.Diplomacy.FindRelationship(TEXT("relationship.synara.forgeweave"));
                const FDADiplomaticReason* Reason = Relationship == nullptr ? nullptr
                    : FindConquestDiplomaticReason(*Relationship, Mutation.SourceId);
                if (Mutation.Route == EDAForgeweaveRoute::Economic)
                {
                    ExpectedMutationId = FName(*(TEXT("conquest.economic.component_dependence.")
                        + Mutation.SourceId.ToString()));
                    bSourceValid = Relationship != nullptr && Reason != nullptr
                        && IsDependenceThresholdReason(*Relationship, *Reason)
                        && Relationship->Dependence >= 25.f
                        && Mutation.Meter == EDAConquestMeter::EconomicAutonomy
                        && Mutation.Delta == -15.0
                        && Campaign.WorldState.Trade.Contracts.ContainsByPredicate(
                        [&Campaign](const FDATradeContractState& Contract)
                        {
                            return HasFulfilledConquestContract(Campaign, Contract.ContractId);
                        });
                }
                else if (Mutation.Route == EDAForgeweaveRoute::Alliance)
                {
                    ExpectedMutationId = FName(*(TEXT("conquest.alliance.reason.")
                        + Mutation.SourceId.ToString()));
                    bSourceValid = Relationship != nullptr && Reason != nullptr
                        && (Reason->Metric == EDADiplomaticMetric::Trust
                            || Reason->Metric == EDADiplomaticMetric::Respect
                            || Reason->Metric == EDADiplomaticMetric::Compatibility)
                        && Mutation.Meter == EDAConquestMeter::AllianceReadiness
                        && FMath::IsNearlyEqual(Mutation.Delta,
                            AllianceReasonDelta(*Relationship, *Reason), 0.0001)
                        && !FMath::IsNearlyZero(Mutation.Delta, 0.0001);
                }
            }
            else if (Mutation.SourceAuthority == TEXT("campaign.history"))
            {
                const bool bForce = Mutation.SourceId == TEXT("forgeweave_elite_defeated")
                    && Mutation.Route == EDAForgeweaveRoute::Force && Mutation.Delta == -20.0;
                const bool bWorker = (Mutation.SourceId == TEXT("workers_protected")
                        || Mutation.SourceId == TEXT("mara_numbers_worker_coalition"))
                    && Campaign.HistoryTags.Contains(Mutation.SourceId)
                    && IsCompletedConquestQuest(Campaign, TEXT("quest.workers_signal"))
                    && Mutation.Route == EDAForgeweaveRoute::Influence && Mutation.Delta == -20.0;
                const bool bCredibility = (Mutation.SourceId == TEXT("mara_evidence_exposed")
                        || Mutation.SourceId == TEXT("grand_forge_preserved"))
                    && Mutation.Route == EDAForgeweaveRoute::Influence && Mutation.Delta == -10.0;
                const double AvailableCrisisBase = AllianceCrisisBaseContributionBeforeMutation(
                    Campaign, Mutation, MutationRevision);
                const double ExpectedAllianceCrisisDelta = 25.0 - AllianceCrisisContribution;
                const bool bAllianceCrisis = Mutation.SourceId == TEXT("joint_forgeweave_crisis_success")
                    && Mutation.Route == EDAForgeweaveRoute::Alliance
                    && Mutation.Meter == EDAConquestMeter::AllianceReadiness
                    && FMath::IsNearlyEqual(AllianceCrisisContribution, AvailableCrisisBase, 0.0001)
                    && ExpectedAllianceCrisisDelta > 0.0
                    && FMath::IsNearlyEqual(Mutation.Delta, ExpectedAllianceCrisisDelta, 0.0001);
                bAllianceCrisisSource = bAllianceCrisis;
                bSourceValid = Campaign.HistoryTags.Contains(Mutation.SourceId)
                    && (bForce || bWorker || bCredibility || bAllianceCrisis)
                    && ((bForce && Mutation.Meter == EDAConquestMeter::MilitarySovereignty)
                        || ((bWorker || bCredibility) && Mutation.Meter == EDAConquestMeter::CivicLegitimacy)
                        || bAllianceCrisis);
                ExpectedMutationId = bForce ? FName(TEXT("conquest.force.elite_defeated"))
                    : bWorker ? FName(TEXT("conquest.influence.worker_endorsement"))
                    : bCredibility ? FName(*(TEXT("conquest.influence.credibility.")
                        + Mutation.SourceId.ToString()))
                    : bAllianceCrisis ? FName(TEXT("conquest.alliance.joint_crisis_success")) : NAME_None;
            }
            else if (Mutation.SourceAuthority.ToString().StartsWith(
                TEXT("campaign.regional_crisis_resolution.")))
            {
                const FDAFoundryShortageResolutionRecord* Record =
                    FindConquestCrisisRecord(Campaign, Mutation.SourceId);
                const bool bRecordAvailable = Record != nullptr
                    && CrisisRecordPrecedesMutation(*Record, Mutation, MutationRevision);
                const bool bEconomic = Record != nullptr
                    && bRecordAvailable
                    && Mutation.SourceAuthority == TEXT("campaign.regional_crisis_resolution.economic")
                    && Record->Resolution == EDAFoundryShortageResolution::MarketExploitation
                    && Mutation.Route == EDAForgeweaveRoute::Economic
                    && Mutation.Meter == EDAConquestMeter::EconomicAutonomy && Mutation.Delta == -15.0;
                const bool bInfluence = Record != nullptr
                    && bRecordAvailable
                    && Mutation.SourceAuthority == TEXT("campaign.regional_crisis_resolution.influence")
                    && Record->Resolution != EDAFoundryShortageResolution::None
                    && Record->Resolution != EDAFoundryShortageResolution::Collapse
                    && Mutation.Route == EDAForgeweaveRoute::Influence
                    && Mutation.Meter == EDAConquestMeter::CivicLegitimacy && Mutation.Delta == -25.0;
                const double ExpectedAllianceDelta = Record != nullptr
                    && Record->Resolution == EDAFoundryShortageResolution::BrokeredCompact ? 25.0 : 16.25;
                const double DesiredAllianceContribution = bJointCrisisHistoryConsumed
                    ? 25.0 : ExpectedAllianceDelta;
                const bool bAlliance = Record != nullptr
                    && bRecordAvailable
                    && Mutation.SourceAuthority == TEXT("campaign.regional_crisis_resolution.alliance")
                    && Record->Resolution != EDAFoundryShortageResolution::None
                    && Record->Resolution != EDAFoundryShortageResolution::Collapse
                    && Mutation.Route == EDAForgeweaveRoute::Alliance
                    && Mutation.Meter == EDAConquestMeter::AllianceReadiness
                    && DesiredAllianceContribution > AllianceCrisisContribution
                    && FMath::IsNearlyEqual(Mutation.Delta,
                        DesiredAllianceContribution - AllianceCrisisContribution, 0.0001);
                bAllianceCrisisSource = bAlliance;
                bSourceValid = bEconomic || bInfluence || bAlliance;
                const FString Prefix = bEconomic ? TEXT("conquest.economic.emergency_finance.")
                    : bInfluence ? TEXT("conquest.influence.service_crisis.")
                    : bAlliance ? TEXT("conquest.alliance.crisis.") : FString();
                ExpectedMutationId = Prefix.IsEmpty()
                    ? NAME_None : FName(*(Prefix + Mutation.SourceId.ToString()));
            }
            else if (Mutation.SourceAuthority == TEXT("narrative.quest"))
            {
                ExpectedMutationId = TEXT("conquest.influence.workers_signal");
                bSourceValid = Mutation.SourceId == TEXT("quest.workers_signal")
                    && IsCompletedConquestQuest(Campaign, Mutation.SourceId)
                    && Mutation.Route == EDAForgeweaveRoute::Influence
                    && Mutation.Meter == EDAConquestMeter::CivicLegitimacy && Mutation.Delta == -30.0;
            }
            else if (Mutation.SourceAuthority == TEXT("campaign.faction_support_reason"))
            {
                ExpectedMutationId = FName(*(TEXT("conquest.influence.faction_support.")
                    + Mutation.SourceId.ToString()));
                const FDASynaraValueReason* Reason = Campaign.SynaraState.FactionSupportReasons.FindByPredicate(
                    [&Mutation](const FDASynaraValueReason& Row)
                    {
                        return Row.ActionId == Mutation.SourceId
                            && Row.SubjectId == TEXT("faction.synara.human_agency")
                            && Row.Baseline < 65.0 && Row.Result >= 65.0;
                    });
                bSourceValid = Reason != nullptr
                    && Campaign.SynaraState.FactionSupport.FindRef(TEXT("faction.synara.human_agency")) >= 65.0
                    && Mutation.Route == EDAForgeweaveRoute::Influence
                    && Mutation.Meter == EDAConquestMeter::CivicLegitimacy && Mutation.Delta == -15.0;
            }
            const FString SourceKey = Mutation.SourceAuthority.ToString() + TEXT("|")
                + Mutation.SourceId.ToString();
            if (!bSourceValid || Mutation.MutationId != ExpectedMutationId
                || ConsumedSources.Contains(SourceKey))
            {
                OutError = TEXT("Every conquest mutation must consume one exact canonical source and mutation id exactly once.");
                return false;
            }
            ConsumedSources.Add(SourceKey);
            if (bAllianceCrisisSource)
            {
                AllianceCrisisContribution += Mutation.Delta;
                bJointCrisisHistoryConsumed |= Mutation.SourceAuthority == TEXT("campaign.history");
            }
        }

        FDAAllianceReadinessComponents ExpectedComponents;
        const FDADiplomaticRelationship* Forge = Campaign.WorldState.Diplomacy.FindRelationship(
            TEXT("relationship.synara.forgeweave"));
        if (Forge != nullptr)
        {
            ExpectedComponents.Trust = FMath::Clamp<double>(Forge->Trust, 0.0, 100.0);
            ExpectedComponents.Respect = FMath::Clamp<double>(Forge->Respect, 0.0, 100.0);
            ExpectedComponents.SharedInterest = FMath::Clamp<double>(Forge->Compatibility, 0.0, 100.0);
        }
        ExpectedComponents.CrisisResolution = Campaign.RegionalCrisis.Resolution
                == EDAFoundryShortageResolution::BrokeredCompact
            || Campaign.HistoryTags.Contains(TEXT("joint_forgeweave_crisis_success")) ? 100.0
            : Campaign.RegionalCrisis.Resolution != EDAFoundryShortageResolution::None
                && Campaign.RegionalCrisis.Resolution != EDAFoundryShortageResolution::Collapse ? 65.0 : 0.0;
        const FDAAllianceReadinessComponents& StoredComponents = Campaign.ConquestState.AllianceComponents;
        const bool bHasAllianceProjection = Campaign.ConquestState.Mutations.ContainsByPredicate(
            [](const FDAConquestMeterMutation& Row){ return Row.Route == EDAForgeweaveRoute::Alliance; });
        if (bHasAllianceProjection && !FMath::IsNearlyEqual(AllianceCrisisContribution,
            ExpectedComponents.CrisisResolution / 4.0, 0.0001))
        {
            OutError = TEXT("Alliance crisis evidence must equal its one canonical saturated component.");
            return false;
        }
        if (bHasAllianceProjection
            && (!FMath::IsNearlyEqual(StoredComponents.Trust, ExpectedComponents.Trust, 0.0001)
                || !FMath::IsNearlyEqual(StoredComponents.Respect, ExpectedComponents.Respect, 0.0001)
                || !FMath::IsNearlyEqual(StoredComponents.SharedInterest, ExpectedComponents.SharedInterest, 0.0001)
                || !FMath::IsNearlyEqual(StoredComponents.CrisisResolution,
                    ExpectedComponents.CrisisResolution, 0.0001)))
        {
            OutError = TEXT("Alliance readiness components must exactly reconcile with canonical diplomacy and crisis authorities.");
            return false;
        }

        return FDAConquestAuthorityValidator::ValidateResolvedRoute(Campaign, OutError);
    }

    int32 CopyLimitForRarity(const EDARarity Rarity)
    {
        switch (Rarity)
        {
        case EDARarity::Common:
        case EDARarity::Specialized: return 3;
        case EDARarity::Elite: return 2;
        case EDARarity::Legendary:
        case EDARarity::Mythic:
        case EDARarity::Wonder:
        case EDARarity::Leader: return 1;
        default: return 0;
        }
    }

    bool ValidateCardOwnershipGraph(const FDACampaignSnapshot& Campaign, FString& OutError)
    {
        const bool bHasCanonicalCardAuthority = Campaign.WorldState.bInitialized
            && (Campaign.WorldState.ClockAuthority.bCaptured
                || Campaign.CitySimulationState.bInitialized
                || !Campaign.DeckState.GetInstanceIds().IsEmpty()
                || !Campaign.CollectionState.Instances.IsEmpty());
        if (!bHasCanonicalCardAuthority)
        {
            return true;
        }
        const FDADeckState& Deck = Campaign.DeckState;
        if (Deck.GetInstanceIds().Num() != FDADeckState::RequiredDeckSize)
        {
            OutError = TEXT("Initialized campaign deck must contain exactly 60 owned instances.");
            return false;
        }

        FDAVerticalSliceContentManifest Manifest;
        TArray<FText> ManifestErrors;
        if (!FDAContentManifestPipeline::LoadCanonical(Manifest, ManifestErrors))
        {
            OutError = TEXT("Campaign validation could not load the canonical content manifest.");
            return false;
        }
        TMap<FName, int32> ExpectedStarterCopies;
        for (const FDAManifestDeckEntry& Entry : Manifest.StarterDeck)
        {
            ExpectedStarterCopies.Add(Entry.DefinitionId, Entry.Quantity);
        }
        TMap<FName, int32> ActualStarterCopies;
        for (const TPair<FGuid, FCardInstance>& Pair : Campaign.CollectionState.Instances)
        {
            if (Pair.Value.AcquisitionSource == EDAAcquisitionSource::StarterDeck)
            {
                ++ActualStarterCopies.FindOrAdd(Pair.Value.DefinitionId);
            }
        }
        if (!ActualStarterCopies.OrderIndependentCompareEqual(ExpectedStarterCopies))
        {
            OutError = TEXT("Campaign collection must retain the exact authored 60-card starter acquisition partition.");
            return false;
        }

        TSet<FGuid> Members;
        TMap<FName, int32> DeckCopies;
        for (const FGuid InstanceId : Deck.GetInstanceIds())
        {
            const FCardInstance* Instance = Campaign.CollectionState.FindInstance(InstanceId);
            if (!InstanceId.IsValid() || Instance == nullptr || Members.Contains(InstanceId))
            {
                OutError = TEXT("Deck membership must be unique and owned by the canonical collection.");
                return false;
            }
            Members.Add(InstanceId);
            ++DeckCopies.FindOrAdd(Instance->DefinitionId);
        }
        for (const TPair<FName, int32>& Copies : DeckCopies)
        {
            const FDAManifestCardDefinition* Definition = Manifest.Definitions.FindByPredicate(
                [&Copies](const FDAManifestCardDefinition& Row)
                { return Row.DefinitionId == Copies.Key; });
            if (Definition == nullptr || Copies.Value > CopyLimitForRarity(Definition->Rarity))
            {
                OutError = TEXT("Deck membership violates the canonical authored rarity copy limit.");
                return false;
            }
        }

        TSet<FGuid> Zoned;
        const TArray<FGuid>* Zones[] = {
            &Deck.GetDrawPile(), &Deck.GetHand(), &Deck.GetReserveQueue(), &Deck.GetDeployed()};
        for (const TArray<FGuid>* Zone : Zones)
        {
            for (const FGuid InstanceId : *Zone)
            {
                if (!Members.Contains(InstanceId) || Zoned.Contains(InstanceId))
                {
                    OutError = TEXT("Every deck instance must occupy exactly one unique runtime zone.");
                    return false;
                }
                Zoned.Add(InstanceId);
            }
        }
        if (Zoned.Num() != Members.Num())
        {
            OutError = TEXT("Draw pile, hand, reserve, and deployed must exactly partition all 60 deck instances.");
            return false;
        }

        for (const FGuid InstanceId : Deck.GetInstanceIds())
        {
            const FCardInstance* Card = Campaign.CollectionState.FindInstance(InstanceId);
            const bool bInDeployedZone = Deck.GetDeployed().Contains(InstanceId);
            const bool bHasWorldDeployment = Card != nullptr
                && Card->WorldAssetId.IsValid()
                && (Card->RecoveryState == EDARecoveryState::Deployed
                    || Card->RecoveryState == EDARecoveryState::Ruined);
            if (Card == nullptr || bInDeployedZone != bHasWorldDeployment
                || (!bInDeployedZone
                    && (Card->RecoveryState != EDARecoveryState::Available
                        || Card->WorldAssetId.IsValid())))
            {
                OutError = TEXT("The deployed deck zone must exactly match each member's canonical recovery and WorldAsset link.");
                return false;
            }
        }

        TSet<FGuid> LinkedCards;
        for (const FDAWorldAssetRecord& Asset : Campaign.WorldAssets)
        {
            const FDAForgeweaveBuildingState* RivalBuilding =
                Campaign.WorldState.Forgeweave.Buildings.FindByPredicate(
                    [&Asset](const FDAForgeweaveBuildingState& Building)
                    {
                        return Building.WorldAssetId == Asset.WorldAssetId;
                    });
            if (RivalBuilding != nullptr)
            {
                if (Asset.OwnerCivilizationId != TEXT("civilization.forgeweave")
                    || Asset.CardInstanceId.IsValid()
                    || !RivalBuilding->ProvenanceId.IsValid()
                    || RivalBuilding->CardDefinitionId != Asset.CardDefinitionId)
                {
                    OutError = TEXT("Rival WorldAssets require Forgeweave provenance and may not enter the player collection.");
                    return false;
                }
                continue;
            }
            const FCardInstance* Card = Campaign.CollectionState.FindInstance(Asset.CardInstanceId);
            if (!Asset.CardInstanceId.IsValid() || Card == nullptr
                || LinkedCards.Contains(Asset.CardInstanceId)
                || Card->WorldAssetId != Asset.WorldAssetId
                || Card->DefinitionId != Asset.CardDefinitionId
                || (Card->RecoveryState != EDARecoveryState::Deployed
                    && Card->RecoveryState != EDARecoveryState::Ruined))
            {
                OutError = TEXT("Every WorldAsset requires one exact bidirectional CardInstance id/definition/recovery link.");
                return false;
            }
            LinkedCards.Add(Asset.CardInstanceId);
        }
        for (const TPair<FGuid, FCardInstance>& Pair : Campaign.CollectionState.Instances)
        {
            const FCardInstance& Card = Pair.Value;
            const bool bRequiresAsset = Card.RecoveryState == EDARecoveryState::Deployed
                || Card.RecoveryState == EDARecoveryState::Ruined;
            if (bRequiresAsset != Card.WorldAssetId.IsValid()
                || (Card.WorldAssetId.IsValid()
                    && !Campaign.WorldAssets.ContainsByPredicate(
                        [&Card](const FDAWorldAssetRecord& Asset)
                        { return Asset.WorldAssetId == Card.WorldAssetId
                            && Asset.CardInstanceId == Card.InstanceId
                            && Asset.CardDefinitionId == Card.DefinitionId; })))
            {
                OutError = TEXT("Card recovery and WorldAsset links must form one exact bidirectional graph.");
                return false;
            }
        }
        return true;
    }

    bool ValidateCitySimulationAuthority(const FDACampaignSnapshot& Campaign, FString& OutError)
    {
        const FDACitySimulationState& City = Campaign.CitySimulationState;
        if (!Campaign.WorldState.bInitialized)
        {
            if (City.bInitialized)
            {
                OutError = TEXT("Uninitialized world state cannot contain initialized city authority.");
                return false;
            }
            return true;
        }
        const bool bHasCanonicalCityAuthority = Campaign.WorldState.ClockAuthority.bCaptured
            || City.bInitialized
            || !Campaign.DeckState.GetInstanceIds().IsEmpty()
            || !Campaign.CollectionState.Instances.IsEmpty();
        if (!bHasCanonicalCityAuthority)
        {
            // Pre-authority in-memory fixtures remain valid for isolated legacy world tests.
            // Production bootstrap and every migrated schema-v19 aggregate carry a marker above.
            return true;
        }
        if (!City.bInitialized || !City.Wallet.IsFinite() || City.Wallet.Capital < 0.f
            || City.Wallet.Insight < 0.f || City.Wallet.Influence < 0.f
            || City.Population < 0 || City.ResolvedDevelopmentCycles < 0
            || City.ResolvedWorldTicks != Campaign.WorldState.CurrentWorldTick
            || !FMath::IsFinite(City.Attractiveness) || City.Attractiveness < 0.f
            || City.Attractiveness > 100.f
            || !FMath::IsFinite(City.IncomingMigrationAccumulator)
            || !FMath::IsFinite(City.OutgoingMigrationAccumulator)
            || City.IncomingMigrationAccumulator < 0.f
            || City.OutgoingMigrationAccumulator < 0.f)
        {
            OutError = TEXT("Persisted city simulation authority has invalid wallet, population, clock, or migration state.");
            return false;
        }

        TSet<FName> CitizenIds;
        for (const FDACitizenRecord& Citizen : City.Citizens)
        {
            if (Citizen.CitizenId.IsNone() || Citizen.CityId.IsNone()
                || CitizenIds.Contains(Citizen.CitizenId))
            {
                OutError = TEXT("Persisted city citizens require stable unique identities and cities.");
                return false;
            }
            CitizenIds.Add(Citizen.CitizenId);
        }
        TSet<FName> HouseholdIds;
        for (const FDAHouseholdRecord& Household : City.Households)
        {
            if (Household.HouseholdId.IsNone() || HouseholdIds.Contains(Household.HouseholdId)
                || !FMath::IsFinite(Household.Wealth) || Household.Wealth < 0.f
                || Household.MemberCitizenIds.ContainsByPredicate(
                    [&CitizenIds](const FName CitizenId) { return !CitizenIds.Contains(CitizenId); }))
            {
                OutError = TEXT("Persisted households require unique identities, bounded wealth, and owned members.");
                return false;
            }
            HouseholdIds.Add(Household.HouseholdId);
        }
        TSet<FGuid> FacilityIds;
        for (const FDAFacilityContext& Facility : City.Facilities)
        {
            const FDAWorldAssetRecord* Asset = Campaign.FindWorldAssetRecord(
                Facility.AssetRecord.WorldAssetId);
            if (Asset == nullptr || FacilityIds.Contains(Asset->WorldAssetId)
                || Asset->OwnerCivilizationId != TEXT("civilization.synara")
                || Asset->CardDefinitionId != Facility.AssetRecord.CardDefinitionId
                || !FMath::IsFinite(Facility.AuthoredMaintenanceCapitalPerCycle)
                || Facility.AuthoredMaintenanceCapitalPerCycle < 0.f)
            {
                OutError = TEXT("Persisted facilities must map uniquely to canonical player-owned WorldAssets.");
                return false;
            }
            FacilityIds.Add(Asset->WorldAssetId);
        }
        for (const FDAWorldAssetRecord& Asset : Campaign.WorldAssets)
        {
            if (Asset.OwnerCivilizationId == TEXT("civilization.synara")
                && !FacilityIds.Contains(Asset.WorldAssetId))
            {
                OutError = TEXT("Every player-owned WorldAsset requires one canonical persisted facility record.");
                return false;
            }
        }

        const FDACampaignLiveSignalState& Signals = Campaign.LiveSignals;
        if (!FMath::IsNearlyEqual(Signals.Capital, City.Wallet.Capital, 0.001)
            || !FMath::IsNearlyEqual(Signals.Insight, City.Wallet.Insight, 0.001)
            || !FMath::IsNearlyEqual(Signals.Influence, City.Wallet.Influence, 0.001)
            || Signals.Population != City.Population
            || Signals.ResolvedDevelopmentCycles != City.ResolvedDevelopmentCycles
            || Signals.Citizens.Num() != City.Citizens.Num()
            || Signals.JobOpenings.Num() != City.JobOpenings.Num()
            || Signals.JobAssignments.Num() != City.JobAssignments.Num()
            || Signals.UtilitySignals.Num() != City.UtilitySignals.Num())
        {
            OutError = TEXT("LiveSignals must be an exact projection of the full persisted city authority.");
            return false;
        }
        for (const FDACitizenRecord& Citizen : City.Citizens)
        {
            const FDACampaignCitizenSignal* Signal = Signals.FindCitizen(Citizen.CitizenId);
            if (Signal == nullptr || Signal->CityId != Citizen.CityId
                || Signal->HomeWorldAssetId != Citizen.HomeAssetId
                || Signal->JobId != Citizen.JobId)
            {
                OutError = TEXT("Citizen LiveSignals do not exactly project persisted citizen authority.");
                return false;
            }
        }
        for (const FDAJobOpening& Opening : City.JobOpenings)
        {
            if (!Signals.JobOpenings.ContainsByPredicate([&Opening](const auto& Signal)
                { return Signal.JobId == Opening.JobId
                    && Signal.CityId == Opening.CityId
                    && Signal.FacilityWorldAssetId == Opening.FacilityWorldAssetId
                    && Signal.OpenPositions == Opening.OpenPositions; }))
            {
                OutError = TEXT("Job-opening LiveSignals do not exactly project persisted jobs.");
                return false;
            }
        }
        for (const FDAJobAssignment& Assignment : City.JobAssignments)
        {
            if (!Signals.JobAssignments.ContainsByPredicate([&Assignment](const auto& Signal)
                { return Signal.CitizenId == Assignment.CitizenId
                    && Signal.JobId == Assignment.JobId
                    && Signal.FacilityWorldAssetId == Assignment.FacilityWorldAssetId; }))
            {
                OutError = TEXT("Job-assignment LiveSignals do not exactly project persisted jobs.");
                return false;
            }
        }
        for (const FDACampaignUtilitySignal& Utility : City.UtilitySignals)
        {
            if (!Signals.UtilitySignals.ContainsByPredicate([&Utility](const auto& Signal)
                { return Signal.WorldAssetId == Utility.WorldAssetId
                    && Signal.Utility == Utility.Utility
                    && Signal.Supply == Utility.Supply; }))
            {
                OutError = TEXT("Utility LiveSignals do not exactly project persisted utility authority.");
                return false;
            }
        }
        return true;
    }
}

bool FDACampaignSnapshot::Validate(FString& OutError) const
{
    if (CampaignMutationRevision < 0
        || !ValidateCardOwnershipGraph(*this, OutError)
        || !ValidateCitySimulationAuthority(*this, OutError))
    {
        return false;
    }
    for (const TPair<FGuid, FCardInstance>& Pair : CollectionState.Instances)
    {
        const FCardInstance& Instance = Pair.Value;
        const bool bReplication = Instance.AcquisitionSource == EDAAcquisitionSource::Replication;
        if (!Pair.Key.IsValid() || Pair.Key != Instance.InstanceId || Instance.DefinitionId.IsNone()
            || static_cast<uint8>(Instance.AcquisitionSource) > static_cast<uint8>(EDAAcquisitionSource::Conquest)
            || Instance.AcquisitionWorldTick < 0 || Instance.MasteryXp < 0
            || bReplication != Instance.SourceCardInstanceId.IsValid()
            || (bReplication && (Instance.SourceCardInstanceId == Instance.InstanceId
                || CollectionState.FindInstance(Instance.SourceCardInstanceId) == nullptr)))
        {
            OutError = TEXT("Card instances require canonical identity, acquisition, and Replication provenance.");
            return false;
        }
    }
    TSet<FGuid> WorldAssetIds;
    for (const FDAWorldAssetRecord& Record : WorldAssets)
    {
        if (!Record.WorldAssetId.IsValid()
            || WorldAssetIds.Contains(Record.WorldAssetId)
            || Record.CardDefinitionId.IsNone()
            || !FMath::IsFinite(Record.StructuralIntegrity)
            || !FMath::IsFinite(Record.ConstructionProgressCycles)
            || Record.ConstructionProgressCycles < 0.f
            || (Record.ConstructionCyclesRequired > 0
                && Record.ConstructionProgressCycles
                    > static_cast<float>(Record.ConstructionCyclesRequired))
            || Record.StructuralIntegrity < 0.f
            || Record.StructuralIntegrity > 100.f)
        {
            OutError = TEXT("World asset records require unique valid ids, definitions, and bounded integrity.");
            return false;
        }
        WorldAssetIds.Add(Record.WorldAssetId);
    }

    TSet<FName> CampaignHistory;
    FName PreviousHistoryTag;
    for (const FName HistoryTag : HistoryTags)
    {
        if (HistoryTag.IsNone() || CampaignHistory.Contains(HistoryTag)
            || (!PreviousHistoryTag.IsNone() && !PreviousHistoryTag.LexicalLess(HistoryTag)))
        {
            OutError = TEXT("Campaign history tags must be non-empty, unique, and deterministically ordered.");
            return false;
        }
        CampaignHistory.Add(HistoryTag);
        PreviousHistoryTag = HistoryTag;
    }

    TSet<FName> ClaimedCityIds;
    for (const FDACityGridClaimState& Claims : CityGridClaims)
    {
        if (ClaimedCityIds.Contains(Claims.CityId) || !Claims.Validate(OutError))
        { if (OutError.IsEmpty()) OutError = TEXT("City grid claim identities must be unique."); return false; }
        ClaimedCityIds.Add(Claims.CityId);
    }
    const FDACityGridClaimState* PlayerClaims = FindCityGridClaims(TEXT("player_capital"));
    for (const FDAWorldAssetRecord& Record : WorldAssets)
        if (Record.CityId == TEXT("player_capital") && (WorldState.bInitialized || !CityGridClaims.IsEmpty())
            && (PlayerClaims == nullptr || !PlayerClaims->Contains(Record.GridOrigin)))
        { OutError = TEXT("Player-capital WorldAsset origins must remain inside persisted canonical grid claims."); return false; }

    if (!OperationConflict.Validate(WorldAssets, OutError)
        || !WorldState.Validate(OutError)
        || !SynaraState.Validate(OutError)
        || !LiveSignals.Validate(OutError)
        || !RegionalCrisis.Validate(WorldState, LiveSignals, HistoryTags, OutError)
        || !ConquestState.Validate(OutError)
        || !ValidateConquestMutationSources(*this, OutError)
        || !FDADaxtonAuthorityValidator::ValidateCampaignState(*this, OutError)
        || !AscensionState.Validate(*this, OutError)
        || !NarrativeState.Validate(WorldAssets, HistoryTags, OutError))
    {
        return false;
    }
    const FDAQuestSaveState* FoundryQuest = NarrativeState.FindQuestState(TEXT("quest.foundry_shortage"));
    if (FoundryQuest != nullptr && FoundryQuest->ProgressState == EDAQuestProgressState::Completed)
    {
        const FName ExpectedChoice = RegionalCrisis.Resolution == EDAFoundryShortageResolution::IndustrialSupport
            ? FName(TEXT("industrial_support"))
            : RegionalCrisis.Resolution == EDAFoundryShortageResolution::EdenRestriction
                ? FName(TEXT("eden_restriction"))
            : RegionalCrisis.Resolution == EDAFoundryShortageResolution::BrokeredCompact
                ? FName(TEXT("brokered_compact"))
            : RegionalCrisis.Resolution == EDAFoundryShortageResolution::MarketExploitation
                ? FName(TEXT("market_exploitation")) : NAME_None;
        const FGuid CrisisActionId = RegionalCrisis.ResolutionRecords.IsEmpty()
            ? FGuid() : RegionalCrisis.ResolutionRecords[0].ActionId;
        const FDANarrativeActionRecord* Action = NarrativeState.ActionRecords.FindByPredicate(
            [CrisisActionId](const FDANarrativeActionRecord& Record){ return Record.ActionId == CrisisActionId; });
        if (ExpectedChoice.IsNone()
            || FoundryQuest->CurrentNodeId != FName(*(TEXT("resolution.") + ExpectedChoice.ToString()))
            || Action == nullptr || !Action->NormalizedActionTags.Contains(ExpectedChoice)
            || !Action->NormalizedActionTags.Contains(TEXT("regional.foundry_shortage")))
        {
            OutError = TEXT("Foundry quest completion must be proven by the same non-collapse crisis resolution action.");
            return false;
        }
    }
    for (const FDASynaraCitizenEmployment& Employment : SynaraState.CitizenEmployment)
    {
        const FDAWorldAssetRecord* Facility = FindWorldAssetRecord(Employment.FacilityWorldAssetId);
        if (Facility == nullptr || Facility->CardDefinitionId != TEXT("synara.cognitive_operations_tower")
            || Facility->CityId != Employment.CityId || Facility->OwnerCivilizationId != TEXT("civilization.synara")
            || Facility->ConstructionState != EDAConstructionState::Operational)
        { OutError = TEXT("Canonical Nia employment must resolve the bound operational Cognitive Operations Tower."); return false; }
    }
    for (const FDACampaignUtilitySignal& Signal : LiveSignals.UtilitySignals)
        if (FindWorldAssetRecord(Signal.WorldAssetId) == nullptr)
        { OutError = TEXT("Persisted utility signal must resolve a canonical campaign WorldAsset."); return false; }
    for (const FDACampaignJobOpeningSignal& Opening : LiveSignals.JobOpenings)
        if (FindWorldAssetRecord(Opening.FacilityWorldAssetId) == nullptr)
        { OutError = TEXT("Persisted job opening must resolve a canonical campaign WorldAsset."); return false; }
    const FDAQuestSaveState* NiaJobQuest = NarrativeState.FindQuestState(TEXT("quest.nia_needs_a_job"));
    if (!NarrativeState.IsFirstHourTransactionInProgress()
        && NiaJobQuest != nullptr && NiaJobQuest->ProgressState == EDAQuestProgressState::Completed)
    {
        const FDAQuestObjectiveAssetBindingRecord* TowerBinding = NarrativeState.QuestObjectiveAssetBindings.FindByPredicate(
            [](const FDAQuestObjectiveAssetBindingRecord& Binding)
            { return Binding.QuestId == TEXT("quest.nia_needs_a_job")
                && Binding.BindingId == TEXT("cognitive_operations_tower"); });
        const int32 CompletedEmploymentCount = SynaraState.CitizenEmployment.FilterByPredicate(
            [TowerBinding](const FDASynaraCitizenEmployment& Employment)
            {
                return TowerBinding != nullptr && Employment.CitizenId == TEXT("citizen.synara.nia_vale")
                    && Employment.CityId == TEXT("player_capital")
                    && Employment.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                    && Employment.FacilityWorldAssetId == TowerBinding->WorldAssetId;
            }).Num();
        const FDACampaignCitizenSignal* LiveNia = LiveSignals.FindCitizen(TEXT("citizen.synara.nia_vale"));
        const bool bHasOpening = TowerBinding != nullptr && LiveSignals.JobOpenings.ContainsByPredicate(
            [TowerBinding](const FDACampaignJobOpeningSignal& Opening)
            { return Opening.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                && Opening.CityId == TEXT("player_capital")
                && Opening.FacilityWorldAssetId == TowerBinding->WorldAssetId; });
        const bool bHasAssignment = TowerBinding != nullptr && LiveSignals.JobAssignments.ContainsByPredicate(
            [TowerBinding](const FDACampaignJobAssignmentSignal& Assignment)
            { return Assignment.CitizenId == TEXT("citizen.synara.nia_vale")
                && Assignment.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                && Assignment.FacilityWorldAssetId == TowerBinding->WorldAssetId; });
        if (TowerBinding == nullptr || CompletedEmploymentCount != 1 || LiveNia == nullptr
            || LiveNia->CityId != TEXT("player_capital")
            || LiveNia->JobId != TEXT("job.synara.cognitive_operations_tower.operator")
            || !bHasOpening || !bHasAssignment)
        { OutError = TEXT("Completed Nia Needs a Job requires one canonical persisted citizen/job/facility transaction."); return false; }
    }
    for (const FDAQuestContentEffectRecord& Effect : NarrativeState.QuestContentEffectRecords)
    {
        const FDASynaraValueReason* DependencyReason = SynaraState.DependencyReasons.FindByPredicate(
            [&Effect](const FDASynaraValueReason& Reason) { return Reason.ActionId == Effect.ActionId; });
        const FDASynaraValueReason* FactionReason = SynaraState.FactionSupportReasons.FindByPredicate(
            [&Effect](const FDASynaraValueReason& Reason) { return Reason.ActionId == Effect.ActionId && Reason.SubjectId == TEXT("faction.synara.human_agency"); });
        const FDASynaraValueReason* RelationshipReason = SynaraState.CitizenRelationshipReasons.FindByPredicate(
            [&Effect](const FDASynaraValueReason& Reason) { return Reason.ActionId == Effect.ActionId && Reason.SubjectId == TEXT("citizen.synara.nia_vale"); });
        if (Effect.bHasDependencyDelta != (DependencyReason != nullptr)
            || Effect.bHasHumanAgencySupportDelta != (FactionReason != nullptr)
            || Effect.bHasCitizenRelationshipDelta != (RelationshipReason != nullptr)
            || (DependencyReason != nullptr && (DependencyReason->Baseline != Effect.BaselineDependency
                || DependencyReason->Delta != Effect.DependencyDelta || DependencyReason->Result != Effect.ResultDependency
                || DependencyReason->WorldTick != Effect.WorldTick))
            || (FactionReason != nullptr && (FactionReason->Baseline != Effect.BaselineHumanAgencySupport
                || FactionReason->Delta != Effect.HumanAgencySupportDelta || FactionReason->Result != Effect.ResultHumanAgencySupport
                || FactionReason->WorldTick != Effect.WorldTick))
            || (RelationshipReason != nullptr && (RelationshipReason->Baseline != Effect.BaselineNiaTrust
                || RelationshipReason->Delta != Effect.CitizenRelationshipDelta || RelationshipReason->Result != Effect.ResultNiaTrust
                || RelationshipReason->WorldTick != Effect.WorldTick)))
        { OutError = TEXT("Quest effect must reconcile its exact historical canonical authority reasons."); return false; }
        const auto HasPolicyResult = [this, &Effect](const FName AuthorityId, const int32 Result)
        { return SynaraState.PolicyReasons.ContainsByPredicate([&Effect, AuthorityId, Result](const FDASynaraPolicyReason& Reason)
            { return Reason.ActionId == Effect.ActionId && Reason.AuthorityId == AuthorityId
                && Reason.Result == Result && Reason.WorldTick == Effect.WorldTick; }); };
        bool bPolicyExact = true;
        if (Effect.QuestId == TEXT("quest.replacement_model"))
            bPolicyExact = HasPolicyResult(TEXT("CapitalEfficiency"), Effect.ChoiceBranchTag == TEXT("accept") ? 2
                : Effect.ChoiceBranchTag == TEXT("modify") ? 1 : 0);
        else if (Effect.QuestId == TEXT("quest.agency_has_a_price"))
            bPolicyExact = HasPolicyResult(TEXT("AgencyPetition"), Effect.ChoiceBranchTag == TEXT("agency_forum") ? 1
                : Effect.ChoiceBranchTag == TEXT("retrain") ? 2 : Effect.ChoiceBranchTag == TEXT("automation_cap") ? 3 : 4);
        else if (Effect.QuestId == TEXT("quest.iron_at_border"))
        {
            const int32 Border = Effect.ChoiceBranchTag == TEXT("accept") ? 1 : Effect.ChoiceBranchTag == TEXT("refuse") ? 2
                : Effect.ChoiceBranchTag == TEXT("favorable_terms") ? 3 : 4;
            const int32 Trust = Effect.ChoiceBranchTag == TEXT("accept") ? 1 : Effect.ChoiceBranchTag == TEXT("refuse") ? 2 : 0;
            const int32 Respect = Effect.ChoiceBranchTag == TEXT("favorable_terms") ? 1 : 0;
            const int32 Dependence = Effect.ChoiceBranchTag == TEXT("accept") || Effect.ChoiceBranchTag == TEXT("favorable_terms") ? 1 : 0;
            bPolicyExact = HasPolicyResult(TEXT("IronBorder"), Border) && HasPolicyResult(TEXT("ForgeweaveTrust"), Trust)
                && HasPolicyResult(TEXT("ForgeweaveRespect"), Respect) && HasPolicyResult(TEXT("ForgeweaveDependence"), Dependence);
            if (WorldState.bInitialized)
            {
                const FDADiplomaticRelationship* Relationship = WorldState.Diplomacy.FindRelationship(TEXT("relationship.synara.forgeweave"));
                for (const FName SemanticEffect : Effect.SemanticEffects)
                {
                    if (SemanticEffect == TEXT("effect.forgeweave.request_deferred")) continue;
                    if (Relationship == nullptr || !Relationship->ReasonLedger.ContainsByPredicate(
                        [&Effect, SemanticEffect](const FDADiplomaticReason& Reason)
                        { return Reason.MutationId == SemanticEffect && Reason.SourceTag == Effect.ActionId
                            && Reason.WorldTick == Effect.WorldTick; })) bPolicyExact = false;
                }
            }
        }
        if (!bPolicyExact) { OutError = TEXT("Quest effect must reconcile its exact historical policy transaction."); return false; }
    }
    TSet<FGuid> GrantedCardIds;
    for (const FDAQuestContentUnlockRecord& Unlock : NarrativeState.QuestContentUnlockRecords)
    {
        if (!ValidateFirstHourUnlockRecord(Unlock, NarrativeState, CollectionState, GrantedCardIds, OutError))
        {
            return false;
        }
    }
    for (const FDAQuestObjectiveAssetBindingRecord& Binding : NarrativeState.QuestObjectiveAssetBindings)
    {
        if (!ValidateFirstHourBindingRecord(Binding, NarrativeState, WorldAssets, OutError))
        {
            return false;
        }
    }
    for (const FDAQuestSaveState& Quest : NarrativeState.QuestStates)
    {
        for (const FDAQuestWorldAssetBinding& RuntimeBinding : Quest.WorldAssetBindings)
        {
            const FString Key = Quest.QuestId.ToString() + TEXT("|") + RuntimeBinding.BindingId.ToString();
            const FFirstHourBindingSpec* Spec = FirstHourBindingSpecs().Find(Key);
            if (Spec == nullptr) continue;
            FDAQuestObjectiveAssetBindingRecord Binding;
            Binding.QuestId = Quest.QuestId; Binding.BindingId = RuntimeBinding.BindingId;
            Binding.DefinitionId = Spec->DefinitionId;
            Binding.WorldAssetId = RuntimeBinding.WorldAssetId;
            Binding.BindWorldTick = RuntimeBinding.BindWorldTick;
            Binding.QuestDefinitionFingerprint = RuntimeBinding.QuestDefinitionFingerprint;
            if (!ValidateFirstHourBindingRecord(Binding, NarrativeState, WorldAssets, OutError))
            {
                return false;
            }
        }
    }
    /* Exact required binding set is derived from the authored path; no global action or same-definition substitution. */
    TSet<FString> RequiredBindingKeys;
    for (const FDAQuestSaveState& Quest : NarrativeState.QuestStates)
    {
        for (const FDAQuestWorldAssetBinding& Binding : Quest.WorldAssetBindings)
            if (FirstHourBindingSpecs().Contains(Quest.QuestId.ToString() + TEXT("|") + Binding.BindingId.ToString()))
                RequiredBindingKeys.Add(Quest.QuestId.ToString() + TEXT("|") + Binding.BindingId.ToString());
    }
    for (const FDAQuestObjectiveAssetBindingRecord& Binding : NarrativeState.QuestObjectiveAssetBindings)
        RequiredBindingKeys.Add(Binding.QuestId.ToString() + TEXT("|") + Binding.BindingId.ToString());
    int32 RequiredBindingRecordCount = NarrativeState.QuestObjectiveAssetBindings.Num();
    for (const FDAQuestSaveState& Quest : NarrativeState.QuestStates)
        for (const FDAQuestWorldAssetBinding& Binding : Quest.WorldAssetBindings)
            if (FirstHourBindingSpecs().Contains(Quest.QuestId.ToString() + TEXT("|") + Binding.BindingId.ToString())) ++RequiredBindingRecordCount;
    if (RequiredBindingKeys.Num() != RequiredBindingRecordCount)
    {
        OutError = TEXT("RequiredBindingKeys must be the exact authored set without duplicate runtime/dynamic bindings.");
        return false;
    }
    for (const TPair<FString, FFirstHourBindingSpec>& Pair : FirstHourBindingSpecs())
    {
        FString QuestText, BindingText; if (!Pair.Key.Split(TEXT("|"), &QuestText, &BindingText)) continue;
        const FName QuestId(*QuestText); const FName BindingId(*BindingText);
        const FDAQuestSaveState* Quest = NarrativeState.FindQuestState(QuestId); if (Quest == nullptr) continue;
        const bool bStartRequired = Pair.Value.EarliestCompletedNodeId.IsNone()
            && Quest->DefinitionManifest.RequiredWorldAssetBindingIds.Contains(BindingId);
        const bool bObjectiveRequired = !Pair.Value.EarliestCompletedNodeId.IsNone()
            && Quest->CompletedNodeIds.Contains(Pair.Value.EarliestCompletedNodeId);
        if ((bStartRequired || bObjectiveRequired) && !RequiredBindingKeys.Contains(Pair.Key))
        { OutError = TEXT("RequiredBindingKeys is missing an exact authored binding reached by the quest path."); return false; }
    }
    const FName SurrenderAcceptedTag(TEXT("forge_guard_surrender_accepted"));
    const bool bHasAcceptedSurrender = OperationConflict.SurrenderRecords.ContainsByPredicate(
        [](const FDASurrenderRecord& Record) { return Record.bAccepted; });
    if (bHasAcceptedSurrender && !CampaignHistory.Contains(SurrenderAcceptedTag))
    {
        OutError = TEXT("Accepted surrender audit must agree with the one canonical campaign history ledger.");
        return false;
    }
    for (const FDACaptureRecord& Record : OperationConflict.CaptureRecords)
    {
        if (!Record.bOutcomeResolved)
        {
            continue;
        }
        FName OutcomeHistoryTag;
        switch (Record.Outcome)
        {
        case EDACaptureOutcome::Preserve: OutcomeHistoryTag = TEXT("capture.preserved"); break;
        case EDACaptureOutcome::Convert: OutcomeHistoryTag = TEXT("capture.conversion_sanctioned"); break;
        case EDACaptureOutcome::Study: OutcomeHistoryTag = TEXT("capture.studied"); break;
        case EDACaptureOutcome::Salvage: OutcomeHistoryTag = TEXT("capture.salvaged"); break;
        case EDACaptureOutcome::Gift: OutcomeHistoryTag = TEXT("capture.gifted"); break;
        default: break;
        }
        if (OutcomeHistoryTag.IsNone() || !CampaignHistory.Contains(OutcomeHistoryTag))
        {
            OutError = TEXT("Resolved capture audit must have its exact tag in canonical campaign history.");
            return false;
        }
    }
    if (!WorldState.bInitialized)
    {
        return true;
    }

    const FDARegionState* Ironheart = WorldState.FindRegion(TEXT("region.ironheart"));
    if (Ironheart == nullptr)
    {
        OutError = TEXT("Forgeweave campaign authority requires Ironheart.");
        return false;
    }
    const TArray<FDAForgeweaveBuildingState>& Buildings = WorldState.Forgeweave.Buildings;
    for (int32 Index = 0; Index < Buildings.Num(); ++Index)
    {
        const FDAForgeweaveBuildingState& Building = Buildings[Index];
        const FDAWorldAssetRecord* Asset = FindWorldAssetRecord(Building.WorldAssetId);
        const FDARegionActorState* Actor = Ironheart->PersistentDelta.LocalActors.FindByPredicate(
            [&Building](const FDARegionActorState& Candidate)
            {
                return Candidate.ActorId == Building.BuildingId;
            });
        const FTransform ExpectedActorTransform = Asset != nullptr
            ? MakeForgeweaveConstructionTransform(*Asset)
            : FTransform::Identity;
        int32 MatchingActorIds = 0;
        int32 MatchingWorldAssetIds = 0;
        int32 MatchingCompleteActors = 0;
        for (const FDARegionState& Region : WorldState.Regions)
        {
            for (const FDARegionActorState& Candidate : Region.PersistentDelta.LocalActors)
            {
                MatchingActorIds += Candidate.ActorId == Building.BuildingId ? 1 : 0;
                MatchingWorldAssetIds += Candidate.WorldAssetId == Building.WorldAssetId ? 1 : 0;
                MatchingCompleteActors += Asset != nullptr
                    && Region.RegionId == TEXT("region.ironheart")
                    && Candidate.ActorId == Building.BuildingId
                    && Candidate.WorldAssetId == Building.WorldAssetId
                    && Candidate.DefinitionId == Asset->CardDefinitionId
                    && Candidate.Transform.Equals(ExpectedActorTransform) ? 1 : 0;
            }
        }
        if (Asset == nullptr
            || Actor == nullptr
            || !Building.ProvenanceId.IsValid()
            || Building.CardDefinitionId != Asset->CardDefinitionId
            || Asset->CardInstanceId.IsValid()
            || MatchingActorIds != 1
            || MatchingWorldAssetIds != 1
            || MatchingCompleteActors != 1
            || !FDAForgeweaveCityState::IsVerticalSliceBuildCard(Asset->CardDefinitionId)
            || Asset->CityId != TEXT("settlement.ore_station_7")
            || Asset->OwnerCivilizationId != TEXT("civilization.forgeweave")
            || Asset->Rotation != 0
            || Asset->GridOrigin.X < 0
            || Asset->GridOrigin.Y < 0
            || Asset->GridOrigin.X > WorldState.Forgeweave.GridWidth - Building.Footprint.X
            || Asset->GridOrigin.Y > WorldState.Forgeweave.GridHeight - Building.Footprint.Y
            || Actor->WorldAssetId != Asset->WorldAssetId
            || Actor->DefinitionId != Asset->CardDefinitionId
            || !Actor->Transform.Equals(ExpectedActorTransform))
        {
            OutError = TEXT("Every Forgeweave building must resolve one canonical campaign WorldAsset and reconstruction actor.");
            return false;
        }
        const FDAStructuralDamageRecord* Damage = OperationConflict.FindStructuralDamageRecord(Asset->WorldAssetId);
        if (FDAStructuralDamagePolicy::SupportsFullModularDestruction(Asset->CardDefinitionId) != (Damage != nullptr))
        {
            OutError = TEXT("Forgeweave modular buildings must resolve exactly one canonical structural-damage record.");
            return false;
        }
        for (int32 OtherIndex = 0; OtherIndex < Index; ++OtherIndex)
        {
            const FDAWorldAssetRecord* OtherAsset = FindWorldAssetRecord(Buildings[OtherIndex].WorldAssetId);
            if (OtherAsset == nullptr || RectanglesOverlap(*Asset, Building, *OtherAsset, Buildings[OtherIndex]))
            {
                OutError = TEXT("Canonical Forgeweave campaign footprints cannot overlap.");
                return false;
            }
        }
    }

    int32 ExpectedTransactionCount = 0;
    for (const FDAForgeweaveDecisionRecord& Decision : WorldState.Forgeweave.DecisionHistory)
    {
        ExpectedTransactionCount += Decision.Type == EDARivalDecisionType::Trade
            || Decision.Type == EDARivalDecisionType::Repair
            || Decision.Type == EDARivalDecisionType::Fortify;
    }
    if (ExpectedTransactionCount != WorldState.Forgeweave.ActionTransactions.Num())
    {
        OutError = TEXT("Every Forgeweave authority mutation requires exactly one durable action transaction.");
        return false;
    }

    for (const FDAForgeweaveDecisionRecord& Decision : WorldState.Forgeweave.DecisionHistory)
    {
        if (Decision.Type != EDARivalDecisionType::Construct)
        {
            continue;
        }
        const FDAForgeweaveBuildingState* Building = Buildings.FindByPredicate(
            [&Decision](const FDAForgeweaveBuildingState& Candidate)
            {
                return Candidate.BuildingId == Decision.TargetBuildingId;
            });
        const FDAWorldAssetRecord* Asset = Building != nullptr ? FindWorldAssetRecord(Building->WorldAssetId) : nullptr;
        if (Building == nullptr
            || Asset == nullptr
            || Asset->CardDefinitionId != Decision.CardDefinitionId
            || Asset->GridOrigin != Decision.Origin
            || !FMath::IsNearlyEqual(Building->DeploymentCapital, Decision.CapitalSpent, 0.001f))
        {
            OutError = TEXT("Every Forgeweave construction decision must resolve its exact canonical building and WorldAsset.");
            return false;
        }
    }
    for (const FDAForgeweaveBuildingState& Building : Buildings)
    {
        int32 MatchingConstructionDecisions = 0;
        for (const FDAForgeweaveDecisionRecord& Decision : WorldState.Forgeweave.DecisionHistory)
        {
            MatchingConstructionDecisions += Decision.Type == EDARivalDecisionType::Construct
                && Decision.TargetBuildingId == Building.BuildingId ? 1 : 0;
        }
        if (MatchingConstructionDecisions != 1)
        {
            OutError = TEXT("Every canonical Forgeweave building requires exactly one construction decision.");
            return false;
        }
    }
    for (const FDAWorldAssetRecord& Asset : WorldAssets)
    {
        if (Asset.CityId == TEXT("settlement.ore_station_7")
            && Asset.OwnerCivilizationId == TEXT("civilization.forgeweave")
            && FDAForgeweaveCityState::IsVerticalSliceBuildCard(Asset.CardDefinitionId)
            && !Buildings.ContainsByPredicate(
                [&Asset](const FDAForgeweaveBuildingState& Building)
                {
                    return Building.WorldAssetId == Asset.WorldAssetId;
                }))
        {
            OutError = TEXT("Planner-generated Forgeweave WorldAssets cannot be orphaned from city authority.");
            return false;
        }
    }

    TMap<FName, TPair<int64, int64>> LatestTradeBalances;
    TMap<FGuid, TPair<float, TArray<FDAForgeweaveModuleRepairDelta>>> LatestRepairBalances;
    for (const FDAForgeweaveActionTransaction& Transaction : WorldState.Forgeweave.ActionTransactions)
    {
        const FDAForgeweaveDecisionRecord* Decision = WorldState.Forgeweave.DecisionHistory.FindByPredicate(
            [&Transaction](const FDAForgeweaveDecisionRecord& Candidate)
            {
                return Candidate.WorldTick == Transaction.WorldTick
                    && Candidate.Type == Transaction.Type
                    && Candidate.TargetBuildingId == Transaction.AuthorityId;
            });
        if (Decision == nullptr
            || !FMath::IsNearlyEqual(Transaction.CapitalBefore - Transaction.CapitalAfter, Decision->CapitalSpent, 0.001f)
            || !FMath::IsNearlyEqual(Transaction.ProductionBefore - Transaction.ProductionAfter, Decision->ProductionSpent, 0.001f))
        {
            OutError = TEXT("Forgeweave action opening/closing economy must reconcile with its decision spend.");
            return false;
        }

        if (Transaction.Type == EDARivalDecisionType::Trade)
        {
            const FDATradeSpotOrderState* Order = WorldState.Trade.FindSpotOrder(Transaction.AuthorityId);
            if (Order == nullptr
                || Order->WorldTick != Transaction.WorldTick
                || Transaction.SourceQuantityBefore - Transaction.SourceQuantityAfter != Order->Quantity
                || Transaction.DestinationQuantityAfter - Transaction.DestinationQuantityBefore != Order->Quantity)
            {
                OutError = TEXT("Forgeweave Trade transaction quantities must reconcile with its spot order.");
                return false;
            }
            const TPair<int64, int64>* Previous = LatestTradeBalances.Find(Order->RouteId);
            if (Previous != nullptr
                && (Transaction.SourceQuantityBefore != Previous->Key
                    || Transaction.DestinationQuantityBefore != Previous->Value))
            {
                OutError = TEXT("Forgeweave Trade transaction opening quantities must chain from the previous closing quantities.");
                return false;
            }
            LatestTradeBalances.Add(
                Order->RouteId,
                TPair<int64, int64>(Transaction.SourceQuantityAfter, Transaction.DestinationQuantityAfter));
        }
        else if (Transaction.Type == EDARivalDecisionType::Repair)
        {
            const FDAForgeweaveBuildingState* Building = Buildings.FindByPredicate(
                [&Transaction](const FDAForgeweaveBuildingState& Candidate)
                {
                    return Candidate.BuildingId == Transaction.AuthorityId;
                });
            const FDAWorldAssetRecord* Asset = Building != nullptr ? FindWorldAssetRecord(Building->WorldAssetId) : nullptr;
            const FDAStructuralDamageRecord* Damage = Asset != nullptr
                ? OperationConflict.FindStructuralDamageRecord(Asset->WorldAssetId)
                : nullptr;
            const float RepairPercent = Transaction.IntegrityAfter - Transaction.IntegrityBefore;
            const float ExpectedRepairPercent = FMath::Min(25.f, 100.f - Transaction.IntegrityBefore);
            if (Building == nullptr
                || Asset == nullptr
                || Damage == nullptr
                || Transaction.IntegrityAfter <= Transaction.IntegrityBefore
                || !FMath::IsNearlyEqual(RepairPercent, ExpectedRepairPercent, 0.001f)
                || !FMath::IsNearlyEqual(
                    Decision->CapitalSpent,
                    Building->DeploymentCapital * (RepairPercent / 100.f) * 0.5f,
                    0.001f)
                || Transaction.ModuleDeltas.Num() != Damage->Modules.Num())
            {
                OutError = TEXT("Forgeweave Repair transaction must bind canonical asset integrity and every structural module.");
                return false;
            }
            for (const FDAForgeweaveModuleRepairDelta& Delta : Transaction.ModuleDeltas)
            {
                const FDAStructureModuleHealthRecord* Module = Damage->Modules.FindByPredicate(
                    [&Delta](const FDAStructureModuleHealthRecord& Candidate)
                    {
                        return Candidate.ModuleId == Delta.ModuleId;
                    });
                if (Module == nullptr
                    || Delta.HealthAfter > Module->MaximumHealth
                    || !FMath::IsNearlyEqual(
                        Delta.HealthAfter,
                        FMath::Min(Module->MaximumHealth, Delta.HealthBefore + RepairPercent),
                        0.001f))
                {
                    OutError = TEXT("Forgeweave Repair module deltas must be intrinsic and bounded by canonical damage modules.");
                    return false;
                }
            }
            LatestRepairBalances.Add(
                Asset->WorldAssetId,
                TPair<float, TArray<FDAForgeweaveModuleRepairDelta>>(
                    Transaction.IntegrityAfter,
                    Transaction.ModuleDeltas));
        }
        else
        {
            int32 MatchingActorIds = 0;
            int32 MatchingCompleteActors = 0;
            for (const FDARegionState& Region : WorldState.Regions)
            {
                for (const FDARegionActorState& Candidate : Region.PersistentDelta.LocalActors)
                {
                    MatchingActorIds += Candidate.ActorId == Transaction.AuthorityId ? 1 : 0;
                    MatchingCompleteActors += Region.RegionId == TEXT("region.ironheart")
                        && Candidate.ActorId == Transaction.AuthorityId
                        && Candidate.DefinitionId == TEXT("forgeweave.defense_cover")
                        && !Candidate.WorldAssetId.IsValid()
                        && Candidate.Transform.Equals(MakeForgeweaveDefenseTransform())
                        && Transaction.ActorTransform.Equals(Candidate.Transform) ? 1 : 0;
                }
            }
            if (MatchingActorIds != 1
                || MatchingCompleteActors != 1
                || Transaction.CoverTypeId != TEXT("cover.hardened")
                || !Transaction.ActorTransform.Equals(MakeForgeweaveDefenseTransform()))
            {
                OutError = TEXT("Forgeweave Fortify transaction must reconcile exact cover type and actor transform.");
                return false;
            }
        }
    }

    for (const TPair<FName, TPair<int64, int64>>& Balance : LatestTradeBalances)
    {
        const FDATradeRouteState* Route = WorldState.Trade.FindRoute(Balance.Key);
        const FDARegionalTradeInventory* Source = Route != nullptr ? WorldState.Trade.FindInventory(Route->SourceRegionId) : nullptr;
        const FDARegionalTradeInventory* Destination = Route != nullptr ? WorldState.Trade.FindInventory(Route->DestinationRegionId) : nullptr;
        const FName GoodId(TEXT("resource.regenerative_materials"));
        if (Source == nullptr || Destination == nullptr
            || Source->Stock.FindRef(GoodId) != Balance.Value.Key
            || Destination->Stock.FindRef(GoodId) != Balance.Value.Value)
        {
            OutError = TEXT("Latest Forgeweave Trade closing balances must equal canonical inventories.");
            return false;
        }
    }
    for (const TPair<FGuid, TPair<float, TArray<FDAForgeweaveModuleRepairDelta>>>& Balance : LatestRepairBalances)
    {
        const FDAWorldAssetRecord* Asset = FindWorldAssetRecord(Balance.Key);
        const FDAStructuralDamageRecord* Damage = OperationConflict.FindStructuralDamageRecord(Balance.Key);
        if (Asset == nullptr || Damage == nullptr || !FMath::IsNearlyEqual(Asset->StructuralIntegrity, Balance.Value.Key, 0.001f))
        {
            OutError = TEXT("Latest Forgeweave Repair closing integrity must equal canonical WorldAsset authority.");
            return false;
        }
        for (const FDAForgeweaveModuleRepairDelta& Delta : Balance.Value.Value)
        {
            const FDAStructureModuleHealthRecord* Module = Damage->Modules.FindByPredicate(
                [&Delta](const FDAStructureModuleHealthRecord& Candidate) { return Candidate.ModuleId == Delta.ModuleId; });
            if (Module == nullptr || !FMath::IsNearlyEqual(Module->CurrentHealth, Delta.HealthAfter, 0.001f))
            {
                OutError = TEXT("Latest Forgeweave Repair module closing health must equal canonical damage authority.");
                return false;
            }
        }
    }

    TSet<FName> CanonicalPlannerAuthorityIds;
    TSet<FGuid> CanonicalPlannerWorldAssetIds;
    for (const FDAForgeweaveBuildingState& Building : Buildings)
    {
        CanonicalPlannerAuthorityIds.Add(Building.BuildingId);
        CanonicalPlannerWorldAssetIds.Add(Building.WorldAssetId);
    }
    for (const FDAForgeweaveDecisionRecord& Decision : WorldState.Forgeweave.DecisionHistory)
    {
        if (!Decision.TargetBuildingId.IsNone())
        {
            CanonicalPlannerAuthorityIds.Add(Decision.TargetBuildingId);
        }
    }
    for (const FDAForgeweaveActionTransaction& Transaction : WorldState.Forgeweave.ActionTransactions)
    {
        if (!Transaction.AuthorityId.IsNone())
        {
            CanonicalPlannerAuthorityIds.Add(Transaction.AuthorityId);
        }
    }

    for (const FDARegionState& Region : WorldState.Regions)
    {
        for (const FDARegionActorState& Actor : Region.PersistentDelta.LocalActors)
        {
            const bool bDefenseActor = Actor.DefinitionId == TEXT("forgeweave.defense_cover");
            const bool bConstructionActor = FDAForgeweaveCityState::IsVerticalSliceBuildCard(Actor.DefinitionId);
            const bool bReusesCanonicalAuthorityId = CanonicalPlannerAuthorityIds.Contains(Actor.ActorId);
            const bool bReusesCanonicalWorldAssetId = Actor.WorldAssetId.IsValid()
                && CanonicalPlannerWorldAssetIds.Contains(Actor.WorldAssetId);
            if (!bDefenseActor
                && !bConstructionActor
                && !bReusesCanonicalAuthorityId
                && !bReusesCanonicalWorldAssetId)
            {
                continue;
            }

            const FDAForgeweaveBuildingState* ActorIdBuilding = Buildings.FindByPredicate(
                [&Actor](const FDAForgeweaveBuildingState& Candidate)
                {
                    return Candidate.BuildingId == Actor.ActorId;
                });
            const FDAForgeweaveBuildingState* WorldAssetBuilding = Actor.WorldAssetId.IsValid()
                ? Buildings.FindByPredicate(
                    [&Actor](const FDAForgeweaveBuildingState& Candidate)
                    {
                        return Candidate.WorldAssetId == Actor.WorldAssetId;
                    })
                : nullptr;
            const FDAForgeweaveBuildingState* ExpectedBuilding = ActorIdBuilding != nullptr
                ? ActorIdBuilding
                : WorldAssetBuilding;
            if (ActorIdBuilding != nullptr
                && WorldAssetBuilding != nullptr
                && ActorIdBuilding != WorldAssetBuilding)
            {
                OutError = TEXT("A reconstruction actor cannot combine identifiers from different Forgeweave buildings.");
                return false;
            }

            if (ExpectedBuilding != nullptr)
            {
                const FDAWorldAssetRecord* Asset = FindWorldAssetRecord(ExpectedBuilding->WorldAssetId);
                if (Region.RegionId != TEXT("region.ironheart")
                    || Asset == nullptr
                    || Actor.ActorId != ExpectedBuilding->BuildingId
                    || Actor.WorldAssetId != ExpectedBuilding->WorldAssetId
                    || Actor.DefinitionId != Asset->CardDefinitionId
                    || !Actor.Transform.Equals(MakeForgeweaveConstructionTransform(*Asset)))
                {
                    OutError = TEXT("Every actor reusing Forgeweave building authority must match its complete canonical Ironheart tuple.");
                    return false;
                }
                continue;
            }

            int32 DecisionCount = 0;
            int32 TransactionCount = 0;
            for (const FDAForgeweaveDecisionRecord& Decision : WorldState.Forgeweave.DecisionHistory)
            {
                DecisionCount += Decision.Type == EDARivalDecisionType::Fortify
                    && Decision.TargetBuildingId == Actor.ActorId ? 1 : 0;
            }
            for (const FDAForgeweaveActionTransaction& Transaction : WorldState.Forgeweave.ActionTransactions)
            {
                TransactionCount += Transaction.Type == EDARivalDecisionType::Fortify
                    && Transaction.AuthorityId == Actor.ActorId
                    && Transaction.CoverTypeId == TEXT("cover.hardened")
                    && Transaction.ActorTransform.Equals(Actor.Transform) ? 1 : 0;
            }
            if (Region.RegionId != TEXT("region.ironheart")
                || !bDefenseActor
                || bConstructionActor
                || !bReusesCanonicalAuthorityId
                || Actor.WorldAssetId.IsValid()
                || DecisionCount != 1
                || TransactionCount != 1
                || !Actor.Transform.Equals(MakeForgeweaveDefenseTransform()))
            {
                if (bReusesCanonicalAuthorityId || bReusesCanonicalWorldAssetId)
                {
                    OutError = TEXT("Every actor reusing planner authority must match one complete canonical construction or defense tuple.");
                }
                else
                {
                    OutError = TEXT("Forgeweave construction and defense definitions cannot be orphaned from canonical planner authority.");
                }
                return false;
            }
        }
    }

    TSet<FName> AuthoritativeTradeOrderIds;
    for (const FDAForgeweaveDecisionRecord& Decision : WorldState.Forgeweave.DecisionHistory)
    {
        if (Decision.Type != EDARivalDecisionType::Trade)
        {
            continue;
        }
        int32 TransactionCount = 0;
        for (const FDAForgeweaveActionTransaction& Transaction : WorldState.Forgeweave.ActionTransactions)
        {
            TransactionCount += Transaction.Type == EDARivalDecisionType::Trade
                && Transaction.AuthorityId == Decision.TargetBuildingId ? 1 : 0;
        }
        if (Decision.TargetBuildingId.IsNone()
            || TransactionCount != 1
            || WorldState.Trade.FindSpotOrder(Decision.TargetBuildingId) == nullptr
            || AuthoritativeTradeOrderIds.Contains(Decision.TargetBuildingId))
        {
            OutError = TEXT("Every Forgeweave Trade decision must uniquely own one exact spot-order transaction.");
            return false;
        }
        AuthoritativeTradeOrderIds.Add(Decision.TargetBuildingId);
    }
    for (const FDATradeSpotOrderState& Order : WorldState.Trade.SpotOrders)
    {
        if (!AuthoritativeTradeOrderIds.Contains(Order.OrderId))
        {
            OutError = TEXT("Every spot order requires exact authority-derived Trade decision and transaction membership.");
            return false;
        }
    }
    if (AuthoritativeTradeOrderIds.Num() != WorldState.Trade.SpotOrders.Num())
    {
        OutError = TEXT("Authority-derived Trade membership and durable spot orders must be bijective.");
        return false;
    }
    for (const FDATradeDeliveryRecord& Delivery : WorldState.Trade.Deliveries)
    {
        if (WorldState.Trade.FindSpotOrder(Delivery.ContractId) != nullptr
            && !AuthoritativeTradeOrderIds.Contains(Delivery.ContractId))
        {
            OutError = TEXT("Every spot-order delivery requires exact authority-derived Trade membership.");
            return false;
        }
    }
    return true;
}
