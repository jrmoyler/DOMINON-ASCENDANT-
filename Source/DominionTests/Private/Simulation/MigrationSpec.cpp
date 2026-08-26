#include "Citizens/DAMigrationSystem.h"
#include "Economy/DAEconomyTypes.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDAMigrationSpec, "Dominion.Simulation.Citizens.Migration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAMigrationSpec)

void FDAMigrationSpec::Define()
{
    It("calculates one and a half incoming citizens for twenty vacancies at seventy attractiveness", [this]()
    {
        TestEqual(
            "v0.8 positive migration rate",
            UDAMigrationSystem::CalculateIncomingPerWorldTick(20, 70.f),
            1.5f,
            0.001f);
    });

    It("accumulates fractional incoming citizens across deterministic world ticks", [this]()
    {
        UDAMigrationSystem* Migration = NewObject<UDAMigrationSystem>();
        FDACitySimulationState State;
        State.Population = 24;
        State.JobVacancies = 20;
        State.Attractiveness = 70.f;

        Migration->ResolveWorldTick(State);
        TestEqual("First tick admits one whole citizen", State.Population, 25);
        TestEqual("First tick retains one half incoming citizen", State.IncomingMigrationAccumulator, 0.5f, 0.001f);

        Migration->ResolveWorldTick(State);
        TestEqual("Two ticks admit three citizens in total", State.Population, 27);
        TestEqual("Two ticks consume the accumulated fraction", State.IncomingMigrationAccumulator, 0.f, 0.001f);
        TestEqual("Two world ticks resolve", State.ResolvedWorldTicks, 2LL);
    });

    It("applies the v0.8 emigration formula below forty attractiveness", [this]()
    {
        UDAMigrationSystem* Migration = NewObject<UDAMigrationSystem>();
        FDACitySimulationState State;
        State.Population = 100;
        State.JobVacancies = 20;
        State.Attractiveness = 20.f;

        TestEqual("v0.8 outgoing rate is one citizen", UDAMigrationSystem::CalculateOutgoingPerWorldTick(100, 20.f), 1.f, 0.001f);
        Migration->ResolveWorldTick(State);

        TestEqual("One resident emigrates", State.Population, 99);
        TestEqual("Low attractiveness admits nobody", State.IncomingMigrationAccumulator, 0.f, 0.001f);
    });
}
