#include "Narrative/DAFirstHourCampaignRuntime.h"

#include "Citizens/DAJobSystem.h"
#include "Content/DAContentManifest.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Economy/DAEconomyTypes.h"
#include "Misc/Crc.h"
#include "Narrative/DAQuestRuntime.h"
#include "Networks/DAUtilityNetwork.h"

namespace
{
    constexpr TCHAR NiaCitizenId[] = TEXT("citizen.synara.nia_vale");
    constexpr TCHAR HumanAgencyFactionId[] = TEXT("faction.synara.human_agency");
    bool IsCompleted(const FDACampaignSnapshot& Campaign, const FName QuestId)
    {
        const FDAQuestSaveState* State = Campaign.NarrativeState.FindQuestState(QuestId);
        return State != nullptr && State->ProgressState == EDAQuestProgressState::Completed;
    }

    const FDAWorldAssetRecord* FindAsset(const FDACampaignSnapshot& Campaign,
        const FDAFirstHourWorldAssetRequirement& Binding)
    {
        return Campaign.WorldAssets.FindByPredicate([&Binding](const FDAWorldAssetRecord& Asset)
        {
            return Asset.WorldAssetId.IsValid() && Asset.CardDefinitionId == Binding.DefinitionId
                && Asset.CityId == Binding.CityId && Asset.OwnerCivilizationId == Binding.OwnerId
                && (!Binding.bRequireOperational || Asset.ConstructionState == EDAConstructionState::Operational);
        });
    }

    const FDAQuestObjectiveAssetBindingRecord* FindDynamicBinding(const FDACampaignSnapshot& Campaign,
        const FName QuestId, const FName BindingId)
    {
        return Campaign.NarrativeState.QuestObjectiveAssetBindings.FindByPredicate(
            [QuestId, BindingId](const FDAQuestObjectiveAssetBindingRecord& Record)
            { return Record.QuestId == QuestId && Record.BindingId == BindingId; });
    }

    const FDAWorldAssetRecord* ResolveBoundAsset(const FDAFirstHourQuestEntry& Quest,
        const FDAFirstHourNode& Node, const FDACampaignSnapshot& Campaign)
    {
        const FDAFirstHourWorldAssetRequirement* Binding = Quest.FindBinding(Node.BindingId);
        if (Binding == nullptr) return nullptr;
        if (Binding->BindWhen == EDAFirstHourBindWhen::Start)
        {
            const FDAQuestSaveState* State = Campaign.NarrativeState.FindQuestState(Quest.Definition.QuestId);
            const FDAQuestWorldAssetBinding* RuntimeBinding = State != nullptr ? State->FindWorldAssetBinding(Binding->BindingId) : nullptr;
            return RuntimeBinding != nullptr ? Campaign.FindWorldAssetRecord(RuntimeBinding->WorldAssetId) : nullptr;
        }
        const FDAQuestObjectiveAssetBindingRecord* Dynamic = FindDynamicBinding(Campaign, Quest.Definition.QuestId, Binding->BindingId);
        return Dynamic != nullptr ? Campaign.FindWorldAssetRecord(Dynamic->WorldAssetId) : nullptr;
    }

    bool HasOwnedCard(const FDACampaignSnapshot& Campaign, const FName DefinitionId)
    {
        for (const TPair<FGuid, FCardInstance>& Pair : Campaign.CollectionState.Instances)
            if (Pair.Value.DefinitionId == DefinitionId) return true;
        return false;
    }

    bool HasUnlock(const FDACampaignSnapshot& Campaign, const FName ActionId)
    {
        return Campaign.NarrativeState.QuestContentUnlockRecords.ContainsByPredicate(
            [ActionId](const FDAQuestContentUnlockRecord& Record) { return Record.ActionId == ActionId; });
    }

    bool IsCanonicalTask19Building(const FName DefinitionId)
    {
        if (DefinitionId == TEXT("special.founder_hall")) return false;
        FDAVerticalSliceContentManifest Task19Manifest; TArray<FText> Errors;
        if (!FDAContentManifestPipeline::LoadCanonical(Task19Manifest, Errors)) return false;
        const FDAManifestCardDefinition* Definition = Task19Manifest.Definitions.FindByPredicate(
            [DefinitionId](const FDAManifestCardDefinition& Candidate) { return Candidate.DefinitionId == DefinitionId; });
        return Definition != nullptr && Definition->bPlaceable;
    }

    const FDAAuditEligibilitySourceRecord* FindEligibilitySource(const FDACampaignSnapshot& Campaign,
        const FGuid ActionId, const FName EligibilityId, const FName RequiredTag,
        const int64 AtOrBeforeTick)
    {
        return Campaign.NarrativeState.AuditEligibilitySourceRecords.FindByPredicate(
            [ActionId, EligibilityId, RequiredTag, AtOrBeforeTick](const FDAAuditEligibilitySourceRecord& Source)
            {
                return Source.EligibilityId == EligibilityId && Source.SourceActionId == ActionId
                    && Source.SourceActionTag == RequiredTag && Source.WorldTick <= AtOrBeforeTick;
            });
    }

    bool IsOutcomeEligible(const FDAFirstHourOutcome& Outcome, const FDAFirstHourProgressionContext& Context,
        const FDACampaignSnapshot& Campaign)
    {
        if (Outcome.EligibilityAny.IsEmpty()) return true;
        return (Outcome.EligibilityAny.Contains(EDAFirstHourEligibility::Vision)
                && FindEligibilitySource(Campaign, Context.VisionActionId, TEXT("Vision"),
                    TEXT("capability.vision.available"), Context.WorldTick) != nullptr)
            || (Outcome.EligibilityAny.Contains(EDAFirstHourEligibility::ResearchAction)
                && FindEligibilitySource(Campaign, Context.ResearchActionId, TEXT("ResearchAction"),
                    TEXT("action.research.replacement_model.completed"), Context.WorldTick) != nullptr);
    }

    bool ApplyIronDiplomacy(const FDAFirstHourOutcome& Outcome, const int64 WorldTick, FDACampaignSnapshot& Campaign)
    {
        if (!Campaign.WorldState.bInitialized) return true;
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>();
        for (const FName Effect : Outcome.SemanticEffects)
        {
            EDADiplomaticMetric Metric = EDADiplomaticMetric::Trust; float Direction = 0.f;
            if (Effect == TEXT("effect.forgeweave.trust_up")) Direction = 1.f;
            else if (Effect == TEXT("effect.forgeweave.trust_down")) Direction = -1.f;
            else if (Effect == TEXT("effect.forgeweave.respect_up")) { Metric = EDADiplomaticMetric::Respect; Direction = 1.f; }
            else if (Effect == TEXT("effect.forgeweave.dependence_up")) { Metric = EDADiplomaticMetric::Dependence; Direction = 1.f; }
            else continue;
            if (!Diplomacy->ApplyReason(Campaign, TEXT("relationship.synara.forgeweave"), Metric,
                Outcome.ActionId, Direction, WorldTick, Effect)) return false;
        }
        return true;
    }

    bool HasLiveNia(const FDAFirstHourProgressionContext& Context, const FGuid HomeAssetId = FGuid())
    {
        if (Context.LiveSignals != nullptr)
        {
            const FDACampaignCitizenSignal* Nia = Context.LiveSignals->FindCitizen(TEXT("citizen.synara.nia_vale"));
            return Nia != nullptr && Nia->CityId == TEXT("player_capital")
                && (!HomeAssetId.IsValid() || Nia->HomeWorldAssetId == HomeAssetId);
        }
        return Context.CityState != nullptr && Context.CityState->Citizens.ContainsByPredicate(
            [HomeAssetId](const FDACitizenRecord& Citizen)
            { return Citizen.CitizenId == TEXT("citizen.synara.nia_vale") && Citizen.CityId == TEXT("player_capital")
                && (!HomeAssetId.IsValid() || Citizen.HomeAssetId == HomeAssetId); });
    }

    bool IsFullySupplied(const FDAFirstHourProgressionContext& Context,
        const FGuid AssetId, const EDACampaignUtilityKind LiveKind, const EDAUtilityType LegacyKind)
    {
        if (Context.LiveSignals != nullptr)
            return Context.LiveSignals->ResolveUtility(LiveKind, AssetId) == EDACampaignUtilitySupply::FullySupplied;
        return Context.UtilityNetwork != nullptr
            && Context.UtilityNetwork->ResolveUtility(LegacyKind, AssetId).State == EDAUtilityState::FullySupplied;
    }

    bool IsPowered(const FDAFirstHourProgressionContext& Context, const FGuid AssetId)
    {
        if (Context.LiveSignals != nullptr)
            return Context.LiveSignals->ResolveUtility(EDACampaignUtilityKind::Power, AssetId)
                != EDACampaignUtilitySupply::Offline;
        return Context.UtilityNetwork != nullptr
            && Context.UtilityNetwork->ResolveUtility(EDAUtilityType::Power, AssetId).State != EDAUtilityState::Offline;
    }

    bool StartConditionSatisfied(const FDAFirstHourQuestEntry& Quest, const FDAFirstHourProgressionContext& Context,
        const FDACampaignSnapshot& Campaign)
    {
        switch (Quest.StartCondition)
        {
        case EDAFirstHourStartCondition::NewCampaign:
            return Campaign.NarrativeState.QuestStates.IsEmpty();
        case EDAFirstHourStartCondition::PrerequisitesMet:
            return true;
        case EDAFirstHourStartCondition::HabitatOccupied:
        {
            const FDAFirstHourWorldAssetRequirement* Habitat = Quest.FindBinding(TEXT("adaptive_habitat"));
            const FDAWorldAssetRecord* Asset = Habitat != nullptr ? FindAsset(Campaign, *Habitat) : nullptr;
            return Asset != nullptr && HasLiveNia(Context, Asset->WorldAssetId);
        }
        case EDAFirstHourStartCondition::AutonomousExchangeAvailable:
        {
            const FDAFirstHourWorldAssetRequirement* Tower = Quest.FindBinding(TEXT("cognitive_operations_tower"));
            const FDAFirstHourWorldAssetRequirement* Exchange = Quest.FindBinding(TEXT("autonomous_exchange"));
            return Tower != nullptr && Exchange != nullptr && FindAsset(Campaign, *Tower) != nullptr
                && (FindAsset(Campaign, *Exchange) != nullptr || HasOwnedCard(Campaign, Exchange->DefinitionId));
        }
        case EDAFirstHourStartCondition::DependencyAbove25:
            return Campaign.SynaraState.Dependency > 25.0;
        case EDAFirstHourStartCondition::SixPlayerBuildings:
            return Campaign.WorldAssets.FilterByPredicate([](const FDAWorldAssetRecord& Asset)
            { return IsCanonicalTask19Building(Asset.CardDefinitionId)
                && Asset.CityId == TEXT("player_capital") && Asset.OwnerCivilizationId == TEXT("civilization.synara")
                && Asset.ConstructionState == EDAConstructionState::Operational; }).Num() >= 6;
        case EDAFirstHourStartCondition::WorldMapUnlocked:
            return Campaign.NarrativeState.WorldMapAuthorityRecords.ContainsByPredicate([&Campaign](const FDAWorldMapAuthorityRecord& Typed)
            {
                return Campaign.NarrativeState.QuestContentUnlockRecords.ContainsByPredicate([&Typed](const FDAQuestContentUnlockRecord& Unlock)
                    { return Typed.SourceQuestId == TEXT("quest.signal_in_foundation")
                        && Unlock.QuestId == Typed.SourceQuestId && Unlock.ActionId == Typed.SourceActionId
                        && Unlock.Type == EDAQuestContentUnlockType::AxiomArchiveFragment
                        && Unlock.ContentId == TEXT("archive.axiom.fragment.01") && Unlock.WorldTick == Typed.WorldTick; });
            });
        case EDAFirstHourStartCondition::ForgeweaveContact:
            return HasUnlock(Campaign, TEXT("reward.iron_at_border.forgeweave_contact"));
        default: return false;
        }
    }

    EDAFirstHourPlayerAction ExpectedAction(const EDAFirstHourNodeCondition Condition)
    {
        switch (Condition)
        {
        case EDAFirstHourNodeCondition::FounderReached: return EDAFirstHourPlayerAction::ReachFounderHall;
        case EDAFirstHourNodeCondition::FounderHallPowered: return EDAFirstHourPlayerAction::RestoreFounderHallPower;
        case EDAFirstHourNodeCondition::FounderHallOnline: return EDAFirstHourPlayerAction::ActivateFounderHallCore;
        case EDAFirstHourNodeCondition::CustodianMarkingsInspected: return EDAFirstHourPlayerAction::InspectCustodianMarkings;
        case EDAFirstHourNodeCondition::AdaptiveHabitatInspected: return EDAFirstHourPlayerAction::InspectAdaptiveHabitatCard;
        case EDAFirstHourNodeCondition::AdaptiveHabitatPlaced: return EDAFirstHourPlayerAction::PlaceAdaptiveHabitat;
        case EDAFirstHourNodeCondition::UtilityExpansionAcknowledged: return EDAFirstHourPlayerAction::AcknowledgeUtilityExpansion;
        case EDAFirstHourNodeCondition::NiaSpokenTo: return EDAFirstHourPlayerAction::SpeakToNia;
        case EDAFirstHourNodeCondition::ReplacementModelDiscovered: return EDAFirstHourPlayerAction::DiscoverReplacementModel;
        case EDAFirstHourNodeCondition::WorkersRetrained: return EDAFirstHourPlayerAction::RetrainWorkers;
        case EDAFirstHourNodeCondition::AutomationCapEnacted: return EDAFirstHourPlayerAction::EnactAutomationCap;
        case EDAFirstHourNodeCondition::EnteredUtilityTunnel: return EDAFirstHourPlayerAction::EnterUtilityTunnel;
        case EDAFirstHourNodeCondition::AncientNodeRestored: return EDAFirstHourPlayerAction::RestoreAncientNode;
        case EDAFirstHourNodeCondition::MaintenanceDronesDefeated: return EDAFirstHourPlayerAction::DefeatMaintenanceDrones;
        case EDAFirstHourNodeCondition::UnknownSymbolInspected: return EDAFirstHourPlayerAction::InspectUnknownSymbol;
        case EDAFirstHourNodeCondition::EdenBasinReached: return EDAFirstHourPlayerAction::ReachEdenBasin;
        case EDAFirstHourNodeCondition::WaterQualityInspected: return EDAFirstHourPlayerAction::InspectWaterQuality;
        case EDAFirstHourNodeCondition::AmaraSpokenTo: return EDAFirstHourPlayerAction::SpeakToAmara;
        case EDAFirstHourNodeCondition::OriSpokenTo: return EDAFirstHourPlayerAction::SpeakToOri;
        default: return EDAFirstHourPlayerAction::None;
        }
    }

    bool NodeConditionSatisfied(const FDAFirstHourQuestEntry& Quest, const FDAFirstHourNode& Node,
        const FDAFirstHourProgressionContext& Context, const FDACampaignSnapshot& Campaign)
    {
        const EDAFirstHourPlayerAction RequiredAction = ExpectedAction(Node.Condition);
        if (RequiredAction != EDAFirstHourPlayerAction::None && Context.PlayerAction != RequiredAction) return false;
        const FDAWorldAssetRecord* Bound = Node.BindingId.IsNone() ? nullptr : ResolveBoundAsset(Quest, Node, Campaign);
        switch (Node.Condition)
        {
        case EDAFirstHourNodeCondition::FounderHallPowered:
            return Bound != nullptr && IsPowered(Context, Bound->WorldAssetId);
        case EDAFirstHourNodeCondition::FounderHallOnline:
        case EDAFirstHourNodeCondition::BoundAssetOperational:
        case EDAFirstHourNodeCondition::HabitatConstructionComplete:
            return Bound != nullptr && Bound->ConstructionState == EDAConstructionState::Operational;
        case EDAFirstHourNodeCondition::AdaptiveHabitatPlaced:
            return Bound != nullptr && Bound->ConstructionState != EDAConstructionState::Preview;
        case EDAFirstHourNodeCondition::NiaPresent:
            return HasLiveNia(Context);
        case EDAFirstHourNodeCondition::HabitatPowerFullySupplied:
            return Bound != nullptr && IsFullySupplied(Context, Bound->WorldAssetId,
                EDACampaignUtilityKind::Power, EDAUtilityType::Power);
        case EDAFirstHourNodeCondition::HabitatWaterFullySupplied:
            return Bound != nullptr && IsFullySupplied(Context, Bound->WorldAssetId,
                EDACampaignUtilityKind::Water, EDAUtilityType::Water);
        case EDAFirstHourNodeCondition::TowerHalfStaffed:
        {
            if (Bound == nullptr) return false;
            if (Context.LiveSignals != nullptr)
            {
                const FDACampaignJobOpeningSignal* Opening = Context.LiveSignals->JobOpenings.FindByPredicate([Bound](const auto& Job)
                    { return Job.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                        && Job.CityId == TEXT("player_capital") && Job.FacilityWorldAssetId == Bound->WorldAssetId; });
                if (Opening == nullptr || Opening->OpenPositions <= 0) return false;
                const int32 Assigned = Context.LiveSignals->JobAssignments.FilterByPredicate([Opening](const auto& Assignment)
                    { return Assignment.JobId == Opening->JobId
                        && Assignment.FacilityWorldAssetId == Opening->FacilityWorldAssetId; }).Num();
                return Assigned * 2 >= Opening->OpenPositions;
            }
            if (Context.CityState == nullptr) return false;
            const FDAJobOpening* Opening = Context.CityState->JobOpenings.FindByPredicate([Bound](const FDAJobOpening& Job)
            { return Job.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                && Job.CityId == TEXT("player_capital") && Job.FacilityWorldAssetId == Bound->WorldAssetId; });
            if (Opening == nullptr || Opening->OpenPositions <= 0) return false;
            const int32 Assigned = Context.CityState->JobAssignments.FilterByPredicate([Opening](const FDAJobAssignment& Assignment)
            { return Assignment.JobId == Opening->JobId && Assignment.FacilityWorldAssetId == Opening->FacilityWorldAssetId; }).Num();
            return Assigned * 2 >= Opening->OpenPositions;
        }
        case EDAFirstHourNodeCondition::NiaAssignedToTower:
            if (Context.LiveSignals != nullptr)
            {
                const FDACampaignCitizenSignal* Nia = Context.LiveSignals->FindCitizen(TEXT("citizen.synara.nia_vale"));
                return Bound != nullptr && Nia != nullptr && Nia->CityId == TEXT("player_capital")
                    && Nia->JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                    && Context.LiveSignals->JobOpenings.ContainsByPredicate([Bound](const auto& Job)
                        { return Job.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                            && Job.CityId == TEXT("player_capital") && Job.FacilityWorldAssetId == Bound->WorldAssetId; })
                    && Context.LiveSignals->JobAssignments.ContainsByPredicate([Bound](const auto& Assignment)
                        { return Assignment.CitizenId == TEXT("citizen.synara.nia_vale")
                            && Assignment.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                            && Assignment.FacilityWorldAssetId == Bound->WorldAssetId; });
            }
            return Context.CityState != nullptr && Bound != nullptr
                && Context.CityState->Citizens.ContainsByPredicate([](const FDACitizenRecord& Citizen)
                { return Citizen.CitizenId == TEXT("citizen.synara.nia_vale")
                    && Citizen.CityId == TEXT("player_capital")
                    && Citizen.JobId == TEXT("job.synara.cognitive_operations_tower.operator"); })
                && Context.CityState->JobOpenings.ContainsByPredicate([Bound](const FDAJobOpening& Job)
                { return Job.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                    && Job.CityId == TEXT("player_capital") && Job.FacilityWorldAssetId == Bound->WorldAssetId; })
                && Context.CityState->JobAssignments.ContainsByPredicate([Bound](const FDAJobAssignment& Assignment)
                { return Assignment.CitizenId == TEXT("citizen.synara.nia_vale")
                    && Assignment.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                    && Assignment.FacilityWorldAssetId == Bound->WorldAssetId; });
        case EDAFirstHourNodeCondition::NewCampaign:
        case EDAFirstHourNodeCondition::PrerequisitesMet:
        case EDAFirstHourNodeCondition::Resolved:
            return true;
        case EDAFirstHourNodeCondition::HabitatOccupied:
        case EDAFirstHourNodeCondition::AutonomousExchangeAvailable:
        case EDAFirstHourNodeCondition::DependencyAbove25:
        case EDAFirstHourNodeCondition::WorldMapUnlocked:
        case EDAFirstHourNodeCondition::ForgeweaveContact:
            return StartConditionSatisfied(Quest, Context, Campaign);
        case EDAFirstHourNodeCondition::ExplicitChoice:
            return !Context.ChoiceBranchTag.IsNone();
        default:
            return RequiredAction != EDAFirstHourPlayerAction::None;
        }
    }

    bool BindObjectiveAsset(const FDAFirstHourQuestEntry& Quest, const FDAFirstHourNode& Node,
        const int64 BindWorldTick, FDACampaignSnapshot& Campaign)
    {
        if (Node.BindingId.IsNone() || FindDynamicBinding(Campaign, Quest.Definition.QuestId, Node.BindingId) != nullptr) return true;
        const FDAFirstHourWorldAssetRequirement* Requirement = Quest.FindBinding(Node.BindingId);
        if (Requirement == nullptr || Requirement->BindWhen != EDAFirstHourBindWhen::Objective) return true;
        const FDAWorldAssetRecord* Asset = FindAsset(Campaign, *Requirement); if (Asset == nullptr) return false;
        FDAQuestObjectiveAssetBindingRecord Record;
        Record.QuestId = Quest.Definition.QuestId; Record.BindingId = Requirement->BindingId;
        Record.DefinitionId = Requirement->DefinitionId; Record.WorldAssetId = Asset->WorldAssetId;
        Record.BindWorldTick = BindWorldTick;
        Record.QuestDefinitionFingerprint = Quest.Definition.BuildManifest().DefinitionFingerprint;
        Campaign.NarrativeState.QuestObjectiveAssetBindings.Add(Record);
        Campaign.NarrativeState.QuestObjectiveAssetBindings.Sort([](const auto& A, const auto& B)
        { return A.QuestId != B.QuestId ? A.QuestId.LexicalLess(B.QuestId) : A.BindingId.LexicalLess(B.BindingId); });
        ++Campaign.NarrativeState.MutationRevision; return true;
    }

    EDAQuestContentUnlockType ToUnlockType(const EDAFirstHourRewardType Type)
    { return static_cast<EDAQuestContentUnlockType>(static_cast<uint8>(Type)); }

    FGuid DeterministicRewardGuid(const FName ActionId, const int32 Index)
    {
        const FString Key = ActionId.ToString() + TEXT("|") + FString::FromInt(Index);
        return FGuid(FCrc::StrCrc32(*Key), FCrc::StrCrc32(*(Key + TEXT("|b"))), FCrc::StrCrc32(*(Key + TEXT("|c"))), FCrc::StrCrc32(*(Key + TEXT("|d"))));
    }

    FGuid DeterministicNarrativeGuid(const FName ActionId)
    {
        const FString Key = ActionId.ToString();
        return FGuid(FCrc::StrCrc32(*Key), FCrc::StrCrc32(*(Key + TEXT("|story"))),
            FCrc::StrCrc32(*(Key + TEXT("|nia"))), FCrc::StrCrc32(*(Key + TEXT("|action"))));
    }

    bool RewardEqual(const FDAQuestContentUnlockRecord& Existing, const FDAFirstHourReward& Reward,
        const FName QuestId, const FString& Fingerprint, const int64 Tick, const FDACampaignSnapshot& Campaign)
    {
        if (Existing.QuestId != QuestId || Existing.Type != ToUnlockType(Reward.Type))
        {
            return false;
        }
        if (Existing.DefinitionId != Reward.DefinitionId || Existing.ContentId != Reward.ContentId
            || Existing.Quantity != Reward.Quantity || Existing.SourceFingerprint != Fingerprint || Existing.WorldTick != Tick)
        {
            return false;
        }
        if (Reward.Type != EDAFirstHourRewardType::CardInstance)
        {
            return Existing.GrantedCardInstanceIds.IsEmpty();
        }
        if (Existing.GrantedCardInstanceIds.Num() != Reward.Quantity) return false;
        for (int32 Index = 0; Index < Reward.Quantity; ++Index)
        {
            const FGuid Id = DeterministicRewardGuid(Reward.ActionId, Index);
            const FCardInstance* Instance = Campaign.CollectionState.FindInstance(Id);
            if (Existing.GrantedCardInstanceIds[Index] != Id || Instance == nullptr
                || Instance->DefinitionId != Reward.DefinitionId || Instance->AcquisitionSource != EDAAcquisitionSource::QuestReward
                || Instance->AcquisitionWorldTick != Existing.WorldTick) return false;
        }
        return true;
    }

    bool ApplyReward(const FDAFirstHourReward& Reward, const FName QuestId, const FString& Fingerprint,
        const int64 Tick, FDACampaignSnapshot& Campaign)
    {
        if (const FDAQuestContentUnlockRecord* Existing = Campaign.NarrativeState.QuestContentUnlockRecords.FindByPredicate(
            [&Reward](const FDAQuestContentUnlockRecord& R) { return R.ActionId == Reward.ActionId; }))
            return RewardEqual(*Existing, Reward, QuestId, Fingerprint, Tick, Campaign);
        FDAQuestContentUnlockRecord Record; Record.ActionId = Reward.ActionId; Record.QuestId = QuestId;
        Record.Type = ToUnlockType(Reward.Type); Record.DefinitionId = Reward.DefinitionId; Record.ContentId = Reward.ContentId;
        Record.Quantity = Reward.Quantity; Record.SourceFingerprint = Fingerprint; Record.WorldTick = Tick;
        if (Reward.Type == EDAFirstHourRewardType::CardInstance)
            for (int32 Index = 0; Index < Reward.Quantity; ++Index)
            {
                const FGuid Id = DeterministicRewardGuid(Reward.ActionId, Index);
                if (!Campaign.CollectionState.Instances.Contains(Id)
                    && !Campaign.CollectionState.AddInstanceWithId(Id, Reward.DefinitionId, EDAAcquisitionSource::QuestReward, Tick)) return false;
                Record.GrantedCardInstanceIds.Add(Id);
            }
        Campaign.NarrativeState.QuestContentUnlockRecords.Add(MoveTemp(Record));
        if (QuestId == TEXT("quest.signal_in_foundation")
            && Reward.Type == EDAFirstHourRewardType::AxiomArchiveFragment
            && Reward.ContentId == TEXT("archive.axiom.fragment.01"))
        {
            FDAWorldMapAuthorityRecord Typed; Typed.SourceQuestId = QuestId;
            Typed.SourceActionId = Reward.ActionId; Typed.WorldTick = Tick;
            Campaign.NarrativeState.WorldMapAuthorityRecords.Add(Typed);
            ++Campaign.NarrativeState.MutationRevision;
        }
        Campaign.NarrativeState.QuestContentUnlockRecords.Sort([](const auto& A, const auto& B) { return A.ActionId.LexicalLess(B.ActionId); });
        ++Campaign.NarrativeState.MutationRevision; return true;
    }

    FName SelectedBranch(const FDAFirstHourQuestEntry& Quest, const FDAQuestSaveState& State, const FName Explicit)
    {
        if (!Explicit.IsNone()) return Explicit;
        const FDAFirstHourNode* Choice = Quest.Nodes.FindByPredicate([](const FDAFirstHourNode& Node) { return Node.RuntimeNode.Type == EDAQuestNodeType::Choice; });
        if (Choice == nullptr) return NAME_None;
        for (const FDAQuestEdgeDefinition& Edge : Choice->RuntimeNode.Edges)
            if (State.CompletedNodeIds.Contains(Edge.TargetNodeId) || State.CurrentNodeId == Edge.TargetNodeId) return Edge.BranchTag;
        return NAME_None;
    }

    bool OutcomeEqual(const FDAQuestContentEffectRecord& Record, const FDAFirstHourOutcome& Outcome,
        const FName QuestId, const FName Branch, const FString& Fingerprint, const int64 Tick)
    {
        return Record.ActionId == Outcome.ActionId && Record.QuestId == QuestId && Record.ChoiceBranchTag == Branch
            && Record.SourceFingerprint == Fingerprint && Record.SemanticEffects == Outcome.SemanticEffects
            && Record.HistoryTags == Outcome.HistoryTags && Record.CitizenStoryState == Outcome.StoryState
            && Record.bHasCitizenRelationshipDelta == Outcome.bHasNiaTrustDelta && Record.CitizenRelationshipDelta == Outcome.NiaTrustDelta
            && Record.bHasHumanAgencySupportDelta == Outcome.bHasHumanAgencySupportDelta && Record.HumanAgencySupportDelta == Outcome.HumanAgencySupportDelta
            && Record.bHasDependencyDelta == Outcome.bHasDependencyDelta && Record.DependencyDelta == Outcome.DependencyDelta
            && Record.WorldTick == Tick
            && FMath::Clamp(Record.BaselineDependency + Outcome.DependencyDelta, 0.0, 100.0) == Record.ResultDependency
            && FMath::Clamp(Record.BaselineHumanAgencySupport + Outcome.HumanAgencySupportDelta, 0.0, 100.0) == Record.ResultHumanAgencySupport
            && FMath::Clamp(Record.BaselineNiaTrust + Outcome.NiaTrustDelta, -100.0, 100.0) == Record.ResultNiaTrust;
    }

    bool ApplyCompletion(const FDAFirstHourQuestManifest& Manifest, const FDAFirstHourQuestEntry& Quest,
        const FName Branch, const FDAFirstHourProgressionContext& Context, FDACampaignSnapshot& Campaign)
    {
        for (const FName Tag : Quest.CompletionHistoryTags) Campaign.HistoryTags.AddUnique(Tag);
        for (const FDAFirstHourReward& Reward : Quest.Rewards)
            if (!ApplyReward(Reward, Quest.Definition.QuestId, Manifest.Fingerprint, Context.WorldTick, Campaign)) return false;
        if (Quest.Definition.QuestId == TEXT("quest.nia_needs_a_job"))
        {
            const FDAQuestObjectiveAssetBindingRecord* Tower = FindDynamicBinding(Campaign, Quest.Definition.QuestId,
                TEXT("cognitive_operations_tower"));
            const bool bLiveAssignment = Context.LiveSignals != nullptr
                && Context.LiveSignals->JobAssignments.ContainsByPredicate([Tower](const FDACampaignJobAssignmentSignal& Assignment)
                    { return Tower != nullptr && Assignment.CitizenId == NiaCitizenId
                        && Assignment.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                        && Assignment.FacilityWorldAssetId == Tower->WorldAssetId; });
            const bool bLegacyAssignment = Context.CityState != nullptr
                && Context.CityState->JobAssignments.ContainsByPredicate([Tower](const FDAJobAssignment& Assignment)
                    { return Tower != nullptr && Assignment.CitizenId == NiaCitizenId
                        && Assignment.JobId == TEXT("job.synara.cognitive_operations_tower.operator")
                        && Assignment.FacilityWorldAssetId == Tower->WorldAssetId; });
            if (Tower == nullptr || (!bLiveAssignment && !bLegacyAssignment)) return false;
            FDASynaraCitizenEmployment Employment; Employment.CitizenId = NiaCitizenId; Employment.CityId = TEXT("player_capital");
            Employment.JobId = TEXT("job.synara.cognitive_operations_tower.operator"); Employment.FacilityWorldAssetId = Tower->WorldAssetId;
            Campaign.SynaraState.CitizenEmployment.RemoveAll([](const FDASynaraCitizenEmployment& Existing)
                { return Existing.CitizenId == NiaCitizenId; });
            Campaign.SynaraState.CitizenEmployment.Add(Employment);
            bool bChangedLiveSignals = false;
            FDACampaignCitizenSignal* LiveNia = Campaign.LiveSignals.Citizens.FindByPredicate(
                [](const FDACampaignCitizenSignal& Citizen) { return Citizen.CitizenId == NiaCitizenId; });
            if (LiveNia == nullptr)
            {
                LiveNia = &Campaign.LiveSignals.Citizens.Emplace_GetRef();
                LiveNia->CitizenId = NiaCitizenId; LiveNia->CityId = TEXT("player_capital");
                bChangedLiveSignals = true;
            }
            if (LiveNia->JobId != Employment.JobId) { LiveNia->JobId = Employment.JobId; bChangedLiveSignals = true; }
            FDACampaignJobOpeningSignal* LiveOpening = Campaign.LiveSignals.JobOpenings.FindByPredicate(
                [&Employment](const FDACampaignJobOpeningSignal& Opening)
                { return Opening.JobId == Employment.JobId
                    && Opening.FacilityWorldAssetId == Employment.FacilityWorldAssetId; });
            if (LiveOpening == nullptr)
            {
                LiveOpening = &Campaign.LiveSignals.JobOpenings.Emplace_GetRef();
                LiveOpening->JobId = Employment.JobId; LiveOpening->CityId = Employment.CityId;
                LiveOpening->FacilityWorldAssetId = Employment.FacilityWorldAssetId;
                LiveOpening->OpenPositions = 1; bChangedLiveSignals = true;
            }
            if (!Campaign.LiveSignals.JobAssignments.ContainsByPredicate([&Employment](const FDACampaignJobAssignmentSignal& Assignment)
                { return Assignment.CitizenId == Employment.CitizenId && Assignment.JobId == Employment.JobId
                    && Assignment.FacilityWorldAssetId == Employment.FacilityWorldAssetId; }))
            {
                FDACampaignJobAssignmentSignal& LiveAssignment = Campaign.LiveSignals.JobAssignments.Emplace_GetRef();
                LiveAssignment.CitizenId = Employment.CitizenId; LiveAssignment.JobId = Employment.JobId;
                LiveAssignment.FacilityWorldAssetId = Employment.FacilityWorldAssetId; bChangedLiveSignals = true;
            }
            if (bChangedLiveSignals)
            {
                if (Campaign.LiveSignals.MutationRevision == MAX_int64) return false;
                ++Campaign.LiveSignals.MutationRevision;
            }
        }
        if (!Branch.IsNone())
        {
            const FDAFirstHourOutcome* Outcome = Quest.Outcomes.Find(Branch); if (Outcome == nullptr) return false;
            if (!IsOutcomeEligible(*Outcome, Context, Campaign)) return false;
            if (const FDAQuestContentEffectRecord* Existing = Campaign.NarrativeState.QuestContentEffectRecords.FindByPredicate(
                [&Quest](const FDAQuestContentEffectRecord& R) { return R.QuestId == Quest.Definition.QuestId; }))
            {
                if (Branch == TEXT("audit"))
                {
                    const FDAQuestEligibilityProofRecord* Proof = Campaign.NarrativeState.QuestEligibilityProofRecords.FindByPredicate(
                        [&Quest](const FDAQuestEligibilityProofRecord& Candidate)
                        { return Candidate.QuestId == Quest.Definition.QuestId && Candidate.BranchTag == TEXT("audit"); });
                    if (Proof == nullptr) return false;
                    const bool bVisionReplay = Proof->EligibilityId == TEXT("Vision")
                        && Proof->SourceActionId == Context.VisionActionId
                        && Proof->SourceActionTag == TEXT("capability.vision.available");
                    const bool bResearchReplay = Proof->EligibilityId == TEXT("ResearchAction")
                        && Proof->SourceActionId == Context.ResearchActionId
                        && Proof->SourceActionTag == TEXT("action.research.replacement_model.completed");
                    if ((!bVisionReplay && !bResearchReplay) || Proof->WorldTick != Context.WorldTick) return false;
                }
                return OutcomeEqual(*Existing, *Outcome, Quest.Definition.QuestId, Branch, Manifest.Fingerprint,
                    Context.WorldTick);
            }
            FDASynaraCampaignState& Authority = Campaign.SynaraState;
            const double BaselineDependency = Authority.Dependency;
            const double BaselineHumanAgencySupport = Authority.FactionSupport.FindRef(HumanAgencyFactionId);
            const double BaselineNiaTrust = Authority.CitizenRelationships.FindRef(NiaCitizenId);
            if (Outcome->bHasDependencyDelta && !Authority.ApplyDependencyReason(Outcome->ActionId, Outcome->DependencyDelta, Context.WorldTick)) return false;
            if (Outcome->bHasNiaTrustDelta && !Authority.ApplyCitizenRelationshipReason(Outcome->ActionId, NiaCitizenId, Outcome->NiaTrustDelta, Context.WorldTick)) return false;
            if (Outcome->bHasHumanAgencySupportDelta && !Authority.ApplyFactionSupportReason(Outcome->ActionId, HumanAgencyFactionId, Outcome->HumanAgencySupportDelta, Context.WorldTick)) return false;
            if (Quest.Definition.QuestId == TEXT("quest.replacement_model"))
            {
                const EDASynaraCapitalEfficiencyState Result = Branch == TEXT("accept") ? EDASynaraCapitalEfficiencyState::Increased
                    : Branch == TEXT("modify") ? EDASynaraCapitalEfficiencyState::SmallerIncrease : EDASynaraCapitalEfficiencyState::Baseline;
                if (!Authority.ApplyPolicyReason(Outcome->ActionId, TEXT("CapitalEfficiency"), static_cast<int32>(Result), Context.WorldTick)) return false;
            }
            else if (Quest.Definition.QuestId == TEXT("quest.agency_has_a_price"))
            {
                const EDAAgencyPetitionResolution Result = Branch == TEXT("agency_forum") ? EDAAgencyPetitionResolution::AgencyForum
                    : Branch == TEXT("retrain") ? EDAAgencyPetitionResolution::RetrainWorkers
                    : Branch == TEXT("automation_cap") ? EDAAgencyPetitionResolution::AutomationCap : EDAAgencyPetitionResolution::Rejected;
                if (!Authority.ApplyPolicyReason(Outcome->ActionId, TEXT("AgencyPetition"), static_cast<int32>(Result), Context.WorldTick)) return false;
            }
            else if (Quest.Definition.QuestId == TEXT("quest.iron_at_border"))
            {
                if (!ApplyIronDiplomacy(*Outcome, Context.WorldTick, Campaign)) return false;
                const EDAIronBorderResolution Border = Branch == TEXT("accept") ? EDAIronBorderResolution::Accepted
                    : Branch == TEXT("refuse") ? EDAIronBorderResolution::Refused
                    : Branch == TEXT("favorable_terms") ? EDAIronBorderResolution::FavorableTerms : EDAIronBorderResolution::Deferred;
                const EDADiplomaticTrend Trust = Branch == TEXT("accept") ? EDADiplomaticTrend::Increased
                    : Branch == TEXT("refuse") ? EDADiplomaticTrend::Decreased : EDADiplomaticTrend::Unchanged;
                const EDADiplomaticTrend Respect = Branch == TEXT("favorable_terms") ? EDADiplomaticTrend::Increased : EDADiplomaticTrend::Unchanged;
                const EDADiplomaticTrend Dependence = Branch == TEXT("accept") || Branch == TEXT("favorable_terms")
                    ? EDADiplomaticTrend::Increased : EDADiplomaticTrend::Unchanged;
                if (!Authority.ApplyPolicyReason(Outcome->ActionId, TEXT("IronBorder"), static_cast<int32>(Border), Context.WorldTick)
                    || !Authority.ApplyPolicyReason(Outcome->ActionId, TEXT("ForgeweaveTrust"), static_cast<int32>(Trust), Context.WorldTick)
                    || !Authority.ApplyPolicyReason(Outcome->ActionId, TEXT("ForgeweaveRespect"), static_cast<int32>(Respect), Context.WorldTick)
                    || !Authority.ApplyPolicyReason(Outcome->ActionId, TEXT("ForgeweaveDependence"), static_cast<int32>(Dependence), Context.WorldTick)) return false;
            }
            if (!Outcome->StoryState.IsNone())
            {
                const FGuid ActionGuid = DeterministicNarrativeGuid(Outcome->ActionId);
                FDANarrativeActionRecord Action; Action.ActionId = ActionGuid; Action.NormalizedActionTags = {Outcome->ActionId}; Action.WorldTick = Context.WorldTick;
                Campaign.NarrativeState.ActionRecords.Add(Action);
                Campaign.NarrativeState.ActionRecords.Sort([](const FDANarrativeActionRecord& A, const FDANarrativeActionRecord& B)
                    { return A.ActionId.ToString(EGuidFormats::Digits) < B.ActionId.ToString(EGuidFormats::Digits); });
                FDACitizenStoryTransitionRecord Transition; Transition.CitizenId = NiaCitizenId;
                Transition.BaselineStoryState = Campaign.NarrativeState.CitizenStoryStates.FindRef(NiaCitizenId);
                Transition.ResultStoryState = Outcome->StoryState; Transition.SourceActionId = ActionGuid;
                Transition.SourceActionTag = Outcome->ActionId; Transition.WorldTick = Context.WorldTick;
                Campaign.NarrativeState.CitizenStoryTransitionRecords.Add(Transition);
                Campaign.NarrativeState.CitizenStoryStates.Add(NiaCitizenId, Outcome->StoryState);
                ++Campaign.NarrativeState.MutationRevision;
            }
            for (const FName Tag : Outcome->HistoryTags) Campaign.HistoryTags.AddUnique(Tag);
            for (const FDAFirstHourReward& Reward : Outcome->Rewards)
                if (!ApplyReward(Reward, Quest.Definition.QuestId, Manifest.Fingerprint, Context.WorldTick, Campaign)) return false;
            if (!Outcome->EligibilityAny.IsEmpty())
            {
                FDAQuestEligibilityProofRecord Proof; Proof.QuestId = Quest.Definition.QuestId; Proof.BranchTag = Branch;
                const FDAAuditEligibilitySourceRecord* SourceProof = nullptr;
                if (Outcome->EligibilityAny.Contains(EDAFirstHourEligibility::Vision)
                    && (SourceProof = FindEligibilitySource(Campaign, Context.VisionActionId, TEXT("Vision"),
                        TEXT("capability.vision.available"), Context.WorldTick)) != nullptr)
                { Proof.EligibilityId = TEXT("Vision"); Proof.SourceActionId = Context.VisionActionId; }
                else
                {
                    SourceProof = FindEligibilitySource(Campaign, Context.ResearchActionId, TEXT("ResearchAction"),
                        TEXT("action.research.replacement_model.completed"), Context.WorldTick);
                    if (SourceProof == nullptr) return false;
                    Proof.EligibilityId = TEXT("ResearchAction"); Proof.SourceActionId = Context.ResearchActionId;
                }
                Proof.SourceActionTag = SourceProof->SourceActionTag;
                Proof.SourceWorldTick = SourceProof->WorldTick;
                Proof.WorldTick = Context.WorldTick; Campaign.NarrativeState.QuestEligibilityProofRecords.Add(Proof);
                ++Campaign.NarrativeState.MutationRevision;
            }
            FDAQuestContentEffectRecord Record; Record.ActionId = Outcome->ActionId; Record.QuestId = Quest.Definition.QuestId;
            Record.ChoiceBranchTag = Branch; Record.SourceFingerprint = Manifest.Fingerprint; Record.SemanticEffects = Outcome->SemanticEffects;
            Record.bHasCitizenRelationshipDelta = Outcome->bHasNiaTrustDelta; Record.CitizenRelationshipDelta = Outcome->NiaTrustDelta;
            Record.bHasHumanAgencySupportDelta = Outcome->bHasHumanAgencySupportDelta; Record.HumanAgencySupportDelta = Outcome->HumanAgencySupportDelta;
            Record.bHasDependencyDelta = Outcome->bHasDependencyDelta; Record.DependencyDelta = Outcome->DependencyDelta;
            Record.BaselineDependency = BaselineDependency; Record.ResultDependency = Authority.Dependency;
            Record.BaselineHumanAgencySupport = BaselineHumanAgencySupport; Record.ResultHumanAgencySupport = Authority.FactionSupport.FindRef(HumanAgencyFactionId);
            Record.BaselineNiaTrust = BaselineNiaTrust; Record.ResultNiaTrust = Authority.CitizenRelationships.FindRef(NiaCitizenId);
            Record.CitizenStoryState = Outcome->StoryState; Record.HistoryTags = Outcome->HistoryTags; Record.WorldTick = Context.WorldTick;
            Campaign.NarrativeState.QuestContentEffectRecords.Add(MoveTemp(Record));
            Campaign.NarrativeState.QuestContentEffectRecords.Sort([](const auto& A, const auto& B) { return A.ActionId.LexicalLess(B.ActionId); });
            ++Campaign.NarrativeState.MutationRevision;
        }
        Campaign.HistoryTags.Sort([](const FName A, const FName B) { return A.LexicalLess(B); });
        return true;
    }

    bool NextIsResolution(const FDAFirstHourNode& Node, const FName Branch)
    {
        const FDAQuestEdgeDefinition* Edge = Node.RuntimeNode.Edges.FindByPredicate([Branch](const FDAQuestEdgeDefinition& E) { return Branch.IsNone() || E.BranchTag == Branch; });
        return Edge != nullptr && Edge->TargetNodeId.ToString().EndsWith(TEXT("resolution"));
    }

    bool CompletedTransactionMatches(const FDAFirstHourQuestManifest& Manifest,
        const FDAFirstHourQuestEntry& Quest, const FDAFirstHourProgressionContext& Context,
        const FDACampaignSnapshot& Campaign)
    {
        const FDAQuestSaveState* State = Campaign.NarrativeState.FindQuestState(Quest.Definition.QuestId);
        if (State == nullptr || State->ProgressState != EDAQuestProgressState::Completed) return false;
        FDACampaignSnapshot Candidate = Campaign;
        const FName Branch = Quest.Outcomes.IsEmpty() ? NAME_None : SelectedBranch(Quest, *State, NAME_None);
        if (!Context.ChoiceBranchTag.IsNone() && !Branch.IsNone() && Context.ChoiceBranchTag != Branch) return false;
        const int64 Revision = Candidate.NarrativeState.MutationRevision;
        const int32 Unlocks = Candidate.NarrativeState.QuestContentUnlockRecords.Num();
        const int32 Effects = Candidate.NarrativeState.QuestContentEffectRecords.Num();
        const int32 Cards = Candidate.CollectionState.Instances.Num();
        const TArray<FName> History = Candidate.HistoryTags;
        if (!ApplyCompletion(Manifest, Quest, Branch, Context, Candidate)) return false;
        return Candidate.NarrativeState.MutationRevision == Revision
            && Candidate.NarrativeState.QuestContentUnlockRecords.Num() == Unlocks
            && Candidate.NarrativeState.QuestContentEffectRecords.Num() == Effects
            && Candidate.CollectionState.Instances.Num() == Cards && Candidate.HistoryTags == History
            && Candidate.NarrativeState.CitizenStoryStates.OrderIndependentCompareEqual(Campaign.NarrativeState.CitizenStoryStates)
            && Candidate.SynaraState.Dependency == Campaign.SynaraState.Dependency
            && Candidate.SynaraState.FactionSupport.OrderIndependentCompareEqual(Campaign.SynaraState.FactionSupport)
            && Candidate.SynaraState.CitizenRelationships.OrderIndependentCompareEqual(Campaign.SynaraState.CitizenRelationships)
            && Candidate.SynaraState.DependencyReasons.Num() == Campaign.SynaraState.DependencyReasons.Num()
            && Candidate.SynaraState.FactionSupportReasons.Num() == Campaign.SynaraState.FactionSupportReasons.Num()
            && Candidate.SynaraState.CitizenRelationshipReasons.Num() == Campaign.SynaraState.CitizenRelationshipReasons.Num()
            && Candidate.SynaraState.PolicyReasons.Num() == Campaign.SynaraState.PolicyReasons.Num()
            && Candidate.SynaraState.CitizenEmployment.Num() == Campaign.SynaraState.CitizenEmployment.Num()
            && Candidate.WorldState.Diplomacy.Relationships.Num() == Campaign.WorldState.Diplomacy.Relationships.Num();
    }
}

EDAFirstHourCampaignResult FDAFirstHourCampaignRuntime::TryStartQuest(const FDAFirstHourQuestManifest& Manifest,
    const FName QuestId, const FDAFirstHourProgressionContext& Context, FDACampaignSnapshot& Campaign)
{
    TArray<FText> Errors; if (!FDAFirstHourQuestPipeline::Validate(Manifest, Errors) || Context.WorldTick < 0) return EDAFirstHourCampaignResult::InvalidManifest;
    FString CampaignError; if (!Campaign.Validate(CampaignError)) return EDAFirstHourCampaignResult::InvalidState;
    const FDAFirstHourQuestEntry* Quest = Manifest.FindQuest(QuestId); if (Quest == nullptr) return EDAFirstHourCampaignResult::InvalidManifest;
    if (Campaign.NarrativeState.FindQuestState(QuestId) != nullptr) return EDAFirstHourCampaignResult::AlreadyApplied;
    for (const FName Prerequisite : Quest->PrerequisiteQuestIds) if (!IsCompleted(Campaign, Prerequisite)) return EDAFirstHourCampaignResult::PrerequisiteMissing;
    if (!StartConditionSatisfied(*Quest, Context, Campaign)) return EDAFirstHourCampaignResult::ConditionUnsatisfied;
    FDACampaignSnapshot Candidate = Campaign;
    TArray<FDAQuestWorldAssetBinding> Bindings;
    for (const FDAFirstHourWorldAssetRequirement& Required : Quest->WorldAssetBindings)
        if (Required.BindWhen == EDAFirstHourBindWhen::Start
            && Quest->Definition.RequiredWorldAssetBindingIds.Contains(Required.BindingId))
        {
            const FDAWorldAssetRecord* Asset = FindAsset(Candidate, Required);
            if (Asset == nullptr) return EDAFirstHourCampaignResult::ConditionUnsatisfied;
            FDAQuestWorldAssetBinding& Binding = Bindings.Emplace_GetRef(); Binding.BindingId = Required.BindingId; Binding.WorldAssetId = Asset->WorldAssetId;
            Binding.BindWorldTick = Context.WorldTick;
            Binding.QuestDefinitionFingerprint = Quest->Definition.BuildManifest().DefinitionFingerprint;
        }
    const EDAQuestRuntimeResult Result = FDAQuestRuntime::StartQuest(Quest->Definition, Bindings, Context.WorldTick, Candidate);
    if (Result != EDAQuestRuntimeResult::Applied) return Result == EDAQuestRuntimeResult::AlreadyApplied ? EDAFirstHourCampaignResult::AlreadyApplied : EDAFirstHourCampaignResult::InvalidState;
    Campaign = MoveTemp(Candidate); return EDAFirstHourCampaignResult::Applied;
}

EDAFirstHourCampaignResult FDAFirstHourCampaignRuntime::AdvanceQuest(const FDAFirstHourQuestManifest& Manifest,
    const FName QuestId, const FDAFirstHourProgressionContext& Context, FDACampaignSnapshot& Campaign)
{
    TArray<FText> Errors; if (!FDAFirstHourQuestPipeline::Validate(Manifest, Errors) || Context.WorldTick < 0) return EDAFirstHourCampaignResult::InvalidManifest;
    FString CampaignError; if (!Campaign.Validate(CampaignError)) return EDAFirstHourCampaignResult::InvalidState;
    const FDAFirstHourQuestEntry* Quest = Manifest.FindQuest(QuestId); const FDAQuestSaveState* ExistingState = Campaign.NarrativeState.FindQuestState(QuestId);
    if (Quest == nullptr || ExistingState == nullptr) return EDAFirstHourCampaignResult::InvalidState;
    if (ExistingState->ProgressState == EDAQuestProgressState::Completed)
        return CompletedTransactionMatches(Manifest, *Quest, Context, Campaign)
            ? EDAFirstHourCampaignResult::AlreadyApplied : EDAFirstHourCampaignResult::ConflictingReplay;
    const FDAFirstHourNode* Node = Quest->FindNode(ExistingState->CurrentNodeId); if (Node == nullptr) return EDAFirstHourCampaignResult::InvalidState;
    if (Node->RuntimeNode.Type == EDAQuestNodeType::Choice && Context.ChoiceBranchTag.IsNone()) return EDAFirstHourCampaignResult::ChoiceRequired;
    if (Node->RuntimeNode.Type == EDAQuestNodeType::Choice
        && !Node->RuntimeNode.Edges.ContainsByPredicate([&Context](const FDAQuestEdgeDefinition& Edge) { return Edge.BranchTag == Context.ChoiceBranchTag; }))
        return EDAFirstHourCampaignResult::InvalidState;
    if (Node->RuntimeNode.Type == EDAQuestNodeType::Choice && Quest->Outcomes.Contains(Context.ChoiceBranchTag))
    {
        const FDAFirstHourOutcome& Outcome = Quest->Outcomes[Context.ChoiceBranchTag];
        if (!IsOutcomeEligible(Outcome, Context, Campaign)) return EDAFirstHourCampaignResult::ChoiceIneligible;
    }
    FDACampaignSnapshot Candidate = Campaign;
    Candidate.NarrativeState.bFirstHourTransactionInProgress = true;
    if (!BindObjectiveAsset(*Quest, *Node, Context.WorldTick, Candidate) || !NodeConditionSatisfied(*Quest, *Node, Context, Candidate)) return EDAFirstHourCampaignResult::ConditionUnsatisfied;
    const FDAQuestSaveState* CandidateState = Candidate.NarrativeState.FindQuestState(QuestId);
    const FName Branch = Quest->Outcomes.IsEmpty() ? NAME_None
        : SelectedBranch(*Quest, *CandidateState, Node->RuntimeNode.Type == EDAQuestNodeType::Choice ? Context.ChoiceBranchTag : NAME_None);
    const bool bCompleting = NextIsResolution(*Node, Node->RuntimeNode.Type == EDAQuestNodeType::Choice ? Context.ChoiceBranchTag : NAME_None);
    Candidate.NarrativeState.bFirstHourTransactionInProgress = true;
    FDAQuestEvaluationContext RuntimeContext; RuntimeContext.WorldTick = Context.WorldTick;
    if (Node->RuntimeNode.Payload.Variant != EDAQuestPayloadVariant::None)
        RuntimeContext.SetMetric(Node->RuntimeNode.Payload.Variant, Node->RuntimeNode.Payload.Condition.EvaluationKey, 1.0);
    const EDAQuestRuntimeResult Result = Node->RuntimeNode.Type == EDAQuestNodeType::Choice
        ? FDAQuestRuntime::SelectChoice(Quest->Definition, Context.ChoiceBranchTag, RuntimeContext, Candidate)
        : [&]() { FName Ignored; return FDAQuestRuntime::EvaluateCurrentNode(Quest->Definition, RuntimeContext, Candidate, Ignored); }();
    if (Result != EDAQuestRuntimeResult::Applied) return EDAFirstHourCampaignResult::InvalidState;
    if (bCompleting && !ApplyCompletion(Manifest, *Quest, Branch, Context, Candidate)) return EDAFirstHourCampaignResult::ConflictingReplay;
    Candidate.NarrativeState.bFirstHourTransactionInProgress = false;
    FString Error; if (!Candidate.Validate(Error)) return EDAFirstHourCampaignResult::InvalidState;
    Campaign = MoveTemp(Candidate); return EDAFirstHourCampaignResult::Applied;
}

EDAFirstHourCampaignResult FDAFirstHourCampaignRuntime::RecordCrisisCompletion(const FDAFirstHourQuestManifest& Manifest,
    const FName CrisisQuestId, const FName CompletionActionId, const FGuid NarrativeActionId, const int64 WorldTick,
    FDACampaignSnapshot& Campaign)
{
    const FDAChampionEligibilityDefinition& E = Manifest.Citizen.ChampionEligibility;
    FString CampaignError; if (!Campaign.Validate(CampaignError)) return EDAFirstHourCampaignResult::InvalidState;
    if (WorldTick < 0 || CrisisQuestId != E.RequiredCrisisQuestId || CompletionActionId != E.RequiredCrisisCompletionActionId
        || E.RequiredCrisisSourceDefinitionId != TEXT("quest.human_override.v1")) return EDAFirstHourCampaignResult::InvalidManifest;
    if (const FDAQuestCrisisCompletionRecord* Existing = Campaign.NarrativeState.QuestCrisisCompletionRecords.FindByPredicate([CrisisQuestId](const auto& R) { return R.QuestId == CrisisQuestId; }))
        return Existing->CompletionActionId == CompletionActionId && Existing->NarrativeActionId == NarrativeActionId
            && Existing->WorldTick == WorldTick ? EDAFirstHourCampaignResult::AlreadyApplied : EDAFirstHourCampaignResult::ConflictingReplay;
    const FDAQuestSaveState* Quest = Campaign.NarrativeState.FindQuestState(CrisisQuestId);
    const FDANarrativeActionRecord* Action = Campaign.NarrativeState.FindActionRecord(NarrativeActionId);
    if (Quest == nullptr || Quest->ProgressState != EDAQuestProgressState::Completed
        || Quest->DefinitionManifest.SourceDefinitionId != E.RequiredCrisisSourceDefinitionId
        || Quest->DefinitionManifest.DefinitionFingerprint != E.RequiredCrisisDefinitionFingerprint
        || Quest->DefinitionManifest.ComputeFingerprint() != Quest->DefinitionManifest.DefinitionFingerprint
        || Action == nullptr || Action->bLegacyIdentityOnly
        || Action->NormalizedActionTags != TArray<FName>{CompletionActionId}
        || Action->WorldTick <= Quest->LastTransitionWorldTick || WorldTick <= Action->WorldTick
        || !Campaign.NarrativeState.CitizenStoryTransitionRecords.ContainsByPredicate([&E, NarrativeActionId, Action](const FDACitizenStoryTransitionRecord& Story)
            { return Story.CitizenId == TEXT("citizen.synara.nia_vale") && Story.ResultStoryState == E.RequiredStoryState
                && Story.SourceActionId == NarrativeActionId && Story.SourceActionTag == E.RequiredCrisisCompletionActionId
                && Story.WorldTick == Action->WorldTick; }))
        return EDAFirstHourCampaignResult::ConditionUnsatisfied;
    FDACampaignSnapshot Candidate = Campaign; FDAQuestCrisisCompletionRecord Record; Record.QuestId = CrisisQuestId;
    Record.CompletionActionId = CompletionActionId; Record.NarrativeActionId = NarrativeActionId;
    Record.QuestDefinitionFingerprint = Quest->DefinitionManifest.DefinitionFingerprint; Record.WorldTick = WorldTick;
    Candidate.NarrativeState.QuestCrisisCompletionRecords.Add(Record); ++Candidate.NarrativeState.MutationRevision;
    FString Error; if (!Candidate.Validate(Error)) return EDAFirstHourCampaignResult::InvalidState;
    Campaign = MoveTemp(Candidate); return EDAFirstHourCampaignResult::Applied;
}

EDAFirstHourCampaignResult FDAFirstHourCampaignRuntime::CommitNiaStoryTransition(const FDAFirstHourQuestManifest& Manifest,
    const FGuid SourceActionId, const FName ResultStoryState, const int64 WorldTick, FDACampaignSnapshot& Campaign)
{
    const FDAChampionEligibilityDefinition& E = Manifest.Citizen.ChampionEligibility;
    FString Error; if (!Campaign.Validate(Error)) return EDAFirstHourCampaignResult::InvalidState;
    if (ResultStoryState != E.RequiredStoryState || WorldTick < 0) return EDAFirstHourCampaignResult::InvalidManifest;
    if (const FDACitizenStoryTransitionRecord* ExistingTransition = Campaign.NarrativeState.CitizenStoryTransitionRecords.FindByPredicate(
        [SourceActionId](const FDACitizenStoryTransitionRecord& Record) { return Record.SourceActionId == SourceActionId; }))
        return ExistingTransition->CitizenId == NiaCitizenId
            && ExistingTransition->ResultStoryState == ResultStoryState
            && ExistingTransition->SourceActionTag == E.RequiredCrisisCompletionActionId
            && ExistingTransition->WorldTick == WorldTick
            ? EDAFirstHourCampaignResult::AlreadyApplied : EDAFirstHourCampaignResult::ConflictingReplay;
    const FDAQuestSaveState* Quest = Campaign.NarrativeState.FindQuestState(E.RequiredCrisisQuestId);
    const FDANarrativeActionRecord* Action = Campaign.NarrativeState.FindActionRecord(SourceActionId);
    const FDACitizenStoryTransitionRecord* PreviousStory = Campaign.NarrativeState.CitizenStoryTransitionRecords.IsEmpty()
        ? nullptr : &Campaign.NarrativeState.CitizenStoryTransitionRecords.Last();
    if (Quest == nullptr || Quest->ProgressState != EDAQuestProgressState::Completed
        || Quest->DefinitionManifest.SourceDefinitionId != E.RequiredCrisisSourceDefinitionId
        || Quest->DefinitionManifest.DefinitionFingerprint != E.RequiredCrisisDefinitionFingerprint
        || Action == nullptr || Action->bLegacyIdentityOnly || Action->WorldTick != WorldTick
        || Action->WorldTick <= Quest->LastTransitionWorldTick
        || (PreviousStory != nullptr && WorldTick <= PreviousStory->WorldTick)
        || Action->NormalizedActionTags != TArray<FName>{E.RequiredCrisisCompletionActionId})
        return EDAFirstHourCampaignResult::ConditionUnsatisfied;
    FDACampaignSnapshot Candidate = Campaign;
    FDACitizenStoryTransitionRecord Record; Record.CitizenId = TEXT("citizen.synara.nia_vale");
    Record.BaselineStoryState = Candidate.NarrativeState.CitizenStoryStates.FindRef(Record.CitizenId);
    Record.ResultStoryState = ResultStoryState; Record.SourceActionId = SourceActionId;
    Record.SourceActionTag = E.RequiredCrisisCompletionActionId; Record.WorldTick = WorldTick;
    Candidate.NarrativeState.CitizenStoryTransitionRecords.Add(Record);
    Candidate.NarrativeState.CitizenStoryStates.Add(Record.CitizenId, ResultStoryState);
    ++Candidate.NarrativeState.MutationRevision;
    if (!Candidate.Validate(Error)) return EDAFirstHourCampaignResult::InvalidState;
    Campaign = MoveTemp(Candidate); return EDAFirstHourCampaignResult::Applied;
}

EDAFirstHourCampaignResult FDAFirstHourCampaignRuntime::RecordAuditEligibilitySource(const FName EligibilityId,
    const FGuid SourceActionId, const FName SourceActionTag, const int64 WorldTick, FDACampaignSnapshot& Campaign)
{
    const FName ExpectedTag = EligibilityId == TEXT("Vision") ? FName(TEXT("capability.vision.available"))
        : EligibilityId == TEXT("ResearchAction") ? FName(TEXT("action.research.replacement_model.completed")) : NAME_None;
    FString Error;
    if (!Campaign.Validate(Error)) return EDAFirstHourCampaignResult::InvalidState;
    if (!SourceActionId.IsValid() || WorldTick < 0 || ExpectedTag.IsNone() || SourceActionTag != ExpectedTag)
        return EDAFirstHourCampaignResult::InvalidManifest;
    if (const FDAAuditEligibilitySourceRecord* Existing = Campaign.NarrativeState.AuditEligibilitySourceRecords.FindByPredicate(
        [SourceActionId](const FDAAuditEligibilitySourceRecord& Record) { return Record.SourceActionId == SourceActionId; }))
        return Existing->EligibilityId == EligibilityId && Existing->SourceActionTag == SourceActionTag
            && Existing->WorldTick == WorldTick ? EDAFirstHourCampaignResult::AlreadyApplied
            : EDAFirstHourCampaignResult::ConflictingReplay;
    const FDANarrativeActionRecord* Action = Campaign.NarrativeState.FindActionRecord(SourceActionId);
    if (Action == nullptr || Action->bLegacyIdentityOnly || Action->WorldTick != WorldTick
        || Action->NormalizedActionTags != TArray<FName>{SourceActionTag})
        return EDAFirstHourCampaignResult::ConditionUnsatisfied;
    FDACampaignSnapshot Candidate = Campaign;
    FDAAuditEligibilitySourceRecord SourceProof;
    SourceProof.EligibilityId = EligibilityId;
    SourceProof.SourceActionId = SourceActionId;
    SourceProof.SourceActionTag = SourceActionTag;
    SourceProof.WorldTick = WorldTick;
    Candidate.NarrativeState.AuditEligibilitySourceRecords.Add(SourceProof);
    ++Candidate.NarrativeState.MutationRevision;
    if (!Candidate.Validate(Error)) return EDAFirstHourCampaignResult::InvalidState;
    Campaign = MoveTemp(Candidate);
    return EDAFirstHourCampaignResult::Applied;
}

bool FDAFirstHourCampaignRuntime::IsNiaChampionEligible(const FDAFirstHourQuestManifest& Manifest, const FDACampaignSnapshot& Campaign)
{
    FString CampaignError; if (!Campaign.Validate(CampaignError)) return false;
    const FDAChampionEligibilityDefinition& E = Manifest.Citizen.ChampionEligibility;
    const FName* Story = Campaign.NarrativeState.CitizenStoryStates.Find(Manifest.Citizen.CitizenId);
    return Story != nullptr && *Story == E.RequiredStoryState
        && Campaign.NarrativeState.QuestCrisisCompletionRecords.ContainsByPredicate([&E](const FDAQuestCrisisCompletionRecord& Record)
        {
            return Record.QuestId == E.RequiredCrisisQuestId
                && Record.CompletionActionId == E.RequiredCrisisCompletionActionId
                && Campaign.NarrativeState.FindQuestState(Record.QuestId) != nullptr
                && Campaign.NarrativeState.FindQuestState(Record.QuestId)->DefinitionManifest.SourceDefinitionId == E.RequiredCrisisSourceDefinitionId;
        });
}
