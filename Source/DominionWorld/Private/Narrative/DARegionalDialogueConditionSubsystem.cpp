#include "Narrative/DARegionalDialogueConditionSubsystem.h"

#include "Narrative/DAFoundryShortageRuntime.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"

void UDARegionalDialogueConditionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDAWorldStateSubsystem>();
    if (UGameInstance* GameInstance = GetGameInstance())
        WorldStateSubsystem = GameInstance->GetSubsystem<UDAWorldStateSubsystem>();
}

FName UDARegionalDialogueConditionSubsystem::EvaluateVariant(const FName ConditionId) const
{
    if (!WorldStateSubsystem.IsValid()) return NAME_None;
    const FDACampaignSnapshot& Campaign = WorldStateSubsystem->GetPersistentCampaign();
    if (ConditionId == TEXT("dialogue.daxton.foundry_followup"))
    {
        static const TPair<FName, FName> Variants[] = {
            {TEXT("mara_numbers_quiet"), TEXT("daxton.mara.quiet")},
            {TEXT("mara_numbers_public"), TEXT("daxton.mara.public")},
            {TEXT("mara_numbers_delivered_to_daxton"), TEXT("daxton.mara.private_delivery")},
            {TEXT("mara_numbers_worker_coalition"), TEXT("daxton.mara.worker_coalition")}};
        for (const TPair<FName, FName>& Variant : Variants)
            if (Campaign.HistoryTags.Contains(Variant.Key)) return Variant.Value;
    }
    if (ConditionId == TEXT("dialogue.amara.foundry_followup"))
    {
        static const TPair<FName, FName> Variants[] = {
            {TEXT("green_line_full_boundary"), TEXT("amara.green_line.full_boundary")},
            {TEXT("green_line_industrial_exception"), TEXT("amara.green_line.industrial_exception")},
            {TEXT("green_line_rejected"), TEXT("amara.green_line.rejected")},
            {TEXT("green_line_engineered_mitigation"), TEXT("amara.green_line.engineered_mitigation")}};
        for (const TPair<FName, FName>& Variant : Variants)
            if (Campaign.HistoryTags.Contains(Variant.Key)) return Variant.Value;
    }
    return FDAFoundryShortageRuntime::EvaluateDialogueVariant(ConditionId, Campaign);
}
