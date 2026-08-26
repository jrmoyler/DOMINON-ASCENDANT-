#include "Commands/DAUIAuthoritativeFeatureSubsystem.h"

#include "Ascension/DAAscensionSystem.h"
#include "Campaign/DADaxtonCampaignState.h"
#include "Commands/DAUICommandEndpoint.h"
#include "Conquest/DAConquestSystem.h"
#include "Engine/GameInstance.h"
#include "Misc/Crc.h"
#include "Narrative/DAFirstHourCampaignCoordinatorSubsystem.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
    FGuid StableActionId(const FString& Seed)
    {
        return FGuid(FCrc::StrCrc32(*(Seed + TEXT("|a"))),
            FCrc::StrCrc32(*(Seed + TEXT("|b"))),
            FCrc::StrCrc32(*(Seed + TEXT("|c"))),
            FCrc::StrCrc32(*(Seed + TEXT("|d"))));
    }

    FGuid RouteId(const EDAForgeweaveRoute Route)
    {
        return StableActionId(FString::Printf(TEXT("ui.conquest.route.%d"),
            static_cast<int32>(Route)));
    }

    bool ResolveRoute(const FGuid Id, EDAForgeweaveRoute& OutRoute)
    {
        for (const EDAForgeweaveRoute Route : {EDAForgeweaveRoute::Force,
            EDAForgeweaveRoute::Economic, EDAForgeweaveRoute::Influence,
            EDAForgeweaveRoute::Alliance})
        {
            if (RouteId(Route) == Id)
            {
                OutRoute = Route;
                return true;
            }
        }
        return false;
    }

    bool ResolveLeaderState(const FName ResolutionId, EDADaxtonLeaderState& OutState)
    {
        static const TMap<FName, EDADaxtonLeaderState> States = {
            {TEXT("leader.governor"), EDADaxtonLeaderState::Governor},
            {TEXT("leader.industrial_advisor"), EDADaxtonLeaderState::IndustrialAdvisor},
            {TEXT("leader.allied_forge_lord"), EDADaxtonLeaderState::AlliedForgeLord},
            {TEXT("leader.exile"), EDADaxtonLeaderState::Exile},
            {TEXT("leader.prisoner"), EDADaxtonLeaderState::Prisoner},
            {TEXT("leader.dead"), EDADaxtonLeaderState::Dead}};
        const EDADaxtonLeaderState* Found = States.Find(ResolutionId);
        if (Found == nullptr) return false;
        OutState = *Found;
        return true;
    }

    FName LeaderResolutionId(const EDADaxtonLeaderState State)
    {
        switch (State)
        {
        case EDADaxtonLeaderState::Governor: return TEXT("leader.governor");
        case EDADaxtonLeaderState::IndustrialAdvisor: return TEXT("leader.industrial_advisor");
        case EDADaxtonLeaderState::AlliedForgeLord: return TEXT("leader.allied_forge_lord");
        case EDADaxtonLeaderState::Exile: return TEXT("leader.exile");
        case EDADaxtonLeaderState::Prisoner: return TEXT("leader.prisoner");
        case EDADaxtonLeaderState::Dead: return TEXT("leader.dead");
        default: return NAME_None;
        }
    }

    bool ResolveFounderInteraction(const FName InteractionId, FName& OutQuestId,
        EDAFirstHourPlayerAction& OutAction)
    {
        struct FInteraction
        {
            FName QuestId;
            EDAFirstHourPlayerAction Action;
        };
        static const TMap<FName, FInteraction> Interactions = {
            {TEXT("interaction.founder_hall.reach"),
                {TEXT("quest.wake_the_hall"), EDAFirstHourPlayerAction::ReachFounderHall}},
            {TEXT("interaction.founder_hall.restore_power"),
                {TEXT("quest.wake_the_hall"), EDAFirstHourPlayerAction::RestoreFounderHallPower}},
            {TEXT("interaction.founder_hall.activate_core"),
                {TEXT("quest.wake_the_hall"), EDAFirstHourPlayerAction::ActivateFounderHallCore}},
            {TEXT("interaction.founder_hall.inspect_markings"),
                {TEXT("quest.wake_the_hall"), EDAFirstHourPlayerAction::InspectCustodianMarkings}},
            {TEXT("interaction.adaptive_habitat.inspect"),
                {TEXT("quest.a_place_to_stay"), EDAFirstHourPlayerAction::InspectAdaptiveHabitatCard}},
            {TEXT("interaction.nia.speak"),
                {TEXT("quest.nia_needs_a_job"), EDAFirstHourPlayerAction::SpeakToNia}},
            {TEXT("interaction.utility_tunnel.enter"),
                {TEXT("quest.signal_in_foundation"), EDAFirstHourPlayerAction::EnterUtilityTunnel}},
            {TEXT("interaction.eden_basin.reach"),
                {TEXT("quest.basin_speaks"), EDAFirstHourPlayerAction::ReachEdenBasin}}};
        const FInteraction* Found = Interactions.Find(InteractionId);
        if (Found == nullptr) return false;
        OutQuestId = Found->QuestId;
        OutAction = Found->Action;
        return true;
    }

    bool IsApplied(const EDAFirstHourCampaignResult Result)
    {
        return Result == EDAFirstHourCampaignResult::Applied
            || Result == EDAFirstHourCampaignResult::AlreadyApplied;
    }
}

void UDAUIAuthoritativeFeatureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDAWorldStateSubsystem>();
    Collection.InitializeDependency<UDAFirstHourCampaignCoordinatorSubsystem>();
    Collection.InitializeDependency<UDAUIAuthoritativeFeatureRegistrySubsystem>();
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        WorldStateSubsystem = GameInstance->GetSubsystem<UDAWorldStateSubsystem>();
        FirstHourCoordinator =
            GameInstance->GetSubsystem<UDAFirstHourCampaignCoordinatorSubsystem>();
        if (UDAUIAuthoritativeFeatureRegistrySubsystem* Registry =
            GameInstance->GetSubsystem<UDAUIAuthoritativeFeatureRegistrySubsystem>())
        {
            FString Error;
            if (!Registry->RegisterAuthoritativeService(this, Error))
            {
                UE_LOG(LogTemp, Error, TEXT("UI feature authority registration failed: %s"), *Error);
            }
        }
    }
}

void UDAUIAuthoritativeFeatureSubsystem::Deinitialize()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UDAUIAuthoritativeFeatureRegistrySubsystem* Registry =
            GameInstance->GetSubsystem<UDAUIAuthoritativeFeatureRegistrySubsystem>())
        {
            Registry->UnregisterAuthoritativeService(this);
        }
    }
    FirstHourCoordinator.Reset();
    WorldStateSubsystem.Reset();
    Super::Deinitialize();
}

bool UDAUIAuthoritativeFeatureSubsystem::ExecuteAuthoritativeUICommand(
    const FName CommandId, const FName SourceScreenId, const FString& PayloadJson,
    FString& OutError)
{
    (void)SourceScreenId;
    FDAUIAuthoritativeCommandPayload Payload;
    if (!FDAUIAuthoritativeCommandPayload::ParseAndValidate(
        CommandId, PayloadJson, Payload, OutError))
    {
        return false;
    }
    if (!WorldStateSubsystem.IsValid())
    {
        OutError = TEXT("ServiceUnavailable: canonical campaign owner is unavailable.");
        return false;
    }
    UDAWorldStateSubsystem& World = *WorldStateSubsystem.Get();
    const int64 Tick = World.GetCurrentWorldTick();

    if (CommandId == TEXT("command.research.start"))
    {
        if (!FirstHourCoordinator.IsValid()
            || Payload.ResearchId != TEXT("research.replacement_model"))
        {
            OutError = TEXT("Research command does not resolve the authored first-hour project.");
            return false;
        }
        const FGuid ActionId = StableActionId(Payload.ResearchId.ToString());
        if (!IsApplied(FirstHourCoordinator->SubmitResearchAction(ActionId)))
        {
            OutError = TEXT("First-hour coordinator rejected the research action.");
            return false;
        }
    }
    else if (CommandId == TEXT("command.conquest.choose_route"))
    {
        EDAForgeweaveRoute Route = EDAForgeweaveRoute::Force;
        if (!ResolveRoute(Payload.RouteAssetId, Route)
            || !World.CompleteForgeweaveConquestRoute(Payload.RouteAssetId, Route, OutError))
        {
            if (OutError.IsEmpty()) OutError = TEXT("Conquest route does not resolve canonical authority.");
            return false;
        }
    }
    else if (CommandId == TEXT("command.leader.confirm_resolution"))
    {
        EDADaxtonLeaderState State = EDADaxtonLeaderState::Governor;
        const FGuid ActionId = StableActionId(FString::Printf(TEXT("%s|%lld"),
            *Payload.ResolutionId.ToString(), static_cast<long long>(Tick)));
        if (!ResolveLeaderState(Payload.ResolutionId, State)
            || !World.ResolveDaxtonLeaderState(ActionId, State, OutError))
        {
            if (OutError.IsEmpty()) OutError = TEXT("Leader resolution is not currently eligible.");
            return false;
        }
    }
    else if (CommandId == TEXT("command.ascension.claim"))
    {
        if (Payload.RewardId != TEXT("reward.ascension.first")
            || !World.CompleteFirstAscension(
                StableActionId(TEXT("reward.ascension.first")), OutError))
        {
            if (OutError.IsEmpty()) OutError = TEXT("First Ascension reward is not currently eligible.");
            return false;
        }
    }
    else if (CommandId == TEXT("command.history.inspect"))
    {
        if (!World.GetPersistentCampaign().HistoryTags.Contains(Payload.HistoryRecordId))
        {
            OutError = TEXT("History record does not resolve canonical campaign history.");
            return false;
        }
    }
    else if (CommandId == TEXT("command.metrics.inspect"))
    {
        static const TSet<FName> Metrics = {TEXT("metric.capital"), TEXT("metric.insight"),
            TEXT("metric.influence"), TEXT("metric.population"), TEXT("metric.dependency"),
            TEXT("metric.conquest.force"), TEXT("metric.conquest.economic"),
            TEXT("metric.conquest.influence"), TEXT("metric.conquest.alliance"),
            TEXT("metric.convergence")};
        if (!Metrics.Contains(Payload.MetricId))
        {
            OutError = TEXT("Metric id does not resolve a canonical campaign aggregate.");
            return false;
        }
    }
    else if (CommandId == TEXT("command.founder.interact"))
    {
        FName QuestId;
        EDAFirstHourPlayerAction Action = EDAFirstHourPlayerAction::None;
        if (!FirstHourCoordinator.IsValid()
            || !ResolveFounderInteraction(Payload.InteractionId, QuestId, Action)
            || !IsApplied(FirstHourCoordinator->SubmitPlayerAction(
                QuestId, Action, NAME_None, FGuid(), FGuid())))
        {
            OutError = TEXT("Founder interaction is not valid for the canonical first-hour state.");
            return false;
        }
    }
    else
    {
        OutError = TEXT("Authoritative feature service does not own this command.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UDAUIAuthoritativeFeatureSubsystem::CaptureAuthoritativeUIState(
    FDAUIAuthoritativeFeatureSnapshot& OutState, FString& OutError) const
{
    OutState = {};
    if (!WorldStateSubsystem.IsValid())
    {
        OutError = TEXT("ServiceUnavailable: canonical campaign owner is unavailable.");
        return false;
    }
    const FDACampaignSnapshot& Campaign = WorldStateSubsystem->GetPersistentCampaign();
    if (!Campaign.Validate(OutError)) return false;

    OutState.Revision = Campaign.NarrativeState.MutationRevision
        + Campaign.LiveSignals.MutationRevision + Campaign.ConquestState.MutationRevision
        + Campaign.DaxtonState.CanonicalActionRecords.Num()
        + Campaign.AscensionState.ReplicationRecords.Num()
        + (Campaign.AscensionState.bForgeweaveAscended ? 1 : 0);
    OutState.ResearchIds = {TEXT("research.replacement_model")};
    for (const FDAAuditEligibilitySourceRecord& Source :
        Campaign.NarrativeState.AuditEligibilitySourceRecords)
    {
        OutState.ResearchReasons.AddUnique(Source.SourceActionTag);
    }
    for (const FDAQuestContentUnlockRecord& Unlock :
        Campaign.NarrativeState.QuestContentUnlockRecords)
    {
        OutState.ResearchReasons.AddUnique(Unlock.ActionId);
    }

    const double Meters[] = {Campaign.ConquestState.MilitarySovereignty,
        Campaign.ConquestState.EconomicAutonomy, Campaign.ConquestState.CivicLegitimacy,
        Campaign.ConquestState.AllianceReadiness};
    const FName RouteReasons[] = {TEXT("meter.military_sovereignty"),
        TEXT("meter.economic_autonomy"), TEXT("meter.civic_legitimacy"),
        TEXT("meter.alliance_readiness")};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FDAUIConquestRouteAuthorityRecord& Row = OutState.ConquestRoutes.Emplace_GetRef();
        Row.RouteAssetId = RouteId(static_cast<EDAForgeweaveRoute>(Index));
        Row.Progress = static_cast<float>(Index == 3 ? Meters[Index] : 100.0 - Meters[Index]);
        Row.Reasons.Add(RouteReasons[Index]);
        if (Campaign.ConquestState.bForgeweaveResolved
            && static_cast<int32>(Campaign.ConquestState.ResolvedRoute) == Index)
        {
            Row.Progress = 100.f;
            Row.Reasons.Add(TEXT("conquest.route.resolved"));
        }
    }

    for (int32 Index = 0; Index < 6; ++Index)
    {
        const EDADaxtonLeaderState State = static_cast<EDADaxtonLeaderState>(Index);
        FDAUILeaderResolutionAuthorityRecord& Row = OutState.LeaderResolutions.Emplace_GetRef();
        Row.ResolutionId = LeaderResolutionId(State);
        Row.LeaderId = TEXT("leader.daxton");
        FString Reason;
        const bool bEligible = FDADaxtonAuthorityValidator::CanResolveLeaderState(
            State, Campaign, Reason);
        Row.Status = Campaign.DaxtonState.bLeaderResolved
            ? (Campaign.DaxtonState.LeaderState == State ? FName(TEXT("resolved"))
                : FName(TEXT("unavailable")))
            : (bEligible ? FName(TEXT("eligible")) : FName(TEXT("locked")));
        if (!Reason.IsEmpty()) Row.Reasons.Add(FName(*Reason));
    }

    FDAUIAscensionRewardAuthorityRecord& Ascension = OutState.AscensionRewards.Emplace_GetRef();
    Ascension.RewardId = TEXT("reward.ascension.first");
    if (Campaign.AscensionState.bForgeweaveAscended)
    {
        Ascension.Status = TEXT("claimed");
    }
    else
    {
        FDACampaignSnapshot Preview = Campaign;
        FString Reason;
        Ascension.Status = FDAAscensionSystem::ApplyFirstAscension(
            StableActionId(TEXT("ui.ascension.preview")), Preview, Reason)
            ? FName(TEXT("eligible")) : FName(TEXT("locked"));
        if (!Reason.IsEmpty()) Ascension.Reasons.Add(FName(*Reason));
    }

    OutState.HistoryTags = Campaign.HistoryTags;
    OutState.MetricValues = {
        {TEXT("metric.capital"), Campaign.CitySimulationState.Wallet.Capital},
        {TEXT("metric.insight"), Campaign.CitySimulationState.Wallet.Insight},
        {TEXT("metric.influence"), Campaign.CitySimulationState.Wallet.Influence},
        {TEXT("metric.population"), static_cast<double>(Campaign.CitySimulationState.Population)},
        {TEXT("metric.dependency"), Campaign.SynaraState.Dependency},
        {TEXT("metric.conquest.force"), Campaign.ConquestState.MilitarySovereignty},
        {TEXT("metric.conquest.economic"), Campaign.ConquestState.EconomicAutonomy},
        {TEXT("metric.conquest.influence"), Campaign.ConquestState.CivicLegitimacy},
        {TEXT("metric.conquest.alliance"), Campaign.ConquestState.AllianceReadiness},
        {TEXT("metric.convergence"), Campaign.AscensionState.ConvergenceAuthority}};
    OutState.FounderInteractionIds = {TEXT("interaction.founder_hall.reach"),
        TEXT("interaction.founder_hall.restore_power"),
        TEXT("interaction.founder_hall.activate_core"),
        TEXT("interaction.founder_hall.inspect_markings"),
        TEXT("interaction.adaptive_habitat.inspect"), TEXT("interaction.nia.speak"),
        TEXT("interaction.utility_tunnel.enter"), TEXT("interaction.eden_basin.reach")};
    OutError.Reset();
    return true;
}
