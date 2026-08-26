#include "Economy/DAEconomySubsystem.h"

#include "Content/DACardDefinition.h"
#include "Engine/GameInstance.h"
#include "Time/DASimulationClockSubsystem.h"

namespace
{
    float SanitizeNonNegative(const float Value)
    {
        return FMath::IsFinite(Value) ? FMath::Max(0.f, Value) : 0.f;
    }

    float GetStaffingMultiplier(const float StaffingPercent, const bool bAutomated)
    {
        const float Staffing = SanitizeNonNegative(StaffingPercent);
        if (Staffing >= 100.f || (bAutomated && Staffing <= 0.f))
        {
            return 1.f;
        }
        if (Staffing >= 75.f)
        {
            return 0.9f;
        }
        if (Staffing >= 50.f)
        {
            return 0.65f;
        }
        if (Staffing >= 25.f)
        {
            return 0.4f;
        }
        if (Staffing > 0.f)
        {
            return 0.15f;
        }
        return 0.f;
    }

    float GetUtilityMultiplier(const EDAUtilityState UtilityState)
    {
        switch (UtilityState)
        {
        case EDAUtilityState::FullySupplied:
            return 1.f;
        case EDAUtilityState::MinorDeficit:
            return 0.85f;
        case EDAUtilityState::SignificantDeficit:
            return 0.6f;
        case EDAUtilityState::Critical:
            return 0.25f;
        case EDAUtilityState::Offline:
        default:
            return 0.f;
        }
    }

    float GetMaintenanceRate(const FDAFacilityContext& Context)
    {
        switch (Context.FacilityType)
        {
        case EDAFacilityType::Residential:
            return 0.005f;
        case EDAFacilityType::Retail:
            return 0.007f;
        case EDAFacilityType::Office:
            return 0.006f;
        case EDAFacilityType::Research:
            return 0.008f;
        case EDAFacilityType::Industrial:
            return 0.01f;
        case EDAFacilityType::Infrastructure:
            return 0.005f;
        case EDAFacilityType::Defense:
            return 0.008f;
        case EDAFacilityType::Wonder:
            return Context.BespokeWonderMaintenanceRate > 0.f
                ? FMath::Clamp(Context.BespokeWonderMaintenanceRate, 0.004f, 0.008f)
                : 0.f;
        case EDAFacilityType::Unspecified:
        default:
            return 0.f;
        }
    }

    float GetMaintenanceConditionMultiplier(const EDAMaintenanceCondition Condition)
    {
        switch (Condition)
        {
        case EDAMaintenanceCondition::Healthy:
            return 1.f;
        case EDAMaintenanceCondition::Worn:
            return 1.15f;
        case EDAMaintenanceCondition::Damaged:
            return 1.4f;
        case EDAMaintenanceCondition::CriticallyDamaged:
        default:
            return 1.8f;
        }
    }

    float GetTierBase(const EDADeploymentTier Tier)
    {
        switch (Tier)
        {
        case EDADeploymentTier::Common:
            return 6.f;
        case EDADeploymentTier::Specialized:
            return 11.f;
        case EDADeploymentTier::Elite:
            return 24.f;
        case EDADeploymentTier::Legendary:
            return 55.f;
        case EDADeploymentTier::Mythic:
            return 120.f;
        case EDADeploymentTier::Wonder:
        default:
            return 200.f;
        }
    }

    float GetFootprintMultiplier(const EDAFootprintClass Footprint)
    {
        switch (Footprint)
        {
        case EDAFootprintClass::OneByOne:
            return 0.8f;
        case EDAFootprintClass::OneByTwo:
            return 0.95f;
        case EDAFootprintClass::TwoByTwo:
            return 1.f;
        case EDAFootprintClass::TwoByThree:
            return 1.15f;
        case EDAFootprintClass::ThreeByThree:
            return 1.35f;
        case EDAFootprintClass::FourByFour:
            return 1.65f;
        case EDAFootprintClass::FiveByFive:
        default:
            return 2.f;
        }
    }

    float GetAvailabilityMultiplier(const EDAConstructionState ConstructionState)
    {
        return ConstructionState == EDAConstructionState::Operational || ConstructionState == EDAConstructionState::Damaged
            ? 1.f
            : 0.f;
    }

    void AddContribution(TArray<FDAEconomyContribution>& Contributions, const FName Name, const float Value)
    {
        Contributions.Emplace(Name, Value);
    }
}

void UDAEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UDAEconomySubsystem::Deinitialize()
{
    Super::Deinitialize();
}

FDAFacilityOutput UDAEconomySubsystem::CalculateFacilityOutput(const FDAFacilityContext& Context) const
{
    FDAFacilityOutput Output;
    Output.WorldAssetId = Context.AssetRecord.WorldAssetId;
    Output.CardDefinitionId = Context.AssetRecord.CardDefinitionId;

    const float BaseCapital = SanitizeNonNegative(Context.BaseOutput.Capital);
    const float BaseInsight = SanitizeNonNegative(Context.BaseOutput.Insight);
    const float BaseInfluence = SanitizeNonNegative(Context.BaseOutput.Influence);
    const float AvailabilityMultiplier = GetAvailabilityMultiplier(Context.AssetRecord.ConstructionState);
    const float StaffingMultiplier = GetStaffingMultiplier(Context.StaffingPercent, Context.bAutomated);
    const float UtilityMultiplier = GetUtilityMultiplier(Context.UtilityState);
    const float DemandMultiplier = SanitizeNonNegative(Context.DemandMultiplier);
    const float ConditionMultiplier = SanitizeNonNegative(Context.ConditionOutputMultiplier);

    AddContribution(Output.Contributions, TEXT("Base.Capital"), BaseCapital);
    AddContribution(Output.Contributions, TEXT("Base.Insight"), BaseInsight);
    AddContribution(Output.Contributions, TEXT("Base.Influence"), BaseInfluence);
    AddContribution(Output.Contributions, TEXT("Factor.Availability"), AvailabilityMultiplier);
    AddContribution(Output.Contributions, TEXT("Factor.Staffing"), StaffingMultiplier);
    AddContribution(Output.Contributions, TEXT("Factor.Utility"), UtilityMultiplier);
    AddContribution(Output.Contributions, TEXT("Factor.Demand"), DemandMultiplier);
    AddContribution(Output.Contributions, TEXT("Factor.Condition"), ConditionMultiplier);
    AddContribution(Output.Contributions, TEXT("Condition.IntegrityPercent"), SanitizeNonNegative(Context.AssetRecord.StructuralIntegrity));

    float StandardModifierSum = 0.f;
    for (const FDAEconomyModifier& Modifier : Context.StandardModifiers)
    {
        const float Amount = FMath::IsFinite(Modifier.Amount) ? Modifier.Amount : 0.f;
        StandardModifierSum += Amount;
        AddContribution(
            Output.Contributions,
            FName(*FString::Printf(TEXT("Modifier.%s"), *Modifier.Name.ToString())),
            Amount);
    }
    const float ModifierStackMultiplier = 1.f + FMath::Clamp(StandardModifierSum, -0.6f, 0.6f);
    AddContribution(Output.Contributions, TEXT("Modifier.StandardSum"), StandardModifierSum);
    AddContribution(Output.Contributions, TEXT("Modifier.StandardClampedAmount"), ModifierStackMultiplier - 1.f);
    AddContribution(Output.Contributions, TEXT("Factor.ModifierStack"), ModifierStackMultiplier);

    const float OutputMultiplier = AvailabilityMultiplier
        * StaffingMultiplier
        * UtilityMultiplier
        * DemandMultiplier
        * ConditionMultiplier
        * ModifierStackMultiplier;
    Output.GrossOutput = FDAWalletValues(
        BaseCapital * OutputMultiplier,
        BaseInsight * OutputMultiplier,
        BaseInfluence * OutputMultiplier);

    AddContribution(Output.Contributions, TEXT("Gross.Capital"), Output.GrossOutput.Capital);
    AddContribution(Output.Contributions, TEXT("Gross.Insight"), Output.GrossOutput.Insight);
    AddContribution(Output.Contributions, TEXT("Gross.Influence"), Output.GrossOutput.Influence);

    const float FallbackMaintenanceRate = GetMaintenanceRate(Context);
    const float BaseMaintenance = Context.AuthoredMaintenanceCapitalPerCycle >= 0.f
        ? SanitizeNonNegative(Context.AuthoredMaintenanceCapitalPerCycle)
        : SanitizeNonNegative(Context.DeploymentCapital) * FallbackMaintenanceRate;
    const float MaintenanceRate = Context.DeploymentCapital > 0.f
        ? BaseMaintenance / Context.DeploymentCapital : 0.f;
    const float MaintenanceConditionMultiplier = GetMaintenanceConditionMultiplier(Context.MaintenanceCondition);
    Output.MaintenanceCapital = BaseMaintenance * MaintenanceConditionMultiplier;

    AddContribution(Output.Contributions, TEXT("Maintenance.Rate"), MaintenanceRate);
    AddContribution(Output.Contributions, TEXT("Maintenance.BaseCapital"), BaseMaintenance);
    AddContribution(Output.Contributions, TEXT("Maintenance.Condition"), MaintenanceConditionMultiplier);
    AddContribution(Output.Contributions, TEXT("Maintenance.Capital"), Output.MaintenanceCapital);

    Output.NetOutput = Output.GrossOutput;
    Output.NetOutput.Capital -= Output.MaintenanceCapital;
    AddContribution(Output.Contributions, TEXT("Net.Capital"), Output.NetOutput.Capital);
    AddContribution(Output.Contributions, TEXT("Net.Insight"), Output.NetOutput.Insight);
    AddContribution(Output.Contributions, TEXT("Net.Influence"), Output.NetOutput.Influence);
    return Output;
}

bool UDAEconomySubsystem::ApplyAutonomousFactoryThroughput(
    FDAFacilityContext& Context, const UDA_CardDefinition& Definition) const
{
    float Amount = 0.f;
    if (Context.FacilityType != EDAFacilityType::Industrial
        || Definition.DefinitionId != TEXT("fusion.autonomous_factory")
        || Context.AssetRecord.CardDefinitionId != Definition.DefinitionId
        || !Definition.TryGetIndustrialThroughputModifier(Amount)
        || !FMath::IsNearlyEqual(Amount, 0.25f, 0.0001f)) return false;
    if (const FDAEconomyModifier* Existing = Context.StandardModifiers.FindByPredicate(
        [](const FDAEconomyModifier& Modifier)
        { return Modifier.Name == TEXT("AutonomousFactory.IndustrialThroughput"); }))
    {
        Context.bAutomated = true;
        return FMath::IsNearlyEqual(Existing->Amount, Amount, 0.0001f);
    }
    FDAEconomyModifier& Modifier = Context.StandardModifiers.Emplace_GetRef();
    Modifier.Name = TEXT("AutonomousFactory.IndustrialThroughput");
    Modifier.Amount = Amount;
    Context.bAutomated = true;
    return true;
}

void UDAEconomySubsystem::ResolveDevelopmentCycle(FDACitySimulationState& State) const
{
    State.LatestFacilityOutputs.Reset(State.Facilities.Num());
    for (const FDAFacilityContext& Facility : State.Facilities)
    {
        FDAFacilityOutput Output = CalculateFacilityOutput(Facility);
        State.Wallet += Output.NetOutput;
        State.LatestFacilityOutputs.Add(MoveTemp(Output));
    }
    ++State.ResolvedDevelopmentCycles;
}

float UDAEconomySubsystem::CalculateDeploymentCapital(
    const EDADeploymentTier Tier,
    const EDAFootprintClass Footprint,
    const float ComplexityMultiplier) const
{
    return GetTierBase(Tier) * GetFootprintMultiplier(Footprint) * SanitizeNonNegative(ComplexityMultiplier);
}

int32 UDAEconomySubsystem::RoundDeploymentCapitalForDisplay(const float PreciseCapital) const
{
    return FMath::RoundToInt(SanitizeNonNegative(PreciseCapital));
}
