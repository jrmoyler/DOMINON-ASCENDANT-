#pragma once

#include "Campaign/DACampaignLiveSignals.h"
#include "CoreMinimal.h"
#include "LOD/DASimulationLODPolicy.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DASimulationLODSubsystem.generated.h"

UENUM(BlueprintType)
enum class EDASimulationLOD : uint8
{
    LOD0_Full = 0,
    LOD1_Detailed = 1,
    LOD2_Aggregated = 2,
    LOD3_Strategic = 3,
};

UENUM(BlueprintType)
enum class EDACitizenLODRepresentation : uint8
{
    Individual,
    ImportantIndividual,
    Cohort,
    PersistentRecord,
    CohortAggregate,
    DistrictAggregate,
};

USTRUCT(BlueprintType)
struct DOMINIONSIMULATION_API FDASimulationLODContext
{
    GENERATED_BODY()

    static FDASimulationLODContext FounderDistrict(bool bCombat = false);
    static FDASimulationLODContext NearbyDistrict();
    static FDASimulationLODContext DistantActiveCity();
    static FDASimulationLODContext UnloadedMajorRegion();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation|LOD")
    bool bFounderDistrict = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation|LOD")
    bool bCombat = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation|LOD")
    bool bCurrentCity = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation|LOD")
    bool bNearbyDistrict = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation|LOD")
    bool bRegionLoaded = false;
};

USTRUCT(BlueprintType)
struct DOMINIONSIMULATION_API FDANamedCitizenLODState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Simulation|LOD")
    FName CitizenId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Simulation|LOD")
    EDASimulationLOD LOD = EDASimulationLOD::LOD3_Strategic;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Simulation|LOD")
    EDACitizenLODRepresentation Representation =
        EDACitizenLODRepresentation::PersistentRecord;
};

UENUM(BlueprintType)
enum class EDABenchmarkEvidenceState : uint8
{
    NotRun,
    NotReady,
    Captured,
};

USTRUCT(BlueprintType)
struct DOMINIONSIMULATION_API FDABenchmarkSummary
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    EDABenchmarkEvidenceState State = EDABenchmarkEvidenceState::NotRun;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    int32 FrameSampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    int32 DevelopmentCycleSampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    double AverageFramesPerSecond = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    double WorstFrameFramesPerSecond = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    double WorstDevelopmentCycleMilliseconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    double WorstModeTransitionSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    TArray<double> ModeTransitionSeconds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    double SevereHitchThresholdMilliseconds = 100.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    double TransitionBudgetSeconds = 1.5;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    bool bReferenceClassHardware = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Performance")
    bool bPerformanceTargetsPassed = false;
};

/** Actual platform description recorded by the benchmark process. */
struct DOMINIONSIMULATION_API FDABenchmarkHardwareCapture
{
    static FDABenchmarkHardwareCapture Detect(const FString& VerifiedStorageClass = TEXT("UNVERIFIED"));

    FString Cpu;
    FString Gpu;
    int32 SystemMemoryGiB = 0;
    FString Storage;
};

/** Canonical owner results measured during the same benchmark execution. */
struct DOMINIONSIMULATION_API FDABenchmarkSoakSummary
{
    int32 DevelopmentCycles = 0;
    int32 EnabledEventCount = 0;
    int32 PlayerEconomyDevelopmentCycles = 0;
    int32 TradeWorldTicks = 0;
    int32 ForgeweaveWorldTicks = 0;
    int32 UtilityTopologyNodeCount = 0;
    bool bUtilityTopologyResolved = false;
    bool bPopulationStayedNonnegative = false;
    int32 EventEmissionCount = 0;
    int32 EventTransitionCount = 0;
    bool bEventRunawayDetected = false;
    bool bStaleRequiredQuestDetected = false;
    bool bRequiredQuestResolutionFailed = false;
    int64 InitialSaveBytes = -1;
    int64 FinalSaveBytes = -1;
    FString RepeatabilityDigestA;
    FString RepeatabilityDigestB;
};

/** Writes only measured candidate evidence; the external runner owns command reconciliation. */
struct DOMINIONSIMULATION_API FDABenchmarkEvidenceCapture
{
    bool WriteCandidate(const FString& EvidencePath, const FString& PlanPath,
        FString& OutError) const;

    FDABenchmarkSummary Performance;
    FDABenchmarkHardwareCapture Hardware;
    FDABenchmarkSoakSummary Soak;
};

/** Exact deterministic workload contract. It is a recipe, not a measured capture. */
struct DOMINIONSIMULATION_API FDABenchmarkSceneLoad
{
    static FDABenchmarkSceneLoad VerticalSlice();
    int32 GetCitizenCount() const;
    bool Validate(FString& OutError) const;

    int32 GameplayAssetCount = 0;
    TArray<int32> CitizensByLOD;
    bool bWeatherActive = false;
    int32 AlliedSquadCount = 0;
    int32 EnemySquadCount = 0;
    int32 VehicleCount = 0;
    int32 ConstructionEffectCount = 0;
    int32 DamagedBuildingCount = 0;
    TArray<FName> ModeTransition;
};

/**
 * Owns transient citizen representations and benchmark samples. Canonical
 * CitizenID records remain in the campaign save and are reconstructed here.
 */
UCLASS()
class DOMINIONSIMULATION_API UDASimulationLODSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static constexpr int32 MinimumBenchmarkFrameSamples = 600;
    static constexpr double TargetFramesPerSecond = 60.0;
    static constexpr double FallbackFramesPerSecond = 30.0;
    static constexpr double SevereHitchThresholdMilliseconds = 100.0;
    static constexpr double TransitionBudgetSeconds = 1.5;

    static EDASimulationLOD ResolveLOD(const FDASimulationLODContext& Context);
    static EDACitizenLODRepresentation ResolveRepresentation(
        EDASimulationLOD LOD, bool bNamedCitizen);

    bool RegisterNamedCitizen(FName CitizenId);
    bool TransitionNamedCitizen(FName CitizenId, const FDASimulationLODContext& Context);
    bool RebuildNamedCitizensFromCampaign(
        const FDACampaignLiveSignalState& Signals, const FDASimulationLODContext& Context);
    const FDANamedCitizenLODState* FindNamedCitizen(FName CitizenId) const;

    void BeginBenchmarkCapture();
    bool RecordFrameSeconds(double Seconds);
    bool RecordDevelopmentCycleSeconds(double Seconds);
    bool RecordModeTransitionSeconds(double Seconds);
    FDABenchmarkSummary FinishBenchmarkCapture(bool bReferenceClassHardware);

private:
    UPROPERTY(Transient)
    TMap<FName, FDANamedCitizenLODState> NamedCitizens;

    bool bCaptureActive = false;
    int32 FrameSampleCount = 0;
    int32 DevelopmentCycleSampleCount = 0;
    int32 ModeTransitionSampleCount = 0;
    double TotalFrameSeconds = 0.0;
    double WorstFrameSeconds = 0.0;
    double WorstDevelopmentCycleSeconds = 0.0;
    double WorstModeTransitionSeconds = 0.0;
    TArray<double> ModeTransitionSamples;
};
