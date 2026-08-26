#pragma once

#include "Save/DACampaignSaveGame.h"

class UDASimulationLODSubsystem;

namespace DA::PerformanceTests
{
    constexpr int32 SoakDevelopmentCycles = 1000;
    constexpr int32 SoakWorldTicks = 200;
    constexpr int64 MaximumSaveBytes = 8 * 1024 * 1024;
    constexpr int64 MaximumSaveGrowthBytes = 2 * 1024 * 1024;
    constexpr int64 MaximumRequiredQuestStaleDevelopmentCycles = 25;

    struct FDASoakResult
    {
        FDACampaignSnapshot Campaign;
        int32 EnabledEventCount = 0;
        int64 InitialSaveBytes = -1;
        int64 FinalSaveBytes = -1;
        FString FinalSave;
        FString FinalSaveSha256;
        bool bAdvanced = false;
        bool bUtilityTopologyResolved = false;
        bool bPopulationStayedNonnegative = true;
        bool bEventRunawayDetected = false;
        bool bStaleRequiredQuestDetected = false;
        bool bQuestResolutionFailed = false;
        int32 UtilityTopologyNodeCount = 0;
        int32 EventEmissionCount = 0;
        int32 EventTransitionCount = 0;
        int32 DevelopmentCycleSamples = 0;
        double WorstDevelopmentCycleSeconds = 0.0;
    };

    FDASoakResult RunCanonicalSoak(const FString& SaveDirectory,
        UDASimulationLODSubsystem* Instrumentation = nullptr);
}
