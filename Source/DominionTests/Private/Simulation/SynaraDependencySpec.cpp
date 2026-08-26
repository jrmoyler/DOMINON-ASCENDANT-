#include "Factions/DAFactionSystem.h"
#include "Factions/DASystemicPressureSystem.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDASynaraDependencySpec, "Dominion.Simulation.Factions.SynaraDependency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDASynaraDependencySpec)

namespace
{
    FDACitySimulationState MakeAutomationState()
    {
        FDACitySimulationState State;
        State.SynaraDependency.AssignedAIWorkers = 1;
        State.SynaraDependency.AutonomousExchanges = 1;
        State.SynaraDependency.FullyAutomatedOffices = 1;
        State.SynaraDependency.FullyAutomatedIndustrials = 1;
        return State;
    }
}

void FDASynaraDependencySpec::Define()
{
    It("adds 0.15 Dependency for each assigned AI Worker", [this]()
    {
        FDACitySimulationState State;
        State.SynaraDependency.AssignedAIWorkers = 1;

        TestEqual("One AI Worker has the v0.8 coefficient", UDAFactionSystem::CalculateDependency(State), 0.15f, 0.001f);
    });

    It("adds 0.10 Dependency for each Autonomous Exchange", [this]()
    {
        FDACitySimulationState State;
        State.SynaraDependency.AutonomousExchanges = 1;

        TestEqual("One Autonomous Exchange has the v0.8 coefficient", UDAFactionSystem::CalculateDependency(State), 0.10f, 0.001f);
    });

    It("adds 0.20 Dependency for each fully automated Office", [this]()
    {
        FDACitySimulationState State;
        State.SynaraDependency.FullyAutomatedOffices = 1;

        TestEqual("One automated Office has the v0.8 coefficient", UDAFactionSystem::CalculateDependency(State), 0.20f, 0.001f);
    });

    It("adds 0.25 Dependency for each fully automated Industrial facility", [this]()
    {
        FDACitySimulationState State;
        State.SynaraDependency.FullyAutomatedIndustrials = 1;

        TestEqual("One automated Industrial facility has the v0.8 coefficient", UDAFactionSystem::CalculateDependency(State), 0.25f, 0.001f);
    });

    It("adds 0.35 Dependency for each Thinking Spire automated district", [this]()
    {
        FDACitySimulationState State;
        State.SynaraDependency.ThinkingSpireAutomatedDistricts = 1;

        TestEqual("One Thinking Spire automated district has the v0.8 coefficient", UDAFactionSystem::CalculateDependency(State), 0.35f, 0.001f);
    });

    It("subtracts 0.20 Dependency for each Agency Forum district", [this]()
    {
        FDACitySimulationState State;
        State.SynaraDependency.AssignedAIWorkers = 2;
        State.SynaraDependency.AgencyForums = 1;

        TestEqual("Agency Forum subtracts its fixed district mitigation", UDAFactionSystem::CalculateDependency(State), 0.10f, 0.001f);
    });

    It("reduces Dependency growth by ten percent only above 70 education", [this]()
    {
        FDACitySimulationState State;
        State.SynaraDependency.ThinkingSpireAutomatedDistricts = 1;
        State.SynaraDependency.EducationPercent = 71.f;

        TestEqual("Education above seventy reduces the 0.35 source to 0.315", UDAFactionSystem::CalculateDependency(State), 0.315f, 0.001f);

        State.SynaraDependency.EducationPercent = 70.f;
        TestEqual("Education at seventy does not qualify", UDAFactionSystem::CalculateDependency(State), 0.35f, 0.001f);
    });

    It("subtracts 0.10 Dependency only above a sixty percent human staffing ratio", [this]()
    {
        FDACitySimulationState State;
        State.SynaraDependency.AssignedAIWorkers = 2;
        State.SynaraDependency.HumanStaffRatioPercent = 61.f;

        TestEqual("Human staffing above sixty applies the fixed district mitigation", UDAFactionSystem::CalculateDependency(State), 0.20f, 0.001f);

        State.SynaraDependency.HumanStaffRatioPercent = 60.f;
        TestEqual("Human staffing at sixty does not qualify", UDAFactionSystem::CalculateDependency(State), 0.30f, 0.001f);
    });

    It("reduces growth by twenty percent for Human Override before fixed mitigations", [this]()
    {
        FDACitySimulationState State = MakeAutomationState();
        State.SynaraDependency.AgencyForums = 1;
        State.SynaraDependency.bHumanOverridePolicy = true;

        TestEqual("Human Override reduces 0.70 growth before the 0.20 Agency Forum reduction", UDAFactionSystem::CalculateDependency(State), 0.36f, 0.001f);
    });

    It("publishes every Dependency threshold crossed", [this]()
    {
        UDASystemicPressureSystem* Pressure = NewObject<UDASystemicPressureSystem>();
        TArray<EDASystemicPressureThreshold> CrossedThresholds;
        Pressure->OnSystemicPressureChanged.AddLambda([&CrossedThresholds](EDASystemicPressureThreshold Threshold, float, float)
        {
            CrossedThresholds.Add(Threshold);
        });

        Pressure->SetDependency(100.f);

        const TArray<EDASystemicPressureThreshold> Expected = {
            EDASystemicPressureThreshold::Dependency25,
            EDASystemicPressureThreshold::Dependency50,
            EDASystemicPressureThreshold::Dependency70,
            EDASystemicPressureThreshold::Dependency85,
            EDASystemicPressureThreshold::Dependency100
        };
        TestEqual("Each upward threshold is exposed to the narrative layer", CrossedThresholds, Expected);
    });

    It("creates the four stable Synara faction records", [this]()
    {
        const TArray<FDAFactionState> Factions = UDAFactionSystem::CreateSynaraFactions();
        const TArray<FName> ExpectedIds = {
            TEXT("faction.synara.ascendants"),
            TEXT("faction.synara.human_agency"),
            TEXT("faction.synara.synthetic_rights"),
            TEXT("faction.synara.moderates")
        };

        TestEqual("Synara has exactly four launch factions", Factions.Num(), 4);
        for (int32 Index = 0; Index < ExpectedIds.Num() && Index < Factions.Num(); ++Index)
        {
            TestEqual(FString::Printf(TEXT("Faction %d keeps its stable identity"), Index), Factions[Index].FactionId, ExpectedIds[Index]);
            TestTrue(FString::Printf(TEXT("Faction %d tracks bounded support"), Index), Factions[Index].Support >= 0.f && Factions[Index].Support <= 100.f);
            TestTrue(FString::Printf(TEXT("Faction %d tracks bounded organization"), Index), Factions[Index].Organization >= 0.f && Factions[Index].Organization <= 100.f);
            TestTrue(FString::Printf(TEXT("Faction %d tracks bounded radicalization"), Index), Factions[Index].Radicalization >= 0.f && Factions[Index].Radicalization <= 100.f);
            TestTrue(FString::Printf(TEXT("Faction %d tracks bounded grievance"), Index), Factions[Index].Grievance >= 0.f && Factions[Index].Grievance <= 100.f);
        }
    });
}
