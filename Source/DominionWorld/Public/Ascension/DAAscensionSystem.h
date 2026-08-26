#pragma once

#include "CoreMinimal.h"
#include "Save/DACampaignSaveGame.h"

#include "DAAscensionSystem.generated.h"

class FDAAdjacencySubsystem;
class UDA_CardDefinition;
class UDAContentRegistrySubsystem;

UENUM(BlueprintType)
enum class EDAAscensionPresentationBeat : uint8
{
    SystemsHaltAndReact,
    ForgeRelicEmerges,
    WorldTransit,
    FounderHallReceivesRelic,
    Unlocks
};

/** A read-only runtime presentation projection. Cinematic playback is deliberately not authoritative. */
USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAAscensionPresentationState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bAscended = false;
    UPROPERTY(BlueprintReadOnly) int32 FounderHallVisualState = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FDAFounderHallRelicPosition> RelicPositions;
    UPROPERTY(BlueprintReadOnly) int32 ActiveRelicSlotCount = 0;
    UPROPERTY(BlueprintReadOnly) bool bHiddenChamberOpen = false;
    UPROPERTY(BlueprintReadOnly) bool bShouldPlayCinematic = false;
    UPROPERTY(BlueprintReadOnly) bool bCinematicMayBeSkipped = true;
    UPROPERTY(BlueprintReadOnly) FSoftObjectPath CinematicSequenceAsset;
    UPROPERTY(BlueprintReadOnly) TArray<EDAAscensionPresentationBeat> OrderedBeats;
    UPROPERTY(BlueprintReadOnly) FString ConvergenceAuthorityLabel;
};

/** Candidate-only mutations for First Ascension, Replication, and Factory cycle pressure. */
class DOMINIONWORLD_API FDAAscensionSystem
{
public:
    static bool ApplyFirstAscension(FGuid ActionId, FDACampaignSnapshot& InOutCampaign,
        FString& OutError);
    static bool ReplicateCard(FGuid ActionId, FGuid SourceCardInstanceId,
        const UDAContentRegistrySubsystem& Registry, int64 DevelopmentCycle,
        FDACampaignSnapshot& InOutCampaign, FGuid& OutReplicatedCardInstanceId,
        FString& OutError);
    static bool ApplyAutonomousFactoryDevelopmentCycle(const TArray<FGuid>& FactoryWorldAssetIds,
        const UDA_CardDefinition& Definition, int64 DevelopmentCycle,
        FDACampaignSnapshot& InOutCampaign, FString& OutError);
    static bool ConfigureAutonomousFactoryAdjacency(FDAAdjacencySubsystem& Adjacency,
        const UDA_CardDefinition& Definition);
    static FDAAscensionPresentationState BuildPresentationState(
        const FDACampaignSnapshot& CommittedCampaign);
};
