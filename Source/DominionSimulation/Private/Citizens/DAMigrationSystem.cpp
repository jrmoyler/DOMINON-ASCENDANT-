#include "Citizens/DAMigrationSystem.h"

#include "Economy/DAEconomyTypes.h"

namespace
{
    float SanitizeAttractiveness(const float Attractiveness)
    {
        return FMath::IsFinite(Attractiveness) ? FMath::Clamp(Attractiveness, 0.f, 100.f) : 40.f;
    }
}

float UDAMigrationSystem::CalculateIncomingPerWorldTick(const int32 Vacancy, const float Attractiveness)
{
    const float SafeAttractiveness = SanitizeAttractiveness(Attractiveness);
    return static_cast<float>(FMath::Max(0, Vacancy))
        * 0.15f
        * FMath::Max(0.f, (SafeAttractiveness - 40.f) / 60.f);
}

float UDAMigrationSystem::CalculateOutgoingPerWorldTick(const int32 Population, const float Attractiveness)
{
    const float SafeAttractiveness = SanitizeAttractiveness(Attractiveness);
    return static_cast<float>(FMath::Max(0, Population))
        * 0.02f
        * FMath::Max(0.f, (40.f - SafeAttractiveness) / 40.f);
}

void UDAMigrationSystem::ResolveWorldTick(FDACitySimulationState& State) const
{
    State.Population = FMath::Max(0, State.Population);
    State.IncomingMigrationAccumulator = FMath::Max(0.f, State.IncomingMigrationAccumulator);
    State.OutgoingMigrationAccumulator = FMath::Max(0.f, State.OutgoingMigrationAccumulator);

    State.IncomingMigrationAccumulator += CalculateIncomingPerWorldTick(State.JobVacancies, State.Attractiveness);
    State.OutgoingMigrationAccumulator += CalculateOutgoingPerWorldTick(State.Population, State.Attractiveness);

    State.LastIncomingMigrants = FMath::FloorToInt(State.IncomingMigrationAccumulator);
    State.IncomingMigrationAccumulator -= static_cast<float>(State.LastIncomingMigrants);

    State.LastOutgoingMigrants = FMath::Min(
        State.Population + State.LastIncomingMigrants,
        FMath::FloorToInt(State.OutgoingMigrationAccumulator));
    State.OutgoingMigrationAccumulator -= static_cast<float>(State.LastOutgoingMigrants);

    State.Population += State.LastIncomingMigrants - State.LastOutgoingMigrants;
    ++State.ResolvedWorldTicks;
}
