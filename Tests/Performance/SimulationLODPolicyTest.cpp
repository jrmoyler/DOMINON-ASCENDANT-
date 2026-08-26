#include "LOD/DASimulationLODPolicy.h"

#include <array>
#include <cassert>
#include <cstdint>

int main()
{
    using namespace DA::SimulationLOD;

    constexpr std::array<Context, 4> Contexts = {{
        {true, false, true, true, true},
        {false, false, true, true, true},
        {false, false, true, false, true},
        {false, false, false, false, false},
    }};
    constexpr std::array<Level, 4> Expected = {{
        Level::Full,
        Level::Detailed,
        Level::Aggregated,
        Level::Strategic,
    }};

    constexpr std::uint64_t NamedCitizenId = 0xDA270001ULL;
    for (std::size_t Index = 0; Index < Contexts.size(); ++Index)
    {
        assert(Resolve(Contexts[Index]) == Expected[Index]);
        assert(PreserveNamedCitizenId(NamedCitizenId, Expected[Index]) == NamedCitizenId);
    }

    assert(Resolve({false, true, false, false, true}) == Level::Full);
    assert(RepresentationFor(Level::Detailed, true) == Representation::ImportantIndividual);
    assert(RepresentationFor(Level::Detailed, false) == Representation::Cohort);
    assert(RepresentationFor(Level::Aggregated, true) == Representation::PersistentRecord);
    assert(RepresentationFor(Level::Strategic, false) == Representation::DistrictAggregate);
    assert(!IsValidNamedCitizenId(0));
    assert(IsValidNamedCitizenId(NamedCitizenId));
    return 0;
}
