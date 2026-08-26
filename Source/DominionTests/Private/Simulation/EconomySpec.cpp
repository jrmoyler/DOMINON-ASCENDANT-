#include "Economy/DAEconomySubsystem.h"
#include "Fixtures/DAGameInstanceSubsystemFixture.h"
#include "Misc/AutomationTest.h"
#include "Time/DASimulationClockSubsystem.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDAEconomySpec, "Dominion.Simulation.Economy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAEconomySpec)

namespace
{
    constexpr float Tolerance = 0.001f;

    FDAFacilityContext MakeOperationalFacility(const TCHAR* DefinitionId, const float BaseCapital = 10.f)
    {
        FDAFacilityContext Context;
        Context.AssetRecord.WorldAssetId = FGuid::NewGuid();
        Context.AssetRecord.CardDefinitionId = FName(DefinitionId);
        Context.AssetRecord.ConstructionState = EDAConstructionState::Operational;
        Context.AssetRecord.StructuralIntegrity = 100.f;
        Context.BaseOutput.Capital = BaseCapital;
        Context.StaffingPercent = 100.f;
        Context.UtilityState = EDAUtilityState::FullySupplied;
        Context.DemandMultiplier = 1.f;
        Context.ConditionOutputMultiplier = 1.f;
        return Context;
    }

    const FDAEconomyContribution* FindContribution(const FDAFacilityOutput& Output, const FName Name)
    {
        return Output.Contributions.FindByPredicate([Name](const FDAEconomyContribution& Contribution)
        {
            return Contribution.Name == Name;
        });
    }

    FDAFacilityContext MakeScenarioFacility(
        const TCHAR* DefinitionId,
        const FDAWalletValues& BaseOutput,
        const EDAFacilityType FacilityType,
        const float DeploymentCapital)
    {
        FDAFacilityContext Context = MakeOperationalFacility(DefinitionId, 0.f);
        Context.BaseOutput = BaseOutput;
        Context.FacilityType = FacilityType;
        Context.DeploymentCapital = DeploymentCapital;
        return Context;
    }

    FDACitySimulationState MakeExplicitBoundednessScenario()
    {
        FDACitySimulationState State;
        State.Wallet = FDAWalletValues{20.f, 4.f, 4.f};

        // These are explicit test-scenario inputs, not tuning for named Dominion assets.
        // Authored facility values will be bound by the content tasks (19/27/28).
        State.Facilities.Add(MakeScenarioFacility(
            TEXT("scenario.facility.capital"),
            FDAWalletValues{0.42f, 0.f, 0.f},
            EDAFacilityType::Retail,
            20.f));
        State.Facilities.Add(MakeScenarioFacility(
            TEXT("scenario.facility.insight"),
            FDAWalletValues{0.f, 0.04f, 0.f},
            EDAFacilityType::Research,
            10.f));
        State.Facilities.Add(MakeScenarioFacility(
            TEXT("scenario.facility.influence"),
            FDAWalletValues{0.f, 0.f, 0.03f},
            EDAFacilityType::Infrastructure,
            10.f));
        return State;
    }
}

void FDAEconomySpec::Define()
{
    It("applies the v0.8 staffing and critical-utility multipliers", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAEconomySubsystem* Economy = Fixture.GetSubsystem<UDAEconomySubsystem>();
        TestNotNull("Economy comes from an initialized GameInstance", Economy);
        if (Economy == nullptr) { return; }

        struct FStaffingCase
        {
            float StaffingPercent;
            float ExpectedCapital;
        };

        const FStaffingCase StaffingCases[] = {
            {100.f, 10.f},
            {99.f, 9.f},
            {75.f, 9.f},
            {74.f, 6.5f},
            {50.f, 6.5f}
        };

        for (const FStaffingCase& StaffingCase : StaffingCases)
        {
            FDAFacilityContext Context = MakeOperationalFacility(TEXT("asset.test.staffing"));
            Context.StaffingPercent = StaffingCase.StaffingPercent;
            const FDAFacilityOutput Output = Economy->CalculateFacilityOutput(Context);
            TestEqual(
                FString::Printf(TEXT("Staffing %.0f percent output"), StaffingCase.StaffingPercent),
                Output.GrossOutput.Capital,
                StaffingCase.ExpectedCapital,
                Tolerance);
        }

        FDAFacilityContext CriticalUtility = MakeOperationalFacility(TEXT("asset.test.utility"));
        CriticalUtility.UtilityState = EDAUtilityState::Critical;
        TestEqual(
            "Critical utility produces 25 percent",
            Economy->CalculateFacilityOutput(CriticalUtility).GrossOutput.Capital,
            2.5f,
            Tolerance);
    });

    It("caps the summed standard modifier stack at plus or minus sixty percent", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAEconomySubsystem* Economy = Fixture.GetSubsystem<UDAEconomySubsystem>();
        TestNotNull("Economy comes from an initialized GameInstance", Economy);
        if (Economy == nullptr) { return; }

        FDAFacilityContext Positive = MakeOperationalFacility(TEXT("asset.test.modifier.positive"));
        Positive.StandardModifiers = {
            FDAEconomyModifier{TEXT("Adjacency"), 0.40f},
            FDAEconomyModifier{TEXT("Policy"), 0.35f}
        };
        const FDAFacilityOutput PositiveOutput = Economy->CalculateFacilityOutput(Positive);
        TestEqual("Positive modifier pool is capped at 1.6x", PositiveOutput.GrossOutput.Capital, 16.f, Tolerance);

        FDAFacilityContext Negative = MakeOperationalFacility(TEXT("asset.test.modifier.negative"));
        Negative.StandardModifiers = {
            FDAEconomyModifier{TEXT("Disruption"), -0.45f},
            FDAEconomyModifier{TEXT("Policy"), -0.30f}
        };
        const FDAFacilityOutput NegativeOutput = Economy->CalculateFacilityOutput(Negative);
        TestEqual("Negative modifier pool is floored at 0.4x", NegativeOutput.GrossOutput.Capital, 4.f, Tolerance);
    });

    It("emits named contributions for every facility-output factor and wallet result", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAEconomySubsystem* Economy = Fixture.GetSubsystem<UDAEconomySubsystem>();
        TestNotNull("Economy comes from an initialized GameInstance", Economy);
        if (Economy == nullptr) { return; }
        FDAFacilityContext Context = MakeOperationalFacility(TEXT("asset.test.trace"));
        Context.BaseOutput.Insight = 2.f;
        Context.BaseOutput.Influence = 1.f;
        Context.StaffingPercent = 80.f;
        Context.UtilityState = EDAUtilityState::MinorDeficit;
        Context.DemandMultiplier = 0.75f;
        Context.ConditionOutputMultiplier = 0.9f;
        Context.StandardModifiers.Add(FDAEconomyModifier{TEXT("Adjacency"), 0.2f});

        const FDAFacilityOutput Output = Economy->CalculateFacilityOutput(Context);

        const FName RequiredNames[] = {
            TEXT("Base.Capital"),
            TEXT("Base.Insight"),
            TEXT("Base.Influence"),
            TEXT("Factor.Staffing"),
            TEXT("Factor.Utility"),
            TEXT("Factor.Demand"),
            TEXT("Factor.Condition"),
            TEXT("Modifier.Adjacency"),
            TEXT("Factor.ModifierStack"),
            TEXT("Gross.Capital"),
            TEXT("Gross.Insight"),
            TEXT("Gross.Influence"),
            TEXT("Maintenance.Rate"),
            TEXT("Maintenance.BaseCapital"),
            TEXT("Maintenance.Condition"),
            TEXT("Maintenance.Capital"),
            TEXT("Net.Capital"),
            TEXT("Net.Insight"),
            TEXT("Net.Influence")
        };

        for (const FName Name : RequiredNames)
        {
            TestNotNull(*FString::Printf(TEXT("Trace contains %s"), *Name.ToString()), FindContribution(Output, Name));
        }

        TestEqual("Traceable output uses all factors", Output.GrossOutput.Capital, 6.1965f, Tolerance);
    });

    It("charges industrial maintenance at one percent before the condition multiplier", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAEconomySubsystem* Economy = Fixture.GetSubsystem<UDAEconomySubsystem>();
        TestNotNull("Economy comes from an initialized GameInstance", Economy);
        if (Economy == nullptr) { return; }
        FDAFacilityContext Context = MakeOperationalFacility(TEXT("asset.test.factory"), 1.f);
        Context.FacilityType = EDAFacilityType::Industrial;
        Context.DeploymentCapital = 40.f;
        Context.MaintenanceCondition = EDAMaintenanceCondition::Damaged;

        const FDAFacilityOutput Output = Economy->CalculateFacilityOutput(Context);
        const FDAEconomyContribution* BaseMaintenance = FindContribution(Output, TEXT("Maintenance.BaseCapital"));

        TestNotNull("Base maintenance is traceable", BaseMaintenance);
        if (BaseMaintenance != nullptr)
        {
            TestEqual("Industrial base maintenance is one percent", BaseMaintenance->Value, 0.4f, Tolerance);
        }
        TestEqual("Damaged condition applies 140 percent maintenance", Output.MaintenanceCapital, 0.56f, Tolerance);
        TestEqual("Maintenance is debited from Capital", Output.NetOutput.Capital, 0.44f, Tolerance);
    });

    It("calculates v0.8 deployment Capital from tier footprint and complexity", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAEconomySubsystem* Economy = Fixture.GetSubsystem<UDAEconomySubsystem>();
        TestNotNull("Economy comes from an initialized GameInstance", Economy);
        if (Economy == nullptr) { return; }

        const float Precise = Economy->CalculateDeploymentCapital(
            EDADeploymentTier::Elite,
            EDAFootprintClass::TwoByThree,
            1.25f);

        TestEqual("Elite 2 by 3 advanced asset costs 34.5 before display rounding", Precise, 34.5f, Tolerance);
        TestEqual("Deployment Capital rounds for player display", Economy->RoundDeploymentCapitalForDisplay(Precise), 35);
    });

    It("keeps an explicit economy scenario bounded for one hundred clock cycles", [this]()
    {
        FDAGameInstanceSubsystemFixture Fixture;
        UDAEconomySubsystem* Economy = Fixture.GetSubsystem<UDAEconomySubsystem>();
        UDASimulationClockSubsystem* Clock = Fixture.GetSubsystem<UDASimulationClockSubsystem>();
        TestNotNull("Economy comes from an initialized GameInstance", Economy);
        TestNotNull("Clock comes from the same initialized GameInstance", Clock);
        if (Economy == nullptr || Clock == nullptr) { return; }
        FDACitySimulationState State = MakeExplicitBoundednessScenario();

        Clock->OnDevelopmentCycle.AddLambda([Economy, &State](const int64)
        {
            Economy->ResolveDevelopmentCycle(State);
        });
        Clock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds * 100.0);

        TestEqual("Exactly one hundred discrete economy cycles resolve", State.ResolvedDevelopmentCycles, 100LL);
        TestEqual("Each explicit facility has a trace for the latest cycle", State.LatestFacilityOutputs.Num(), 3);
        TestEqual("Capital follows the explicit one-hundred-cycle inputs", State.Wallet.Capital, 35.f, 0.01f);
        TestEqual("Insight follows the explicit one-hundred-cycle inputs", State.Wallet.Insight, 8.f, 0.01f);
        TestEqual("Influence follows the explicit one-hundred-cycle inputs", State.Wallet.Influence, 7.f, 0.01f);
        TestTrue("Capital remains solvent", State.Wallet.Capital >= 0.f);
        TestTrue("All wallet values remain finite", State.Wallet.IsFinite());

        Clock->AdvanceSimulation(UDASimulationClockSubsystem::DevelopmentCycleDurationSeconds * 20.0);

        TestEqual("One first-hour interval is one hundred twenty cycles", State.ResolvedDevelopmentCycles, 120LL);
        TestTrue("Capital is within the v1.1 first-hour reserve target", State.Wallet.Capital >= 15.f && State.Wallet.Capital <= 40.f);
        TestTrue("Insight is within the v1.1 first-hour reserve target", State.Wallet.Insight >= 4.f && State.Wallet.Insight <= 12.f);
        TestTrue("Influence is within the v1.1 first-hour reserve target", State.Wallet.Influence >= 4.f && State.Wallet.Influence <= 10.f);
    });
}
