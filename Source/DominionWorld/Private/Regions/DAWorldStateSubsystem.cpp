#include "Regions/DAWorldStateSubsystem.h"

#include "AI/DAForgeweaveStrategy.h"
#include "Adjacency/DAAdjacencySubsystem.h"
#include "Ascension/DAAscensionSystem.h"
#include "Boss/Daxton/DADaxtonEncounter.h"
#include "Cards/DACollectionState.h"
#include "City/DACityGridSubsystem.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Construction/DAConstructionComponent.h"
#include "Citizens/DAJobSystem.h"
#include "Citizens/DAMigrationSystem.h"
#include "Economy/DAEconomySubsystem.h"
#include "Factions/DASystemicPressureSystem.h"
#include "Content/DARegionalCrisisContent.h"
#include "Content/DARegionalCrisisContentRegistrySubsystem.h"
#include "Conquest/DAConquestSystem.h"
#include "Narrative/DAFoundryShortageRuntime.h"
#include "Narrative/DARegionalCampaignCoordinatorSubsystem.h"
#include "Misc/Crc.h"
#include "Subsystems/SubsystemCollection.h"
#include "Time/DASimulationClockSubsystem.h"

namespace
{
    FGuid MakeDeterministicCampaignGuid(const FString& Seed)
    {
        return FGuid(
            FCrc::StrCrc32(*(Seed + TEXT("|A"))),
            FCrc::StrCrc32(*(Seed + TEXT("|B"))),
            FCrc::StrCrc32(*(Seed + TEXT("|C"))),
            FCrc::StrCrc32(*(Seed + TEXT("|D"))));
    }

    void ProjectCitySimulationState(FDACampaignSnapshot& Campaign, const bool bIncrementRevision)
    {
        FDACampaignLiveSignalState& Signals = Campaign.LiveSignals;
        const int64 PreviousRevision = Signals.MutationRevision;
        const FDACitySimulationState& City = Campaign.CitySimulationState;
        Signals.Population = City.Population;
        Signals.Capital = City.Wallet.Capital;
        Signals.Insight = City.Wallet.Insight;
        Signals.Influence = City.Wallet.Influence;
        Signals.ResolvedDevelopmentCycles = City.ResolvedDevelopmentCycles;
        Signals.Citizens.Reset(City.Citizens.Num());
        for (const FDACitizenRecord& Citizen : City.Citizens)
        {
            FDACampaignCitizenSignal& Signal = Signals.Citizens.Emplace_GetRef();
            Signal.CitizenId = Citizen.CitizenId;
            Signal.CityId = Citizen.CityId;
            Signal.HomeWorldAssetId = Citizen.HomeAssetId;
            Signal.JobId = Citizen.JobId;
        }
        Signals.Citizens.Sort([](const auto& A, const auto& B)
            { return A.CitizenId.LexicalLess(B.CitizenId); });
        Signals.JobOpenings.Reset(City.JobOpenings.Num());
        for (const FDAJobOpening& Opening : City.JobOpenings)
        {
            FDACampaignJobOpeningSignal& Signal = Signals.JobOpenings.Emplace_GetRef();
            Signal.JobId = Opening.JobId;
            Signal.CityId = Opening.CityId;
            Signal.FacilityWorldAssetId = Opening.FacilityWorldAssetId;
            Signal.OpenPositions = Opening.OpenPositions;
        }
        Signals.JobOpenings.Sort([](const auto& A, const auto& B)
        {
            return A.JobId != B.JobId ? A.JobId.LexicalLess(B.JobId)
                : A.FacilityWorldAssetId.ToString().Compare(B.FacilityWorldAssetId.ToString()) < 0;
        });
        Signals.JobAssignments.Reset(City.JobAssignments.Num());
        for (const FDAJobAssignment& Assignment : City.JobAssignments)
        {
            FDACampaignJobAssignmentSignal& Signal = Signals.JobAssignments.Emplace_GetRef();
            Signal.CitizenId = Assignment.CitizenId;
            Signal.JobId = Assignment.JobId;
            Signal.FacilityWorldAssetId = Assignment.FacilityWorldAssetId;
        }
        Signals.JobAssignments.Sort([](const auto& A, const auto& B)
            { return A.CitizenId.LexicalLess(B.CitizenId); });
        Signals.UtilitySignals = City.UtilitySignals;
        Signals.UtilitySignals.Sort([](const auto& A, const auto& B)
        {
            return A.WorldAssetId != B.WorldAssetId
                ? A.WorldAssetId.ToString().Compare(B.WorldAssetId.ToString()) < 0
                : static_cast<uint8>(A.Utility) < static_cast<uint8>(B.Utility);
        });
        Signals.MutationRevision = bIncrementRevision ? PreviousRevision + 1 : PreviousRevision;
    }

    EDAFacilityType FacilityTypeForCard(const EDACardType CardType)
    {
        switch (CardType)
        {
        case EDACardType::Residential: return EDAFacilityType::Residential;
        case EDACardType::Retail: return EDAFacilityType::Retail;
        case EDACardType::Office: return EDAFacilityType::Office;
        case EDACardType::Research: return EDAFacilityType::Research;
        case EDACardType::Industrial: return EDAFacilityType::Industrial;
        case EDACardType::Defense: return EDAFacilityType::Defense;
        case EDACardType::Wonder: return EDAFacilityType::Wonder;
        default: return EDAFacilityType::Infrastructure;
        }
    }

    FDAFacilityContext MakeFacilityContext(
        const FDAWorldAssetRecord& Asset,
        const UDA_CardDefinition& Definition)
    {
        FDAFacilityContext Facility;
        Facility.AssetRecord = Asset;
        Facility.FacilityType = FacilityTypeForCard(Definition.CardType);
        Definition.TryGetBaseCapitalPerCycle(Facility.BaseOutput.Capital);
        Definition.TryGetBaseInsightPerCycle(Facility.BaseOutput.Insight);
        Definition.TryGetBaseInfluencePerCycle(Facility.BaseOutput.Influence);
        int32 DeploymentCapital = 0;
        Definition.TryGetDeploymentCapital(DeploymentCapital);
        Facility.DeploymentCapital = DeploymentCapital;
        float AuthoredMaintenance = 0.f;
        Definition.TryGetMaintenanceCapitalPerCycle(AuthoredMaintenance);
        Facility.AuthoredMaintenanceCapitalPerCycle = AuthoredMaintenance;
        return Facility;
    }

    bool SynchronizeFacilityRecords(FDACampaignSnapshot& Campaign, FString& OutError)
    {
        for (FDAFacilityContext& Facility : Campaign.CitySimulationState.Facilities)
        {
            const FDAWorldAssetRecord* Asset = Campaign.FindWorldAssetRecord(
                Facility.AssetRecord.WorldAssetId);
            if (Asset == nullptr || Asset->CardDefinitionId != Facility.AssetRecord.CardDefinitionId)
            {
                OutError = TEXT("Persisted facility lost its exact canonical WorldAsset identity.");
                return false;
            }
            Facility.AssetRecord = *Asset;
        }
        return true;
    }

    bool HasReached(const EDATravelHandoffStage Current, const EDATravelHandoffStage Required)
    {
        return static_cast<uint8>(Current) >= static_cast<uint8>(Required);
    }

    bool LiveSignalsEqual(const FDACampaignLiveSignalState& Left, const FDACampaignLiveSignalState& Right)
    {
        if (Left.Population != Right.Population || Left.Capital != Right.Capital
            || Left.Insight != Right.Insight || Left.Influence != Right.Influence
            || Left.ResolvedDevelopmentCycles != Right.ResolvedDevelopmentCycles
            || Left.Citizens.Num() != Right.Citizens.Num()
            || Left.JobOpenings.Num() != Right.JobOpenings.Num()
            || Left.JobAssignments.Num() != Right.JobAssignments.Num()
            || Left.UtilitySignals.Num() != Right.UtilitySignals.Num()) return false;
        for (int32 Index = 0; Index < Left.Citizens.Num(); ++Index)
            if (Left.Citizens[Index].CitizenId != Right.Citizens[Index].CitizenId
                || Left.Citizens[Index].CityId != Right.Citizens[Index].CityId
                || Left.Citizens[Index].HomeWorldAssetId != Right.Citizens[Index].HomeWorldAssetId
                || Left.Citizens[Index].JobId != Right.Citizens[Index].JobId) return false;
        for (int32 Index = 0; Index < Left.JobOpenings.Num(); ++Index)
            if (Left.JobOpenings[Index].JobId != Right.JobOpenings[Index].JobId
                || Left.JobOpenings[Index].CityId != Right.JobOpenings[Index].CityId
                || Left.JobOpenings[Index].FacilityWorldAssetId != Right.JobOpenings[Index].FacilityWorldAssetId
                || Left.JobOpenings[Index].OpenPositions != Right.JobOpenings[Index].OpenPositions) return false;
        for (int32 Index = 0; Index < Left.JobAssignments.Num(); ++Index)
            if (Left.JobAssignments[Index].CitizenId != Right.JobAssignments[Index].CitizenId
                || Left.JobAssignments[Index].JobId != Right.JobAssignments[Index].JobId
                || Left.JobAssignments[Index].FacilityWorldAssetId != Right.JobAssignments[Index].FacilityWorldAssetId) return false;
        for (int32 Index = 0; Index < Left.UtilitySignals.Num(); ++Index)
            if (Left.UtilitySignals[Index].WorldAssetId != Right.UtilitySignals[Index].WorldAssetId
                || Left.UtilitySignals[Index].Utility != Right.UtilitySignals[Index].Utility
                || Left.UtilitySignals[Index].Supply != Right.UtilitySignals[Index].Supply) return false;
        return true;
    }

    bool ClockAuthoritiesEqual(const FDASimulationClockAuthorityState& Left,
        const FDASimulationClockAuthorityState& Right)
    {
        return Left.bCaptured == Right.bCaptured
            && Left.CurrentDevelopmentCycle == Right.CurrentDevelopmentCycle
            && Left.CurrentWorldTick == Right.CurrentWorldTick
            && Left.AccumulatedSimulationSeconds == Right.AccumulatedSimulationSeconds;
    }
}

TArray<FDARegionState> FDARegionSeedCatalog::MakeVerticalSliceRegions()
{
    FDARegionState Synara;
    Synara.RegionId = TEXT("region.synara_frontier");
    Synara.MapAssetPath = FSoftObjectPath(TEXT("/Game/Maps/Regions/L_SynaraFrontier.L_SynaraFrontier"));
    Synara.OwnerId = TEXT("civilization.synara");
    Synara.SettlementIds = {TEXT("settlement.arden_reservoir")};

    FDARegionState Ironheart;
    Ironheart.RegionId = TEXT("region.ironheart");
    Ironheart.MapAssetPath = FSoftObjectPath(TEXT("/Game/Maps/Regions/L_Ironheart.L_Ironheart"));
    Ironheart.OwnerId = TEXT("civilization.forgeweave");
    Ironheart.SettlementIds = {TEXT("settlement.ore_station_7")};

    FDARegionState Eden;
    Eden.RegionId = TEXT("region.eden_basin");
    Eden.MapAssetPath = FSoftObjectPath(TEXT("/Game/Maps/Regions/L_EdenBasin.L_EdenBasin"));
    Eden.OwnerId = TEXT("civilization.eden_circuit");
    Eden.SettlementIds = {TEXT("settlement.river_crossing")};

    return {MoveTemp(Synara), MoveTemp(Ironheart), MoveTemp(Eden)};
}

void UDAWorldStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDASimulationClockSubsystem>();
    Collection.InitializeDependency<UDAEconomySubsystem>();
    Collection.InitializeDependency<UDAContentRegistrySubsystem>();
    Collection.InitializeDependency<UDARegionalCrisisContentRegistrySubsystem>();
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        SimulationClock = GameInstance->GetSubsystem<UDASimulationClockSubsystem>();
        EconomySubsystem = GameInstance->GetSubsystem<UDAEconomySubsystem>();
        ContentRegistry = GameInstance->GetSubsystem<UDAContentRegistrySubsystem>();
        RegionalCrisisContentRegistry = GameInstance->GetSubsystem<UDARegionalCrisisContentRegistrySubsystem>();
        if (SimulationClock != nullptr)
        {
            WorldTickPreCommitHandle = SimulationClock->OnWorldTickPreCommit.AddUObject(
                this,
                &UDAWorldStateSubsystem::HandleSimulationWorldTickPreCommit);
            WorldTickAbortHandle = SimulationClock->OnWorldTickTransitionAborted.AddUObject(
                this,
                &UDAWorldStateSubsystem::HandleSimulationWorldTickAborted);
            WorldTickHandle = SimulationClock->OnWorldTick.AddUObject(
                this,
                &UDAWorldStateSubsystem::HandleSimulationWorldTick);
            DevelopmentCycleHandle = SimulationClock->OnDevelopmentCycle.AddUObject(
                this, &UDAWorldStateSubsystem::HandleDevelopmentCycle);
            DevelopmentCyclesPreCommitHandle = SimulationClock->OnDevelopmentCyclesPreCommit.AddUObject(
                this, &UDAWorldStateSubsystem::HandleDevelopmentCyclesPreCommit);
            ClockAuthorityHandle = SimulationClock->OnAuthorityChanged.AddUObject(
                this, &UDAWorldStateSubsystem::HandleClockAuthorityChanged);
        }
    }
    EnsureSystems();
    if (!PersistentCampaign.WorldState.bInitialized)
    {
        InitializeVerticalSliceState(TEXT("region.synara_frontier"), 0);
    }
}

void UDAWorldStateSubsystem::Deinitialize()
{
    if (SimulationClock != nullptr && WorldTickHandle.IsValid())
    {
        SimulationClock->OnWorldTick.Remove(WorldTickHandle);
    }
    if (SimulationClock != nullptr && WorldTickPreCommitHandle.IsValid())
    {
        SimulationClock->OnWorldTickPreCommit.Remove(WorldTickPreCommitHandle);
    }
    if (SimulationClock != nullptr && WorldTickAbortHandle.IsValid())
    {
        SimulationClock->OnWorldTickTransitionAborted.Remove(WorldTickAbortHandle);
    }
    if (SimulationClock != nullptr && DevelopmentCycleHandle.IsValid())
    {
        SimulationClock->OnDevelopmentCycle.Remove(DevelopmentCycleHandle);
    }
    if (SimulationClock != nullptr && DevelopmentCyclesPreCommitHandle.IsValid())
    {
        SimulationClock->OnDevelopmentCyclesPreCommit.Remove(DevelopmentCyclesPreCommitHandle);
    }
    if (SimulationClock != nullptr && ClockAuthorityHandle.IsValid())
    {
        SimulationClock->OnAuthorityChanged.Remove(ClockAuthorityHandle);
    }
    WorldTickHandle.Reset();
    WorldTickPreCommitHandle.Reset();
    WorldTickAbortHandle.Reset();
    DevelopmentCycleHandle.Reset();
    DevelopmentCyclesPreCommitHandle.Reset();
    ClockAuthorityHandle.Reset();
    PreparedWorldTickState.Reset();
    PreparedWorldTick = INDEX_NONE;
    SimulationClock = nullptr;
    EconomySubsystem = nullptr;
    ForgeweaveStrategy = nullptr;
    RegionalCrisisContentRegistry = nullptr;
    ContentRegistry = nullptr;
    SystemicPressureSystem = nullptr;
    JobSystem = nullptr;
    MigrationSystem = nullptr;
    RegisteredTravelRuntime.Reset();
    OnTravelStageCompleted.Clear();
    OnWorldTickStateCommitted.Clear();
    Super::Deinitialize();
}

bool UDAWorldStateSubsystem::InitializeVerticalSliceState(
    const FName InitialRegionId,
    const int64 InitialWorldTick)
{
    EnsureSystems();
    if (InitialRegionId.IsNone() || InitialWorldTick < 0 || ContentRegistry == nullptr)
    {
        return false;
    }

    FDACampaignSnapshot CandidateCampaign;
    FString Error;
    if (!ContentRegistry->BuildStarterCampaignContent(
        CandidateCampaign.CollectionState, CandidateCampaign.DeckState, Error))
    {
        return false;
    }
    CandidateCampaign.RebindRuntimeReferences();
    CandidateCampaign.DeckState.DrawOpeningHand();
    CandidateCampaign.CityGridClaims.Add(FDACityGridClaimState::MakeCanonicalPlayerCapital());

    const FGuid FounderHallCardId = MakeDeterministicCampaignGuid(TEXT("DA.VerticalSlice.FounderHall.Card"));
    const FGuid FounderHallAssetId = MakeDeterministicCampaignGuid(TEXT("DA.VerticalSlice.FounderHall.Asset"));
    if (!CandidateCampaign.CollectionState.AddInstanceWithId(
        FounderHallCardId, TEXT("special.founder_hall"), EDAAcquisitionSource::WorldDiscovery,
        InitialWorldTick))
    {
        return false;
    }
    FCardInstance* FounderHallCard = CandidateCampaign.CollectionState.FindInstance(FounderHallCardId);
    UDA_CardDefinition* FounderHallDefinition =
        ContentRegistry->GetCardDefinition(TEXT("special.founder_hall"));
    if (FounderHallCard == nullptr || FounderHallDefinition == nullptr)
    {
        return false;
    }
    FounderHallCard->RecoveryState = EDARecoveryState::Deployed;
    FounderHallCard->WorldAssetId = FounderHallAssetId;
    FDAWorldAssetRecord& FounderHall = CandidateCampaign.WorldAssets.Emplace_GetRef();
    FounderHall.WorldAssetId = FounderHallAssetId;
    FounderHall.CardInstanceId = FounderHallCardId;
    FounderHall.CardDefinitionId = TEXT("special.founder_hall");
    FounderHall.CityId = TEXT("player_capital");
    FounderHall.GridOrigin = FIntPoint(15, 15);
    FounderHall.ConstructionState = EDAConstructionState::Operational;
    FounderHall.StructuralIntegrity = 100.f;
    FounderHall.OwnerCivilizationId = TEXT("civilization.synara");

    FDACitySimulationState& City = CandidateCampaign.CitySimulationState;
    City.bInitialized = true;
    City.Wallet = FDAWalletValues(40.f, 12.f, 8.f);
    City.Population = 24;
    City.Attractiveness = 40.f;
    City.ResolvedWorldTicks = InitialWorldTick;
    City.Citizens = UDAJobSystem::CreateNamedCitizenRoster();
    for (FDACitizenRecord& Citizen : City.Citizens)
    {
        if (Citizen.CitizenId.ToString().StartsWith(TEXT("citizen.synara.")))
        {
            Citizen.CityId = TEXT("player_capital");
            Citizen.HomeAssetId = FounderHallAssetId;
            Citizen.DistrictId = TEXT("district.player_capital.residential");
            Citizen.CommuteZone = TEXT("commute.player_capital.central");
        }
        FDAHouseholdRecord& Household = City.Households.Emplace_GetRef();
        Household.HouseholdId = Citizen.HouseholdId;
        Household.HomeAssetId = Citizen.HomeAssetId;
        Household.CityId = Citizen.CityId;
        Household.DistrictId = Citizen.DistrictId;
        Household.CommuteZone = Citizen.CommuteZone;
        Household.MemberCitizenIds.Add(Citizen.CitizenId);
    }
    City.Facilities.Add(MakeFacilityContext(FounderHall, *FounderHallDefinition));
    ProjectCitySimulationState(CandidateCampaign, false);

    FDAWorldCampaignState& Candidate = CandidateCampaign.WorldState;
    Candidate.bInitialized = true;
    Candidate.CurrentWorldTick = InitialWorldTick;
    Candidate.CurrentRegionId = InitialRegionId;
    Candidate.Regions = FDARegionSeedCatalog::MakeVerticalSliceRegions();
    Candidate.Trade.LastProcessedWorldTick = InitialWorldTick;
    FDARegionalTradeInventory EdenInventory;
    EdenInventory.RegionId = TEXT("region.eden_basin");
    EdenInventory.Stock.Add(TEXT("resource.regenerative_materials"), 1000);
    Candidate.Trade.Inventories.Add(EdenInventory);
    FDARegionalTradeInventory IronheartInventory;
    IronheartInventory.RegionId = TEXT("region.ironheart");
    IronheartInventory.Stock.Add(TEXT("resource.regenerative_materials"), 0);
    Candidate.Trade.Inventories.Add(IronheartInventory);
    FDATradeRouteState ReliefRoute;
    ReliefRoute.RouteId = TEXT("route.eden_ironheart_relief");
    ReliefRoute.SourceRegionId = TEXT("region.eden_basin");
    ReliefRoute.DestinationRegionId = TEXT("region.ironheart");
    ReliefRoute.CapacityPerWorldTick = 10;
    ReliefRoute.CapacityWorldTick = InitialWorldTick;
    Candidate.Trade.Routes.Add(ReliefRoute);
    Candidate.Forgeweave = UDAForgeweaveStrategy::MakeVerticalSliceInitialState(1701, InitialWorldTick);

    if (SimulationClock == nullptr
        || !SimulationClock->RestoreWorldTickAuthority(InitialWorldTick))
    {
        return false;
    }
    Candidate.ClockAuthority = SimulationClock->CaptureAuthorityState();
    if (!CandidateCampaign.Validate(Error))
    {
        return false;
    }

    PersistentCampaign = MoveTemp(CandidateCampaign);
    PreparedWorldTickState.Reset();
    PreparedWorldTick = INDEX_NONE;
    const FDACommittedCampaignSnapshot InitializedState = MakeShared<FDACampaignSnapshot>(PersistentCampaign);
    OnWorldTickStateCommitted.Broadcast(InitializedState);
    return true;
}

bool UDAWorldStateSubsystem::RestorePersistentCampaign(const FDACampaignSnapshot& InCampaign)
{
    FDACampaignSnapshot Candidate = InCampaign;
    FString Error;
    if (!Candidate.Validate(Error))
    {
        return false;
    }
    Candidate.WorldState.NormalizeEphemeralTravelState();
    if (!Candidate.Validate(Error))
    {
        return false;
    }

    EnsureSystems();
    if (SimulationClock == nullptr
        || !(Candidate.WorldState.ClockAuthority.bCaptured
            ? SimulationClock->RestoreAuthorityState(Candidate.WorldState.ClockAuthority)
            : SimulationClock->RestoreWorldTickAuthority(Candidate.WorldState.CurrentWorldTick)))
    {
        return false;
    }
    Candidate.WorldState.ClockAuthority = SimulationClock->CaptureAuthorityState();
    if (!Candidate.Validate(Error))
    {
        return false;
    }
    PersistentCampaign = MoveTemp(Candidate);
    if (SystemicPressureSystem != nullptr)
        SystemicPressureSystem->SetDependency(
            static_cast<float>(PersistentCampaign.SynaraState.Dependency));
    PreparedWorldTickState.Reset();
    PreparedWorldTick = INDEX_NONE;
    const FDACommittedCampaignSnapshot RestoredState = MakeShared<FDACampaignSnapshot>(PersistentCampaign);
    OnWorldTickStateCommitted.Broadcast(RestoredState);
    return true;
}

bool UDAWorldStateSubsystem::RestorePersistentState(const FDAWorldCampaignState& InState)
{
    FDACampaignSnapshot Candidate = PersistentCampaign;
    Candidate.WorldState = InState;
    return RestorePersistentCampaign(Candidate);
}

bool UDAWorldStateSubsystem::TryCommitPersistentCampaign(const FDACampaignSnapshot& Candidate,
    const int64 ExpectedNarrativeRevision, const int64 ExpectedSignalRevision, const int64 ExpectedWorldTick)
{
    FString Error;
    const int64 CurrentSignalRevision = PersistentCampaign.LiveSignals.MutationRevision;
    const FDASimulationClockAuthorityState LiveClockAuthority = SimulationClock != nullptr
        ? SimulationClock->CaptureAuthorityState() : FDASimulationClockAuthorityState{};
    const bool bSignalsEqual = LiveSignalsEqual(Candidate.LiveSignals, PersistentCampaign.LiveSignals);
    const bool bUnchangedSignalRevision = bSignalsEqual
        && Candidate.LiveSignals.MutationRevision == ExpectedSignalRevision;
    const bool bSingleSignalMutation = !bSignalsEqual
        && ExpectedSignalRevision < MAX_int64
        && Candidate.LiveSignals.MutationRevision == ExpectedSignalRevision + 1;
    if (SimulationClock == nullptr
        || PreparedWorldTickState.IsSet()
        || PersistentCampaign.CampaignMutationRevision == MAX_int64
        || Candidate.CampaignMutationRevision != PersistentCampaign.CampaignMutationRevision
        || (SimulationClock != nullptr && SimulationClock->GetCurrentWorldTick() != ExpectedWorldTick)
        || PersistentCampaign.NarrativeState.MutationRevision != ExpectedNarrativeRevision
        || CurrentSignalRevision != ExpectedSignalRevision
        || !ClockAuthoritiesEqual(Candidate.WorldState.ClockAuthority, LiveClockAuthority)
        || (!bUnchangedSignalRevision && !bSingleSignalMutation)
        || PersistentCampaign.WorldState.CurrentWorldTick != ExpectedWorldTick
        || Candidate.WorldState.CurrentWorldTick != ExpectedWorldTick
        || !Candidate.Validate(Error))
    {
        return false;
    }
    FDACampaignSnapshot Committed = Candidate;
    Committed.CampaignMutationRevision = Candidate.CampaignMutationRevision + 1;
    PersistentCampaign = MoveTemp(Committed);
    const FDACommittedCampaignSnapshot CommittedState = MakeShared<FDACampaignSnapshot>(PersistentCampaign);
    OnWorldTickStateCommitted.Broadcast(CommittedState);
    return true;
}

bool UDAWorldStateSubsystem::CompleteForgeweaveConquestRoute(const FGuid ActionId,
    const EDAForgeweaveRoute Route, FString& OutError)
{
    FDACampaignSnapshot Candidate = PersistentCampaign;
    if (!FDAConquestSystem::Synchronize(Candidate, OutError)
        || !FDAConquestSystem::CompleteRoute(ActionId, Route, Candidate, OutError)) return false;
    const FDACampaignSnapshot& Authority = PersistentCampaign;
    return TryCommitPersistentCampaign(Candidate, Authority.NarrativeState.MutationRevision,
        Authority.LiveSignals.MutationRevision, Authority.WorldState.CurrentWorldTick);
}

bool UDAWorldStateSubsystem::TryCommitDaxtonCandidate(const FDACampaignSnapshot& Candidate,
    const int64 ExpectedNarrativeRevision, const int64 ExpectedSignalRevision,
    const int64 ExpectedWorldTick, FString& OutError)
{
    if (!Candidate.Validate(OutError)) return false;
    if (!TryCommitPersistentCampaign(Candidate, ExpectedNarrativeRevision,
        ExpectedSignalRevision, ExpectedWorldTick))
    {
        OutError = TEXT("Daxton campaign compare-and-swap rejected stale or non-canonical authority.");
        return false;
    }
    return true;
}

#define DA_COMMIT_ENCOUNTER_ACTION(Mutation) \
    const int64 ExpectedNarrativeRevision = PersistentCampaign.NarrativeState.MutationRevision; \
    const int64 ExpectedSignalRevision = PersistentCampaign.LiveSignals.MutationRevision; \
    const int64 ExpectedWorldTick = PersistentCampaign.WorldState.CurrentWorldTick; \
    FDACampaignSnapshot Candidate = PersistentCampaign; \
    if (!(Mutation)) return false; \
    return TryCommitDaxtonCandidate(Candidate, ExpectedNarrativeRevision, \
        ExpectedSignalRevision, ExpectedWorldTick, OutError)

bool UDAWorldStateSubsystem::StartDaxtonEncounter(const FGuid ActionId, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::Start(ActionId, Candidate, OutError));
}

bool UDAWorldStateSubsystem::AdvanceDaxtonGrandForgeProduction(
    const FGuid ActionId, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::AdvanceGrandForgeProduction(
        ActionId, Candidate, OutError));
}

bool UDAWorldStateSubsystem::DeployDaxtonHardenedCover(const FGuid ActionId, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::DeployHardenedCover(
        ActionId, Candidate, OutError));
}

bool UDAWorldStateSubsystem::ReinforceDaxtonForgeGuard(const FGuid ActionId, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::ReinforceForgeGuard(
        ActionId, Candidate, OutError));
}

bool UDAWorldStateSubsystem::CompleteDaxtonPhaseOneIndustrialObjective(
    const FGuid ActionId, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::CompletePhaseOneIndustrialObjective(
        ActionId, Candidate, OutError));
}

bool UDAWorldStateSubsystem::ApplyDaxtonInteraction(const FGuid ActionId,
    const EDADaxtonInteraction Interaction, const float Strength, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::ApplySystemInteraction(
        ActionId, Interaction, Strength, Candidate, OutError));
}

bool UDAWorldStateSubsystem::EnterDaxtonChoicePhase(const FGuid ActionId, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::EnterPhaseThree(
        ActionId, Candidate, OutError));
}

bool UDAWorldStateSubsystem::CompleteDaxtonChoiceObjective(const FGuid ActionId,
    const EDADaxtonChoiceObjective Objective, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::CompleteChoiceObjective(
        ActionId, Objective, Candidate, OutError));
}

bool UDAWorldStateSubsystem::ResolveDaxtonLeaderState(const FGuid ActionId,
    const EDADaxtonLeaderState State, FString& OutError)
{
    DA_COMMIT_ENCOUNTER_ACTION(FDADaxtonEncounter::ResolveLeaderState(
        ActionId, State, Candidate, OutError));
}

bool UDAWorldStateSubsystem::CompleteFirstAscension(
    const FGuid ActionId, FString& OutError)
{
    const int64 ExpectedNarrativeRevision = PersistentCampaign.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = PersistentCampaign.LiveSignals.MutationRevision;
    const int64 ExpectedWorldTick = PersistentCampaign.WorldState.CurrentWorldTick;
    FDACampaignSnapshot Candidate = PersistentCampaign;
    if (!FDAAscensionSystem::ApplyFirstAscension(ActionId, Candidate, OutError)) return false;
    if (!TryCommitPersistentCampaign(Candidate, ExpectedNarrativeRevision,
        ExpectedSignalRevision, ExpectedWorldTick))
    {
        OutError = TEXT("First Ascension compare-and-swap rejected stale campaign authority.");
        return false;
    }
    OutError.Reset();
    return true;
}

FDAAscensionPresentationState UDAWorldStateSubsystem::GetAscensionPresentationState() const
{
    return FDAAscensionSystem::BuildPresentationState(PersistentCampaign);
}

bool UDAWorldStateSubsystem::ReplicateCard(const FGuid ActionId,
    const FGuid SourceCardInstanceId, FGuid& OutReplicatedCardInstanceId,
    FString& OutError)
{
    EnsureSystems();
    if (SimulationClock == nullptr || ContentRegistry == nullptr
        || ContentRegistry->GetRegisteredCardCount() <= 0)
    {
        OutError = TEXT("Replication requires ready clock and canonical content authorities.");
        return false;
    }
    const int64 ExpectedNarrativeRevision = PersistentCampaign.NarrativeState.MutationRevision;
    const int64 ExpectedSignalRevision = PersistentCampaign.LiveSignals.MutationRevision;
    const int64 ExpectedWorldTick = PersistentCampaign.WorldState.CurrentWorldTick;
    FDACampaignSnapshot Candidate = PersistentCampaign;
    if (!FDAAscensionSystem::ReplicateCard(ActionId, SourceCardInstanceId, *ContentRegistry,
        SimulationClock->GetCurrentDevelopmentCycle(), Candidate,
        OutReplicatedCardInstanceId, OutError)) return false;
    if (!TryCommitPersistentCampaign(Candidate, ExpectedNarrativeRevision,
        ExpectedSignalRevision, ExpectedWorldTick))
    {
        OutReplicatedCardInstanceId.Invalidate();
        OutError = TEXT("Replication compare-and-swap rejected stale campaign authority.");
        return false;
    }
    OutError.Reset();
    return true;
}

#undef DA_COMMIT_ENCOUNTER_ACTION

bool UDAWorldStateSubsystem::SubmitCitizenSignal(const FDACampaignCitizenSignal& Signal)
{
    if (Signal.CitizenId.IsNone() || Signal.CityId.IsNone()
        || PersistentCampaign.LiveSignals.MutationRevision == MAX_int64) return false;
    const int64 ExpectedRevision = PersistentCampaign.LiveSignals.MutationRevision;
    FDACampaignSnapshot Candidate = PersistentCampaign;
    FDACitizenRecord* Existing = Candidate.CitySimulationState.Citizens.FindByPredicate(
        [&Signal](const FDACitizenRecord& Row) { return Row.CitizenId == Signal.CitizenId; });
    if (Existing == nullptr)
    {
        Existing = &Candidate.CitySimulationState.Citizens.Emplace_GetRef();
        Existing->CitizenId = Signal.CitizenId;
    }
    Existing->CityId = Signal.CityId;
    Existing->HomeAssetId = Signal.HomeWorldAssetId;
    Existing->JobId = Signal.JobId;
    ProjectCitySimulationState(Candidate, true);
    return TryCommitPersistentCampaign(Candidate,
        PersistentCampaign.NarrativeState.MutationRevision, ExpectedRevision, GetCurrentWorldTick());
}

bool UDAWorldStateSubsystem::SubmitJobOpeningSignal(const FDACampaignJobOpeningSignal& Signal)
{
    EnsureSystems();
    if (PersistentCampaign.LiveSignals.MutationRevision == MAX_int64) return false;
    FDACampaignJobOpeningSignal ResolvedSignal = Signal;
    const FDAWorldAssetRecord* Facility = PersistentCampaign.FindWorldAssetRecord(
        Signal.FacilityWorldAssetId);
    if (Facility != nullptr
        && Facility->CardDefinitionId == TEXT("fusion.autonomous_factory"))
    {
        const UDA_CardDefinition* Definition = ContentRegistry == nullptr ? nullptr
            : ContentRegistry->GetCardDefinition(Facility->CardDefinitionId);
        if (Facility->ConstructionState != EDAConstructionState::Operational
            || Definition == nullptr) return false;
        ResolvedSignal.OpenPositions = UDAJobSystem::CalculateFacilityWorkforceRequirement(
            Signal.OpenPositions, *Definition);
    }
    const int64 ExpectedRevision = PersistentCampaign.LiveSignals.MutationRevision;
    FDACampaignSnapshot Candidate = PersistentCampaign;
    FDAJobOpening* Existing = Candidate.CitySimulationState.JobOpenings.FindByPredicate(
        [&ResolvedSignal](const auto& Row)
        { return Row.JobId == ResolvedSignal.JobId
            && Row.FacilityWorldAssetId == ResolvedSignal.FacilityWorldAssetId; });
    if (Existing == nullptr) Existing = &Candidate.CitySimulationState.JobOpenings.Emplace_GetRef();
    Existing->JobId = ResolvedSignal.JobId;
    Existing->CityId = ResolvedSignal.CityId;
    Existing->FacilityWorldAssetId = ResolvedSignal.FacilityWorldAssetId;
    Existing->OpenPositions = ResolvedSignal.OpenPositions;
    Candidate.CitySimulationState.JobOpenings.Sort([](const auto& A, const auto& B)
        { return A.JobId != B.JobId ? A.JobId.LexicalLess(B.JobId)
            : A.FacilityWorldAssetId.ToString().Compare(B.FacilityWorldAssetId.ToString()) < 0; });
    ProjectCitySimulationState(Candidate, true);
    return TryCommitPersistentCampaign(Candidate,
        PersistentCampaign.NarrativeState.MutationRevision, ExpectedRevision, GetCurrentWorldTick());
}

bool UDAWorldStateSubsystem::SubmitJobAssignmentSignal(const FDACampaignJobAssignmentSignal& Signal)
{
    if (PersistentCampaign.LiveSignals.MutationRevision == MAX_int64) return false;
    const int64 ExpectedRevision = PersistentCampaign.LiveSignals.MutationRevision;
    FDACampaignSnapshot Candidate = PersistentCampaign;
    FDACitizenRecord* Citizen = Candidate.CitySimulationState.Citizens.FindByPredicate(
        [&Signal](const auto& Row) { return Row.CitizenId == Signal.CitizenId; });
    FDAJobOpening* Opening = Candidate.CitySimulationState.JobOpenings.FindByPredicate(
        [&Signal](const auto& Row)
        { return Row.JobId == Signal.JobId
            && Row.FacilityWorldAssetId == Signal.FacilityWorldAssetId; });
    if (Citizen == nullptr || Opening == nullptr || Opening->CityId != Citizen->CityId
        || Opening->OpenPositions <= 0) return false;
    FDAJobAssignment* Existing = Candidate.CitySimulationState.JobAssignments.FindByPredicate(
        [&Signal](const auto& Row) { return Row.CitizenId == Signal.CitizenId; });
    if (Existing != nullptr
        && Existing->JobId == Signal.JobId
        && Existing->FacilityWorldAssetId == Signal.FacilityWorldAssetId)
    {
        return false;
    }
    if (Existing != nullptr)
    {
        FDAJobOpening* PreviousOpening = Candidate.CitySimulationState.JobOpenings.FindByPredicate(
            [Existing](const FDAJobOpening& Row)
            {
                return Row.JobId == Existing->JobId
                    && Row.FacilityWorldAssetId == Existing->FacilityWorldAssetId;
            });
        if (PreviousOpening == nullptr || PreviousOpening->OpenPositions == MAX_int32)
        {
            return false;
        }
        ++PreviousOpening->OpenPositions;
    }
    --Opening->OpenPositions;
    if (Existing == nullptr) Existing = &Candidate.CitySimulationState.JobAssignments.Emplace_GetRef();
    Existing->CitizenId = Signal.CitizenId;
    Existing->JobId = Signal.JobId;
    Existing->FacilityWorldAssetId = Signal.FacilityWorldAssetId;
    Existing->MatchQuality = EDAJobMatchQuality::Acceptable;
    Existing->OutputMultiplier = 0.85f;
    Citizen->JobId = Signal.JobId;
    Candidate.CitySimulationState.JobAssignments.Sort(
        [](const auto& A, const auto& B) { return A.CitizenId.LexicalLess(B.CitizenId); });
    ProjectCitySimulationState(Candidate, true);
    return TryCommitPersistentCampaign(Candidate,
        PersistentCampaign.NarrativeState.MutationRevision, ExpectedRevision, GetCurrentWorldTick());
}

bool UDAWorldStateSubsystem::SubmitUtilitySignal(const FDACampaignUtilitySignal& Signal)
{
    if (PersistentCampaign.LiveSignals.MutationRevision == MAX_int64) return false;
    const int64 ExpectedRevision = PersistentCampaign.LiveSignals.MutationRevision;
    FDACampaignSnapshot Candidate = PersistentCampaign;
    FDACampaignUtilitySignal* Existing = Candidate.CitySimulationState.UtilitySignals.FindByPredicate([&Signal](const auto& Row)
        { return Row.WorldAssetId == Signal.WorldAssetId && Row.Utility == Signal.Utility; });
    if (Existing != nullptr) *Existing = Signal; else Candidate.CitySimulationState.UtilitySignals.Add(Signal);
    Candidate.CitySimulationState.UtilitySignals.Sort([](const auto& A, const auto& B)
        { return A.WorldAssetId != B.WorldAssetId ? A.WorldAssetId.ToString().Compare(B.WorldAssetId.ToString()) < 0
            : static_cast<uint8>(A.Utility) < static_cast<uint8>(B.Utility); });
    ProjectCitySimulationState(Candidate, true);
    return TryCommitPersistentCampaign(Candidate,
        PersistentCampaign.NarrativeState.MutationRevision, ExpectedRevision, GetCurrentWorldTick());
}

void UDAWorldStateSubsystem::HandleDevelopmentCycle(const int64 CycleIndex)
{
    if (EconomySubsystem == nullptr)
    {
        ensureAlwaysMsgf(false,
            TEXT("Development Cycle reached publication without its economy owner."));
        return;
    }
    FDACampaignSnapshot& Authority = PreparedWorldTickState.IsSet()
        ? PreparedWorldTickState.GetValue() : PersistentCampaign;
    if (Authority.LiveSignals.MutationRevision == MAX_int64
        || Authority.CitySimulationState.ResolvedDevelopmentCycles == MAX_int64
        || JobSystem == nullptr)
    {
        ensureAlwaysMsgf(false, TEXT("Development Cycle reached a capacity boundary after precommit."));
        return;
    }
    FDACampaignSnapshot Candidate = Authority;

    FString Error;
    if (!ensureAlwaysMsgf(AdvancePlayerConstruction(Candidate, Error),
        TEXT("Preflighted player construction diverged during publication: %s"), *Error))
        return;
    if (!ensureAlwaysMsgf(SynchronizeFacilityRecords(Candidate, Error),
        TEXT("Development Cycle facility projection diverged: %s"), *Error))
        return;
    EconomySubsystem->ResolveDevelopmentCycle(Candidate.CitySimulationState);
    JobSystem->ResolveAssignments(Candidate.CitySimulationState);
    ProjectCitySimulationState(Candidate, true);
    if (Candidate.AscensionState.bForgeweaveAscended)
    {
        if (!ensureAlwaysMsgf(ContentRegistry != nullptr && SystemicPressureSystem != nullptr,
            TEXT("Ascended Development Cycle reached publication without content/pressure owners.")))
            return;
        UDA_CardDefinition* Factory = ContentRegistry->GetCardDefinition(TEXT("fusion.autonomous_factory"));
        if (!ensureAlwaysMsgf(Factory != nullptr,
            TEXT("Ascended Development Cycle lost the canonical Autonomous Factory definition.")))
            return;
        TArray<FGuid> OperationalFactories;
        for (const FDAWorldAssetRecord& Asset : Candidate.WorldAssets)
        {
            if (Asset.CardDefinitionId == TEXT("fusion.autonomous_factory")
                && Asset.ConstructionState == EDAConstructionState::Operational)
                OperationalFactories.Add(Asset.WorldAssetId);
        }
        OperationalFactories.Sort([](const FGuid& Left, const FGuid& Right)
        { return Left.ToString().Compare(Right.ToString()) < 0; });
        if (!OperationalFactories.IsEmpty()
            && !ensureAlwaysMsgf(FDAAscensionSystem::ApplyAutonomousFactoryDevelopmentCycle(
                OperationalFactories, *Factory, CycleIndex, Candidate, Error),
                TEXT("Preflighted Autonomous Factory pressure diverged during publication: %s"),
                *Error)) return;
    }
    if (PreparedWorldTickState.IsSet())
    {
        if (ensureAlwaysMsgf(Candidate.Validate(Error),
            TEXT("A preflighted Development Cycle produced invalid prepared campaign state: %s"),
            *Error))
        {
            PreparedWorldTickState = MoveTemp(Candidate);
        }
        return;
    }
    if (SimulationClock != nullptr)
        Candidate.WorldState.ClockAuthority = SimulationClock->CaptureAuthorityState();
    const bool bPublished = TryCommitPersistentCampaign(Candidate,
        Authority.NarrativeState.MutationRevision,
        Authority.LiveSignals.MutationRevision, GetCurrentWorldTick());
    ensureAlwaysMsgf(bPublished,
        TEXT("A preflighted Development Cycle failed publication."));
    if (bPublished && SystemicPressureSystem != nullptr)
        SystemicPressureSystem->SetDependency(
            static_cast<float>(PersistentCampaign.SynaraState.Dependency));
}

void UDAWorldStateSubsystem::HandleDevelopmentCyclesPreCommit(
    const int64 FirstCycle, const int64 CycleCount, FDAWorldTickVeto& Veto)
{
    const FDACampaignLiveSignalState& Signals = PersistentCampaign.LiveSignals;
    const bool bUnexpectedFirstCycle = SimulationClock == nullptr
        || SimulationClock->GetCurrentDevelopmentCycle() == MAX_int64
        || FirstCycle != SimulationClock->GetCurrentDevelopmentCycle() + 1;
    FString PreviewError;
    if (Veto.IsRejected() || CycleCount <= 0 || bUnexpectedFirstCycle
        || Signals.MutationRevision > MAX_int64 - CycleCount
        || Signals.ResolvedDevelopmentCycles > MAX_int64 - CycleCount
        || !PreviewDevelopmentCycles(FirstCycle, CycleCount, PreviewError))
    {
        Veto.Reject();
        return;
    }
    const bool bHasOperationalFactory = PersistentCampaign.WorldAssets.ContainsByPredicate(
        [](const FDAWorldAssetRecord& Asset)
        {
            return Asset.CardDefinitionId == TEXT("fusion.autonomous_factory")
                && Asset.ConstructionState == EDAConstructionState::Operational;
        });
    if (PersistentCampaign.AscensionState.bForgeweaveAscended && bHasOperationalFactory
        && (ContentRegistry == nullptr
            || ContentRegistry->GetCardDefinition(TEXT("fusion.autonomous_factory")) == nullptr
            || SystemicPressureSystem == nullptr)) Veto.Reject();
}

bool UDAWorldStateSubsystem::AdvancePlayerConstruction(
    FDACampaignSnapshot& Candidate, FString& OutError) const
{
    if (ContentRegistry == nullptr)
    {
        OutError = TEXT("Player construction requires the canonical content registry.");
        return false;
    }
    const UDA_CardDefinition* FactoryDefinition = ContentRegistry->GetCardDefinition(
        TEXT("fusion.autonomous_factory"));
    for (FDAWorldAssetRecord& Asset : Candidate.WorldAssets)
    {
        if (Asset.OwnerCivilizationId != TEXT("civilization.synara")
            || Asset.CityId != TEXT("player_capital")
            || (Asset.ConstructionState != EDAConstructionState::Foundation
                && Asset.ConstructionState != EDAConstructionState::Frame
                && Asset.ConstructionState != EDAConstructionState::Shell
                && Asset.ConstructionState != EDAConstructionState::Systems)) continue;
        const UDA_CardDefinition* Definition = ContentRegistry->GetCardDefinition(
            Asset.CardDefinitionId);
        const FCardInstance* Card = Candidate.CollectionState.FindInstance(Asset.CardInstanceId);
        if (Definition == nullptr || Card == nullptr)
        {
            OutError = TEXT("Player construction lost its authored card/instance authority.");
            return false;
        }

        float SpeedMultiplier = 1.f;
        if (FactoryDefinition != nullptr)
        {
            FDAAdjacencySubsystem Adjacency;
            if (!FDAAscensionSystem::ConfigureAutonomousFactoryAdjacency(
                Adjacency, *FactoryDefinition))
            {
                OutError = TEXT("Autonomous Factory adjacency authorship is invalid.");
                return false;
            }
            const auto RotatedFootprint = [](const FDAWorldAssetRecord& Record,
                const UDA_CardDefinition& CardDefinition)
            {
                return (Record.Rotation % 2) == 0 ? CardDefinition.Footprint
                    : FIntPoint(CardDefinition.Footprint.Y, CardDefinition.Footprint.X);
            };
            Adjacency.RegisterAsset(FDAWorldAssetId(Asset.WorldAssetId),
                Definition->DefinitionId, Asset.GridOrigin, Definition->CardType,
                RotatedFootprint(Asset, *Definition));
            for (const FDAWorldAssetRecord& Neighbor : Candidate.WorldAssets)
            {
                if (Neighbor.WorldAssetId == Asset.WorldAssetId
                    || Neighbor.OwnerCivilizationId != TEXT("civilization.synara")
                    || Neighbor.CityId != Asset.CityId
                    || Neighbor.CardDefinitionId != TEXT("fusion.autonomous_factory")
                    || Neighbor.ConstructionState != EDAConstructionState::Operational) continue;
                Adjacency.RegisterAsset(FDAWorldAssetId(Neighbor.WorldAssetId),
                    FactoryDefinition->DefinitionId, Neighbor.GridOrigin,
                    FactoryDefinition->CardType,
                    RotatedFootprint(Neighbor, *FactoryDefinition));
            }
            for (const FDAAdjacencyModifier& Modifier :
                Adjacency.RebuildForAsset(FDAWorldAssetId(Asset.WorldAssetId)))
                if (Modifier.Name
                    == TEXT("AutonomousFactory.AdjacentIndustrialConstructionSpeed"))
                    SpeedMultiplier += Modifier.Amount;
        }

        UDAConstructionComponent* Construction =
            NewObject<UDAConstructionComponent>(const_cast<UDAWorldStateSubsystem*>(this));
        if (Construction == nullptr
            || !Construction->InitializeFromRecord(Asset, Card, *Definition)
            || !Construction->AdvanceCycleWithSpeed(SpeedMultiplier))
        {
            OutError = TEXT("Player construction owner rejected its authored cycle.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool UDAWorldStateSubsystem::PreviewDevelopmentCycles(
    const int64 FirstCycle, const int64 CycleCount, FString& OutError) const
{
    if (EconomySubsystem == nullptr || JobSystem == nullptr || ContentRegistry == nullptr
        || SystemicPressureSystem == nullptr || CycleCount <= 0) return false;
    const UDA_CardDefinition* Factory = ContentRegistry->GetCardDefinition(
        TEXT("fusion.autonomous_factory"));
    FDACampaignSnapshot Candidate = PersistentCampaign;
    for (int64 Offset = 0; Offset < CycleCount; ++Offset)
    {
        if (!AdvancePlayerConstruction(Candidate, OutError)) return false;
        if (!SynchronizeFacilityRecords(Candidate, OutError)) return false;
        EconomySubsystem->ResolveDevelopmentCycle(Candidate.CitySimulationState);
        JobSystem->ResolveAssignments(Candidate.CitySimulationState);
        ProjectCitySimulationState(Candidate, true);
        if (!Candidate.AscensionState.bForgeweaveAscended) continue;
        if (Factory == nullptr) return false;
        TArray<FGuid> FactoryIds;
        for (const FDAWorldAssetRecord& Asset : Candidate.WorldAssets)
            if (Asset.CardDefinitionId == TEXT("fusion.autonomous_factory")
                && Asset.ConstructionState == EDAConstructionState::Operational)
                FactoryIds.Add(Asset.WorldAssetId);
        FactoryIds.Sort([](const FGuid& Left, const FGuid& Right)
        { return Left.ToString().Compare(Right.ToString()) < 0; });
        if (!FactoryIds.IsEmpty()
            && !FDAAscensionSystem::ApplyAutonomousFactoryDevelopmentCycle(
                FactoryIds, *Factory, FirstCycle + Offset, Candidate, OutError)) return false;
    }
    return Candidate.Validate(OutError);
}

void UDAWorldStateSubsystem::HandleClockAuthorityChanged(const FDASimulationClockAuthorityState& Authority)
{
    FString Error;
    const bool bAuthorityChanged = !ClockAuthoritiesEqual(
        Authority, PersistentCampaign.WorldState.ClockAuthority);
    if (Authority.Validate(Error)
        && Authority.CurrentWorldTick == PersistentCampaign.WorldState.CurrentWorldTick)
    {
        if (bAuthorityChanged && !PreparedWorldTickState.IsSet()
            && PersistentCampaign.CampaignMutationRevision == MAX_int64)
        {
            return;
        }
        PersistentCampaign.WorldState.ClockAuthority = Authority;
        // A clock transition occurring inside a prepared world-tick transaction is
        // published by HandleSimulationWorldTick as part of that one commit. Standalone
        // pause/rate changes are canonical mutations in their own right and must retire
        // every candidate captured from the previous aggregate revision.
        if (bAuthorityChanged && !PreparedWorldTickState.IsSet())
        {
            ++PersistentCampaign.CampaignMutationRevision;
        }
    }
}

bool UDAWorldStateSubsystem::AdvanceWorldTicks(const int32 WorldTickCount)
{
    EnsureSystems();
    FString ValidationError;
    if (!PersistentCampaign.WorldState.bInitialized
        || WorldTickCount < 0
        || SimulationClock == nullptr
        || SimulationClock->GetCurrentWorldTick() != PersistentCampaign.WorldState.CurrentWorldTick
        || !PersistentCampaign.Validate(ValidationError)
        || PersistentCampaign.WorldState.CurrentWorldTick > MAX_int64 - static_cast<int64>(WorldTickCount))
    {
        return false;
    }

    const int64 ExpectedWorldTick = PersistentCampaign.WorldState.CurrentWorldTick + WorldTickCount;
    if (!SimulationClock->AdvanceStrategicWorldTicks(WorldTickCount))
    {
        return false;
    }
    return SimulationClock->GetCurrentWorldTick() == ExpectedWorldTick
        && PersistentCampaign.WorldState.CurrentWorldTick == ExpectedWorldTick;
}

EDATravelResult UDAWorldStateSubsystem::Travel(
    const FName RequestId,
    const FName DestinationRegionId,
    const int32 TravelWorldTicks,
    IDARegionTravelRuntime& Runtime)
{
    if (!PersistentCampaign.WorldState.bInitialized
        || RequestId.IsNone()
        || DestinationRegionId.IsNone()
        || TravelWorldTicks <= 0
        || PreparedWorldTickState.IsSet()
        || PersistentCampaign.CampaignMutationRevision
            > MAX_int64 - static_cast<int64>(TravelWorldTicks) - 7
        || PersistentCampaign.CampaignMutationRevision == MAX_int64)
    {
        return EDATravelResult::Rejected;
    }

    if (PersistentCampaign.WorldState.CompletedTravelRequestIds.Contains(RequestId))
    {
        return EDATravelResult::AlreadyCompleted;
    }

    const bool bMayStart = PersistentCampaign.WorldState.TravelHandoff.Stage == EDATravelHandoffStage::None
        || PersistentCampaign.WorldState.TravelHandoff.Stage == EDATravelHandoffStage::Completed;
    if (bMayStart)
    {
        if (DestinationRegionId == PersistentCampaign.WorldState.CurrentRegionId
            || PersistentCampaign.WorldState.FindRegion(DestinationRegionId) == nullptr)
        {
            return EDATravelResult::Rejected;
        }

        PersistentCampaign.WorldState.TravelHandoff = FDATravelHandoffState{};
        PersistentCampaign.WorldState.TravelHandoff.RequestId = RequestId;
        PersistentCampaign.WorldState.TravelHandoff.SourceRegionId = PersistentCampaign.WorldState.CurrentRegionId;
        PersistentCampaign.WorldState.TravelHandoff.DestinationRegionId = DestinationRegionId;
        PersistentCampaign.WorldState.TravelHandoff.DepartureWorldTick = PersistentCampaign.WorldState.CurrentWorldTick;
        PersistentCampaign.WorldState.TravelHandoff.TravelWorldTicks = TravelWorldTicks;
        PersistentCampaign.WorldState.TravelHandoff.Stage = EDATravelHandoffStage::Started;
        ++PersistentCampaign.CampaignMutationRevision;
    }
    else if (PersistentCampaign.WorldState.TravelHandoff.RequestId != RequestId
        || PersistentCampaign.WorldState.TravelHandoff.DestinationRegionId != DestinationRegionId
        || PersistentCampaign.WorldState.TravelHandoff.TravelWorldTicks != TravelWorldTicks)
    {
        return EDATravelResult::Rejected;
    }

    FDARegionState* SourceRegion = PersistentCampaign.WorldState.FindRegion(PersistentCampaign.WorldState.TravelHandoff.SourceRegionId);
    FDARegionState* DestinationRegion = PersistentCampaign.WorldState.FindRegion(PersistentCampaign.WorldState.TravelHandoff.DestinationRegionId);
    if (SourceRegion == nullptr || DestinationRegion == nullptr)
    {
        return EDATravelResult::Rejected;
    }

    if (!HasReached(PersistentCampaign.WorldState.TravelHandoff.Stage, EDATravelHandoffStage::SourceSnapshotted))
    {
        FDARegionPersistentDelta Snapshot;
        FString Error;
        if (!Runtime.SnapshotPersistentDelta(*SourceRegion, Snapshot)
            || Snapshot.Revision <= SourceRegion->PersistentDelta.Revision
            || !Snapshot.Validate(Error))
        {
            return EDATravelResult::RuntimeFailure;
        }
        SourceRegion->PersistentDelta = MoveTemp(Snapshot);
        if (!CompleteTravelStage(EDATravelHandoffStage::SourceSnapshotted))
        {
            return EDATravelResult::RuntimeFailure;
        }
    }

    if (!HasReached(PersistentCampaign.WorldState.TravelHandoff.Stage, EDATravelHandoffStage::SourceUnloaded))
    {
        if (!Runtime.UnloadRegionRepresentation(PersistentCampaign.WorldState.TravelHandoff.SourceRegionId))
        {
            return EDATravelResult::RuntimeFailure;
        }
        if (!CompleteTravelStage(EDATravelHandoffStage::SourceUnloaded))
        {
            return EDATravelResult::RuntimeFailure;
        }
    }

    if (!HasReached(PersistentCampaign.WorldState.TravelHandoff.Stage, EDATravelHandoffStage::TimeAdvanced))
    {
        const int64 DepartureWorldTick = PersistentCampaign.WorldState.TravelHandoff.DepartureWorldTick;
        const int64 TravelWorldTicks64 = PersistentCampaign.WorldState.TravelHandoff.TravelWorldTicks;
        if (DepartureWorldTick < 0
            || TravelWorldTicks64 <= 0
            || DepartureWorldTick > MAX_int64 - TravelWorldTicks64)
        {
            return EDATravelResult::Rejected;
        }
        const int64 ArrivalWorldTick = DepartureWorldTick + TravelWorldTicks64;
        if (PersistentCampaign.WorldState.CurrentWorldTick < DepartureWorldTick
            || PersistentCampaign.WorldState.CurrentWorldTick > ArrivalWorldTick)
        {
            return EDATravelResult::Rejected;
        }
        const int64 PendingTravelWorldTicks64 = ArrivalWorldTick - PersistentCampaign.WorldState.CurrentWorldTick;
        if (PendingTravelWorldTicks64 > MAX_int32)
        {
            return EDATravelResult::Rejected;
        }
        const int32 PendingTravelWorldTicks = static_cast<int32>(PendingTravelWorldTicks64);
        if (!AdvanceWorldTicks(PendingTravelWorldTicks))
        {
            return EDATravelResult::RuntimeFailure;
        }
        if (!CompleteTravelStage(EDATravelHandoffStage::TimeAdvanced))
        {
            return EDATravelResult::RuntimeFailure;
        }
    }

    if (!HasReached(PersistentCampaign.WorldState.TravelHandoff.Stage, EDATravelHandoffStage::DestinationLoaded))
    {
        if (!Runtime.EnsureRegionLoaded(
            PersistentCampaign.WorldState.TravelHandoff.RequestId,
            PersistentCampaign.WorldState.TravelHandoff.DestinationRegionId))
        {
            return EDATravelResult::RuntimeFailure;
        }
        if (!CompleteTravelStage(EDATravelHandoffStage::DestinationLoaded))
        {
            return EDATravelResult::RuntimeFailure;
        }
    }

    DestinationRegion = PersistentCampaign.WorldState.FindRegion(PersistentCampaign.WorldState.TravelHandoff.DestinationRegionId);
    if (!HasReached(PersistentCampaign.WorldState.TravelHandoff.Stage, EDATravelHandoffStage::DestinationReconstructed))
    {
        if (DestinationRegion == nullptr
            || !Runtime.EnsureRegionReconstructed(
                PersistentCampaign.WorldState.TravelHandoff.RequestId,
                *DestinationRegion,
                PersistentCampaign))
        {
            return EDATravelResult::RuntimeFailure;
        }
        if (!CompleteTravelStage(EDATravelHandoffStage::DestinationReconstructed))
        {
            return EDATravelResult::RuntimeFailure;
        }
    }

    FDACampaignSnapshot CompletedCampaign = PersistentCampaign;
    FDAWorldCampaignState& CompletedState = CompletedCampaign.WorldState;
    CompletedState.CurrentRegionId = CompletedState.TravelHandoff.DestinationRegionId;
    CompletedState.CompletedTravelRequestIds.Add(CompletedState.TravelHandoff.RequestId);
    CompletedState.TravelHandoff.Stage = EDATravelHandoffStage::Completed;
    if (CompletedCampaign.CampaignMutationRevision == MAX_int64)
    {
        return EDATravelResult::RuntimeFailure;
    }
    ++CompletedCampaign.CampaignMutationRevision;
    FString Error;
    if (!CompletedCampaign.Validate(Error))
    {
        return EDATravelResult::RuntimeFailure;
    }
    PersistentCampaign = MoveTemp(CompletedCampaign);
    OnTravelStageCompleted.Broadcast(EDATravelHandoffStage::Completed);
    return EDATravelResult::Completed;
}

bool UDAWorldStateSubsystem::RegisterTravelRuntime(UObject* Runtime, FString& OutError)
{
    if (Runtime == nullptr || Cast<IDARegionTravelRuntimeService>(Runtime) == nullptr)
    {
        OutError = TEXT("ServiceUnavailable: world travel runtime must implement "
            "IDARegionTravelRuntimeService.");
        return false;
    }
    RegisteredTravelRuntime = Runtime;
    OutError.Reset();
    return true;
}

void UDAWorldStateSubsystem::UnregisterTravelRuntime(UObject* Runtime)
{
    if (RegisteredTravelRuntime.Get() == Runtime) RegisteredTravelRuntime.Reset();
}

EDATravelResult UDAWorldStateSubsystem::TravelUsingRegisteredRuntime(
    const FName RequestId, const FName DestinationRegionId, const int32 TravelWorldTicks,
    FString& OutError)
{
    IDARegionTravelRuntimeService* Runtime =
        Cast<IDARegionTravelRuntimeService>(RegisteredTravelRuntime.Get());
    if (Runtime == nullptr)
    {
        OutError = TEXT("ServiceUnavailable: no live world streaming/reconstruction runtime is registered.");
        return EDATravelResult::RuntimeFailure;
    }
    class FRegisteredTravelRuntime final : public IDARegionTravelRuntime
    {
    public:
        explicit FRegisteredTravelRuntime(IDARegionTravelRuntimeService& InRuntime) : Target(InRuntime) {}
        virtual bool SnapshotPersistentDelta(const FDARegionState& Source,
            FDARegionPersistentDelta& OutDelta) override
        { return Target.SnapshotPersistentDelta(Source, OutDelta); }
        virtual bool UnloadRegionRepresentation(const FName RegionId) override
        { return Target.UnloadRegionRepresentation(RegionId); }
        virtual bool EnsureRegionLoaded(const FName RequestIdValue, const FName RegionId) override
        { return Target.EnsureRegionLoaded(RequestIdValue, RegionId); }
        virtual bool EnsureRegionReconstructed(const FName RequestIdValue,
            const FDARegionState& Region, const FDACampaignSnapshot& Campaign) override
        { return Target.EnsureRegionReconstructed(RequestIdValue, Region, Campaign); }
    private:
        IDARegionTravelRuntimeService& Target;
    } Forwarder(*Runtime);
    const EDATravelResult Result = Travel(RequestId, DestinationRegionId, TravelWorldTicks, Forwarder);
    if (Result == EDATravelResult::Completed || Result == EDATravelResult::AlreadyCompleted)
        OutError.Reset();
    else
        OutError = TEXT("Registered world travel handoff rejected the request.");
    return Result;
}

bool UDAWorldStateSubsystem::TryPlacePlayerWorldAsset(
    const FGuid CardInstanceId, const FName CityId, const FIntPoint Origin,
    const FIntPoint RequestedFootprint, const EGridRotation Rotation,
    FGuid& OutWorldAssetId, FString& OutError)
{
    OutWorldAssetId.Invalidate();
    if (CityId != TEXT("player_capital")
        || PersistentCampaign.WorldState.CurrentRegionId != TEXT("region.synara_frontier"))
    {
        OutError = TEXT("Placement target is not the claimed player capital in the current region.");
        return false;
    }

    UDAContentRegistrySubsystem* Registry = GetGameInstance() == nullptr ? nullptr
        : GetGameInstance()->GetSubsystem<UDAContentRegistrySubsystem>();
    FDACampaignSnapshot Candidate = PersistentCampaign;
    FCardInstance* Card = Candidate.CollectionState.FindInstance(CardInstanceId);
    UDA_CardDefinition* Definition = Card == nullptr || Registry == nullptr
        ? nullptr : Registry->GetCardDefinition(Card->DefinitionId);
    if (Card == nullptr || Definition == nullptr || !Definition->bPlaceable
        || Card->RecoveryState != EDARecoveryState::Available
        || Definition->Footprint.X <= 0 || Definition->Footprint.Y <= 0
        || RequestedFootprint != Definition->Footprint)
    {
        OutError = TEXT("Placement requires an available card and its exact definition-authored footprint.");
        return false;
    }

    const FDACityGridClaimState* CityGridClaims = Candidate.FindCityGridClaims(CityId);
    if (CityGridClaims == nullptr)
    { OutError = TEXT("Placement requires persisted canonical grid claims for the target city."); return false; }
    FDACityGridSubsystem Grid(CityGridClaims->Width, CityGridClaims->Height);
    if (!ReconstructClaimedCityGrid(Candidate, CityId, *Registry, Grid, OutError)) return false;

    const FGuid ConstructionId = FGuid::NewGuid();
    if (!Grid.ReserveFootprint(FDAWorldAssetId(ConstructionId), Origin,
        Definition->Footprint, Rotation))
    {
        OutError = TEXT("Canonical claimed city grid rejected the authored footprint.");
        return false;
    }

    int32 Capital = 0; int32 Insight = 0; int32 Influence = 0;
    Definition->TryGetDeploymentCapital(Capital);
    Definition->TryGetDeploymentInsight(Insight);
    Definition->TryGetDeploymentInfluence(Influence);
    if (Candidate.CitySimulationState.Wallet.Capital < Capital
        || Candidate.CitySimulationState.Wallet.Insight < Insight
        || Candidate.CitySimulationState.Wallet.Influence < Influence
        || Candidate.LiveSignals.MutationRevision == MAX_int64)
    {
        OutError = TEXT("Placement wallet requirements are not met.");
        return false;
    }

    int32 AuthoredConstructionCycles = 0;
    const bool bHasAuthoredConstructionTiming =
        Definition->TryGetConstructionCycles(AuthoredConstructionCycles);
    if (bHasAuthoredConstructionTiming && AuthoredConstructionCycles <= 0)
    {
        OutError = TEXT("Placement rejects invalid authored construction timing.");
        return false;
    }
    if (!Candidate.DeckState.TryDeployFromHand(CardInstanceId, OutError))
    {
        return false;
    }
    FDAWorldAssetRecord& Asset = Candidate.WorldAssets.Emplace_GetRef();
    Asset.WorldAssetId = ConstructionId;
    Asset.CardInstanceId = Card->InstanceId;
    Asset.CardDefinitionId = Card->DefinitionId;
    Asset.CityId = CityId;
    Asset.GridOrigin = Origin;
    Asset.Rotation = static_cast<uint8>(Rotation);
    Asset.OwnerCivilizationId = TEXT("civilization.synara");
    // Definitions with an explicit duration enter construction. Definitions whose
    // authored schema intentionally omits timing deploy immediately; absence is not
    // laundered into an invented duration.
    Asset.ConstructionState = bHasAuthoredConstructionTiming
        ? EDAConstructionState::Foundation : EDAConstructionState::Operational;
    Asset.ConstructionCyclesRequired = bHasAuthoredConstructionTiming
        ? AuthoredConstructionCycles : 0;
    Card->RecoveryState = EDARecoveryState::Deployed;
    Card->WorldAssetId = ConstructionId;
    Candidate.CitySimulationState.Wallet.Capital -= Capital;
    Candidate.CitySimulationState.Wallet.Insight -= Insight;
    Candidate.CitySimulationState.Wallet.Influence -= Influence;
    Candidate.CitySimulationState.Facilities.Add(MakeFacilityContext(Asset, *Definition));
    ProjectCitySimulationState(Candidate, true);

    const FDACampaignSnapshot& Authority = PersistentCampaign;
    if (!TryCommitPersistentCampaign(Candidate, Authority.NarrativeState.MutationRevision,
        Authority.LiveSignals.MutationRevision, Authority.WorldState.CurrentWorldTick))
    {
        OutError = TEXT("Canonical WorldAsset construction transaction lost authority.");
        return false;
    }
    OutWorldAssetId = ConstructionId;
    OutError.Reset();
    return true;
}

bool UDAWorldStateSubsystem::CraftUnlockedBlueprint(
    const FName BlueprintId, const int32 Quantity,
    TArray<FGuid>& OutCardInstanceIds, FString& OutError)
{
    OutCardInstanceIds.Reset();
    EnsureSystems();
    const bool bAscensionBlueprint =
        PersistentCampaign.AscensionState.UnlockedBlueprintIds.Contains(BlueprintId);
    const bool bQuestBlueprint =
        PersistentCampaign.NarrativeState.QuestContentUnlockRecords.ContainsByPredicate(
            [BlueprintId](const FDAQuestContentUnlockRecord& Unlock)
            {
                return Unlock.Type == EDAQuestContentUnlockType::Blueprint
                    && Unlock.DefinitionId == BlueprintId;
            });
    UDA_CardDefinition* Definition = ContentRegistry == nullptr ? nullptr
        : ContentRegistry->GetCardDefinition(BlueprintId);
    int32 Capital = 0;
    int32 Insight = 0;
    FName RequiredFacility;
    if (Quantity < 1 || Quantity > 100 || (!bAscensionBlueprint && !bQuestBlueprint)
        || Definition == nullptr
        || !Definition->TryGetCraftCapital(Capital)
        || !Definition->TryGetCraftInsight(Insight)
        || !Definition->TryGetRequiredCraftingFacilityId(RequiredFacility)
        || Capital < 0 || Insight < 0)
    {
        OutError = TEXT("Crafting requires an unlocked authored blueprint and bounded quantity.");
        return false;
    }
    const bool bHasFacility = PersistentCampaign.WorldAssets.ContainsByPredicate(
        [RequiredFacility](const FDAWorldAssetRecord& Asset)
        {
            return Asset.CardDefinitionId == RequiredFacility
                && Asset.ConstructionState == EDAConstructionState::Operational;
        });
    const int64 CapitalCost = static_cast<int64>(Capital) * static_cast<int64>(Quantity);
    const int64 InsightCost = static_cast<int64>(Insight) * static_cast<int64>(Quantity);
    if (!bHasFacility || PersistentCampaign.CitySimulationState.Wallet.Capital < CapitalCost
        || PersistentCampaign.CitySimulationState.Wallet.Insight < InsightCost
        || PersistentCampaign.LiveSignals.MutationRevision == MAX_int64)
    {
        OutError = TEXT("Crafting facility or wallet requirements are not met.");
        return false;
    }

    FDACampaignSnapshot Candidate = PersistentCampaign;
    Candidate.CitySimulationState.Wallet.Capital -= CapitalCost;
    Candidate.CitySimulationState.Wallet.Insight -= InsightCost;
    ProjectCitySimulationState(Candidate, true);
    for (int32 Index = 0; Index < Quantity; ++Index)
        OutCardInstanceIds.Add(Candidate.CollectionState.AddInstance(
            BlueprintId, EDAAcquisitionSource::Crafting,
            Candidate.WorldState.CurrentWorldTick));
    const FDACampaignSnapshot& Authority = PersistentCampaign;
    if (!TryCommitPersistentCampaign(Candidate,
        Authority.NarrativeState.MutationRevision,
        Authority.LiveSignals.MutationRevision,
        Authority.WorldState.CurrentWorldTick))
    {
        OutCardInstanceIds.Reset();
        OutError = TEXT("Crafting transaction lost canonical campaign authority.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UDAWorldStateSubsystem::TryCancelPlayerWorldAsset(
    const FGuid WorldAssetId, FString& OutError)
{
    FDACampaignSnapshot Candidate = PersistentCampaign;
    const int32 Index = Candidate.WorldAssets.IndexOfByPredicate(
        [WorldAssetId](const FDAWorldAssetRecord& Asset)
        {
            return Asset.WorldAssetId == WorldAssetId;
        });
    if (Index == INDEX_NONE
        || Candidate.WorldAssets[Index].OwnerCivilizationId != TEXT("civilization.synara")
        || Candidate.WorldAssets[Index].ConstructionState == EDAConstructionState::Operational)
    {
        OutError = TEXT("Only an active non-operational player placement can be cancelled.");
        return false;
    }
    const FGuid CardInstanceId = Candidate.WorldAssets[Index].CardInstanceId;
    FCardInstance* Card = Candidate.CollectionState.FindInstance(CardInstanceId);
    if (Card == nullptr
        || !Candidate.DeckState.TryRecoverDeployedInstance(CardInstanceId, OutError))
    {
        return false;
    }
    Card->RecoveryState = EDARecoveryState::Available;
    Card->WorldAssetId.Invalidate();
    Candidate.CitySimulationState.Facilities.RemoveAll(
        [WorldAssetId](const FDAFacilityContext& Facility)
        {
            return Facility.AssetRecord.WorldAssetId == WorldAssetId;
        });
    Candidate.WorldAssets.RemoveAt(Index);
    ProjectCitySimulationState(Candidate, true);
    const FDACampaignSnapshot& Authority = PersistentCampaign;
    if (!TryCommitPersistentCampaign(Candidate,
        Authority.NarrativeState.MutationRevision,
        Authority.LiveSignals.MutationRevision,
        Authority.WorldState.CurrentWorldTick))
    {
        OutError = TEXT("Canonical cancellation transaction lost authority.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UDAWorldStateSubsystem::ResolvePlayerFacilityOutput(
    const FGuid WorldAssetId, const FDAWalletValues& BaseOutput,
    FDAFacilityOutput& OutOutput, FString& OutError)
{
    OutOutput = {};
    EnsureSystems();
    const FDAWorldAssetRecord* Asset = PersistentCampaign.FindWorldAssetRecord(WorldAssetId);
    const UDA_CardDefinition* Definition = Asset == nullptr || ContentRegistry == nullptr
        ? nullptr : ContentRegistry->GetCardDefinition(Asset->CardDefinitionId);
    if (EconomySubsystem == nullptr || Asset == nullptr || Definition == nullptr
        || Asset->OwnerCivilizationId != TEXT("civilization.synara")
        || Asset->ConstructionState != EDAConstructionState::Operational
        || Asset->CardDefinitionId != TEXT("fusion.autonomous_factory"))
    {
        OutError = TEXT("Facility output requires the operational player-owned Autonomous Factory.");
        return false;
    }
    FDAFacilityContext Context;
    Context.AssetRecord = *Asset;
    Context.BaseOutput = BaseOutput;
    Context.FacilityType = EDAFacilityType::Industrial;
    if (!EconomySubsystem->ApplyAutonomousFactoryThroughput(Context, *Definition))
    {
        OutError = TEXT("Facility economy rejected Autonomous Factory throughput authorship.");
        return false;
    }
    OutOutput = EconomySubsystem->CalculateFacilityOutput(Context);
    OutError.Reset();
    return true;
}

bool UDAWorldStateSubsystem::ReconstructClaimedCityGrid(const FDACampaignSnapshot& Campaign,
    const FName CityId, UDAContentRegistrySubsystem& Registry, FDACityGridSubsystem& OutGrid,
    FString& OutError)
{
    const FDACityGridClaimState* CityGridClaims = Campaign.FindCityGridClaims(CityId);
    FString ClaimError;
    if (CityGridClaims == nullptr || !CityGridClaims->Validate(ClaimError))
    {
        OutError = TEXT("Canonical city grid reconstruction requires valid persisted grid claims.");
        return false;
    }
    for (const FIntPoint Cell : CityGridClaims->ClaimedCells)
        if (!OutGrid.SetCellClaimed(Cell, true))
        { OutError = TEXT("Persisted city grid claim is outside the authored grid."); return false; }
    for (const FDAWorldAssetRecord& Existing : Campaign.WorldAssets)
    {
        if (Existing.CityId != CityId) continue;
        UDA_CardDefinition* ExistingDefinition = Registry.GetCardDefinition(Existing.CardDefinitionId);
        int32 AuthoredCycles = 0;
        const bool bHasAuthoredCycles = ExistingDefinition != nullptr
            && ExistingDefinition->TryGetConstructionCycles(AuthoredCycles);
        const bool bUntimedOperational = Existing.ConstructionState
                == EDAConstructionState::Operational
            && Existing.ConstructionCyclesRequired == 0
            && !bHasAuthoredCycles;
        if (ExistingDefinition == nullptr || ExistingDefinition->Footprint.X <= 0
            || ExistingDefinition->Footprint.Y <= 0 || Existing.Rotation > 3
            || (!bUntimedOperational
                && (!bHasAuthoredCycles
                    || AuthoredCycles <= 0
                    || Existing.ConstructionCyclesRequired != AuthoredCycles))
            || !OutGrid.ReserveFootprint(FDAWorldAssetId(Existing.WorldAssetId), Existing.GridOrigin,
                ExistingDefinition->Footprint, static_cast<EGridRotation>(Existing.Rotation)))
        {
            OutError = TEXT("Canonical city grid could not reconstruct exact authored footprint and timing records.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

EDAFoundryShortageActionResult UDAWorldStateSubsystem::ResolveFoundryShortage(
    const FGuid ActionId, const EDAFoundryShortageResolution Resolution)
{
    EnsureSystems();
    if (DiplomacySystem == nullptr || RegionalCrisisContentRegistry == nullptr
        || !RegionalCrisisContentRegistry->IsReady())
        return EDAFoundryShortageActionResult::Rejected;
    FDACampaignSnapshot Candidate = PersistentCampaign;
    FString Error;
    const EDAFoundryShortageActionResult Result = FDAFoundryShortageRuntime::Resolve(
        RegionalCrisisContentRegistry->GetManifest(), ActionId, Resolution, Candidate.WorldState.CurrentWorldTick,
        Candidate, *DiplomacySystem, Error);
    if (Result != EDAFoundryShortageActionResult::Applied) return Result;
    if (!FDARegionalCampaignCoordinatorRuntime::ApplyFoundryQuestResolution(
        RegionalCrisisContentRegistry->GetManifest(), ActionId, Resolution,
        Candidate.WorldState.CurrentWorldTick, Candidate, Error))
        return EDAFoundryShortageActionResult::Rejected;
    if (!Candidate.Validate(Error)) return EDAFoundryShortageActionResult::Rejected;
    const FDACampaignSnapshot& Authority = PersistentCampaign;
    return TryCommitPersistentCampaign(Candidate, Authority.NarrativeState.MutationRevision,
        Authority.LiveSignals.MutationRevision, Authority.WorldState.CurrentWorldTick)
        ? Result : EDAFoundryShortageActionResult::Rejected;
}

void UDAWorldStateSubsystem::EnsureSystems()
{
    if (TradeSystem == nullptr)
    {
        TradeSystem = NewObject<UDATradeSystem>(this);
    }
    if (DiplomacySystem == nullptr)
    {
        DiplomacySystem = NewObject<UDADiplomacySystem>(this);
    }
    if (SimulationClock == nullptr)
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            SimulationClock = GameInstance->GetSubsystem<UDASimulationClockSubsystem>();
        }
    }
    if (ForgeweaveStrategy == nullptr)
    {
        ForgeweaveStrategy = NewObject<UDAForgeweaveStrategy>(this);
    }
    if (RegionalCrisisContentRegistry == nullptr && GetGameInstance() != nullptr)
        RegionalCrisisContentRegistry = GetGameInstance()->GetSubsystem<UDARegionalCrisisContentRegistrySubsystem>();
    if (ContentRegistry == nullptr && GetGameInstance() != nullptr)
        ContentRegistry = GetGameInstance()->GetSubsystem<UDAContentRegistrySubsystem>();
    if (SystemicPressureSystem == nullptr)
        SystemicPressureSystem = NewObject<UDASystemicPressureSystem>(this);
    if (JobSystem == nullptr)
        JobSystem = NewObject<UDAJobSystem>(this);
    if (MigrationSystem == nullptr)
        MigrationSystem = NewObject<UDAMigrationSystem>(this);
}

bool UDAWorldStateSubsystem::CompleteTravelStage(const EDATravelHandoffStage Stage)
{
    if (PersistentCampaign.CampaignMutationRevision == MAX_int64)
    {
        return false;
    }
    PersistentCampaign.WorldState.TravelHandoff.Stage = Stage;
    ++PersistentCampaign.CampaignMutationRevision;
    OnTravelStageCompleted.Broadcast(Stage);
    return true;
}

void UDAWorldStateSubsystem::HandleSimulationWorldTickPreCommit(
    const int64 WorldTick,
    FDAWorldTickVeto& Veto)
{
    PreparedWorldTickState.Reset();
    PreparedWorldTick = INDEX_NONE;
    if (!PersistentCampaign.WorldState.bInitialized)
    {
        return;
    }
    if (Veto.IsRejected()
        || PersistentCampaign.WorldState.CurrentWorldTick == MAX_int64
        || PersistentCampaign.CampaignMutationRevision == MAX_int64
        || WorldTick != PersistentCampaign.WorldState.CurrentWorldTick + 1)
    {
        Veto.Reject();
        return;
    }

    FDACampaignSnapshot Candidate = PersistentCampaign;
    if (TradeSystem == nullptr
        || DiplomacySystem == nullptr
        || ForgeweaveStrategy == nullptr
        || TradeSystem->ProcessWorldTick(
            Candidate.WorldState.Trade,
            Candidate.WorldState.Diplomacy,
            *DiplomacySystem,
            WorldTick) != EDATradeTickResult::Processed)
    {
        Veto.Reject();
        return;
    }
    if (Candidate.WorldState.Forgeweave.bInitialized)
    {
        const FDAForgeweaveTickResult ForgeweaveTick = ForgeweaveStrategy->ProcessWorldTick(
            Candidate,
            WorldTick);
        if (!ForgeweaveTick.bCommitted)
        {
            Veto.Reject();
            return;
        }
    }
    else
    {
        Candidate.WorldState.CurrentWorldTick = WorldTick;
    }

    if (MigrationSystem == nullptr || Candidate.LiveSignals.MutationRevision == MAX_int64)
    {
        Veto.Reject();
        return;
    }
    MigrationSystem->ResolveWorldTick(Candidate.CitySimulationState);
    ProjectCitySimulationState(Candidate, true);

    // The simulation clock has not committed its cycle/tick counters while this vetoable
    // candidate is prepared. Do not persist the previous tick as if it owned the candidate;
    // HandleSimulationWorldTick captures the real post-commit clock before publication.
    Candidate.WorldState.ClockAuthority = FDASimulationClockAuthorityState{};

    FString CrisisRuntimeError;
    if (RegionalCrisisContentRegistry == nullptr || !RegionalCrisisContentRegistry->IsReady()
        || !FDAFoundryShortageRuntime::ProcessWorldTick(
            RegionalCrisisContentRegistry->GetManifest(), WorldTick, Candidate, *DiplomacySystem, CrisisRuntimeError))
    {
        Veto.Reject();
        return;
    }

    FString Error;
    if (!FDAConquestSystem::Synchronize(Candidate, Error) || !Candidate.Validate(Error))
    {
        Veto.Reject();
        return;
    }
    PreparedWorldTickState.Emplace(MoveTemp(Candidate));
    PreparedWorldTick = WorldTick;
}

void UDAWorldStateSubsystem::HandleSimulationWorldTickAborted(const int64 WorldTick)
{
    if (PreparedWorldTickState.IsSet() && PreparedWorldTick == WorldTick)
    {
        PreparedWorldTickState.Reset();
        PreparedWorldTick = INDEX_NONE;
    }
}

void UDAWorldStateSubsystem::HandleSimulationWorldTick(const int64 WorldTick)
{
    if (!PersistentCampaign.WorldState.bInitialized)
    {
        return;
    }
    if (!PreparedWorldTickState.IsSet() || PreparedWorldTick != WorldTick)
    {
        return;
    }

    PreparedWorldTickState->WorldState.ClockAuthority = SimulationClock != nullptr
        ? SimulationClock->CaptureAuthorityState() : FDASimulationClockAuthorityState{};
    const bool bEmitFoundryWarning = PreparedWorldTickState->RegionalCrisis.WarningEmissionCount
        > PersistentCampaign.RegionalCrisis.WarningEmissionCount;
    PreparedWorldTickState->CampaignMutationRevision =
        PersistentCampaign.CampaignMutationRevision + 1;
    PersistentCampaign = MoveTemp(PreparedWorldTickState.GetValue());
    if (SystemicPressureSystem != nullptr)
        SystemicPressureSystem->SetDependency(
            static_cast<float>(PersistentCampaign.SynaraState.Dependency));
    PreparedWorldTickState.Reset();
    PreparedWorldTick = INDEX_NONE;

    // Broadcast an immutable owned snapshot. A listener may retain it or
    // re-enter this subsystem without changing the payload seen by peers.
    const FDACommittedCampaignSnapshot CommittedState =
        MakeShared<FDACampaignSnapshot>(PersistentCampaign);
    OnWorldTickStateCommitted.Broadcast(CommittedState);
    if (bEmitFoundryWarning) OnFoundryShortageWarning.Broadcast(CommittedState);
}
