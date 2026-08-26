#include "Construction/DAConstructionComponent.h"

#include "Cards/DACardInstance.h"
#include "Content/DACardDefinition.h"

bool UDAConstructionComponent::InitializeFromRecord(
    FDAWorldAssetRecord& InRecord,
    const FCardInstance* InCardInstance,
    const UDA_CardDefinition& InCardDefinition)
{
    int32 AuthoredConstructionCycles = 0;
    if (InCardInstance == nullptr
        || !InRecord.WorldAssetId.IsValid()
        || !InCardInstance->InstanceId.IsValid()
        || InCardInstance->WorldAssetId != InRecord.WorldAssetId
        || InRecord.CardInstanceId != InCardInstance->InstanceId
        || InRecord.CardDefinitionId != InCardInstance->DefinitionId
        || InRecord.CardDefinitionId != InCardDefinition.DefinitionId
        || !InCardDefinition.TryGetConstructionCycles(AuthoredConstructionCycles)
        || AuthoredConstructionCycles <= 0)
    {
        Record = nullptr;
        return false;
    }

    if (InRecord.ConstructionCyclesRequired == 0)
    {
        InRecord.ConstructionCyclesRequired = AuthoredConstructionCycles;
    }

    if (InRecord.ConstructionCyclesRequired != AuthoredConstructionCycles)
    {
        Record = nullptr;
        return false;
    }

    InRecord.ConstructionCyclesCompleted = FMath::Clamp(
        InRecord.ConstructionCyclesCompleted,
        0,
        InRecord.ConstructionCyclesRequired);
    if (!FMath::IsFinite(InRecord.ConstructionProgressCycles)
        || InRecord.ConstructionProgressCycles < 0.f
        || InRecord.ConstructionProgressCycles > static_cast<float>(InRecord.ConstructionCyclesRequired))
    {
        Record = nullptr;
        return false;
    }
    InRecord.ConstructionProgressCycles = FMath::Max(
        InRecord.ConstructionProgressCycles,
        static_cast<float>(InRecord.ConstructionCyclesCompleted));
    Record = &InRecord;
    return true;
}

bool UDAConstructionComponent::AdvanceCycle()
{
    return AdvanceCycleWithSpeed(1.f);
}

bool UDAConstructionComponent::AdvanceCycleWithSpeed(const float ConstructionSpeedMultiplier)
{
    if (Record == nullptr || !IsConstructionState(Record->ConstructionState))
    {
        return false;
    }
    if (!FMath::IsFinite(ConstructionSpeedMultiplier)
        || ConstructionSpeedMultiplier <= 0.f) return false;
    Record->ConstructionProgressCycles = FMath::Min(
        Record->ConstructionProgressCycles + ConstructionSpeedMultiplier,
        static_cast<float>(Record->ConstructionCyclesRequired));
    Record->ConstructionCyclesCompleted = FMath::Clamp(
        FMath::FloorToInt(Record->ConstructionProgressCycles + KINDA_SMALL_NUMBER),
        0, Record->ConstructionCyclesRequired);
    const EDAConstructionState PreviousState = Record->ConstructionState;
    Record->ConstructionState = GetStateForCompletedCycles(
        Record->ConstructionCyclesCompleted,
        Record->ConstructionCyclesRequired);

    if (Record->ConstructionState != PreviousState)
    {
        OnStageChanged.Broadcast(Record->ConstructionState);
    }

    if (Record->ConstructionState == EDAConstructionState::Operational)
    {
        OnConstructionCompleted.Broadcast();
    }

    return true;
}

EDAConstructionState UDAConstructionComponent::GetConstructionState() const
{
    return Record != nullptr ? Record->ConstructionState : EDAConstructionState::Preview;
}

bool UDAConstructionComponent::GetPresentationRecord(FDAWorldAssetRecord& OutRecord) const
{
    if (Record == nullptr) return false;
    OutRecord = *Record;
    return true;
}

EDAConstructionState UDAConstructionComponent::GetStateForCompletedCycles(const int32 CompletedCycles, const int32 RequiredCycles)
{
    if (CompletedCycles >= RequiredCycles)
    {
        return EDAConstructionState::Operational;
    }

    constexpr int32 IntermediateStageCount = 4;
    const int32 StageIndex = FMath::Clamp(
        FMath::DivideAndRoundUp(CompletedCycles * IntermediateStageCount, RequiredCycles),
        0,
        IntermediateStageCount - 1);

    static constexpr EDAConstructionState ConstructionStages[IntermediateStageCount] =
    {
        EDAConstructionState::Foundation,
        EDAConstructionState::Frame,
        EDAConstructionState::Shell,
        EDAConstructionState::Systems
    };
    return ConstructionStages[StageIndex];
}

bool UDAConstructionComponent::IsConstructionState(const EDAConstructionState State)
{
    switch (State)
    {
    case EDAConstructionState::Foundation:
    case EDAConstructionState::Frame:
    case EDAConstructionState::Shell:
    case EDAConstructionState::Systems:
        return true;

    default:
        return false;
    }
}
