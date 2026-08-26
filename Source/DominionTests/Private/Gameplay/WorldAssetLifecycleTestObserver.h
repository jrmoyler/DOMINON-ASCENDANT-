#pragma once

#include "City/DAWorldAssetRecord.h"
#include "UObject/Object.h"

#include "WorldAssetLifecycleTestObserver.generated.h"

UCLASS()
class UWorldAssetLifecycleTestObserver : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION()
    void HandleStageChanged(const EDAConstructionState NewState)
    {
        ++StageChangeCount;
        LastStage = NewState;
    }

    UFUNCTION()
    void HandleConstructionCompleted()
    {
        ++CompletionCount;
    }

    int32 StageChangeCount = 0;
    int32 CompletionCount = 0;
    EDAConstructionState LastStage = EDAConstructionState::Preview;
};
