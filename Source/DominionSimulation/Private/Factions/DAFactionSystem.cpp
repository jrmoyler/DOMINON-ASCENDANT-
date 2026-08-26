#include "Factions/DAFactionSystem.h"
#include "Save/DACampaignSaveGame.h"

namespace
{
    constexpr float AIWorkerDependency = 0.15f;
    constexpr float AutonomousExchangeDependency = 0.10f;
    constexpr float AutomatedOfficeDependency = 0.20f;
    constexpr float AutomatedIndustrialDependency = 0.25f;
    constexpr float ThinkingSpireDependency = 0.35f;
    constexpr float AgencyForumReduction = 0.20f;
    constexpr float HumanStaffReduction = 0.10f;
    constexpr float HighEducationGrowthMultiplier = 0.90f;
    constexpr float HumanOverrideGrowthMultiplier = 0.80f;

    float SanitizeNonNegative(const float Value)
    {
        return FMath::IsFinite(Value) ? FMath::Max(0.f, Value) : 0.f;
    }

    int32 SanitizeCount(const int32 Value)
    {
        return FMath::Max(0, Value);
    }

    FDAFactionState MakeSynaraFaction(const TCHAR* Id, const TCHAR* DisplayName, const TCHAR* Demand)
    {
        FDAFactionState Faction;
        Faction.FactionId = FName(Id);
        Faction.DisplayName = DisplayName;
        Faction.Demand = Demand;
        Faction.Support = 25.f;
        return Faction;
    }
}

TArray<FDAFactionState> UDAFactionSystem::CreateSynaraFactions()
{
    return {
        MakeSynaraFaction(TEXT("faction.synara.ascendants"), TEXT("Ascendants"), TEXT("Accelerate orchestration")),
        MakeSynaraFaction(TEXT("faction.synara.human_agency"), TEXT("Human Agency League"), TEXT("Limit automation")),
        MakeSynaraFaction(TEXT("faction.synara.synthetic_rights"), TEXT("Synthetic Rights Front"), TEXT("Recognize synthetic personhood")),
        MakeSynaraFaction(TEXT("faction.synara.moderates"), TEXT("Practical Moderates"), TEXT("Balance stability and prosperity"))
    };
}

float UDAFactionSystem::CalculateDependency(const FDACitySimulationState& State)
{
    const FDASynaraDependencyMetrics& Metrics = State.SynaraDependency;
    float Growth = (SanitizeCount(Metrics.AssignedAIWorkers) * AIWorkerDependency)
        + (SanitizeCount(Metrics.AutonomousExchanges) * AutonomousExchangeDependency)
        + (SanitizeCount(Metrics.FullyAutomatedOffices) * AutomatedOfficeDependency)
        + (SanitizeCount(Metrics.FullyAutomatedIndustrials) * AutomatedIndustrialDependency)
        + (SanitizeCount(Metrics.ThinkingSpireAutomatedDistricts) * ThinkingSpireDependency);

    if (SanitizeNonNegative(Metrics.EducationPercent) > 70.f)
    {
        Growth *= HighEducationGrowthMultiplier;
    }
    if (Metrics.bHumanOverridePolicy)
    {
        Growth *= HumanOverrideGrowthMultiplier;
    }
    const float AgencyForumMitigation = SanitizeCount(Metrics.AgencyForums) * AgencyForumReduction;
    const float HumanStaffMitigation = SanitizeNonNegative(Metrics.HumanStaffRatioPercent) > 60.f
        ? HumanStaffReduction
        : 0.f;
    return FMath::Max(0.f, Growth - AgencyForumMitigation - HumanStaffMitigation);
}

float UDAFactionSystem::GetSupport(const FDACampaignSnapshot& Campaign, const FName FactionId)
{ return static_cast<float>(Campaign.SynaraState.FactionSupport.FindRef(FactionId)); }

bool UDAFactionSystem::ApplySupportReason(FDACampaignSnapshot& Campaign, const FName ActionId,
    const FName FactionId, const double Delta, const int64 WorldTick)
{ return Campaign.SynaraState.ApplyFactionSupportReason(ActionId, FactionId, Delta, WorldTick); }
