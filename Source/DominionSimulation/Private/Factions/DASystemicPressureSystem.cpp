#include "Factions/DASystemicPressureSystem.h"
#include "Save/DACampaignSaveGame.h"

namespace
{
    constexpr EDASystemicPressureThreshold DependencyThresholds[] = {
        EDASystemicPressureThreshold::Dependency25,
        EDASystemicPressureThreshold::Dependency50,
        EDASystemicPressureThreshold::Dependency70,
        EDASystemicPressureThreshold::Dependency85,
        EDASystemicPressureThreshold::Dependency100
    };

    float ToDependencyValue(const EDASystemicPressureThreshold Threshold)
    {
        return static_cast<float>(static_cast<uint8>(Threshold));
    }
}

bool UDASystemicPressureSystem::ApplyDependencyReason(FDACampaignSnapshot& Campaign, const FName ActionId,
    const double Delta, const int64 WorldTick)
{
    if (!Campaign.SynaraState.ApplyDependencyReason(ActionId, Delta, WorldTick)) return false;
    SetDependency(static_cast<float>(Campaign.SynaraState.Dependency)); return true;
}

float UDASystemicPressureSystem::GetDependency(const FDACampaignSnapshot& Campaign) const
{ return static_cast<float>(Campaign.SynaraState.Dependency); }

void UDASystemicPressureSystem::SetDependency(const float NewDependency)
{
    const float PreviousDependency = Dependency;
    Dependency = FMath::Clamp(FMath::IsFinite(NewDependency) ? NewDependency : 0.f, 0.f, 100.f);

    for (const EDASystemicPressureThreshold Threshold : DependencyThresholds)
    {
        const float ThresholdValue = ToDependencyValue(Threshold);
        const bool bCrossedUpward = PreviousDependency < ThresholdValue && Dependency >= ThresholdValue;
        const bool bCrossedDownward = PreviousDependency >= ThresholdValue && Dependency < ThresholdValue;
        if (bCrossedUpward || bCrossedDownward)
        {
            OnSystemicPressureChanged.Broadcast(Threshold, PreviousDependency, Dependency);
        }
    }
}
