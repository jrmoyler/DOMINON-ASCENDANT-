#include "Conquest/DAConquestSystem.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr TCHAR FrozenConquestFingerprint[] = TEXT("9a337e852f4be7fd140b6126e1e207c501c583ce");

    bool IsCompletedQuest(const FDACampaignSnapshot& Campaign, const FName QuestId)
    {
        const FDAQuestSaveState* Quest = Campaign.NarrativeState.FindQuestState(QuestId);
        return Quest != nullptr && Quest->ProgressState == EDAQuestProgressState::Completed;
    }

    bool HasHistory(const FDACampaignSnapshot& Campaign, const FName Tag)
    {
        return Campaign.HistoryTags.Contains(Tag);
    }

    bool HasWorkerEndorsement(const FDACampaignSnapshot& Campaign)
    {
        return IsCompletedQuest(Campaign, TEXT("quest.workers_signal"))
            && (HasHistory(Campaign, TEXT("workers_protected"))
                || HasHistory(Campaign, TEXT("mara_numbers_worker_coalition")));
    }

    bool HasSuccessfulServiceCrisis(const FDACampaignSnapshot& Campaign)
    {
        return Campaign.RegionalCrisis.Resolution != EDAFoundryShortageResolution::None
            && Campaign.RegionalCrisis.Resolution != EDAFoundryShortageResolution::Collapse;
    }

    bool HasFulfilledForgeweaveContract(const FDACampaignSnapshot& Campaign)
    {
        return Campaign.WorldState.Trade.Contracts.ContainsByPredicate([&Campaign](const FDATradeContractState& Contract)
        {
            return Contract.RelationshipId == TEXT("relationship.synara.forgeweave")
                && (Contract.SourceRegionId == TEXT("region.ironheart")
                    || Contract.DestinationRegionId == TEXT("region.ironheart"))
                && Contract.bCompleted && Contract.SuccessfulDeliveryCount > 0
                && Campaign.WorldState.Trade.Deliveries.ContainsByPredicate([&Contract](const FDATradeDeliveryRecord& Delivery)
                {
                    return Delivery.ContractId == Contract.ContractId && Delivery.Quantity > 0;
                });
        });
    }

    const FDADiplomaticReason* FindDependenceThresholdReason(const FDADiplomaticRelationship& Relationship)
    {
        float Dependence = 0.f;
        for (const FDADiplomaticReason& Reason : Relationship.ReasonLedger)
        {
            if (Reason.Metric != EDADiplomaticMetric::Dependence) continue;
            const float Before = Dependence;
            Dependence += Reason.Magnitude;
            if (Before < 25.f && Dependence >= 25.f) return &Reason;
        }
        return nullptr;
    }

    const FDASynaraValueReason* FindFactionSupportThresholdReason(const FDACampaignSnapshot& Campaign)
    {
        return Campaign.SynaraState.FactionSupportReasons.FindByPredicate(
            [](const FDASynaraValueReason& Reason)
            {
                return Reason.SubjectId == TEXT("faction.synara.human_agency")
                    && Reason.Baseline < 65.0 && Reason.Result >= 65.0;
            });
    }

    const FDAFoundryShortageResolutionRecord* FindCrisisResolutionRecord(
        const FDACampaignSnapshot& Campaign)
    {
        return Campaign.RegionalCrisis.ResolutionRecords.FindByPredicate(
            [&Campaign](const FDAFoundryShortageResolutionRecord& Record)
            {
                return Record.ActionId.IsValid() && Record.Resolution == Campaign.RegionalCrisis.Resolution;
            });
    }

    double* ResolveMeter(FDAConquestCampaignState& State, const EDAConquestMeter Meter)
    {
        switch (Meter)
        {
        case EDAConquestMeter::MilitarySovereignty: return &State.MilitarySovereignty;
        case EDAConquestMeter::EconomicAutonomy: return &State.EconomicAutonomy;
        case EDAConquestMeter::CivicLegitimacy: return &State.CivicLegitimacy;
        case EDAConquestMeter::AllianceReadiness: return &State.AllianceReadiness;
        default: return nullptr;
        }
    }

    double* ResolveWeight(FDAConquestCampaignState& State, const EDAForgeweaveRoute Route)
    {
        switch (Route)
        {
        case EDAForgeweaveRoute::Force: return &State.ForceWeight;
        case EDAForgeweaveRoute::Economic: return &State.EconomicWeight;
        case EDAForgeweaveRoute::Influence: return &State.InfluenceWeight;
        case EDAForgeweaveRoute::Alliance: return &State.AllianceWeight;
        default: return nullptr;
        }
    }

    bool ApplyEvidence(FDACampaignSnapshot& Campaign, const FName MutationId,
        const EDAForgeweaveRoute Route, const EDAConquestMeter Meter,
        const FName SourceAuthority, const FName SourceId, const double Delta)
    {
        FDAConquestCampaignState& State = Campaign.ConquestState;
        if (State.FindMutation(MutationId) != nullptr) return false;
        double* MeterValue = ResolveMeter(State, Meter);
        double* Weight = ResolveWeight(State, Route);
        if (MeterValue == nullptr || Weight == nullptr || Delta == 0.0) return false;
        FDAConquestMeterMutation& Mutation = State.Mutations.Emplace_GetRef();
        Mutation.MutationId = MutationId;
        Mutation.Route = Route;
        Mutation.Meter = Meter;
        Mutation.SourceAuthority = SourceAuthority;
        Mutation.SourceId = SourceId;
        Mutation.Delta = Delta;
        Mutation.Result = FMath::Clamp(*MeterValue + Delta, 0.0, 100.0);
        Mutation.WorldTick = FMath::Max<int64>(0, Campaign.WorldState.CurrentWorldTick);
        *MeterValue = Mutation.Result;
        *Weight += FMath::Abs(Delta);
        State.MutationRevision = State.Mutations.Num();
        return true;
    }

    void AppendWeightHistory(FDAConquestCampaignState& State, const int64 WorldTick)
    {
        if (State.Mutations.IsEmpty()) return;
        if (!State.RouteWeightHistory.IsEmpty()
            && State.RouteWeightHistory.Last().Revision == State.MutationRevision) return;
        FDAConquestRouteWeightRecord& Record = State.RouteWeightHistory.Emplace_GetRef();
        Record.WorldTick = FMath::Max<int64>(0, WorldTick);
        Record.Revision = State.MutationRevision;
        Record.Force = State.ForceWeight;
        Record.Economic = State.EconomicWeight;
        Record.Influence = State.InfluenceWeight;
        Record.Alliance = State.AllianceWeight;
    }

    double CrisisReadiness(const FDACampaignSnapshot& Campaign)
    {
        if (Campaign.RegionalCrisis.Resolution == EDAFoundryShortageResolution::BrokeredCompact
            || HasHistory(Campaign, TEXT("joint_forgeweave_crisis_success"))) return 100.0;
        return HasSuccessfulServiceCrisis(Campaign) ? 65.0 : 0.0;
    }

    bool IsAllianceCrisisMutation(const FDAConquestMeterMutation& Mutation)
    {
        return Mutation.Route == EDAForgeweaveRoute::Alliance
            && (Mutation.SourceAuthority == TEXT("campaign.regional_crisis_resolution.alliance")
                || (Mutation.SourceAuthority == TEXT("campaign.history")
                    && Mutation.SourceId == TEXT("joint_forgeweave_crisis_success")));
    }

    double AllianceCrisisContribution(const FDAConquestCampaignState& State)
    {
        double Contribution = 0.0;
        for (const FDAConquestMeterMutation& Mutation : State.Mutations)
            if (IsAllianceCrisisMutation(Mutation)) Contribution += Mutation.Delta;
        return Contribution;
    }

    FString EscapeJson(const FString& Value)
    {
        FString Out(TEXT("\""));
        for (const TCHAR Character : Value)
        {
            if (Character == TEXT('"')) Out += TEXT("\\\"");
            else if (Character == TEXT('\\')) Out += TEXT("\\\\");
            else if (Character == TEXT('\n')) Out += TEXT("\\n");
            else Out.AppendChar(Character);
        }
        return Out + TEXT("\"");
    }

    void CanonicalJson(const TSharedPtr<FJsonValue>& Value, FString& Out, const bool bRoot)
    {
        if (!Value.IsValid()) return;
        switch (Value->Type)
        {
        case EJson::Null: Out += TEXT("null"); break;
        case EJson::String: Out += EscapeJson(Value->AsString()); break;
        case EJson::Boolean: Out += Value->AsBool() ? TEXT("true") : TEXT("false"); break;
        case EJson::Number:
        {
            const double Number = Value->AsNumber();
            const int64 Integer = static_cast<int64>(Number);
            Out += Number == static_cast<double>(Integer)
                ? FString::Printf(TEXT("%lld"), static_cast<long long>(Integer))
                : FString::Printf(TEXT("%.15g"), Number);
            break;
        }
        case EJson::Array:
            Out += TEXT("[");
            for (int32 Index = 0; Index < Value->AsArray().Num(); ++Index)
            {
                if (Index > 0) Out += TEXT(",");
                CanonicalJson(Value->AsArray()[Index], Out, false);
            }
            Out += TEXT("]");
            break;
        case EJson::Object:
        {
            Out += TEXT("{");
            TArray<FString> Keys;
            Value->AsObject()->Values.GetKeys(Keys);
            Keys.Sort();
            bool bFirst = true;
            for (const FString& Key : Keys)
            {
                if (bRoot && Key == TEXT("fingerprint")) continue;
                if (!bFirst) Out += TEXT(",");
                bFirst = false;
                Out += EscapeJson(Key) + TEXT(":");
                CanonicalJson(Value->AsObject()->Values[Key], Out, false);
            }
            Out += TEXT("}");
            break;
        }
        default: break;
        }
    }

    FString JsonFingerprint(const TSharedPtr<FJsonObject>& Root)
    {
        FString Canonical;
        CanonicalJson(MakeShared<FJsonValueObject>(Root), Canonical, true);
        FTCHARToUTF8 Utf8(*Canonical);
        uint8 Hash[FSHA1::DigestSize];
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }

    TSet<FString> Keys(std::initializer_list<const TCHAR*> Values)
    {
        TSet<FString> Result;
        for (const TCHAR* Value : Values) Result.Add(Value);
        return Result;
    }

    bool ExactKeys(const TSharedPtr<FJsonObject>& Object, const TSet<FString>& Required,
        const FString& At, TArray<FText>& Errors)
    {
        if (!Object.IsValid()) return false;
        bool bValid = true;
        for (const FString& Key : Required)
            if (!Object->HasField(Key)) { Errors.Add(FText::FromString(At + TEXT(" missing ") + Key)); bValid = false; }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
            if (!Required.Contains(Pair.Key)) { Errors.Add(FText::FromString(At + TEXT(" unknown key ") + Pair.Key)); bValid = false; }
        return bValid;
    }

    bool ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        TArray<FName>& Out, const FString& At, TArray<FText>& Errors)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
        { Errors.Add(FText::FromString(At + TEXT(".") + Field + TEXT(" must be an array."))); return false; }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (!Value.IsValid() || !Value->TryGetString(Text) || Text.IsEmpty())
            { Errors.Add(FText::FromString(At + TEXT(".") + Field + TEXT(" has invalid entry."))); return false; }
            Out.Add(FName(*Text));
        }
        return true;
    }

    FString QuestProjection(const FDARegionalQuestEntry& Quest)
    {
        FString Out = Quest.QuestId.ToString() + TEXT("|") + Quest.Title + TEXT("|")
            + Quest.AssetPath + TEXT("|") + Quest.Trigger;
        for (const FName Citizen : Quest.CitizenIds) Out += TEXT("|") + Citizen.ToString();
        for (const FName Choice : Quest.Choices) Out += TEXT("|") + Choice.ToString();
        for (const FName System : Quest.Systems) Out += TEXT("|") + System.ToString();
        for (const FName Tag : Quest.OutcomeTags) Out += TEXT("|") + Tag.ToString();
        TArray<FName> ChoiceKeys;
        Quest.ChoiceOutcomeTags.GetKeys(ChoiceKeys);
        ChoiceKeys.Sort([](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
        for (const FName Choice : ChoiceKeys)
            Out += TEXT("|") + Choice.ToString() + TEXT("=") + Quest.ChoiceOutcomeTags[Choice].ToString();
        for (const FName Condition : Quest.DialogueConditions) Out += TEXT("|") + Condition.ToString();
        for (const FDARegionalQuestNodeEntry& Node : Quest.Nodes)
        {
            Out += TEXT("|") + Node.NodeId.ToString() + TEXT(":") + Node.NodeType.ToString();
            for (const FDARegionalQuestEdgeEntry& Edge : Node.Edges)
                Out += TEXT("|") + Edge.Branch.ToString() + TEXT("->") + Edge.Target.ToString();
        }
        return Out;
    }
}

bool FDAConquestSystem::Synchronize(FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    if (InOutCampaign.ConquestState.bForgeweaveResolved)
        return FDAConquestAuthorityValidator::ValidateResolvedRoute(InOutCampaign, OutError);
    const FDAConquestCampaignState OriginalState = InOutCampaign.ConquestState;
    bool bChanged = false;
    const FDARegionState* Ironheart = InOutCampaign.WorldState.FindRegion(TEXT("region.ironheart"));
    if (Ironheart != nullptr)
    {
        for (const FName Tag : Ironheart->PersistentDelta.StateTags)
            if (Tag.ToString().StartsWith(TEXT("control_zone.synara.")))
                bChanged |= ApplyEvidence(InOutCampaign, FName(*(TEXT("conquest.force.zone.") + Tag.ToString())),
                    EDAForgeweaveRoute::Force, EDAConquestMeter::MilitarySovereignty,
                    TEXT("world.region.control_zone"), Tag, -15.0);
    }
    for (const FDACaptureRecord& Capture : InOutCampaign.OperationConflict.CaptureRecords)
    {
        const FDAWorldAssetRecord* Asset = InOutCampaign.FindWorldAssetRecord(Capture.WorldAssetId);
        const bool bMilitary = Asset != nullptr && (Asset->CardDefinitionId == TEXT("forgeweave.heavy_carrier")
            || Asset->CardDefinitionId == TEXT("forgeweave.command_bastion")
            || Asset->CardDefinitionId == TEXT("forgeweave.grand_forge"));
        if (bMilitary && Capture.bCaptureCompleted && Capture.bOutcomeResolved
            && Capture.OriginalOwnerCivilizationId == TEXT("civilization.forgeweave")
            && Capture.CapturingCivilizationId == TEXT("civilization.synara"))
        {
            const FName Source(*Capture.WorldAssetId.ToString(EGuidFormats::Digits));
            bChanged |= ApplyEvidence(InOutCampaign, FName(*(TEXT("conquest.force.capture.") + Source.ToString())),
                EDAForgeweaveRoute::Force, EDAConquestMeter::MilitarySovereignty,
                TEXT("conflict.capture"), Source, -20.0);
        }
    }
    if (HasHistory(InOutCampaign, TEXT("forgeweave_elite_defeated")))
        bChanged |= ApplyEvidence(InOutCampaign, TEXT("conquest.force.elite_defeated"), EDAForgeweaveRoute::Force,
            EDAConquestMeter::MilitarySovereignty, TEXT("campaign.history"), TEXT("forgeweave_elite_defeated"), -20.0);
    for (const FDAStructuralDamageRecord& Damage : InOutCampaign.OperationConflict.StructuralDamageRecords)
        if (Damage.bProductionDisabled && (Damage.CardDefinitionId == TEXT("forgeweave.city_shield")
            || Damage.CardDefinitionId == TEXT("forgeweave.command_core")))
        {
            const FName Source(*Damage.WorldAssetId.ToString(EGuidFormats::Digits));
            bChanged |= ApplyEvidence(InOutCampaign, FName(*(TEXT("conquest.force.structure.") + Source.ToString())),
                EDAForgeweaveRoute::Force, EDAConquestMeter::MilitarySovereignty,
                TEXT("conflict.structural_damage"), Source, -25.0);
        }

    for (const FDATradeContractState& Contract : InOutCampaign.WorldState.Trade.Contracts)
        if (Contract.RelationshipId == TEXT("relationship.synara.forgeweave")
            && (Contract.SourceRegionId == TEXT("region.ironheart") || Contract.DestinationRegionId == TEXT("region.ironheart"))
            && Contract.bCompleted && Contract.SuccessfulDeliveryCount > 0
            && InOutCampaign.WorldState.Trade.Deliveries.ContainsByPredicate([&Contract](const FDATradeDeliveryRecord& Delivery)
                { return Delivery.ContractId == Contract.ContractId && Delivery.Quantity > 0; }))
            bChanged |= ApplyEvidence(InOutCampaign, FName(*(TEXT("conquest.economic.contract.") + Contract.ContractId.ToString())),
                EDAForgeweaveRoute::Economic, EDAConquestMeter::EconomicAutonomy,
                TEXT("trade.contract"), Contract.ContractId, -FDAConquestRules::EconomicActionLossCap);
    const FDARegionState* Freight = InOutCampaign.WorldState.FindRegion(TEXT("region.freight_corridor"));
    if (Freight != nullptr && Freight->OwnerId == TEXT("civilization.synara"))
        bChanged |= ApplyEvidence(InOutCampaign, TEXT("conquest.economic.freight_share"), EDAForgeweaveRoute::Economic,
            EDAConquestMeter::EconomicAutonomy, TEXT("world.region.owner"), Freight->RegionId, -15.0);
    const FDADiplomaticRelationship* Forge = InOutCampaign.WorldState.Diplomacy.FindRelationship(
        TEXT("relationship.synara.forgeweave"));
    const FDADiplomaticReason* DependenceReason = Forge == nullptr
        ? nullptr : FindDependenceThresholdReason(*Forge);
    if (DependenceReason != nullptr && Forge->Dependence >= 25.f
        && HasFulfilledForgeweaveContract(InOutCampaign))
        bChanged |= ApplyEvidence(InOutCampaign,
            FName(*(TEXT("conquest.economic.component_dependence.")
                + DependenceReason->MutationId.ToString())), EDAForgeweaveRoute::Economic,
            EDAConquestMeter::EconomicAutonomy, TEXT("diplomacy.reason"),
            DependenceReason->MutationId, -15.0);
    const FDAFoundryShortageResolutionRecord* CrisisRecord = FindCrisisResolutionRecord(InOutCampaign);
    if (InOutCampaign.RegionalCrisis.Resolution == EDAFoundryShortageResolution::MarketExploitation
        && CrisisRecord != nullptr && HasFulfilledForgeweaveContract(InOutCampaign))
    {
        const FName Source(*CrisisRecord->ActionId.ToString(EGuidFormats::Digits));
        bChanged |= ApplyEvidence(InOutCampaign,
            FName(*(TEXT("conquest.economic.emergency_finance.") + Source.ToString())),
            EDAForgeweaveRoute::Economic, EDAConquestMeter::EconomicAutonomy,
            TEXT("campaign.regional_crisis_resolution.economic"), Source, -15.0);
    }

    if (IsCompletedQuest(InOutCampaign, TEXT("quest.workers_signal")))
        bChanged |= ApplyEvidence(InOutCampaign, TEXT("conquest.influence.workers_signal"), EDAForgeweaveRoute::Influence,
            EDAConquestMeter::CivicLegitimacy, TEXT("narrative.quest"), TEXT("quest.workers_signal"), -30.0);
    if (HasWorkerEndorsement(InOutCampaign))
        bChanged |= ApplyEvidence(InOutCampaign, TEXT("conquest.influence.worker_endorsement"), EDAForgeweaveRoute::Influence,
            EDAConquestMeter::CivicLegitimacy, TEXT("campaign.history"),
            HasHistory(InOutCampaign, TEXT("workers_protected"))
                ? FName(TEXT("workers_protected")) : FName(TEXT("mara_numbers_worker_coalition")), -20.0);
    if (HasSuccessfulServiceCrisis(InOutCampaign) && CrisisRecord != nullptr)
    {
        const FName Source(*CrisisRecord->ActionId.ToString(EGuidFormats::Digits));
        bChanged |= ApplyEvidence(InOutCampaign,
            FName(*(TEXT("conquest.influence.service_crisis.") + Source.ToString())),
            EDAForgeweaveRoute::Influence, EDAConquestMeter::CivicLegitimacy,
            TEXT("campaign.regional_crisis_resolution.influence"), Source, -25.0);
    }
    for (const FName Tag : {FName(TEXT("mara_evidence_exposed")), FName(TEXT("grand_forge_preserved"))})
        if (HasHistory(InOutCampaign, Tag))
            bChanged |= ApplyEvidence(InOutCampaign, FName(*(TEXT("conquest.influence.credibility.") + Tag.ToString())),
                EDAForgeweaveRoute::Influence, EDAConquestMeter::CivicLegitimacy,
                TEXT("campaign.history"), Tag, -10.0);
    const FDASynaraValueReason* FactionReason = FindFactionSupportThresholdReason(InOutCampaign);
    if (FactionReason != nullptr
        && InOutCampaign.SynaraState.FactionSupport.FindRef(TEXT("faction.synara.human_agency")) >= 65.0)
        bChanged |= ApplyEvidence(InOutCampaign,
            FName(*(TEXT("conquest.influence.faction_support.") + FactionReason->ActionId.ToString())),
            EDAForgeweaveRoute::Influence, EDAConquestMeter::CivicLegitimacy,
            TEXT("campaign.faction_support_reason"), FactionReason->ActionId, -15.0);

    FDAAllianceReadinessComponents Components;
    if (Forge != nullptr)
    {
        Components.Trust = FMath::Clamp<double>(Forge->Trust, 0.0, 100.0);
        Components.Respect = FMath::Clamp<double>(Forge->Respect, 0.0, 100.0);
        Components.SharedInterest = FMath::Clamp<double>(Forge->Compatibility, 0.0, 100.0);
    }
    Components.CrisisResolution = CrisisReadiness(InOutCampaign);
    InOutCampaign.ConquestState.AllianceComponents = Components;
    if (Forge != nullptr)
    {
        double RawTotals[3] = {};
        double ClampedTotals[3] = {};
        for (const FDADiplomaticReason& Reason : Forge->ReasonLedger)
        {
            const int32 ComponentIndex = Reason.Metric == EDADiplomaticMetric::Trust ? 0
                : Reason.Metric == EDADiplomaticMetric::Compatibility ? 1
                : Reason.Metric == EDADiplomaticMetric::Respect ? 2 : INDEX_NONE;
            if (ComponentIndex == INDEX_NONE) continue;
            const double Before = ClampedTotals[ComponentIndex];
            RawTotals[ComponentIndex] += Reason.Magnitude;
            ClampedTotals[ComponentIndex] = FMath::Clamp(RawTotals[ComponentIndex], 0.0, 100.0);
            const double Delta = (ClampedTotals[ComponentIndex] - Before) / 4.0;
            if (!FMath::IsNearlyZero(Delta, 0.0001))
                bChanged |= ApplyEvidence(InOutCampaign,
                    FName(*(TEXT("conquest.alliance.reason.") + Reason.MutationId.ToString())),
                    EDAForgeweaveRoute::Alliance, EDAConquestMeter::AllianceReadiness,
                    TEXT("diplomacy.reason"), Reason.MutationId, Delta);
        }
    }
    double BaseCrisisReadiness = 0.0;
    double ProjectedCrisisContribution = AllianceCrisisContribution(InOutCampaign.ConquestState);
    if (CrisisRecord != nullptr && CrisisReadiness(InOutCampaign) > 0.0)
    {
        const FName Source(*CrisisRecord->ActionId.ToString(EGuidFormats::Digits));
        BaseCrisisReadiness = CrisisRecord->Resolution == EDAFoundryShortageResolution::BrokeredCompact
            ? 100.0 : 65.0;
        const bool bJointHistoryAlreadyProjected = InOutCampaign.ConquestState.FindMutation(
            TEXT("conquest.alliance.joint_crisis_success")) != nullptr;
        const double DesiredContribution = bJointHistoryAlreadyProjected
            ? 25.0 : BaseCrisisReadiness / 4.0;
        const double Remaining = DesiredContribution - ProjectedCrisisContribution;
        if (Remaining < -0.0001)
        {
            InOutCampaign.ConquestState = OriginalState;
            OutError = TEXT("Alliance crisis evidence cannot exceed its canonical readiness component.");
            return false;
        }
        if (Remaining > 0.0001)
        {
            bChanged |= ApplyEvidence(InOutCampaign,
                FName(*(TEXT("conquest.alliance.crisis.") + Source.ToString())),
                EDAForgeweaveRoute::Alliance, EDAConquestMeter::AllianceReadiness,
                TEXT("campaign.regional_crisis_resolution.alliance"), Source, Remaining);
            ProjectedCrisisContribution = AllianceCrisisContribution(InOutCampaign.ConquestState);
        }
    }
    if (BaseCrisisReadiness < 100.0
        && HasHistory(InOutCampaign, TEXT("joint_forgeweave_crisis_success")))
    {
        const double Remaining = 25.0 - ProjectedCrisisContribution;
        if (Remaining < -0.0001)
        {
            InOutCampaign.ConquestState = OriginalState;
            OutError = TEXT("Alliance joint-crisis history cannot exceed its canonical readiness component.");
            return false;
        }
        if (Remaining > 0.0001)
            bChanged |= ApplyEvidence(InOutCampaign, TEXT("conquest.alliance.joint_crisis_success"),
                EDAForgeweaveRoute::Alliance, EDAConquestMeter::AllianceReadiness,
                TEXT("campaign.history"), TEXT("joint_forgeweave_crisis_success"), Remaining);
    }
    if (bChanged) AppendWeightHistory(InOutCampaign.ConquestState, InOutCampaign.WorldState.CurrentWorldTick);
    if (!InOutCampaign.ConquestState.Validate(OutError))
    {
        InOutCampaign.ConquestState = OriginalState;
        return false;
    }
    return true;
}

bool FDAConquestSystem::CanCompleteRoute(const EDAForgeweaveRoute Route,
    const FDACampaignSnapshot& Campaign, FString& OutError)
{
    return FDAConquestAuthorityValidator::CanCompleteRoute(Route, Campaign, OutError);
}

bool FDAConquestSystem::CompleteRoute(const FGuid ActionId, const EDAForgeweaveRoute Route,
    FDACampaignSnapshot& InOutCampaign, FString& OutError)
{
    if (!ActionId.IsValid()) { OutError = TEXT("Conquest resolution requires a stable action id."); return false; }
    if (!CanCompleteRoute(Route, InOutCampaign, OutError)) return false;
    FDACampaignSnapshot Candidate = InOutCampaign;
    Candidate.ConquestState.bForgeweaveResolved = true;
    Candidate.ConquestState.ResolvedRoute = Route;
    Candidate.ConquestState.ResolutionActionId = ActionId;
    Candidate.ConquestState.ResolvedWorldTick = FMath::Max<int64>(0, Candidate.WorldState.CurrentWorldTick);
    static const FName Tags[] = {TEXT("forgeweave_forced"), TEXT("forgeweave_economic_union"),
        TEXT("forgeweave_influence_transfer"), TEXT("forgeweave_allied")};
    Candidate.HistoryTags.AddUnique(Tags[static_cast<int32>(Route)]);
    Candidate.HistoryTags.Sort([](const FName Left, const FName Right){ return Left.LexicalLess(Right); });
    if (!Candidate.ConquestState.Validate(OutError)
        || !FDAConquestAuthorityValidator::ValidateResolvedRoute(Candidate, OutError)) return false;
    InOutCampaign = MoveTemp(Candidate);
    return true;
}

FString FDAForgeweaveConquestQuestPipeline::GetCanonicalManifestPath()
{
    return FPaths::ProjectContentDir() / TEXT("DA/Manifests/ForgeweaveConquest.json");
}

bool FDAForgeweaveConquestQuestPipeline::LoadCanonical(
    FDAForgeweaveConquestQuestManifest& OutManifest, TArray<FText>& Errors)
{
    return LoadFile(GetCanonicalManifestPath(), OutManifest, Errors);
}

bool FDAForgeweaveConquestQuestPipeline::LoadFile(const FString& Filename,
    FDAForgeweaveConquestQuestManifest& OutManifest, TArray<FText>& Errors)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Filename))
    { Errors.Add(FText::FromString(TEXT("Could not load Forgeweave conquest manifest."))); return false; }
    return ParseJson(Json, OutManifest, Errors);
}

bool FDAForgeweaveConquestQuestPipeline::ParseJson(const FString& Json,
    FDAForgeweaveConquestQuestManifest& OutManifest, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num();
    OutManifest = {};
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    { Errors.Add(FText::FromString(TEXT("Forgeweave conquest manifest is not JSON."))); return false; }
    ExactKeys(Root, Keys({TEXT("schemaVersion"), TEXT("campaignId"), TEXT("fingerprint"), TEXT("quests")}),
        TEXT("manifest"), Errors);
    double Version = 0.0;
    FString CampaignId;
    Root->TryGetNumberField(TEXT("schemaVersion"), Version);
    Root->TryGetStringField(TEXT("campaignId"), CampaignId);
    Root->TryGetStringField(TEXT("fingerprint"), OutManifest.Fingerprint);
    OutManifest.SchemaVersion = static_cast<int32>(Version);
    OutManifest.CampaignId = FName(*CampaignId);
    if (Version != 1.0 || OutManifest.Fingerprint != JsonFingerprint(Root)
        || OutManifest.Fingerprint != FrozenConquestFingerprint)
        Errors.Add(FText::FromString(TEXT("Forgeweave conquest identity or fingerprint diverges.")));
    const TArray<TSharedPtr<FJsonValue>>* Quests = nullptr;
    if (!Root->TryGetArrayField(TEXT("quests"), Quests) || Quests == nullptr)
        Errors.Add(FText::FromString(TEXT("manifest.quests must be an array.")));
    else for (int32 Index = 0; Index < Quests->Num(); ++Index)
    {
        const FString At = FString::Printf(TEXT("quests[%d]"), Index);
        if (!(*Quests)[Index].IsValid() || (*Quests)[Index]->Type != EJson::Object)
        { Errors.Add(FText::FromString(At + TEXT(" must be an object."))); continue; }
        const TSharedPtr<FJsonObject> Object = (*Quests)[Index]->AsObject();
        ExactKeys(Object, Keys({TEXT("id"), TEXT("title"), TEXT("assetPath"), TEXT("citizenIds"), TEXT("trigger"),
            TEXT("choices"), TEXT("systems"), TEXT("outcomeTags"), TEXT("choiceOutcomeTags"),
            TEXT("dialogueConditions"), TEXT("nodes"), TEXT("requirements")}), At, Errors);
        FDARegionalQuestEntry& Quest = OutManifest.Quests.Emplace_GetRef();
        FString Text;
        if (Object->TryGetStringField(TEXT("id"), Text)) Quest.QuestId = FName(*Text);
        if (Object->TryGetStringField(TEXT("title"), Text)) Quest.Title = Text;
        if (Object->TryGetStringField(TEXT("assetPath"), Text)) Quest.AssetPath = Text;
        Object->TryGetStringField(TEXT("trigger"), Quest.Trigger);
        ReadStringArray(Object, TEXT("citizenIds"), Quest.CitizenIds, At, Errors);
        ReadStringArray(Object, TEXT("choices"), Quest.Choices, At, Errors);
        ReadStringArray(Object, TEXT("systems"), Quest.Systems, At, Errors);
        ReadStringArray(Object, TEXT("outcomeTags"), Quest.OutcomeTags, At, Errors);
        ReadStringArray(Object, TEXT("dialogueConditions"), Quest.DialogueConditions, At, Errors);
        const TSharedPtr<FJsonObject>* Outcomes = nullptr;
        if (Object->TryGetObjectField(TEXT("choiceOutcomeTags"), Outcomes) && Outcomes != nullptr)
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Outcomes)->Values)
            {
                FString Outcome;
                if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(Outcome) || Outcome.IsEmpty())
                    Errors.Add(FText::FromString(At + TEXT(" has malformed choice outcome.")));
                else Quest.ChoiceOutcomeTags.Add(FName(*Pair.Key), FName(*Outcome));
            }
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        if (!Object->TryGetArrayField(TEXT("nodes"), Nodes) || Nodes == nullptr)
            Errors.Add(FText::FromString(At + TEXT(".nodes must be an array.")));
        else for (int32 NodeIndex = 0; NodeIndex < Nodes->Num(); ++NodeIndex)
        {
            if (!(*Nodes)[NodeIndex].IsValid() || (*Nodes)[NodeIndex]->Type != EJson::Object) continue;
            const TSharedPtr<FJsonObject> NodeObject = (*Nodes)[NodeIndex]->AsObject();
            ExactKeys(NodeObject, Keys({TEXT("id"), TEXT("type"), TEXT("edges")}), At + TEXT(".node"), Errors);
            FDARegionalQuestNodeEntry& Node = Quest.Nodes.Emplace_GetRef();
            if (NodeObject->TryGetStringField(TEXT("id"), Text)) Node.NodeId = FName(*Text);
            if (NodeObject->TryGetStringField(TEXT("type"), Text)) Node.NodeType = FName(*Text);
            const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
            if (NodeObject->TryGetArrayField(TEXT("edges"), Edges) && Edges != nullptr)
                for (const TSharedPtr<FJsonValue>& EdgeValue : *Edges)
                {
                    if (!EdgeValue.IsValid() || EdgeValue->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> EdgeObject = EdgeValue->AsObject();
                    ExactKeys(EdgeObject, Keys({TEXT("branch"), TEXT("target")}), At + TEXT(".edge"), Errors);
                    FDARegionalQuestEdgeEntry& Edge = Node.Edges.Emplace_GetRef();
                    if (EdgeObject->TryGetStringField(TEXT("branch"), Text)) Edge.Branch = FName(*Text);
                    if (EdgeObject->TryGetStringField(TEXT("target"), Text)) Edge.Target = FName(*Text);
                }
        }
    }
    return Errors.Num() == Before && Validate(OutManifest, Errors);
}

bool FDAForgeweaveConquestQuestPipeline::Validate(
    const FDAForgeweaveConquestQuestManifest& Manifest, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num();
    static const TArray<FName> Ids = {TEXT("quest.broker_of_ironheart"), TEXT("quest.workers_signal"),
        TEXT("quest.supply_noose"), TEXT("quest.operation_iron_veil"), TEXT("quest.third_foundry")};
    static const TArray<FString> Paths = {TEXT("/Game/DA/Quests/Q_BrokerOfIronheart"),
        TEXT("/Game/DA/Quests/Q_WorkersSignal"), TEXT("/Game/DA/Quests/Q_SupplyNoose"),
        TEXT("/Game/DA/Quests/Q_OperationIronVeil"), TEXT("/Game/DA/Quests/Q_ThirdFoundry")};
    if (Manifest.SchemaVersion != 1 || Manifest.CampaignId != TEXT("campaign.vertical_slice.forgeweave_conquest")
        || Manifest.Fingerprint != FrozenConquestFingerprint || Manifest.Quests.Num() != Ids.Num())
        Errors.Add(FText::FromString(TEXT("Forgeweave conquest manifest identity/count is not canonical.")));
    for (int32 Index = 0; Index < Manifest.Quests.Num(); ++Index)
    {
        const FDARegionalQuestEntry& Quest = Manifest.Quests[Index];
        if (!Ids.IsValidIndex(Index) || Quest.QuestId != Ids[Index] || Quest.AssetPath != Paths[Index]
            || !Quest.Nodes.ContainsByPredicate([](const FDARegionalQuestNodeEntry& Node)
                { return Node.NodeId == TEXT("start") && Node.NodeType == TEXT("start"); })
            || !Quest.Nodes.ContainsByPredicate([](const FDARegionalQuestNodeEntry& Node)
                { return Node.NodeType == TEXT("resolution"); }))
            Errors.Add(FText::FromString(TEXT("Forgeweave conquest quest order/path/graph diverges.")));
        for (const FDARegionalQuestNodeEntry& Node : Quest.Nodes)
            for (const FDARegionalQuestEdgeEntry& Edge : Node.Edges)
                if (!Quest.Nodes.ContainsByPredicate([&Edge](const FDARegionalQuestNodeEntry& Target)
                    { return Target.NodeId == Edge.Target; }))
                    Errors.Add(FText::FromString(TEXT("Forgeweave conquest graph targets a foreign node.")));
    }
    return Errors.Num() == Before;
}

bool FDAForgeweaveConquestQuestPipeline::BuildAssets(const FDAForgeweaveConquestQuestManifest& Manifest,
    TArray<UDARegionalQuestDefinition*>& OutQuests, TArray<FText>& Errors)
{
    if (!Validate(Manifest, Errors)) return false;
    OutQuests.Reset();
    for (const FDARegionalQuestEntry& Entry : Manifest.Quests)
    {
        UDARegionalQuestDefinition* Asset = NewObject<UDARegionalQuestDefinition>(GetTransientPackage());
        Asset->Quest = Entry;
        Asset->SourceFingerprint = Manifest.Fingerprint;
        Asset->bRuntimeManifestFallback = true;
        OutQuests.Add(Asset);
    }
    return true;
}

bool FDAForgeweaveConquestQuestPipeline::ValidateGeneratedCache(
    const FDAForgeweaveConquestQuestManifest& Manifest, const TArray<UDARegionalQuestDefinition*>& Quests,
    TArray<FText>& Errors)
{
    const int32 Before = Errors.Num();
    if (!Validate(Manifest, Errors)) return false;
    if (Quests.Num() != Manifest.Quests.Num())
        Errors.Add(FText::FromString(TEXT("Generated Forgeweave conquest cache is incomplete.")));
    for (int32 Index = 0; Index < Quests.Num() && Index < Manifest.Quests.Num(); ++Index)
        if (Quests[Index] == nullptr || Quests[Index]->bRuntimeManifestFallback
            || Quests[Index]->SourceFingerprint != Manifest.Fingerprint
            || Quests[Index]->GetOutermost() == GetTransientPackage()
            || Quests[Index]->GetOutermost()->GetName() != Manifest.Quests[Index].AssetPath
            || QuestProjection(Quests[Index]->Quest) != QuestProjection(Manifest.Quests[Index]))
            Errors.Add(FText::FromString(TEXT("Generated Forgeweave quest lacks exact semantic/path parity.")));
    return Errors.Num() == Before;
}
