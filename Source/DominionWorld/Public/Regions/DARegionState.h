#pragma once

#include "CoreMinimal.h"
#include "World/DARegionalWorldState.h"

struct DOMINIONWORLD_API FDARegionSeedCatalog
{
    static TArray<FDARegionState> MakeVerticalSliceRegions();
};
