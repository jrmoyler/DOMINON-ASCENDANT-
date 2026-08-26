#include "ViewModels/DAUIViewModels.h"

#include "AbilitySystemComponent.h"
#include "Camera/DACameraModeController.h"
#include "Cards/DADeckRules.h"
#include "Combat/DACombatAttributeSet.h"
#include "Command/DACommandSubsystem.h"
#include "Commands/DAUICommandEndpoint.h"
#include "City/DACityGridSubsystem.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Content/DACardDefinition.h"
#include "Economy/DAEconomySubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Save/DASaveService.h"
#include "Manifest/DAUIGeneratedMetadata.h"

namespace
{
    FName CurrentObjective(const FDACampaignSnapshot& Campaign)
    {
        const FDAQuestSaveState* Selected = nullptr;
        for (const FDAQuestSaveState& Quest : Campaign.NarrativeState.QuestStates)
        {
            if (Quest.ProgressState != EDAQuestProgressState::Active) continue;
            if (Selected == nullptr || Quest.StartedWorldTick < Selected->StartedWorldTick
                || (Quest.StartedWorldTick == Selected->StartedWorldTick && Quest.QuestId.LexicalLess(Selected->QuestId)))
                Selected = &Quest;
        }
        return Selected == nullptr ? NAME_None
            : FName(*(Selected->QuestId.ToString() + TEXT(".") + Selected->CurrentNodeId.ToString()));
    }

    FString JoinExact(const FString& Label, const TArray<FString>& Values)
    {
        return Label + TEXT(": ") + (Values.IsEmpty() ? TEXT("none") : FString::Join(Values, TEXT("; ")));
    }

    FString RenderChannel(const FName Channel, const FDACampaignSnapshot& Campaign,
        const FDAUIScreenProjection& Projection)
    {
        const FString Id = Channel.ToString();
        if (Id == TEXT("wallets.capital")) return FString::Printf(TEXT("Capital %.2f"), Projection.CityHUD.Wallets.Capital);
        if (Id == TEXT("wallets.insight")) return FString::Printf(TEXT("Insight %.2f"), Projection.CityHUD.Wallets.Insight);
        if (Id == TEXT("wallets.influence")) return FString::Printf(TEXT("Influence %.2f"), Projection.CityHUD.Wallets.Influence);
        if (Id == TEXT("population")) return FString::Printf(TEXT("Population %d"), Projection.CityHUD.Population);
        if (Id == TEXT("founder.health")) return FString::Printf(TEXT("Health %.1f"), Projection.FounderHUD.Founder.Health);
        if (Id == TEXT("founder.guard")) return FString::Printf(TEXT("Guard %.1f"), Projection.FounderHUD.Founder.Guard);
        if (Id == TEXT("founder.stamina")) return FString::Printf(TEXT("Stamina %.1f"), Projection.FounderHUD.Founder.Stamina);
        if (Id == TEXT("founder.abilities"))
        {
            TArray<FString> Values;
            for (const FName AbilityId : Projection.FounderHUD.Founder.AbilityIds) Values.Add(AbilityId.ToString());
            return JoinExact(TEXT("Abilities"), Values);
        }
        if (Id == TEXT("founder.interaction")) return FString::Printf(TEXT("Interaction %s"), *Projection.FounderHUD.Founder.InteractionId.ToString());
        if (Id == TEXT("founder.status")) return FString::Printf(TEXT("Founder %s"), *Projection.CommandHUD.Command.FounderStatus.ToString());
        if (Id == TEXT("command.selected_squads"))
        {
            TArray<FString> Values;
            for (const FDACommandSquadViewModel& Squad : Projection.CommandHUD.Command.Squads)
                if (Squad.bSelected) Values.Add(FString::Printf(TEXT("%s morale=%.2f supply=%.2f"),
                    *Squad.SquadId.ToString(), Squad.Morale, Squad.Supply));
            return JoinExact(TEXT("Selected squads"), Values);
        }
        if (Id == TEXT("command.control_zones"))
        {
            TArray<FString> Values;
            for (const FName ZoneId : Projection.CommandHUD.Command.ControlZoneIds) Values.Add(ZoneId.ToString());
            return JoinExact(TEXT("Control zones"), Values);
        }
        if (Id == TEXT("command.supply")) return FString::Printf(TEXT("Supply %.1f"), Projection.CommandHUD.Command.Supply);
        if (Id == TEXT("command.enemy_known_positions"))
        {
            TArray<FString> Values;
            for (const FName EnemyId : Projection.CommandHUD.Command.KnownEnemyIds) Values.Add(EnemyId.ToString());
            return JoinExact(TEXT("Known enemies"), Values);
        }
        if (Id == TEXT("command.command_points")) return FString::Printf(TEXT("Command points %d"), Projection.CommandHUD.Command.CommandPoints);
        if (Id == TEXT("command.sovereignty")) return FString::Printf(TEXT("Sovereignty %.2f"), Projection.CommandHUD.Command.Sovereignty);
        if (Id == TEXT("command.tactical_alert")) return FString::Printf(TEXT("Alert %s"), *Projection.CommandHUD.Command.TacticalAlertId.ToString());
        if (Id == TEXT("quest.objective")) return FString::Printf(TEXT("Objective %s"),
            *(Projection.CityHUD.ObjectiveId.IsNone() ? CurrentObjective(Campaign) : Projection.CityHUD.ObjectiveId).ToString());
        if (Id.Contains(TEXT("dependency")))
        {
            TArray<FString> Values;
            for (const FDASynaraValueReason& Reason : Campaign.SynaraState.DependencyReasons)
                Values.Add(FString::Printf(TEXT("%s baseline=%.2f delta=%.2f result=%.2f tick=%lld"),
                    *Reason.ActionId.ToString(), Reason.Baseline, Reason.Delta, Reason.Result,
                    static_cast<long long>(Reason.WorldTick)));
            return FString::Printf(TEXT("Dependency %.2f | %s"), Campaign.SynaraState.Dependency,
                *JoinExact(TEXT("reasons"), Values));
        }
        if (Id == TEXT("deck.rule_results"))
            return JoinExact(TEXT("Deck rule results"), Projection.Deck.RuleResults);
        if (Id.StartsWith(TEXT("deck.")))
        {
            TArray<FString> Values;
            for (const FGuid InstanceId : Campaign.DeckState.GetInstanceIds()) Values.Add(TEXT("deck=") + InstanceId.ToString());
            for (const FGuid InstanceId : Campaign.DeckState.GetHand()) Values.Add(TEXT("hand=") + InstanceId.ToString());
            for (const FGuid InstanceId : Campaign.DeckState.GetReserveQueue()) Values.Add(TEXT("reserve=") + InstanceId.ToString());
            return JoinExact(TEXT("Deck authority"), Values);
        }
        if (Id == TEXT("collection.selected_instance"))
            return JoinExact(TEXT("Selected card"), Projection.CardInspect.SimulationTrace);
        if (Id.StartsWith(TEXT("collection.")) || Id.StartsWith(TEXT("tooltip.")) || Id.Contains(TEXT("rule")))
        {
            TArray<FString> Values;
            for (const FCardInstance& Instance : Projection.Collection.Instances)
                Values.Add(FString::Printf(TEXT("%s definition=%s source=%d recovery=%d mastery=%d worldAsset=%s"),
                    *Instance.InstanceId.ToString(), *Instance.DefinitionId.ToString(),
                    static_cast<int32>(Instance.AcquisitionSource), static_cast<int32>(Instance.RecoveryState),
                    Instance.MasteryXp, *Instance.WorldAssetId.ToString()));
            return JoinExact(Id.Contains(TEXT("rule")) ? TEXT("Deck rule inputs") : TEXT("Collection instances"), Values);
        }
        if (Id.StartsWith(TEXT("citizen.")) || Id.StartsWith(TEXT("jobs.")))
        {
            TArray<FString> Values;
            for (const FDACampaignCitizenSignal& Citizen : Campaign.LiveSignals.Citizens)
                Values.Add(FString::Printf(TEXT("citizen=%s city=%s job=%s home=%s"), *Citizen.CitizenId.ToString(),
                    *Citizen.CityId.ToString(), *Citizen.JobId.ToString(), *Citizen.HomeWorldAssetId.ToString()));
            for (const FDACampaignJobOpeningSignal& Opening : Campaign.LiveSignals.JobOpenings)
                Values.Add(FString::Printf(TEXT("opening=%s facility=%s positions=%d"), *Opening.JobId.ToString(),
                    *Opening.FacilityWorldAssetId.ToString(), Opening.OpenPositions));
            for (const FDACampaignJobAssignmentSignal& Assignment : Campaign.LiveSignals.JobAssignments)
                Values.Add(FString::Printf(TEXT("assignment citizen=%s job=%s facility=%s"),
                    *Assignment.CitizenId.ToString(), *Assignment.JobId.ToString(),
                    *Assignment.FacilityWorldAssetId.ToString()));
            return JoinExact(TEXT("Citizen/job authority"), Values);
        }
        if (Id.StartsWith(TEXT("crafting.")))
        {
            TArray<FString> Values;
            for (const FDACraftingBlueprintViewModel& Blueprint : Projection.Crafting.Blueprints)
                Values.Add(FString::Printf(TEXT("blueprint=%s sourceQuest=%s quantity=%d granted=%s requirements=%s"),
                    *Blueprint.BlueprintId.ToString(), *Blueprint.SourceQuestId.ToString(), Blueprint.Quantity,
                    *FString::JoinBy(Blueprint.GrantedInstanceIds, TEXT(","), [](const FGuid IdValue) { return IdValue.ToString(); }),
                    *FString::Join(Blueprint.RequirementReasons, TEXT(","))));
            Values.Add(FString::Printf(TEXT("wallet capital=%.2f insight=%.2f influence=%.2f"),
                Projection.Crafting.Wallets.Capital, Projection.Crafting.Wallets.Insight,
                Projection.Crafting.Wallets.Influence));
            return JoinExact(TEXT("Crafting authority"), Values);
        }
        if (Id.StartsWith(TEXT("utility.")))
        {
            TArray<FString> Values;
            for (const FDACampaignUtilitySignal& Signal : Campaign.LiveSignals.UtilitySignals)
                Values.Add(FString::Printf(TEXT("asset=%s utility=%d supply=%d"), *Signal.WorldAssetId.ToString(),
                    static_cast<int32>(Signal.Utility), static_cast<int32>(Signal.Supply)));
            return JoinExact(TEXT("Utility authority"), Values);
        }
        if (Id.StartsWith(TEXT("world_asset.")) || Id.StartsWith(TEXT("economy.")))
        {
            TArray<FString> Values;
            for (const FDAWorldAssetRecord& Asset : Campaign.WorldAssets)
                Values.Add(FString::Printf(TEXT("%s definition=%s city=%s grid=(%d,%d) rotation=%d construction=%d integrity=%.2f"),
                    *Asset.WorldAssetId.ToString(), *Asset.CardDefinitionId.ToString(), *Asset.CityId.ToString(),
                    Asset.GridOrigin.X, Asset.GridOrigin.Y, Asset.Rotation,
                    static_cast<int32>(Asset.ConstructionState), Asset.StructuralIntegrity));
            for (const FDAFacilityTooltipViewModel& Output : Projection.CityMetrics.FacilityOutputs)
                Values.Add(FString::Printf(TEXT("output asset=%s gross=(%.2f,%.2f,%.2f) net=(%.2f,%.2f,%.2f) maintenance=%.2f"),
                    *Output.WorldAssetId.ToString(), Output.GrossOutput.Capital, Output.GrossOutput.Insight,
                    Output.GrossOutput.Influence, Output.NetOutput.Capital, Output.NetOutput.Insight,
                    Output.NetOutput.Influence, Output.MaintenanceCapital));
            return JoinExact(TEXT("World/economy authority"), Values);
        }
        if (Id.StartsWith(TEXT("factions.")))
        {
            TArray<FString> Values;
            for (const FDAFactionSupportViewModel& Faction : Projection.Factions.Factions)
                for (const FDASynaraValueReason& Reason : Faction.Reasons)
                    Values.Add(FString::Printf(TEXT("%s support=%.2f reason=%s delta=%.2f result=%.2f"),
                        *Faction.FactionId.ToString(), Faction.Support, *Reason.ActionId.ToString(), Reason.Delta, Reason.Result));
            if (Values.IsEmpty())
                for (const FDAFactionSupportViewModel& Faction : Projection.Factions.Factions)
                    Values.Add(FString::Printf(TEXT("%s support=%.2f"), *Faction.FactionId.ToString(), Faction.Support));
            return JoinExact(TEXT("Faction support ledger"), Values);
        }
        if (Id.StartsWith(TEXT("diplomacy.")) || Id.StartsWith(TEXT("treaty.")))
        {
            TArray<FString> Values;
            for (const FDADiplomacyRelationshipViewModel& Relationship : Projection.Diplomacy.Relationships)
            {
                Values.Add(FString::Printf(TEXT("%s trust=%.2f respect=%.2f fear=%.2f dependence=%.2f grievance=%.2f compatibility=%.2f"),
                    *Relationship.RelationshipId.ToString(), Relationship.Trust, Relationship.Respect,
                    Relationship.Fear, Relationship.Dependence, Relationship.Grievance, Relationship.Compatibility));
                for (const FDADiplomaticReason& Reason : Relationship.Reasons)
                    Values.Add(FString::Printf(TEXT("reason=%s source=%s metric=%d magnitude=%.2f tick=%lld"),
                        *Reason.MutationId.ToString(), *Reason.SourceTag.ToString(), static_cast<int32>(Reason.Metric),
                        Reason.Magnitude, static_cast<long long>(Reason.WorldTick)));
            }
            return JoinExact(TEXT("Diplomacy ledger"), Values);
        }
        if (Id.StartsWith(TEXT("world.")) || Id.StartsWith(TEXT("world_map.")))
        {
            TArray<FString> Values;
            for (const FDARegionState& Region : Projection.WorldMap.Regions)
                Values.Add(FString::Printf(TEXT("region=%s owner=%s settlements=%s revision=%lld"),
                    *Region.RegionId.ToString(), *Region.OwnerId.ToString(),
                    *FString::JoinBy(Region.SettlementIds, TEXT(","), [](const FName Name) { return Name.ToString(); }),
                    static_cast<long long>(Region.PersistentDelta.Revision)));
            for (const FDAWorldMapAuthorityRecord& Reason : Projection.WorldMap.AuthorityReasons)
                Values.Add(FString::Printf(TEXT("authority quest=%s action=%s tick=%lld"),
                    *Reason.SourceQuestId.ToString(), *Reason.SourceActionId.ToString(), static_cast<long long>(Reason.WorldTick)));
            return JoinExact(TEXT("World map authority"), Values);
        }
        if (Id.StartsWith(TEXT("quest.")) || Id.StartsWith(TEXT("narrative.")))
        {
            TArray<FString> Values;
            for (const FDAQuestSaveState& Quest : Projection.QuestJournal.Quests)
                Values.Add(FString::Printf(TEXT("quest=%s node=%s state=%d entered=%lld transitioned=%lld"),
                    *Quest.QuestId.ToString(), *Quest.CurrentNodeId.ToString(), static_cast<int32>(Quest.ProgressState),
                    static_cast<long long>(Quest.CurrentNodeEnteredWorldTick), static_cast<long long>(Quest.LastTransitionWorldTick)));
            for (const FDAQuestObjectiveAssetBindingRecord& Binding : Projection.QuestJournal.ObjectiveBindings)
                Values.Add(FString::Printf(TEXT("binding=%s quest=%s asset=%s definition=%s"),
                    *Binding.BindingId.ToString(), *Binding.QuestId.ToString(), *Binding.WorldAssetId.ToString(),
                    *Binding.DefinitionId.ToString()));
            return JoinExact(TEXT("Quest authority"), Values);
        }
        if (Id.StartsWith(TEXT("history.")))
        {
            TArray<FString> Values;
            for (const FName Tag : Projection.HistoryTimeline.HistoryTags) Values.Add(TEXT("tag=") + Tag.ToString());
            for (const FDANarrativeActionRecord& Action : Projection.HistoryTimeline.Actions)
                Values.Add(FString::Printf(TEXT("action=%s tick=%lld fulfilled=%d breached=%d"),
                    *Action.ActionId.ToString(), static_cast<long long>(Action.WorldTick),
                    Action.FulfilledPromiseIds.Num(), Action.BreachedPromiseIds.Num()));
            for (const FDAPromiseRecord& Promise : Projection.HistoryTimeline.PromiseReasons)
                Values.Add(FString::Printf(TEXT("promise=%s definition=%s state=%d resolution=%s"),
                    *Promise.PromiseId.ToString(), *Promise.PromiseDefinitionId.ToString(),
                    static_cast<int32>(Promise.State), *Promise.ResolutionActionTag.ToString()));
            return JoinExact(TEXT("History ledger"), Values);
        }
        if (Id.StartsWith(TEXT("save.")) || Id == TEXT("campaign.save_status"))
        {
            TArray<FString> Values;
            for (const FDASaveSlotViewModel& Slot : Projection.SaveLoad.Slots)
                Values.Add(FString::Printf(TEXT("slot=%s exists=%s loadable=%s validation=%s"), *Slot.SlotId,
                    Slot.bExists ? TEXT("true") : TEXT("false"), Slot.bLoadable ? TEXT("true") : TEXT("false"),
                    *Slot.ValidationReason));
            Values.Add(FString::Printf(TEXT("worldTick=%lld signalRevision=%lld"),
                static_cast<long long>(Projection.SaveLoad.CurrentWorldTick),
                static_cast<long long>(Projection.SaveLoad.SignalRevision)));
            return JoinExact(TEXT("Save slots"), Values);
        }
        if (Id == TEXT("settings.current") || Id == TEXT("accessibility.persisted_settings")) return FString::Printf(
            TEXT("textScale=%.2f subtitles=%s speakerLabels=%s subtitleBackground=%.2f colorMarkers=%s colorPreset=%d cameraShake=%.2f FOV=%.2f motionBlur=%s reducedMotion=%s reducedFlash=%s tacticalPause=%s aimAssist=%.2f buildSnap=%.2f tutorialRecall=%s tooltipMode=%d keyboardRemaps=%d controllerRemaps=%d holdToggleModes=%d"),
            Projection.Accessibility.TextScale,
            Projection.Accessibility.bSubtitles ? TEXT("on") : TEXT("off"),
            Projection.Accessibility.bSpeakerLabels ? TEXT("on") : TEXT("off"),
            Projection.Accessibility.SubtitleBackgroundOpacity,
            Projection.Accessibility.bColorIndependentMarkers ? TEXT("on") : TEXT("off"),
            static_cast<int32>(Projection.Accessibility.ColorVisionPreset), Projection.Accessibility.CameraShake,
            Projection.Accessibility.FieldOfView, Projection.Accessibility.bMotionBlur ? TEXT("on") : TEXT("off"),
            Projection.Accessibility.bReducedMotion ? TEXT("on") : TEXT("off"),
            Projection.Accessibility.bReducedFlash ? TEXT("on") : TEXT("off"),
            Projection.Accessibility.bTacticalPause ? TEXT("on") : TEXT("off"), Projection.Accessibility.AimAssist,
            Projection.Accessibility.BuildSnapStrength, Projection.Accessibility.bTutorialRecall ? TEXT("on") : TEXT("off"),
            static_cast<int32>(Projection.Accessibility.TooltipMode), Projection.Accessibility.KeyboardBindings.Num(),
            Projection.Accessibility.ControllerBindings.Num(), Projection.Accessibility.HoldToggleModes.Num());
        if (Id.StartsWith(TEXT("campaign."))) return FString::Printf(TEXT("Campaign region=%s tick=%lld preset=%s"),
            *Projection.MainMenu.CurrentRegionId.ToString(), static_cast<long long>(Campaign.WorldState.CurrentWorldTick),
            *Projection.NewCampaign.SelectedPresetId.ToString());
        if (Id.StartsWith(TEXT("research.")))
        {
            TArray<FString> Values;
            for (const FName ResearchId : Projection.LateFeatureAuthority.ResearchIds)
                Values.Add(TEXT("research=") + ResearchId.ToString());
            for (const FDAQuestContentUnlockRecord& Unlock : Projection.Research.AuthoredUnlocks)
                Values.Add(FString::Printf(TEXT("unlock action=%s definition=%s content=%s quantity=%d tick=%lld"),
                    *Unlock.ActionId.ToString(), *Unlock.DefinitionId.ToString(), *Unlock.ContentId.ToString(),
                    Unlock.Quantity, static_cast<long long>(Unlock.WorldTick)));
            for (const FDAAuditEligibilitySourceRecord& Reason : Projection.Research.EligibilityReasons)
                Values.Add(FString::Printf(TEXT("eligibility=%s action=%s source=%s tick=%lld"),
                    *Reason.EligibilityId.ToString(), *Reason.SourceActionId.ToString(),
                    *Reason.SourceActionTag.ToString(), static_cast<long long>(Reason.WorldTick)));
            return JoinExact(TEXT("Research authority"), Values);
        }
        if (Id.StartsWith(TEXT("conquest.")))
        {
            TArray<FString> Values;
            if (Id.Contains(TEXT("leader")) || Id.Contains(TEXT("resolution")))
                for (const FDAUILeaderResolutionAuthorityRecord& Record : Projection.LeaderResolution.Records)
                    Values.Add(FString::Printf(TEXT("resolution=%s leader=%s status=%s reasons=%s"),
                        *Record.ResolutionId.ToString(), *Record.LeaderId.ToString(), *Record.Status.ToString(),
                        *FString::JoinBy(Record.Reasons, TEXT(","),
                            [](const FName Name) { return Name.ToString(); })));
            for (const FDAConquestRouteMeterViewModel& Meter : Projection.Conquest.RouteMeters)
                Values.Add(FString::Printf(TEXT("route=%s progress=%.3f capitalReward=%.2f insightReward=%.2f reasons=%s"),
                    *Meter.RouteAssetId.ToString(), Meter.Progress, Meter.CapitalReward, Meter.InsightReward,
                    *FString::JoinBy(Meter.Reasons, TEXT(","), [](const FName Name) { return Name.ToString(); })));
            return JoinExact(TEXT("Conquest routes"), Values);
        }
        if (Id.StartsWith(TEXT("city.metrics"))) return FString::Printf(
            TEXT("Capital=%.2f Insight=%.2f Influence=%.2f Population=%d Dependency=%.2f"),
            Projection.CityMetrics.Wallets.Capital, Projection.CityMetrics.Wallets.Insight,
            Projection.CityMetrics.Wallets.Influence, Projection.CityMetrics.Population,
            Projection.CityMetrics.Dependency);
        if (Id.StartsWith(TEXT("ascension.")))
        {
            TArray<FString> Values;
            for (const FDAUIAscensionRewardAuthorityRecord& Reward :
                Projection.AscensionReward.AuthorityRewards)
                Values.Add(FString::Printf(TEXT("reward=%s status=%s reasons=%s"),
                    *Reward.RewardId.ToString(), *Reward.Status.ToString(),
                    *FString::JoinBy(Reward.Reasons, TEXT(","),
                        [](const FName Name) { return Name.ToString(); })));
            return JoinExact(TEXT("Ascension proof"), Values);
        }
        return FString();
    }

    FString GuidText(const FGuid& Id)
    {
        return Id.IsValid() ? Id.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("none");
    }

    FName PlayModeName(const EDAPlayMode Mode)
    {
        switch (Mode)
        {
        case EDAPlayMode::Founder: return TEXT("founder");
        case EDAPlayMode::City: return TEXT("city");
        case EDAPlayMode::Command: return TEXT("command");
        default: return TEXT("unknown");
        }
    }

    FString SerializeKeyBindings(const TMap<FName, FKey>& Bindings)
    {
        TArray<FName> ActionIds;
        Bindings.GetKeys(ActionIds);
        ActionIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        TArray<FString> Values;
        for (const FName ActionId : ActionIds)
            Values.Add(FString::Printf(TEXT("\"%s\":\"%s\""), *ActionId.ToString(),
                *Bindings.FindChecked(ActionId).GetFName().ToString()));
        return TEXT("{") + FString::Join(Values, TEXT(",")) + TEXT("}");
    }

    FString SerializeHoldToggle(const TMap<FName, EDAHoldToggleMode>& Modes)
    {
        TArray<FName> ActionIds;
        Modes.GetKeys(ActionIds);
        ActionIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        TArray<FString> Values;
        for (const FName ActionId : ActionIds)
            Values.Add(FString::Printf(TEXT("\"%s\":%d"), *ActionId.ToString(),
                static_cast<int32>(Modes.FindChecked(ActionId))));
        return TEXT("{") + FString::Join(Values, TEXT(",")) + TEXT("}");
    }

    FString MetricName(const EDADiplomaticMetric Metric)
    {
        switch (Metric)
        {
        case EDADiplomaticMetric::Trust: return TEXT("trust");
        case EDADiplomaticMetric::Respect: return TEXT("respect");
        case EDADiplomaticMetric::Fear: return TEXT("fear");
        case EDADiplomaticMetric::Dependence: return TEXT("dependence");
        case EDADiplomaticMetric::Grievance: return TEXT("grievance");
        case EDADiplomaticMetric::Compatibility: return TEXT("compatibility");
        default: return FString();
        }
    }

    bool FindPlacementProposal(const FDACampaignSnapshot& Campaign,
        UDAContentRegistrySubsystem* Registry, const FGuid CardInstanceId,
        FIntPoint& OutOrigin, FIntPoint& OutFootprint)
    {
        const FCardInstance* Card = Campaign.CollectionState.FindInstance(CardInstanceId);
        UDA_CardDefinition* Definition = Card == nullptr || Registry == nullptr
            ? nullptr : Registry->GetCardDefinition(Card->DefinitionId);
        if (Definition == nullptr || !Definition->bPlaceable
            || Definition->Footprint.X <= 0 || Definition->Footprint.Y <= 0) return false;
        const FDACityGridClaimState* CityGridClaims = Campaign.FindCityGridClaims(TEXT("player_capital"));
        if (CityGridClaims == nullptr) return false;
        FDACityGridSubsystem Grid(CityGridClaims->Width, CityGridClaims->Height);
        FString ReconstructionError;
        if (!UDAWorldStateSubsystem::ReconstructClaimedCityGrid(
            Campaign, TEXT("player_capital"), *Registry, Grid, ReconstructionError)) return false;
        for (int32 Y = 0; Y < FDACityGridSubsystem::DefaultGridHeight; ++Y)
            for (int32 X = 0; X < FDACityGridSubsystem::DefaultGridWidth; ++X)
            {
                FDACardPlacementRequest Request;
                Request.AssetId = FDAWorldAssetId(FGuid::NewGuid());
                Request.Origin = FIntPoint(X, Y);
                Request.Footprint = Definition->Footprint;
                if (Grid.ValidatePlacement(Request).bCanPlace)
                { OutOrigin = Request.Origin; OutFootprint = Definition->Footprint; return true; }
            }
        return false;
    }

    FString BuildActionPayload(const FName ActionId, const FDAUIScreenDescriptor& Descriptor,
        const FDACampaignSnapshot& Campaign, const FDAUIScreenProjection& Projection,
        const TMap<FName, FName>& StableSelections, const int32 PayloadIndex,
        UDAContentRegistrySubsystem* Registry, const bool bUseStableSelection = false)
    {
        FString Field = TEXT("targetId");
        FString Target = StableSelections.FindRef(Descriptor.Id).ToString();
        if (Target.IsEmpty() || Target == TEXT("None")) Target.Reset();
        const FString Action = ActionId.ToString();
        if (!bUseStableSelection && (Action == TEXT("city.select_card")
            || Action == TEXT("founder.activate_ability") || Action == TEXT("command.select_squad")))
            Target.Reset();
        if (Action == TEXT("city.place_building"))
        {
            if (Target.IsEmpty() && !Projection.CityHUD.Hand.IsEmpty()) Target = GuidText(Projection.CityHUD.Hand[0]);
            FGuid CardId; FGuid::Parse(Target, CardId);
            FIntPoint Origin; FIntPoint Footprint;
            if (!FindPlacementProposal(Campaign, Registry, CardId, Origin, Footprint))
                return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\"}"),
                    *Descriptor.Id.ToString(), *Action);
            return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"cardInstanceId\":\"%s\",\"cityId\":\"player_capital\",\"gridX\":%d,\"gridY\":%d,\"footprintX\":%d,\"footprintY\":%d,\"rotation\":0}"),
                *Descriptor.Id.ToString(), *Action, *Target, Origin.X, Origin.Y, Footprint.X, Footprint.Y);
        }
        if (Action == TEXT("city.cancel_build"))
        {
            if (Target.IsEmpty())
                if (const FDAWorldAssetRecord* Preview = Campaign.WorldAssets.FindByPredicate(
                    [](const FDAWorldAssetRecord& Asset) { return Asset.ConstructionState != EDAConstructionState::Operational; }))
                    Target = GuidText(Preview->WorldAssetId);
            return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"worldAssetId\":\"%s\"}"),
                *Descriptor.Id.ToString(), *Action, *Target);
        }
        if (Action == TEXT("command.issue_order"))
        {
            const FDACommandSquadViewModel* Squad = Projection.CommandHUD.Command.Squads.IsValidIndex(PayloadIndex)
                ? &Projection.CommandHUD.Command.Squads[PayloadIndex]
                : (Projection.CommandHUD.Command.Squads.IsEmpty() ? nullptr : &Projection.CommandHUD.Command.Squads[0]);
            if (Target.IsEmpty() && Squad != nullptr) Target = Squad->SquadId.ToString();
            const FVector Destination = Squad == nullptr ? FVector::ZeroVector : Squad->SuggestedOrderDestination;
            return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"squadId\":\"%s\",\"order\":\"move\",\"destination\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f}}"),
                *Descriptor.Id.ToString(), *Action, *Target, Destination.X, Destination.Y, Destination.Z);
        }
        if (Action == TEXT("citizen.assign_job"))
        {
            FString CitizenId = Target;
            if (CitizenId.IsEmpty()) CitizenId = Projection.Citizen.Citizen.CitizenId.ToString();
            const FDACampaignJobOpeningSignal* Opening = Campaign.LiveSignals.JobOpenings.IsEmpty()
                ? nullptr : &Campaign.LiveSignals.JobOpenings[0];
            return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"citizenId\":\"%s\",\"jobId\":\"%s\",\"facilityWorldAssetId\":\"%s\"}"),
                *Descriptor.Id.ToString(), *Action, *CitizenId,
                Opening == nullptr ? TEXT("") : *Opening->JobId.ToString(),
                Opening == nullptr ? TEXT("") : *GuidText(Opening->FacilityWorldAssetId));
        }
        if (Action == TEXT("treaty.commit"))
        {
            if (Target.IsEmpty()) Target = Projection.Treaty.RelationshipId.ToString();
            const FDADiplomaticReason* Term = Projection.Treaty.TermReasons.IsValidIndex(PayloadIndex)
                ? &Projection.Treaty.TermReasons[PayloadIndex] : nullptr;
            if (Term == nullptr) return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\"}"),
                *Descriptor.Id.ToString(), *Action);
            return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"relationshipId\":\"%s\",\"treatyTerms\":[{\"termId\":\"%s\",\"metric\":\"%s\",\"magnitude\":%.3f}]}"),
                *Descriptor.Id.ToString(), *Action, *Target, *Term->MutationId.ToString(),
                *MetricName(Term->Metric), Term->Magnitude);
        }
        if (Action == TEXT("card.inspect_rotate"))
        {
            if (Target.IsEmpty() && !Projection.Collection.Instances.IsEmpty())
                Target = GuidText(Projection.Collection.Instances[0].InstanceId);
            return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"cardInstanceId\":\"%s\",\"axis\":%.1f}"),
                *Descriptor.Id.ToString(), *Action, *Target, PayloadIndex == 0 ? -1.f : 1.f);
        }
        if (Action == TEXT("conquest.choose_route"))
        {
            Field = TEXT("routeAssetId");
            if (Target.IsEmpty() && Projection.Conquest.RouteMeters.IsValidIndex(PayloadIndex))
                Target = GuidText(Projection.Conquest.RouteMeters[PayloadIndex].RouteAssetId);
        }
        else if (Action.StartsWith(TEXT("citizen.")))
        {
            Field = TEXT("citizenId");
            if (Target.IsEmpty()) Target = Projection.Citizen.Citizen.CitizenId.ToString();
            if (Target.IsEmpty() && !Campaign.LiveSignals.Citizens.IsEmpty())
                Target = Campaign.LiveSignals.Citizens[0].CitizenId.ToString();
        }
        else if (Action.StartsWith(TEXT("building.")))
        {
            Field = TEXT("worldAssetId");
            if (Target.IsEmpty()) Target = GuidText(Projection.Building.Asset.WorldAssetId);
            if ((Target.IsEmpty() || Target == TEXT("none")) && !Campaign.WorldAssets.IsEmpty())
                Target = GuidText(Campaign.WorldAssets[0].WorldAssetId);
        }
        else if (Action.StartsWith(TEXT("treaty.")) || Action.StartsWith(TEXT("diplomacy.")))
        {
            Field = TEXT("relationshipId");
            if (Target.IsEmpty()) Target = Projection.Treaty.RelationshipId.ToString();
            if (Target.IsEmpty() && !Projection.Diplomacy.Relationships.IsEmpty())
                Target = Projection.Diplomacy.Relationships[0].RelationshipId.ToString();
        }
        else if (Action.StartsWith(TEXT("command.")))
        {
            Field = TEXT("squadId");
            if (Target.IsEmpty() && Projection.CommandHUD.Command.Squads.IsValidIndex(PayloadIndex))
                Target = Projection.CommandHUD.Command.Squads[PayloadIndex].SquadId.ToString();
        }
        else if (Action == TEXT("city.select_card")
            || Action.StartsWith(TEXT("deck.")) || Action.StartsWith(TEXT("collection."))
            || Action.StartsWith(TEXT("card.")))
        {
            Field = TEXT("cardInstanceId");
            if (Target.IsEmpty() && Projection.CityHUD.Hand.IsValidIndex(PayloadIndex))
                Target = GuidText(Projection.CityHUD.Hand[PayloadIndex]);
            if (Target.IsEmpty() && Projection.Collection.Instances.IsValidIndex(PayloadIndex))
                Target = GuidText(Projection.Collection.Instances[PayloadIndex].InstanceId);
            if (Target.IsEmpty() && Projection.Deck.Deck.IsValidIndex(PayloadIndex))
                Target = GuidText(Projection.Deck.Deck[PayloadIndex]);
        }
        else if (Action == TEXT("world.travel"))
        {
            Field = TEXT("regionId");
            if (Target.IsEmpty())
            {
                TArray<FName> Destinations;
                for (const FDARegionState& Region : Projection.WorldMap.Regions)
                    if (Region.RegionId != Campaign.WorldState.CurrentRegionId)
                        Destinations.Add(Region.RegionId);
                if (Destinations.IsValidIndex(PayloadIndex)) Target = Destinations[PayloadIndex].ToString();
                else if (!Destinations.IsEmpty()) Target = Destinations[0].ToString();
            }
        }
        else if (Action.StartsWith(TEXT("founder.")))
        {
            Field = Action == TEXT("founder.interact") ? TEXT("interactionId") : TEXT("abilityId");
            if (Target.IsEmpty()) Target = Action == TEXT("founder.interact")
                ? Projection.FounderHUD.Founder.InteractionId.ToString()
                : (Projection.FounderHUD.Founder.AbilityIds.IsValidIndex(PayloadIndex)
                    ? Projection.FounderHUD.Founder.AbilityIds[PayloadIndex].ToString() : FString());
        }
        else if (Action.StartsWith(TEXT("campaign.")))
        { Field = Action == TEXT("campaign.resume") ? TEXT("saveSlotId") : TEXT("campaignPresetId");
          if (Target.IsEmpty()) Target = Action == TEXT("campaign.resume") ? TEXT("autosave") : TEXT("synara.standard"); }
        else if (Action == TEXT("settings.apply") || Action == TEXT("accessibility.apply"))
        {
            const FDAAccessibilitySettings& Settings = Action == TEXT("accessibility.apply")
                ? Projection.AccessibilityScreen.Draft : Projection.Settings.PersistedSettings;
            return FDAUIViewModelFactory::BuildAccessibilityPayload(Descriptor.Id, ActionId, Settings);
        }
        else if (Action.StartsWith(TEXT("ui.open_")))
        { Field = TEXT("routeId"); if (Target.IsEmpty()) Target = Action.RightChop(8); }
        else if (Action == TEXT("ui.tactical_pause"))
        { Field = TEXT("timeCommandId"); if (Target.IsEmpty()) Target = TEXT("tactical_pause"); }
        else if (Action == TEXT("crafting.craft"))
        {
            const FDACraftingBlueprintViewModel* Blueprint =
                Projection.Crafting.Blueprints.IsValidIndex(PayloadIndex)
                ? &Projection.Crafting.Blueprints[PayloadIndex] : nullptr;
            if (Target.IsEmpty() && Blueprint != nullptr) Target = Blueprint->BlueprintId.ToString();
            return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"blueprintId\":\"%s\",\"quantity\":1}"),
                *Descriptor.Id.ToString(), *Action, *Target);
        }
        else if (Action == TEXT("faction.open_diplomacy"))
        {
            Field = TEXT("factionId");
            if (Target.IsEmpty() && !Projection.Factions.Factions.IsEmpty())
                Target = Projection.Factions.Factions[0].FactionId.ToString();
        }
        else if (Action == TEXT("quest.track"))
        { Field = TEXT("questId"); if (Target.IsEmpty()) Target = CurrentObjective(Campaign).ToString(); }
        else if (Action == TEXT("history.inspect"))
        { Field = TEXT("historyRecordId"); if (Target.IsEmpty() && !Campaign.HistoryTags.IsEmpty())
            Target = Campaign.HistoryTags[0].ToString(); }
        else if (Action == TEXT("research.start"))
        {
            Field = TEXT("researchId");
            if (Target.IsEmpty() && Projection.LateFeatureAuthority.ResearchIds.IsValidIndex(PayloadIndex))
                Target = Projection.LateFeatureAuthority.ResearchIds[PayloadIndex].ToString();
        }
        else if (Action == TEXT("metrics.inspect"))
        { Field = TEXT("metricId"); if (Target.IsEmpty() && Descriptor.DataChannels.IsValidIndex(PayloadIndex))
            Target = Descriptor.DataChannels[PayloadIndex].ToString(); }
        else if (Action == TEXT("leader.confirm_resolution"))
        { Field = TEXT("leaderResolutionId"); if (Target.IsEmpty()
            && Projection.LateFeatureAuthority.LeaderResolutions.IsValidIndex(PayloadIndex))
            Target = Projection.LateFeatureAuthority.LeaderResolutions[PayloadIndex].ResolutionId.ToString(); }
        else if (Action == TEXT("ascension.claim"))
        { Field = TEXT("rewardId"); if (Target.IsEmpty()
            && Projection.LateFeatureAuthority.AscensionRewards.IsValidIndex(PayloadIndex))
            Target = Projection.LateFeatureAuthority.AscensionRewards[PayloadIndex].RewardId.ToString(); }
        else if (Action.StartsWith(TEXT("save.")))
        { Field = TEXT("saveSlotId"); if (Target.IsEmpty()) Target = TEXT("autosave"); }
        else if (Action == TEXT("recap.continue")) return FString::Printf(
            TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\"}"), *Descriptor.Id.ToString(), *Action);
        if (Target.IsEmpty()) return FString::Printf(
            TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\"}"), *Descriptor.Id.ToString(), *Action);
        return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"%s\":\"%s\"}"),
            *Descriptor.Id.ToString(), *Action, *Field, *Target);
    }
}

FString FDAUIViewModelFactory::BuildAccessibilityPayload(
    const FName ScreenId, const FName ActionId, const FDAAccessibilitySettings& Settings)
{
    return FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\",\"keyboardBindings\":%s,\"controllerBindings\":%s,\"textScale\":%.3f,\"subtitles\":%s,\"speakerLabels\":%s,\"subtitleBackgroundOpacity\":%.3f,\"colorIndependentMarkers\":%s,\"colorVisionPreset\":%d,\"cameraShake\":%.3f,\"fieldOfView\":%.3f,\"motionBlur\":%s,\"reducedMotion\":%s,\"reducedFlash\":%s,\"tacticalPause\":%s,\"holdToggleModes\":%s,\"aimAssist\":%.3f,\"buildSnapStrength\":%.3f,\"tutorialRecall\":%s,\"tooltipMode\":%d}"),
        *ScreenId.ToString(), *ActionId.ToString(),
        *SerializeKeyBindings(Settings.KeyboardBindings), *SerializeKeyBindings(Settings.ControllerBindings),
        Settings.TextScale, Settings.bSubtitles ? TEXT("true") : TEXT("false"),
        Settings.bSpeakerLabels ? TEXT("true") : TEXT("false"), Settings.SubtitleBackgroundOpacity,
        Settings.bColorIndependentMarkers ? TEXT("true") : TEXT("false"),
        static_cast<int32>(Settings.ColorVisionPreset), Settings.CameraShake, Settings.FieldOfView,
        Settings.bMotionBlur ? TEXT("true") : TEXT("false"),
        Settings.bReducedMotion ? TEXT("true") : TEXT("false"),
        Settings.bReducedFlash ? TEXT("true") : TEXT("false"),
        Settings.bTacticalPause ? TEXT("true") : TEXT("false"),
        *SerializeHoldToggle(Settings.HoldToggleModes), Settings.AimAssist, Settings.BuildSnapStrength,
        Settings.bTutorialRecall ? TEXT("true") : TEXT("false"), static_cast<int32>(Settings.TooltipMode));
}

void UDAUIViewModelProvider::SetAuthoritativeSnapshot(const FDACampaignSnapshot& InCampaign)
{
    Campaign = InCampaign; bHasAuthority = true; OnProjectionInvalidated.Broadcast();
}

void UDAUIViewModelProvider::BindRuntimePlayer(APlayerController* PlayerController)
{
    if (AccessibilityAuthority.IsValid() && AccessibilityChangedHandle.IsValid())
        AccessibilityAuthority->OnSettingsChanged.Remove(AccessibilityChangedHandle);
    AccessibilityAuthority.Reset(); AccessibilityChangedHandle.Reset();
    if (CameraModeAuthority.IsValid())
        CameraModeAuthority->OnModeChanged.RemoveDynamic(this, &UDAUIViewModelProvider::HandleCameraModeChanged);
    CameraModeAuthority.Reset();
    if (AbilitySystemAuthority.IsValid())
    {
        AbilitySystemAuthority->GetGameplayAttributeValueChangeDelegate(
            UDACombatAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
        AbilitySystemAuthority->GetGameplayAttributeValueChangeDelegate(
            UDACombatAttributeSet::GetGuardAttribute()).Remove(GuardChangedHandle);
        AbilitySystemAuthority->GetGameplayAttributeValueChangeDelegate(
            UDACombatAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
    }
    AbilitySystemAuthority.Reset(); HealthChangedHandle.Reset(); GuardChangedHandle.Reset(); StaminaChangedHandle.Reset();
    RuntimePlayerController = PlayerController;
    if (PlayerController != nullptr)
        if (UGameInstance* GameInstance = PlayerController->GetGameInstance())
            if (UDAAccessibilitySettingsSubsystem* Accessibility =
                GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>())
            {
                AccessibilityAuthority = Accessibility;
                AccessibilityChangedHandle = Accessibility->OnSettingsChanged.AddUObject(
                    this, &UDAUIViewModelProvider::HandleAccessibilityChanged);
            }
    if (PlayerController != nullptr)
    {
        UDACameraModeController* CameraMode = PlayerController->FindComponentByClass<UDACameraModeController>();
        if (CameraMode == nullptr && PlayerController->GetPawn() != nullptr)
            CameraMode = PlayerController->GetPawn()->FindComponentByClass<UDACameraModeController>();
        if (CameraMode != nullptr)
        {
            CameraModeAuthority = CameraMode;
            CameraMode->OnModeChanged.AddDynamic(this, &UDAUIViewModelProvider::HandleCameraModeChanged);
        }
        if (APawn* Pawn = PlayerController->GetPawn())
            if (UAbilitySystemComponent* AbilitySystem = Pawn->FindComponentByClass<UAbilitySystemComponent>())
            {
                AbilitySystemAuthority = AbilitySystem;
                HealthChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(
                    UDACombatAttributeSet::GetHealthAttribute()).AddUObject(
                        this, &UDAUIViewModelProvider::HandleCombatAttributeChanged);
                GuardChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(
                    UDACombatAttributeSet::GetGuardAttribute()).AddUObject(
                        this, &UDAUIViewModelProvider::HandleCombatAttributeChanged);
                StaminaChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(
                    UDACombatAttributeSet::GetStaminaAttribute()).AddUObject(
                        this, &UDAUIViewModelProvider::HandleCombatAttributeChanged);
            }
    }
    RefreshRuntimeSources();
}

void UDAUIViewModelProvider::HandleAccessibilityChanged(const FDAAccessibilityRuntimePolicy& Policy)
{
    SetAccessibilitySettings(Policy.Settings);
}

void UDAUIViewModelProvider::HandleCombatAttributeChanged(const FOnAttributeChangeData&)
{
    RefreshRuntimeSources();
}

void UDAUIViewModelProvider::HandleCameraModeChanged(const EDAPlayMode, const EDAPlayMode)
{
    RefreshRuntimeSources();
}

void UDAUIViewModelProvider::RefreshRuntimeSources()
{
    bHasFounderLiveAuthority = false;
    bHasCommandLiveAuthority = false;
    APlayerController* PlayerController = RuntimePlayerController.Get();
    if (PlayerController == nullptr)
    { FounderSource = {}; CommandSource = {}; OnProjectionInvalidated.Broadcast(); return; }
    if (const UDACameraModeController* Mode = PlayerController->FindComponentByClass<UDACameraModeController>())
    {
        FounderSource.PlayMode = PlayModeName(Mode->GetCurrentMode());
        CommandSource.PlayMode = FounderSource.PlayMode;
    }
    else if (const APawn* Pawn = PlayerController->GetPawn())
        if (const UDACameraModeController* Mode = Pawn->FindComponentByClass<UDACameraModeController>())
        {
            FounderSource.PlayMode = PlayModeName(Mode->GetCurrentMode());
            CommandSource.PlayMode = FounderSource.PlayMode;
        }
    if (APawn* Pawn = PlayerController->GetPawn())
        if (const UAbilitySystemComponent* AbilitySystem = Pawn->FindComponentByClass<UAbilitySystemComponent>())
            if (const UDACombatAttributeSet* Attributes = AbilitySystem->GetSet<UDACombatAttributeSet>())
            {
                FounderSource.Health = Attributes->GetHealth();
                FounderSource.Guard = Attributes->GetGuard();
                FounderSource.Stamina = Attributes->GetStamina();
            }
    if (UGameInstance* GameInstance = PlayerController->GetGameInstance())
    {
        if (const UDAAccessibilitySettingsSubsystem* Accessibility =
            GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>())
            AccessibilitySettings = Accessibility->GetSettings();
        if (const UDACommandSubsystem* Commands = GameInstance->GetSubsystem<UDACommandSubsystem>())
        {
            CommandSource.CommandPoints = Commands->GetCommandPoints();
            CommandSource.Squads.Reset();
            float TotalSupply = 0.f;
            for (const UDASquadEntity* Squad : Commands->GetRegisteredSquads())
                if (Squad != nullptr)
                {
                    FDACommandSquadViewModel& Row = CommandSource.Squads.Emplace_GetRef();
                    Row.SquadId = Squad->GetFName(); Row.Morale = Squad->GetMorale(); Row.Supply = Squad->GetSupply();
                    Row.bSelected = Commands->GetDirectlySelectedSquads().ContainsByPredicate(
                        [Squad](const TObjectPtr<UDASquadEntity>& Selected) { return Selected.Get() == Squad; });
                    TotalSupply += Row.Supply;
                }
            CommandSource.Supply = CommandSource.Squads.IsEmpty() ? 0.f : TotalSupply / CommandSource.Squads.Num();
        }
        DeckRuleResults.Reset();
        TArray<FText> RuleErrors;
        const UDAContentRegistrySubsystem* Registry = GameInstance->GetSubsystem<UDAContentRegistrySubsystem>();
        bDeckRulesValid = Registry != nullptr && FDADeckRules::Validate(Campaign.DeckState, *Registry, RuleErrors);
        if (Registry == nullptr) DeckRuleResults.Add(TEXT("Content registry unavailable."));
        else for (const FText& Error : RuleErrors) DeckRuleResults.Add(Error.ToString());
        if (bDeckRulesValid) DeckRuleResults.Add(TEXT("Deck satisfies authoritative rules."));
        FacilityOutputs.Reset();
        const UDAEconomySubsystem* Economy = GameInstance->GetSubsystem<UDAEconomySubsystem>();
        if (Economy != nullptr && Registry != nullptr)
            for (const FDAWorldAssetRecord& Asset : Campaign.WorldAssets)
                if (const UDA_CardDefinition* Definition = Registry->GetCardDefinition(Asset.CardDefinitionId))
                {
                    FDAFacilityContext Context; Context.AssetRecord = Asset;
                    Definition->TryGetBaseCapitalPerCycle(Context.BaseOutput.Capital);
                    Definition->TryGetBaseInsightPerCycle(Context.BaseOutput.Insight);
                    Definition->TryGetBaseInfluencePerCycle(Context.BaseOutput.Influence);
                    float DeploymentCapital = 0.f;
                    int32 DeploymentCapitalInt = 0;
                    if (Definition->TryGetDeploymentCapital(DeploymentCapitalInt)) DeploymentCapital = DeploymentCapitalInt;
                    Context.DeploymentCapital = DeploymentCapital;
                    Context.ConditionOutputMultiplier = FMath::Clamp(Asset.StructuralIntegrity / 100.f, 0.f, 1.f);
                    Context.MaintenanceCondition = Asset.StructuralIntegrity >= 80.f ? EDAMaintenanceCondition::Healthy
                        : Asset.StructuralIntegrity >= 50.f ? EDAMaintenanceCondition::Worn
                        : Asset.StructuralIntegrity >= 20.f ? EDAMaintenanceCondition::Damaged
                        : EDAMaintenanceCondition::CriticallyDamaged;
                    switch (Definition->CardType)
                    {
                    case EDACardType::Residential: Context.FacilityType = EDAFacilityType::Residential; break;
                    case EDACardType::Retail: Context.FacilityType = EDAFacilityType::Retail; break;
                    case EDACardType::Office: Context.FacilityType = EDAFacilityType::Office; break;
                    case EDACardType::Research: Context.FacilityType = EDAFacilityType::Research; break;
                    case EDACardType::Industrial:
                    case EDACardType::Production: Context.FacilityType = EDAFacilityType::Industrial; break;
                    case EDACardType::Infrastructure:
                    case EDACardType::Utility: Context.FacilityType = EDAFacilityType::Infrastructure; break;
                    case EDACardType::Defense:
                    case EDACardType::Military: Context.FacilityType = EDAFacilityType::Defense; break;
                    case EDACardType::Wonder: Context.FacilityType = EDAFacilityType::Wonder; break;
                    default: Context.FacilityType = EDAFacilityType::Unspecified; break;
                    }
                    EDACampaignUtilitySupply WorstSupply = EDACampaignUtilitySupply::FullySupplied;
                    for (const FDACampaignUtilitySignal& Signal : Campaign.LiveSignals.UtilitySignals)
                        if (Signal.WorldAssetId == Asset.WorldAssetId
                            && static_cast<uint8>(Signal.Supply) > static_cast<uint8>(WorstSupply))
                            WorstSupply = Signal.Supply;
                    Context.UtilityState = static_cast<EDAUtilityState>(WorstSupply);
                    FacilityOutputs.Add(Economy->CalculateFacilityOutput(Context));
                }
    }
    FString LiveSourceError;
    if (const IDAFounderHUDLiveSource* FounderLive =
        Cast<IDAFounderHUDLiveSource>(FounderHUDLiveAuthority.Get()))
    {
        FDAFounderHUDSource Captured;
        if (FounderLive->CaptureFounderHUDState(Captured, LiveSourceError))
        {
            Captured.Health = FounderSource.Health; Captured.Guard = FounderSource.Guard;
            Captured.Stamina = FounderSource.Stamina; Captured.PlayMode = FounderSource.PlayMode;
            FounderSource = MoveTemp(Captured);
            bHasFounderLiveAuthority = true;
        }
        else FounderSource = {};
    }
    else FounderSource = {};
    if (const IDACommandHUDLiveSource* CommandLive =
        Cast<IDACommandHUDLiveSource>(CommandHUDLiveAuthority.Get()))
    {
        FDACommandHUDSource Captured;
        if (CommandLive->CaptureCommandHUDState(Captured, LiveSourceError))
        {
            Captured.CommandPoints = CommandSource.CommandPoints;
            for (FDACommandSquadViewModel& CapturedSquad : Captured.Squads)
                if (const FDACommandSquadViewModel* GameplaySquad = CommandSource.Squads.FindByPredicate(
                    [&CapturedSquad](const FDACommandSquadViewModel& Row)
                    { return Row.SquadId == CapturedSquad.SquadId; }))
                {
                    CapturedSquad.Morale = GameplaySquad->Morale;
                    CapturedSquad.Supply = GameplaySquad->Supply;
                    CapturedSquad.bSelected = GameplaySquad->bSelected;
                }
            Captured.PlayMode = CommandSource.PlayMode;
            CommandSource = MoveTemp(Captured);
            bHasCommandLiveAuthority = true;
        }
        else CommandSource = {};
    }
    else CommandSource = {};
    OnProjectionInvalidated.Broadcast();
}

bool UDAUIViewModelProvider::RegisterFounderHUDLiveSource(UObject* Source, FString& OutError)
{
    IDAFounderHUDLiveSource* Typed = Source == nullptr ? nullptr : Cast<IDAFounderHUDLiveSource>(Source);
    if (Typed == nullptr)
    { OutError = TEXT("Founder HUD source must implement IDAFounderHUDLiveSource."); return false; }
    FDAFounderHUDSource Captured;
    if (!Typed->CaptureFounderHUDState(Captured, OutError)) return false;
    FounderHUDLiveAuthority = Source; FounderSource = MoveTemp(Captured);
    bHasFounderLiveAuthority = true; OutError.Reset(); RefreshRuntimeSources(); return true;
}

bool UDAUIViewModelProvider::RegisterCommandHUDLiveSource(UObject* Source, FString& OutError)
{
    IDACommandHUDLiveSource* Typed = Source == nullptr ? nullptr : Cast<IDACommandHUDLiveSource>(Source);
    if (Typed == nullptr)
    { OutError = TEXT("Command HUD source must implement IDACommandHUDLiveSource."); return false; }
    FDACommandHUDSource Captured;
    if (!Typed->CaptureCommandHUDState(Captured, OutError)) return false;
    CommandHUDLiveAuthority = Source; CommandSource = MoveTemp(Captured);
    bHasCommandLiveAuthority = true; OutError.Reset(); RefreshRuntimeSources(); return true;
}

void UDAUIViewModelProvider::UnregisterFounderHUDLiveSource(UObject* Source)
{
    if (FounderHUDLiveAuthority.Get() == Source)
    { FounderHUDLiveAuthority.Reset(); FounderSource = {}; bHasFounderLiveAuthority = false; OnProjectionInvalidated.Broadcast(); }
}

void UDAUIViewModelProvider::UnregisterCommandHUDLiveSource(UObject* Source)
{
    if (CommandHUDLiveAuthority.Get() == Source)
    { CommandHUDLiveAuthority.Reset(); CommandSource = {}; bHasCommandLiveAuthority = false; OnProjectionInvalidated.Broadcast(); }
}

FDACityHUDViewModel FDAUIViewModelFactory::BuildCityHUD(const FDACampaignSnapshot& Campaign)
{
    FDACityHUDViewModel Result;
    Result.Wallets.Capital = Campaign.LiveSignals.Capital;
    Result.Wallets.Insight = Campaign.LiveSignals.Insight;
    Result.Wallets.Influence = Campaign.LiveSignals.Influence;
    Result.Population = Campaign.LiveSignals.Population;
    Result.Dependency = Campaign.SynaraState.Dependency;
    Result.DependencyReasons = Campaign.SynaraState.DependencyReasons;
    Result.Hand = Campaign.DeckState.GetHand();
    Result.ObjectiveId = CurrentObjective(Campaign);
    Result.CurrentDevelopmentCycle = Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle;
    Result.CurrentWorldTick = Campaign.WorldState.CurrentWorldTick;
    return Result;
}

FDAFounderHUDViewModel FDAUIViewModelFactory::BuildFounderHUD(
    const FDACampaignSnapshot& Campaign, const FDAFounderHUDSource& Source)
{
    FDAFounderHUDViewModel Result; Result.Founder = Source; Result.ObjectiveId = CurrentObjective(Campaign);
    Result.Founder.CurrentDevelopmentCycle = Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle;
    Result.Founder.CurrentWorldTick = Campaign.WorldState.CurrentWorldTick;
    return Result;
}

FDACommandHUDViewModel FDAUIViewModelFactory::BuildCommandHUD(
    const FDACampaignSnapshot& Campaign, const FDACommandHUDSource& Source)
{
    FDACommandHUDViewModel Result; Result.Command = Source; Result.ObjectiveId = CurrentObjective(Campaign);
    Result.Command.CurrentDevelopmentCycle = Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle;
    Result.Command.CurrentWorldTick = Campaign.WorldState.CurrentWorldTick;
    return Result;
}

FDAFacilityTooltipViewModel FDAUIViewModelFactory::BuildFacilityTooltip(const FDAFacilityOutput& Output)
{
    FDAFacilityTooltipViewModel Result;
    Result.WorldAssetId = Output.WorldAssetId; Result.GrossOutput = Output.GrossOutput;
    Result.NetOutput = Output.NetOutput; Result.MaintenanceCapital = Output.MaintenanceCapital;
    Result.Contributions = Output.Contributions;
    return Result;
}

FDADiplomacyPanelViewModel FDAUIViewModelFactory::BuildDiplomacy(const FDACampaignSnapshot& Campaign)
{
    FDADiplomacyPanelViewModel Result;
    for (const FDADiplomaticRelationship& Source : Campaign.WorldState.Diplomacy.Relationships)
    {
        FDADiplomacyRelationshipViewModel& Row = Result.Relationships.Emplace_GetRef();
        Row.RelationshipId = Source.RelationshipId; Row.Trust = Source.Trust; Row.Respect = Source.Respect;
        Row.Fear = Source.Fear; Row.Dependence = Source.Dependence; Row.Grievance = Source.Grievance;
        Row.Compatibility = Source.Compatibility; Row.Reasons = Source.ReasonLedger;
    }
    return Result;
}

FDAWorldMapViewModel FDAUIViewModelFactory::BuildWorldMap(const FDACampaignSnapshot& Campaign)
{
    FDAWorldMapViewModel Result; Result.Regions = Campaign.WorldState.Regions;
    Result.TradeCorridors = Campaign.WorldState.Trade.Routes;
    Result.AuthorityReasons = Campaign.NarrativeState.WorldMapAuthorityRecords;
    return Result;
}

FDAConquestDashboardViewModel FDAUIViewModelFactory::BuildConquest(const FDACampaignSnapshot& Campaign)
{
    FDAConquestDashboardViewModel Result; Result.Captures = Campaign.OperationConflict.CaptureRecords;
    Result.Surrenders = Campaign.OperationConflict.SurrenderRecords;
    for (const FDACaptureRecord& Capture : Result.Captures)
    {
        Result.OutcomeReasons.Append(Capture.History);
        FDAConquestRouteMeterViewModel& Meter = Result.RouteMeters.Emplace_GetRef();
        Meter.RouteAssetId = Capture.WorldAssetId;
        Meter.Progress = Capture.RequiredCaptureTimeSeconds > 0.f
            ? FMath::Clamp(Capture.CaptureProgressSeconds / Capture.RequiredCaptureTimeSeconds, 0.f, 1.f)
            : (Capture.bCaptureCompleted ? 1.f : 0.f);
        Meter.CapitalReward = Capture.SalvageCapitalReward;
        Meter.InsightReward = Capture.StudyInsightReward;
        Meter.Reasons = Capture.History;
    }
    for (const FDASurrenderRecord& Surrender : Result.Surrenders) Result.OutcomeReasons.Append(Surrender.History);
    return Result;
}

FDACollectionViewModel FDAUIViewModelFactory::BuildCollection(const FDACampaignSnapshot& Campaign)
{
    FDACollectionViewModel Result;
    Campaign.CollectionState.Instances.GenerateValueArray(Result.Instances);
    Result.Instances.Sort([](const FCardInstance& Left, const FCardInstance& Right)
        { return Left.InstanceId.ToString().Compare(Right.InstanceId.ToString()) < 0; });
    return Result;
}

FDADeckViewModel FDAUIViewModelFactory::BuildDeck(const FDACampaignSnapshot& Campaign)
{
    FDADeckViewModel Result; Result.Deck = Campaign.DeckState.GetInstanceIds();
    Result.Hand = Campaign.DeckState.GetHand(); Result.Reserve = Campaign.DeckState.GetReserveQueue(); return Result;
}

FDABuildingInspectViewModel FDAUIViewModelFactory::BuildBuildingInspect(
    const FDACampaignSnapshot& Campaign, const FGuid WorldAssetId, const FDAFacilityOutput* Output)
{
    FDABuildingInspectViewModel Result;
    if (const FDAWorldAssetRecord* Asset = Campaign.FindWorldAssetRecord(WorldAssetId)) Result.Asset = *Asset;
    if (Output != nullptr && Output->WorldAssetId == WorldAssetId)
    {
        Result.bHasEconomyTrace = true;
        Result.EconomyTrace = BuildFacilityTooltip(*Output);
    }
    return Result;
}

FDACitizenInspectViewModel FDAUIViewModelFactory::BuildCitizenInspect(
    const FDACampaignSnapshot& Campaign, const FName CitizenId)
{
    FDACitizenInspectViewModel Result;
    if (const FDACampaignCitizenSignal* Citizen = Campaign.LiveSignals.FindCitizen(CitizenId)) Result.Citizen = *Citizen;
    Result.StoryState = Campaign.NarrativeState.CitizenStoryStates.FindRef(CitizenId);
    Result.RelationshipReasons = Campaign.SynaraState.CitizenRelationshipReasons.FilterByPredicate(
        [CitizenId](const FDASynaraValueReason& Reason) { return Reason.SubjectId == CitizenId; });
    return Result;
}

FDAFactionPanelViewModel FDAUIViewModelFactory::BuildFactionPanel(const FDACampaignSnapshot& Campaign)
{
    FDAFactionPanelViewModel Result;
    TArray<FName> Ids; Campaign.SynaraState.FactionSupport.GetKeys(Ids);
    Ids.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
    for (const FName Id : Ids)
    {
        FDAFactionSupportViewModel& Row = Result.Factions.Emplace_GetRef();
        Row.FactionId = Id; Row.Support = Campaign.SynaraState.FactionSupport.FindRef(Id);
        Row.Reasons = Campaign.SynaraState.FactionSupportReasons.FilterByPredicate(
            [Id](const FDASynaraValueReason& Reason) { return Reason.SubjectId == Id; });
    }
    return Result;
}

FDAResearchViewModel FDAUIViewModelFactory::BuildResearch(const FDACampaignSnapshot& Campaign)
{
    FDAResearchViewModel Result; Result.AuthoredUnlocks = Campaign.NarrativeState.QuestContentUnlockRecords;
    Result.EligibilityReasons = Campaign.NarrativeState.AuditEligibilitySourceRecords; return Result;
}

FDATreatyBuilderViewModel FDAUIViewModelFactory::BuildTreaty(
    const FDACampaignSnapshot& Campaign, const FName RelationshipId)
{
    FDATreatyBuilderViewModel Result; Result.RelationshipId = RelationshipId;
    if (const FDADiplomaticRelationship* Relationship = Campaign.WorldState.Diplomacy.FindRelationship(RelationshipId))
        Result.TermReasons = Relationship->ReasonLedger;
    return Result;
}

bool UDAUIViewModelProvider::BuildScreen(
    const FDAUIScreenDescriptor& Descriptor, FDAUIScreenProjection& Out) const
{
    if (!bHasAuthority) return false;
    Out = {}; Out.ScreenId = Descriptor.Id; Out.bAuthoritative = true;
    Out.DataChannels = Descriptor.DataChannels; Out.Accessibility = AccessibilitySettings;
    const bool bLateFeatureScreen = Descriptor.Id == TEXT("research")
        || Descriptor.Id == TEXT("conquest_dashboard")
        || Descriptor.Id == TEXT("leader_resolution")
        || Descriptor.Id == TEXT("ascension_reward");
    FString FeatureError;
    if (bLateFeatureScreen)
    {
        Out.bAuthoritative = FeatureAuthority.IsValid()
            && FeatureAuthority->CaptureRegisteredState(Out.LateFeatureAuthority, FeatureError);
        if (!Out.bAuthoritative && FeatureError.IsEmpty())
            FeatureError = TEXT("ServiceUnavailable: late feature authority is not registered.");
    }
    const bool bMissingFounderAuthority = Descriptor.Id == TEXT("founder_hud")
        && !bHasFounderLiveAuthority;
    const bool bMissingCommandAuthority = Descriptor.Id == TEXT("command_hud")
        && !bHasCommandLiveAuthority;
    if (bMissingFounderAuthority || bMissingCommandAuthority) Out.bAuthoritative = false;
    Out.MainMenu.bCampaignAuthorityAvailable = true;
    Out.MainMenu.CurrentRegionId = Campaign.WorldState.CurrentRegionId;
    Out.MainMenu.CurrentWorldTick = Campaign.WorldState.CurrentWorldTick;
    Out.NewCampaign.PresetIds = {TEXT("synara.standard"), TEXT("synara.accessible"), TEXT("synara.challenge")};
    Out.NewCampaign.SelectedPresetId = StableSelections.FindRef(TEXT("new_campaign"));
    if (Out.NewCampaign.SelectedPresetId.IsNone()) Out.NewCampaign.SelectedPresetId = TEXT("synara.standard");
    FString CampaignValidation;
    if (!Campaign.Validate(CampaignValidation)) Out.NewCampaign.ValidationReasons.Add(CampaignValidation);
    Out.Settings.PersistedSettings = AccessibilitySettings;
    Out.AccessibilityScreen.Draft = AccessibilitySettings;
    FDAUIManifest CanonicalManifest;
    TArray<FText> ManifestErrors;
    if (FDAUIManifest::LoadCanonical(CanonicalManifest, ManifestErrors))
    {
        for (const FDAUICampaignActionDescriptor& Action : CanonicalManifest.CampaignCriticalActions)
            Out.Settings.RemappableActionIds.Add(Action.Id);
        for (const FDAUIAccessibilityOptionDescriptor& Option : CanonicalManifest.AccessibilityOptions)
            Out.AccessibilityScreen.OptionIds.Add(Option.Id);
    }
    else for (const FText& Error : ManifestErrors)
    {
        Out.Settings.BindingConflicts.Add(Error.ToString());
        Out.AccessibilityScreen.ValidationReasons.Add(Error.ToString());
    }
    FString AccessibilityValidation;
    if (!AccessibilitySettings.Validate(AccessibilityValidation))
        Out.AccessibilityScreen.ValidationReasons.Add(AccessibilityValidation);
    Out.Pause.SaveSlotId = TEXT("autosave");
    Out.Pause.WorldTick = Campaign.WorldState.CurrentWorldTick;
    Out.Pause.SignalRevision = Campaign.LiveSignals.MutationRevision;
    Out.Pause.bCampaignValid = Campaign.Validate(CampaignValidation);
    if (!Out.Pause.bCampaignValid) Out.Pause.ValidationReasons.Add(CampaignValidation);
    Out.QuestJournal.Quests = Campaign.NarrativeState.QuestStates;
    Out.QuestJournal.ObjectiveBindings = Campaign.NarrativeState.QuestObjectiveAssetBindings;
    Out.QuestJournal.TrackedObjectiveId = StableSelections.FindRef(TEXT("quest_journal"));
    if (Out.QuestJournal.TrackedObjectiveId.IsNone())
        Out.QuestJournal.TrackedObjectiveId = CurrentObjective(Campaign);
    Out.HistoryTimeline.HistoryTags = Campaign.HistoryTags;
    Out.HistoryTimeline.Actions = Campaign.NarrativeState.ActionRecords;
    Out.HistoryTimeline.PromiseReasons = Campaign.NarrativeState.PromiseRecords;
    Out.CityMetrics.Wallets.Capital = Campaign.LiveSignals.Capital;
    Out.CityMetrics.Wallets.Insight = Campaign.LiveSignals.Insight;
    Out.CityMetrics.Wallets.Influence = Campaign.LiveSignals.Influence;
    Out.CityMetrics.Population = Campaign.LiveSignals.Population;
    Out.CityMetrics.Dependency = Campaign.SynaraState.Dependency;
    Out.CityMetrics.DependencyReasons = Campaign.SynaraState.DependencyReasons;
    for (const FDAFacilityOutput& Output : FacilityOutputs)
        Out.CityMetrics.FacilityOutputs.Add(FDAUIViewModelFactory::BuildFacilityTooltip(Output));
    Out.LeaderResolution.Records = Out.LateFeatureAuthority.LeaderResolutions;
    for (const FDAUILeaderResolutionAuthorityRecord& Record : Out.LeaderResolution.Records)
        Out.LeaderResolution.ResolutionReasons.Append(Record.Reasons);
    Out.AscensionReward.AuthorityRewards = Out.LateFeatureAuthority.AscensionRewards;
    Out.SaveLoad.CurrentWorldTick = Campaign.WorldState.CurrentWorldTick;
    Out.SaveLoad.SignalRevision = Campaign.LiveSignals.MutationRevision;
    FDASaveService Saves;
    for (const FString& SlotId : TArray<FString>{TEXT("autosave"), TEXT("manual_1"), TEXT("manual_2")})
    {
        FDASaveSlotViewModel& Slot = Out.SaveLoad.Slots.Emplace_GetRef();
        Slot.SlotId = SlotId;
        const TResult<FDACampaignSnapshot, FDASaveError> Loaded = Saves.LoadCampaign(SlotId);
        Slot.bExists = Loaded.HasValue(); Slot.bLoadable = Loaded.HasValue();
        Slot.ValidationReason = Loaded.HasValue() ? TEXT("Validated against current save schema.")
            : Loaded.GetError().Message;
    }
    Out.ReturningPlayerRecap.CurrentObjectiveId = CurrentObjective(Campaign);
    Out.ReturningPlayerRecap.HistoryTags = Campaign.HistoryTags;
    const int32 RecentStart = FMath::Max(0, Campaign.NarrativeState.ActionRecords.Num() - 5);
    for (int32 Index = RecentStart; Index < Campaign.NarrativeState.ActionRecords.Num(); ++Index)
        Out.ReturningPlayerRecap.RecentActions.Add(Campaign.NarrativeState.ActionRecords[Index]);
    for (const FDAQuestContentUnlockRecord& Unlock : Campaign.NarrativeState.QuestContentUnlockRecords)
        if (Unlock.Type == EDAQuestContentUnlockType::Blueprint)
        {
            FDACraftingBlueprintViewModel& Row = Out.Crafting.Blueprints.Emplace_GetRef();
            Row.BlueprintId = Unlock.DefinitionId; Row.SourceQuestId = Unlock.QuestId;
            Row.Quantity = Unlock.Quantity; Row.GrantedInstanceIds = Unlock.GrantedCardInstanceIds;
            Row.RequirementReasons.Add(FString::Printf(TEXT("sourceFingerprint=%s action=%s tick=%lld"),
                *Unlock.SourceFingerprint, *Unlock.ActionId.ToString(), static_cast<long long>(Unlock.WorldTick)));
        }
    for (const FName BlueprintId : Campaign.AscensionState.UnlockedBlueprintIds)
        if (!Out.Crafting.Blueprints.ContainsByPredicate(
            [BlueprintId](const FDACraftingBlueprintViewModel& Row)
            { return Row.BlueprintId == BlueprintId; }))
        {
            FDACraftingBlueprintViewModel& Row = Out.Crafting.Blueprints.Emplace_GetRef();
            Row.BlueprintId = BlueprintId;
            Row.SourceQuestId = TEXT("quest.convergence_authority");
            Row.Quantity = 1;
            Row.RequirementReasons.Add(TEXT("Committed First Ascension Fusion blueprint."));
        }
    Out.Crafting.Wallets = Out.CityMetrics.Wallets;
    if (Descriptor.Id == TEXT("city_hud")) Out.CityHUD = FDAUIViewModelFactory::BuildCityHUD(Campaign);
    else if (Descriptor.Id == TEXT("founder_hud"))
        Out.FounderHUD = FDAUIViewModelFactory::BuildFounderHUD(Campaign, FounderSource);
    else if (Descriptor.Id == TEXT("command_hud"))
        Out.CommandHUD = FDAUIViewModelFactory::BuildCommandHUD(Campaign, CommandSource);
    else if (Descriptor.Id == TEXT("card_collection"))
        Out.Collection = FDAUIViewModelFactory::BuildCollection(Campaign);
    else if (Descriptor.Id == TEXT("deck_builder"))
    {
        Out.Deck = FDAUIViewModelFactory::BuildDeck(Campaign);
        Out.Deck.bRulesValid = bDeckRulesValid; Out.Deck.RuleResults = DeckRuleResults;
    }
    else if (Descriptor.Id == TEXT("card_inspect"))
    {
        FGuid Selected;
        FGuid::Parse(StableSelections.FindRef(Descriptor.Id).ToString(), Selected);
        if (!Selected.IsValid() && !Campaign.CollectionState.Instances.IsEmpty())
        {
            TArray<FGuid> InstanceIds;
            Campaign.CollectionState.Instances.GetKeys(InstanceIds);
            Selected = InstanceIds[0];
        }
        if (const FCardInstance* Instance = Campaign.CollectionState.FindInstance(Selected))
        {
            Out.CardInspect.Instance = *Instance;
            Out.CardInspect.SimulationTrace.Add(FString::Printf(
                TEXT("instance=%s definition=%s acquiredTick=%lld source=%d mastery=%d level=%d recovery=%d worldAsset=%s"),
                *Instance->InstanceId.ToString(), *Instance->DefinitionId.ToString(),
                static_cast<long long>(Instance->AcquisitionWorldTick), static_cast<int32>(Instance->AcquisitionSource),
                Instance->MasteryXp, Instance->UpgradeState.Level, static_cast<int32>(Instance->RecoveryState),
                *Instance->WorldAssetId.ToString()));
            Out.CardInspect.SimulationTrace.Add(FString::Printf(TEXT("inspectionRotation=%.3f"),
                StableAxes.FindRef(Descriptor.Id)));
        }
    }
    else if (Descriptor.Id == TEXT("building_inspect"))
    {
        FGuid Selected;
        FGuid::Parse(StableSelections.FindRef(Descriptor.Id).ToString(), Selected);
        if (!Selected.IsValid() && !Campaign.WorldAssets.IsEmpty()) Selected = Campaign.WorldAssets[0].WorldAssetId;
        const FDAFacilityOutput* EconomyOutput = FacilityOutputs.FindByPredicate(
            [Selected](const FDAFacilityOutput& Row) { return Row.WorldAssetId == Selected; });
        Out.Building = FDAUIViewModelFactory::BuildBuildingInspect(Campaign, Selected, EconomyOutput);
    }
    else if (Descriptor.Id == TEXT("citizen_inspect"))
    {
        FName Selected = StableSelections.FindRef(Descriptor.Id);
        if (Selected.IsNone() && !Campaign.LiveSignals.Citizens.IsEmpty()) Selected = Campaign.LiveSignals.Citizens[0].CitizenId;
        Out.Citizen = FDAUIViewModelFactory::BuildCitizenInspect(Campaign, Selected);
    }
    else if (Descriptor.Id == TEXT("diplomacy"))
        Out.Diplomacy = FDAUIViewModelFactory::BuildDiplomacy(Campaign);
    else if (Descriptor.Id == TEXT("treaty_builder"))
    {
        Out.Diplomacy = FDAUIViewModelFactory::BuildDiplomacy(Campaign);
        FName Selected = StableSelections.FindRef(Descriptor.Id);
        if (Selected.IsNone() && !Campaign.WorldState.Diplomacy.Relationships.IsEmpty())
            Selected = Campaign.WorldState.Diplomacy.Relationships[0].RelationshipId;
        Out.Treaty = FDAUIViewModelFactory::BuildTreaty(Campaign, Selected);
    }
    else if (Descriptor.Id == TEXT("world_map")) Out.WorldMap = FDAUIViewModelFactory::BuildWorldMap(Campaign);
    else if (Descriptor.Id == TEXT("conquest_dashboard") || Descriptor.Id == TEXT("leader_resolution"))
        for (const FDAUIConquestRouteAuthorityRecord& Record : Out.LateFeatureAuthority.ConquestRoutes)
        {
            FDAConquestRouteMeterViewModel& Meter = Out.Conquest.RouteMeters.Emplace_GetRef();
            Meter.RouteAssetId = Record.RouteAssetId; Meter.Progress = Record.Progress;
            Meter.CapitalReward = Record.CapitalReward; Meter.InsightReward = Record.InsightReward;
            Meter.Reasons = Record.Reasons;
        }
    else if (Descriptor.Id == TEXT("faction_panel")) Out.Factions = FDAUIViewModelFactory::BuildFactionPanel(Campaign);
    else if (Descriptor.Id == TEXT("research"))
        for (const FName Reason : Out.LateFeatureAuthority.ResearchReasons)
        {
            FDAAuditEligibilitySourceRecord& Row = Out.Research.EligibilityReasons.Emplace_GetRef();
            Row.SourceActionTag = Reason;
        }
    // Objective and core HUD fields remain available to secondary surfaces that render or payload them.
    if (Out.CityHUD.ObjectiveId.IsNone()) Out.CityHUD = FDAUIViewModelFactory::BuildCityHUD(Campaign);
    if (Out.FounderHUD.ObjectiveId.IsNone()) Out.FounderHUD = FDAUIViewModelFactory::BuildFounderHUD(Campaign, FounderSource);
    if (Out.CommandHUD.ObjectiveId.IsNone()) Out.CommandHUD = FDAUIViewModelFactory::BuildCommandHUD(Campaign, CommandSource);
    if (Out.Collection.Instances.IsEmpty()) Out.Collection = FDAUIViewModelFactory::BuildCollection(Campaign);
    if (Out.Deck.Deck.IsEmpty())
    {
        Out.Deck = FDAUIViewModelFactory::BuildDeck(Campaign);
        Out.Deck.bRulesValid = bDeckRulesValid; Out.Deck.RuleResults = DeckRuleResults;
    }
    if (Out.WorldMap.Regions.IsEmpty()) Out.WorldMap = FDAUIViewModelFactory::BuildWorldMap(Campaign);
    if (bLateFeatureScreen && !Out.bAuthoritative)
    {
        for (int32 Index = 0; Index < Descriptor.DataChannels.Num(); ++Index)
            Out.RenderedValues.Add(FeatureError);
    }
    else if (bMissingFounderAuthority || bMissingCommandAuthority)
        for (int32 Index = 0; Index < Descriptor.DataChannels.Num(); ++Index)
            Out.RenderedValues.Add(TEXT("ServiceUnavailable: registered live HUD source is required."));
    for (const FName Channel : Descriptor.DataChannels)
    {
        if ((bLateFeatureScreen && !Out.bAuthoritative)
            || bMissingFounderAuthority || bMissingCommandAuthority) break;
        const FString Value = RenderChannel(Channel, Campaign, Out);
        if (Value.IsEmpty()) return false;
        Out.RenderedValues.Add(Value);
    }
    for (const FName ActionId : Descriptor.CampaignCriticalActions)
    {
        const FDAUICampaignActionDescriptor* Action = CanonicalManifest.FindAction(ActionId);
        FString BindingError;
        const TArray<FDAUIInputBindingDescriptor> Bindings = Action == nullptr
            ? TArray<FDAUIInputBindingDescriptor>()
            : FDAUIGeneratedCache::GetBindingDescriptors(*Action, BindingError);
        int32 PayloadCount = 1;
        for (const FDAUIInputBindingDescriptor& Binding : Bindings)
            PayloadCount = FMath::Max(PayloadCount, Binding.PayloadIndex + 1);
        FDAUIIndexedActionPayloads& Indexed = Out.IndexedActionPayloads.Add(ActionId);
        for (int32 PayloadIndex = 0; PayloadIndex < PayloadCount; ++PayloadIndex)
            Indexed.Values.Add(BuildActionPayload(ActionId, Descriptor, Campaign, Out, StableSelections,
                PayloadIndex, ContentAuthority.Get()));
        if (ActionId == TEXT("city.select_card") || ActionId == TEXT("founder.activate_ability")
            || ActionId == TEXT("command.select_squad"))
            Indexed.ControllerValue = BuildActionPayload(ActionId, Descriptor, Campaign, Out,
                StableSelections, 0, ContentAuthority.Get(), true);
        Out.ActionPayloads.Add(ActionId, Indexed.Values[0]);
    }
    return true;
}

bool UDAUIViewModelProvider::CycleStableSelection(
    const FName ScreenId, const int32 Direction, FString& OutError)
{
    if (Direction != -1 && Direction != 1)
    { OutError = TEXT("Selection cycle direction must be exactly previous or next."); return false; }
    TArray<FName> Ordered;
    if (ScreenId == TEXT("city_hud"))
        for (const FGuid CardId : Campaign.DeckState.GetHand()) Ordered.Add(FName(*CardId.ToString()));
    else if (ScreenId == TEXT("founder_hud")) Ordered = FounderSource.AbilityIds;
    else if (ScreenId == TEXT("command_hud"))
        for (const FDACommandSquadViewModel& Squad : CommandSource.Squads) Ordered.Add(Squad.SquadId);
    else
    { OutError = TEXT("Selection cycle screen is not a canonical multi-record HUD."); return false; }
    Ordered.RemoveAll([](const FName Id){ return Id.IsNone(); });
    if (Ordered.IsEmpty())
    { OutError = TEXT("Selection cycle requires registered authoritative records."); return false; }
    int32 Index = Ordered.IndexOfByKey(StableSelections.FindRef(ScreenId));
    Index = Index == INDEX_NONE ? (Direction > 0 ? 0 : Ordered.Num() - 1)
        : (Index + Direction + Ordered.Num()) % Ordered.Num();
    if (StableSelections.FindRef(ScreenId) == Ordered[Index])
    { OutError.Reset(); return true; }
    StableSelections.Add(ScreenId, Ordered[Index]);
    OutError.Reset(); OnProjectionInvalidated.Broadcast(); return true;
}

bool UDAUIViewModelProvider::ApplySelectionCommand(const FName CommandId,
    const FName SourceScreenId, const FString& PayloadJson, FString& OutError)
{
    FDAUIAuthoritativeCommandPayload Payload;
    if (!FDAUIAuthoritativeCommandPayload::ParseAndValidate(
        CommandId, PayloadJson, Payload, OutError)) return false;
    if (CommandId == TEXT("command.city.select_card")
        || CommandId == TEXT("command.collection.inspect_card")
        || CommandId == TEXT("command.card.rotate"))
    {
        if (Campaign.CollectionState.FindInstance(Payload.CardInstanceId) == nullptr)
        { OutError = TEXT("Selection does not resolve an authoritative card instance."); return false; }
        const FName TargetScreen = CommandId == TEXT("command.city.select_card")
            ? TEXT("city_hud") : TEXT("card_inspect");
        const FName Selection(*Payload.CardInstanceId.ToString());
        if (CommandId == TEXT("command.city.select_card")
            && StableSelections.FindRef(TargetScreen) == Selection)
        { OutError.Reset(); return true; }
        StableSelections.Add(TargetScreen, Selection);
        if (CommandId == TEXT("command.card.rotate"))
            StableAxes.Add(TargetScreen, StableAxes.FindRef(TargetScreen) + Payload.Axis);
    }
    else if (CommandId == TEXT("command.building.track"))
    {
        if (!Campaign.WorldAssets.ContainsByPredicate([&Payload](const FDAWorldAssetRecord& Row)
            { return Row.WorldAssetId == Payload.WorldAssetId; }))
        { OutError = TEXT("Selection does not resolve an authoritative WorldAsset."); return false; }
        const FName Selection(*Payload.WorldAssetId.ToString());
        if (StableSelections.FindRef(TEXT("building_inspect")) == Selection)
        { OutError = TEXT("WorldAsset is already tracked; no projection mutation was performed."); return false; }
        StableSelections.Add(TEXT("building_inspect"), Selection);
    }
    else if (CommandId == TEXT("command.quest.track"))
    {
        const bool bFound = Campaign.NarrativeState.QuestStates.ContainsByPredicate(
            [&Payload](const FDAQuestSaveState& Row)
            { return Row.QuestId == Payload.QuestId
                || FName(*(Row.QuestId.ToString() + TEXT(".") + Row.CurrentNodeId.ToString())) == Payload.QuestId; });
        if (!bFound) { OutError = TEXT("Selection does not resolve an authoritative quest."); return false; }
        if (StableSelections.FindRef(TEXT("quest_journal")) == Payload.QuestId)
        { OutError = TEXT("Quest is already tracked; no projection mutation was performed."); return false; }
        StableSelections.Add(TEXT("quest_journal"), Payload.QuestId);
    }
    else if (CommandId == TEXT("command.history.inspect"))
    {
        if (!Campaign.HistoryTags.Contains(Payload.HistoryRecordId))
        { OutError = TEXT("Selection does not resolve an authoritative history record."); return false; }
        if (StableSelections.FindRef(TEXT("history_timeline")) == Payload.HistoryRecordId)
        { OutError = TEXT("History record is already selected; no projection mutation was performed."); return false; }
        StableSelections.Add(TEXT("history_timeline"), Payload.HistoryRecordId);
    }
    else if (CommandId == TEXT("command.metrics.inspect"))
    {
        if (StableSelections.FindRef(TEXT("city_metrics")) == Payload.MetricId)
        { OutError = TEXT("Metric is already selected; no projection mutation was performed."); return false; }
        StableSelections.Add(TEXT("city_metrics"), Payload.MetricId);
    }
    else if (CommandId == TEXT("command.command.select_squad"))
        StableSelections.Add(TEXT("command_hud"), Payload.SquadId);
    else
    { OutError = TEXT("Projection authority has no selection route for this command."); return false; }
    OutError.Reset();
    OnProjectionInvalidated.Broadcast();
    return true;
}

bool UDAUIViewModelProvider::BindAuthoritativeWorld(UDAWorldStateSubsystem* InWorld)
{
    if (WorldAuthority.IsValid() && CampaignCommittedHandle.IsValid())
        WorldAuthority->OnWorldTickStateCommitted.Remove(CampaignCommittedHandle);
    WorldAuthority = InWorld; CampaignCommittedHandle.Reset();
    ContentAuthority.Reset(); FeatureAuthority.Reset();
    if (InWorld == nullptr) { bHasAuthority = false; return false; }
    if (UGameInstance* GameInstance = InWorld->GetGameInstance())
    {
        ContentAuthority = GameInstance->GetSubsystem<UDAContentRegistrySubsystem>();
        FeatureAuthority = GameInstance->GetSubsystem<UDAUIAuthoritativeFeatureRegistrySubsystem>();
    }
    SetAuthoritativeSnapshot(InWorld->GetPersistentCampaign());
    CampaignCommittedHandle = InWorld->OnWorldTickStateCommitted.AddUObject(
        this, &UDAUIViewModelProvider::HandleCampaignCommitted);
    return true;
}

void UDAUIViewModelProvider::HandleCampaignCommitted(TSharedRef<const FDACampaignSnapshot> Snapshot)
{
    Campaign = *Snapshot; bHasAuthority = true;
    if (RuntimePlayerController.IsValid()) RefreshRuntimeSources();
    else OnProjectionInvalidated.Broadcast();
}

void UDAUIViewModelProvider::BeginDestroy()
{
    BindRuntimePlayer(nullptr);
    if (WorldAuthority.IsValid() && CampaignCommittedHandle.IsValid())
        WorldAuthority->OnWorldTickStateCommitted.Remove(CampaignCommittedHandle);
    CampaignCommittedHandle.Reset(); WorldAuthority.Reset();
    if (AccessibilityAuthority.IsValid() && AccessibilityChangedHandle.IsValid())
        AccessibilityAuthority->OnSettingsChanged.Remove(AccessibilityChangedHandle);
    AccessibilityChangedHandle.Reset(); AccessibilityAuthority.Reset();
    Super::BeginDestroy();
}

bool UDAUIViewModelProvider::BuildOverlay(const FDAUIOverlayDescriptor& Descriptor,
    const bool bColorIndependentMarkers, FDAUIOverlayReadModel& Out) const
{
    if (!bHasAuthority) return false;
    Out = {}; Out.OverlayId = Descriptor.Id; Out.bAuthoritative = true;
    Out.MarkerShape = Descriptor.MarkerShape; Out.bColorIndependentMarkers = bColorIndependentMarkers;
    if (Descriptor.Id == TEXT("power") || Descriptor.Id == TEXT("water") || Descriptor.Id == TEXT("data"))
    {
        const EDACampaignUtilityKind Utility = Descriptor.Id == TEXT("power") ? EDACampaignUtilityKind::Power
            : Descriptor.Id == TEXT("water") ? EDACampaignUtilityKind::Water : EDACampaignUtilityKind::Data;
        Out.Kind = Descriptor.Id == TEXT("power") ? EDAUIOverlayKind::Power
            : Descriptor.Id == TEXT("water") ? EDAUIOverlayKind::Water : EDAUIOverlayKind::Data;
        Out.UtilitySignals = Campaign.LiveSignals.UtilitySignals.FilterByPredicate(
            [Utility](const FDACampaignUtilitySignal& Signal) { return Signal.Utility == Utility; });
        for (const FDACampaignUtilitySignal& Signal : Out.UtilitySignals)
            Out.RenderedValues.Add(FString::Printf(TEXT("asset=%s utility=%d supply=%d marker=%s"),
                *Signal.WorldAssetId.ToString(), static_cast<int32>(Signal.Utility),
                static_cast<int32>(Signal.Supply), *Out.MarkerShape.ToString()));
    }
    else if (Descriptor.Id == TEXT("employment"))
    {
        Out.Kind = EDAUIOverlayKind::Employment;
        Out.Citizens = Campaign.LiveSignals.Citizens;
        for (const FDACampaignCitizenSignal& Citizen : Out.Citizens)
        {
            FDAUIEmploymentOverlayRow& Row = Out.EmploymentRows.Emplace_GetRef();
            Row.CitizenId = Citizen.CitizenId; Row.JobId = Citizen.JobId;
            const FDACampaignJobAssignmentSignal* Assignment = Campaign.LiveSignals.JobAssignments.FindByPredicate(
                [&Citizen](const FDACampaignJobAssignmentSignal& Value) { return Value.CitizenId == Citizen.CitizenId; });
            if (Assignment != nullptr) Row.FacilityWorldAssetId = Assignment->FacilityWorldAssetId;
            const FDACampaignJobOpeningSignal* Opening = Campaign.LiveSignals.JobOpenings.FindByPredicate(
                [&Row](const FDACampaignJobOpeningSignal& Value) { return Value.JobId == Row.JobId
                    && (!Row.FacilityWorldAssetId.IsValid() || Value.FacilityWorldAssetId == Row.FacilityWorldAssetId); });
            if (Opening != nullptr) Row.OpenPositions = Opening->OpenPositions;
            Row.Reason = Row.JobId.IsNone() ? TEXT("Unassigned: no authoritative job assignment.")
                : Assignment == nullptr ? TEXT("Citizen job signal has no matching assignment record.")
                : TEXT("Assignment resolves to the authoritative facility.");
            Out.RenderedValues.Add(FString::Printf(TEXT("citizen=%s job=%s facility=%s openPositions=%d reason=%s"),
                *Row.CitizenId.ToString(), *Row.JobId.ToString(), *Row.FacilityWorldAssetId.ToString(),
                Row.OpenPositions, *Row.Reason));
        }
    }
    else if (Descriptor.Id == TEXT("housing"))
    {
        Out.Kind = EDAUIOverlayKind::Housing;
        Out.Citizens = Campaign.LiveSignals.Citizens;
        for (const FDACampaignCitizenSignal& Citizen : Out.Citizens)
        {
            FDAUIHousingOverlayRow& Row = Out.HousingRows.Emplace_GetRef();
            Row.CitizenId = Citizen.CitizenId; Row.HomeWorldAssetId = Citizen.HomeWorldAssetId;
            Row.Reason = Row.HomeWorldAssetId.IsValid() ? TEXT("Authoritative home assignment.")
                : TEXT("Unhoused: home world asset is absent.");
            Out.RenderedValues.Add(FString::Printf(TEXT("citizen=%s home=%s reason=%s"),
                *Row.CitizenId.ToString(), *Row.HomeWorldAssetId.ToString(), *Row.Reason));
        }
    }
    else if (Descriptor.Id == TEXT("happiness"))
    {
        Out.Kind = EDAUIOverlayKind::Happiness; Out.Citizens = Campaign.LiveSignals.Citizens;
        for (const FDACampaignCitizenSignal& Citizen : Out.Citizens)
        {
            FDAUICitizenValueOverlayRow& Row = Out.CitizenValueRows.Emplace_GetRef();
            Row.CitizenId = Citizen.CitizenId;
            Row.Value = Campaign.SynaraState.CitizenRelationships.FindRef(Citizen.CitizenId);
            Row.Reasons = Campaign.SynaraState.CitizenRelationshipReasons.FilterByPredicate(
                [&Citizen](const FDASynaraValueReason& Reason) { return Reason.SubjectId == Citizen.CitizenId; });
            TArray<FString> Reasons;
            for (const FDASynaraValueReason& Reason : Row.Reasons)
                Reasons.Add(FString::Printf(TEXT("%s delta=%.2f result=%.2f tick=%lld"),
                    *Reason.ActionId.ToString(), Reason.Delta, Reason.Result, static_cast<long long>(Reason.WorldTick)));
            Out.RenderedValues.Add(FString::Printf(TEXT("citizen=%s happiness=%.2f reasons=%s"),
                *Row.CitizenId.ToString(), Row.Value, *FString::Join(Reasons, TEXT(","))));
        }
    }
    else if (Descriptor.Id == TEXT("dependency"))
    {
        Out.Kind = EDAUIOverlayKind::Dependency; Out.Citizens = Campaign.LiveSignals.Citizens;
        Out.DependencyValue = Campaign.SynaraState.Dependency;
        Out.Reasons = Campaign.SynaraState.DependencyReasons;
        for (const FDASynaraValueReason& Reason : Out.Reasons)
            Out.RenderedValues.Add(FString::Printf(
                TEXT("dependency=%.2f reason=%s subject=%s baseline=%.2f delta=%.2f result=%.2f tick=%lld"),
                Out.DependencyValue, *Reason.ActionId.ToString(), *Reason.SubjectId.ToString(),
                Reason.Baseline, Reason.Delta, Reason.Result, static_cast<long long>(Reason.WorldTick)));
    }
    else
    {
        Out.Kind = EDAUIOverlayKind::Adjacency; Out.WorldAssets = Campaign.WorldAssets;
        for (int32 From = 0; From < Out.WorldAssets.Num(); ++From)
            for (int32 To = From + 1; To < Out.WorldAssets.Num(); ++To)
            {
                FDAUIAdjacencyOverlayEdge& Edge = Out.AdjacencyEdges.Emplace_GetRef();
                Edge.FromWorldAssetId = Out.WorldAssets[From].WorldAssetId;
                Edge.ToWorldAssetId = Out.WorldAssets[To].WorldAssetId;
                Edge.FromGrid = Out.WorldAssets[From].GridOrigin; Edge.ToGrid = Out.WorldAssets[To].GridOrigin;
                const int32 ManhattanDistance = FMath::Abs(Edge.FromGrid.X - Edge.ToGrid.X)
                    + FMath::Abs(Edge.FromGrid.Y - Edge.ToGrid.Y);
                Edge.bAdjacent = ManhattanDistance == 1;
                Edge.Reason = FString::Printf(TEXT("Manhattan distance %d %s adjacency requirement."),
                    ManhattanDistance, Edge.bAdjacent ? TEXT("meets") : TEXT("does not meet"));
                Out.RenderedValues.Add(FString::Printf(TEXT("from=%s grid=(%d,%d) to=%s grid=(%d,%d) adjacent=%s reason=%s"),
                    *Edge.FromWorldAssetId.ToString(), Edge.FromGrid.X, Edge.FromGrid.Y,
                    *Edge.ToWorldAssetId.ToString(), Edge.ToGrid.X, Edge.ToGrid.Y,
                    Edge.bAdjacent ? TEXT("true") : TEXT("false"), *Edge.Reason));
            }
    }
    if (Out.RenderedValues.IsEmpty())
        Out.RenderedValues.Add(FString::Printf(TEXT("%s: authoritative projection contains no matching records."),
            *Descriptor.Id.ToString()));
    return true;
}
