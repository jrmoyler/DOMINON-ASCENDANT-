#pragma once

#include "Cards/DACollectionState.h"
#include "Cards/DADeckState.h"
#include "Campaign/DACampaignAuthority.h"
#include "Campaign/DACampaignLiveSignals.h"
#include "Campaign/DAConquestCampaignState.h"
#include "Campaign/DADaxtonCampaignState.h"
#include "Campaign/DAAscensionCampaignState.h"
#include "Campaign/DARegionalCrisisCampaignState.h"
#include "City/DACityGridClaimState.h"
#include "Conflict/DAConflictRecords.h"
#include "GameFramework/SaveGame.h"
#include "Narrative/DANarrativeRecords.h"
#include "Simulation/DACitySimulationState.h"
#include "World/DAWorldAssetRecord.h"
#include "World/DARegionalWorldState.h"

#include "DACampaignSaveGame.generated.h"

// A persistence-only aggregate of authoritative records. It deliberately contains no actor references.
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACampaignSnapshot
{
    GENERATED_BODY()

    FDACampaignSnapshot()
    {
        RebindRuntimeReferences();
    }

    FDACampaignSnapshot(const FDACampaignSnapshot& Other)
        : CampaignMutationRevision(Other.CampaignMutationRevision)
        , CollectionState(Other.CollectionState)
        , DeckState(Other.DeckState)
        , WorldAssets(Other.WorldAssets)
        , OperationConflict(Other.OperationConflict)
        , WorldState(Other.WorldState)
        , SynaraState(Other.SynaraState)
        , LiveSignals(Other.LiveSignals)
        , CitySimulationState(Other.CitySimulationState)
        , CityGridClaims(Other.CityGridClaims)
        , RegionalCrisis(Other.RegionalCrisis)
        , ConquestState(Other.ConquestState)
        , DaxtonState(Other.DaxtonState)
        , AscensionState(Other.AscensionState)
        , HistoryTags(Other.HistoryTags)
        , NarrativeState(Other.NarrativeState)
    {
        RebindRuntimeReferences();
    }

    FDACampaignSnapshot(FDACampaignSnapshot&& Other)
        : CampaignMutationRevision(Other.CampaignMutationRevision)
        , CollectionState(MoveTemp(Other.CollectionState))
        , DeckState(MoveTemp(Other.DeckState))
        , WorldAssets(MoveTemp(Other.WorldAssets))
        , OperationConflict(MoveTemp(Other.OperationConflict))
        , WorldState(MoveTemp(Other.WorldState))
        , SynaraState(MoveTemp(Other.SynaraState))
        , LiveSignals(MoveTemp(Other.LiveSignals))
        , CitySimulationState(MoveTemp(Other.CitySimulationState))
        , CityGridClaims(MoveTemp(Other.CityGridClaims))
        , RegionalCrisis(MoveTemp(Other.RegionalCrisis))
        , ConquestState(MoveTemp(Other.ConquestState))
        , DaxtonState(MoveTemp(Other.DaxtonState))
        , AscensionState(MoveTemp(Other.AscensionState))
        , HistoryTags(MoveTemp(Other.HistoryTags))
        , NarrativeState(MoveTemp(Other.NarrativeState))
    {
        RebindRuntimeReferences();
    }

    FDACampaignSnapshot& operator=(const FDACampaignSnapshot& Other)
    {
        if (this != &Other)
        {
            CampaignMutationRevision = Other.CampaignMutationRevision;
            CollectionState = Other.CollectionState;
            DeckState = Other.DeckState;
            WorldAssets = Other.WorldAssets;
            OperationConflict = Other.OperationConflict;
            WorldState = Other.WorldState;
            SynaraState = Other.SynaraState;
            LiveSignals = Other.LiveSignals;
            CitySimulationState = Other.CitySimulationState;
            CityGridClaims = Other.CityGridClaims;
            RegionalCrisis = Other.RegionalCrisis;
            ConquestState = Other.ConquestState;
            DaxtonState = Other.DaxtonState;
            AscensionState = Other.AscensionState;
            HistoryTags = Other.HistoryTags;
            NarrativeState = Other.NarrativeState;
            RebindRuntimeReferences();
        }
        return *this;
    }

    FDACampaignSnapshot& operator=(FDACampaignSnapshot&& Other)
    {
        if (this != &Other)
        {
            CampaignMutationRevision = Other.CampaignMutationRevision;
            CollectionState = MoveTemp(Other.CollectionState);
            DeckState = MoveTemp(Other.DeckState);
            WorldAssets = MoveTemp(Other.WorldAssets);
            OperationConflict = MoveTemp(Other.OperationConflict);
            WorldState = MoveTemp(Other.WorldState);
            SynaraState = MoveTemp(Other.SynaraState);
            LiveSignals = MoveTemp(Other.LiveSignals);
            CitySimulationState = MoveTemp(Other.CitySimulationState);
            CityGridClaims = MoveTemp(Other.CityGridClaims);
            RegionalCrisis = MoveTemp(Other.RegionalCrisis);
            ConquestState = MoveTemp(Other.ConquestState);
            DaxtonState = MoveTemp(Other.DaxtonState);
            AscensionState = MoveTemp(Other.AscensionState);
            HistoryTags = MoveTemp(Other.HistoryTags);
            NarrativeState = MoveTemp(Other.NarrativeState);
            RebindRuntimeReferences();
        }
        return *this;
    }

    void RebindRuntimeReferences()
    {
        DeckState.BindCollection(CollectionState);
    }

    FDAWorldAssetRecord* FindWorldAssetRecord(FGuid WorldAssetId);
    const FDAWorldAssetRecord* FindWorldAssetRecord(FGuid WorldAssetId) const;
    FDACityGridClaimState* FindCityGridClaims(FName CityId);
    const FDACityGridClaimState* FindCityGridClaims(FName CityId) const;
    bool Validate(FString& OutError) const;

    /** Universal persisted CAS token. Every accepted aggregate mutation advances exactly once. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campaign")
    int64 CampaignMutationRevision = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDACollectionState CollectionState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDADeckState DeckState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    TArray<FDAWorldAssetRecord> WorldAssets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDAOperationConflictSnapshot OperationConflict;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDAWorldCampaignState WorldState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDASynaraCampaignState SynaraState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDACampaignLiveSignalState LiveSignals;

    /** Canonical persisted city/economy/citizen state; LiveSignals is its transactional projection. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDACitySimulationState CitySimulationState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    TArray<FDACityGridClaimState> CityGridClaims;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDARegionalCrisisCampaignState RegionalCrisis;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDAConquestCampaignState ConquestState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDADaxtonCampaignState DaxtonState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDAAscensionCampaignState AscensionState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    TArray<FName> HistoryTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campaign")
    FDANarrativeCampaignState NarrativeState;
};

UCLASS()
class DOMINIONCORE_API UDACampaignSaveGame : public USaveGame, public IDACampaignAuthority
{
    GENERATED_BODY()

public:
    virtual const FDACampaignSnapshot& GetPersistentCampaign() const override { return Snapshot; }
    virtual bool TryCommitPersistentCampaign(const FDACampaignSnapshot& Candidate,
        int64 ExpectedNarrativeRevision, int64 ExpectedSignalRevision,
        int64 ExpectedWorldTick) override;

    UPROPERTY(VisibleAnywhere, Category = "Campaign")
    FDACampaignSnapshot Snapshot;
};
