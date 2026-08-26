#pragma once

#include "CoreMinimal.h"
#include "Campaign/DACampaignLiveSignals.h"
#include "World/DAWorldAssetRecord.h"

#include "DACitySimulationState.generated.h"

UENUM(BlueprintType)
enum class EDACitizenRepresentation : uint8
{
    Unloaded,
    Cohort,
    LightweightCrowd,
    FullActor
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACitizenRecord
{
    GENERATED_BODY()

    void PromoteToNamed(const FString& InDisplayName) { bNamed = true; DisplayName = InDisplayName; }
    void SetRepresentation(const EDACitizenRepresentation InRepresentation) { Representation = InRepresentation; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FName CitizenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FString DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FName CohortId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FName HouseholdId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FGuid HomeAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FName CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FName DistrictId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FName CommuteZone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FName CitizenClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") TArray<FName> SkillTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen", meta = (ClampMin = "0")) int32 EducationLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FName JobId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") FString JobTitle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") TArray<FName> History;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") bool bAvailableForWork = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen") bool bNamed = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizen")
    EDACitizenRepresentation Representation = EDACitizenRepresentation::Cohort;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAHouseholdRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Household") FName HouseholdId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Household") FGuid HomeAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Household") FName CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Household") FName DistrictId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Household") FName CommuteZone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Household") TArray<FName> MemberCitizenIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Household", meta = (ClampMin = "0.0"))
    float Wealth = 0.f;
};

UENUM(BlueprintType)
enum class EDAJobMatchQuality : uint8
{
    Unqualified,
    Poor,
    Acceptable,
    Good,
    Excellent
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAJobOpening
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job") FName JobId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job") FGuid FacilityWorldAssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job") FName CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job") FName DistrictId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job") FName CommuteZone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job") FName PreferredClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job") TArray<FName> RequiredSkillTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job", meta = (ClampMin = "0"))
    int32 MinimumEducationLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizen|Job", meta = (ClampMin = "0"))
    int32 OpenPositions = 1;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAJobAssignment
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizen|Job") FName CitizenId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizen|Job") FName JobId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizen|Job") FGuid FacilityWorldAssetId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizen|Job")
    EDAJobMatchQuality MatchQuality = EDAJobMatchQuality::Unqualified;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizen|Job") float OutputMultiplier = 0.f;
};

UENUM(BlueprintType)
enum class EDAUtilityState : uint8
{
    FullySupplied,
    MinorDeficit,
    SignificantDeficit,
    Critical,
    Offline
};

UENUM(BlueprintType)
enum class EDAFacilityType : uint8
{
    Unspecified,
    Residential,
    Retail,
    Office,
    Research,
    Industrial,
    Infrastructure,
    Defense,
    Wonder
};

UENUM(BlueprintType)
enum class EDAMaintenanceCondition : uint8
{
    Healthy,
    Worn,
    Damaged,
    CriticallyDamaged
};

UENUM(BlueprintType)
enum class EDADeploymentTier : uint8
{
    Common,
    Specialized,
    Elite,
    Legendary,
    Mythic,
    Wonder
};

UENUM(BlueprintType)
enum class EDAFootprintClass : uint8
{
    OneByOne,
    OneByTwo,
    TwoByTwo,
    TwoByThree,
    ThreeByThree,
    FourByFour,
    FiveByFive
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAWalletValues
{
    GENERATED_BODY()

    FDAWalletValues() = default;
    FDAWalletValues(const float InCapital, const float InInsight, const float InInfluence)
        : Capital(InCapital), Insight(InInsight), Influence(InInfluence) {}

    bool IsFinite() const
    {
        return FMath::IsFinite(Capital) && FMath::IsFinite(Insight) && FMath::IsFinite(Influence);
    }

    FDAWalletValues& operator+=(const FDAWalletValues& Other)
    {
        Capital += Other.Capital;
        Insight += Other.Insight;
        Influence += Other.Influence;
        return *this;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") float Capital = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") float Insight = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") float Influence = 0.f;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAEconomyModifier
{
    GENERATED_BODY()

    FDAEconomyModifier() = default;
    FDAEconomyModifier(const FName InName, const float InAmount) : Name(InName), Amount(InAmount) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") FName Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") float Amount = 0.f;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAEconomyContribution
{
    GENERATED_BODY()

    FDAEconomyContribution() = default;
    FDAEconomyContribution(const FName InName, const float InValue) : Name(InName), Value(InValue) {}

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") FName Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") float Value = 0.f;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAFacilityContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") FDAWorldAssetRecord AssetRecord;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") FDAWalletValues BaseOutput;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0.0"))
    float StaffingPercent = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") bool bAutomated = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    EDAUtilityState UtilityState = EDAUtilityState::FullySupplied;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0.0"))
    float DemandMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0.0"))
    float ConditionOutputMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") TArray<FDAEconomyModifier> StandardModifiers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    EDAFacilityType FacilityType = EDAFacilityType::Unspecified;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0.004", ClampMax = "0.008"))
    float BespokeWonderMaintenanceRate = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0.0"))
    float DeploymentCapital = 0.f;
    /** Exact authored base maintenance. Negative is accepted only by isolated legacy fixtures. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    float AuthoredMaintenanceCapitalPerCycle = -1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    EDAMaintenanceCondition MaintenanceCondition = EDAMaintenanceCondition::Healthy;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAFacilityOutput
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") FGuid WorldAssetId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") FName CardDefinitionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") FDAWalletValues GrossOutput;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") float MaintenanceCapital = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") FDAWalletValues NetOutput;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") TArray<FDAEconomyContribution> Contributions;
};

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDASynaraDependencyMetrics
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara", meta = (ClampMin = "0")) int32 AssignedAIWorkers = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara", meta = (ClampMin = "0")) int32 AutonomousExchanges = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara", meta = (ClampMin = "0")) int32 FullyAutomatedOffices = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara", meta = (ClampMin = "0")) int32 FullyAutomatedIndustrials = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara", meta = (ClampMin = "0")) int32 ThinkingSpireAutomatedDistricts = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara", meta = (ClampMin = "0")) int32 AgencyForums = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara", meta = (ClampMin = "0.0", ClampMax = "100.0")) float EducationPercent = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara", meta = (ClampMin = "0.0", ClampMax = "100.0")) float HumanStaffRatioPercent = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara") bool bHumanOverridePolicy = false;
};

/** Full persistent city authority. LiveSignals is a projection of this aggregate, never another owner. */
USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDACitySimulationState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City") bool bInitialized = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") FDAWalletValues Wallet;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") TArray<FDAFacilityContext> Facilities;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") TArray<FDAFacilityOutput> LatestFacilityOutputs;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Economy") int64 ResolvedDevelopmentCycles = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizens", meta = (ClampMin = "0")) int32 Population = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizens") TArray<FDACitizenRecord> Citizens;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizens") TArray<FDAHouseholdRecord> Households;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizens|Jobs") TArray<FDAJobOpening> JobOpenings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizens|Jobs") TArray<FDAJobAssignment> JobAssignments;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City|Utilities") TArray<FDACampaignUtilitySignal> UtilitySignals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizens|Migration", meta = (ClampMin = "0")) int32 JobVacancies = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Citizens|Migration", meta = (ClampMin = "0.0", ClampMax = "100.0")) float Attractiveness = 40.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizens|Migration") float IncomingMigrationAccumulator = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizens|Migration") float OutgoingMigrationAccumulator = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factions|Synara") FDASynaraDependencyMetrics SynaraDependency;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizens|Migration") int32 LastIncomingMigrants = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizens|Migration") int32 LastOutgoingMigrants = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Citizens|Migration") int64 ResolvedWorldTicks = 0;
};
