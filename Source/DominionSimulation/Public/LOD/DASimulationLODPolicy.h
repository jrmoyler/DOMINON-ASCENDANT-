#pragma once

#include <cstdint>

namespace DA::SimulationLOD
{
    enum class Level : std::uint8_t
    {
        Full = 0,
        Detailed = 1,
        Aggregated = 2,
        Strategic = 3,
    };

    enum class Representation : std::uint8_t
    {
        Individual,
        ImportantIndividual,
        Cohort,
        PersistentRecord,
        CohortAggregate,
        DistrictAggregate,
    };

    struct Context
    {
        bool FounderDistrict = false;
        bool Combat = false;
        bool CurrentCity = false;
        bool NearbyDistrict = false;
        bool RegionLoaded = false;
    };

    constexpr Level Resolve(const Context& Value)
    {
        if (Value.FounderDistrict || Value.Combat) return Level::Full;
        if (Value.RegionLoaded && Value.CurrentCity && Value.NearbyDistrict)
            return Level::Detailed;
        if (Value.RegionLoaded && Value.CurrentCity) return Level::Aggregated;
        return Level::Strategic;
    }

    constexpr Representation RepresentationFor(const Level Value, const bool NamedCitizen)
    {
        switch (Value)
        {
        case Level::Full:
            return Representation::Individual;
        case Level::Detailed:
            return NamedCitizen ? Representation::ImportantIndividual
                : Representation::Cohort;
        case Level::Aggregated:
            return NamedCitizen ? Representation::PersistentRecord
                : Representation::CohortAggregate;
        case Level::Strategic:
        default:
            return NamedCitizen ? Representation::PersistentRecord
                : Representation::DistrictAggregate;
        }
    }

    constexpr bool IsValidNamedCitizenId(const std::uint64_t CitizenId)
    {
        return CitizenId != 0;
    }

    constexpr std::uint64_t PreserveNamedCitizenId(
        const std::uint64_t CitizenId, const Level)
    {
        return CitizenId;
    }
}
