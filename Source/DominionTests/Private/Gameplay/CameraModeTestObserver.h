#pragma once

#include "Camera/DACameraModeController.h"
#include "UObject/Object.h"

#include "CameraModeTestObserver.generated.h"

UCLASS()
class UCameraModeTestObserver : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION()
    void HandleModeChanged(const EDAPlayMode PreviousMode, const EDAPlayMode NewMode)
    {
        ++ModeChangeCount;
        LastPreviousMode = PreviousMode;
        LastNewMode = NewMode;
    }

    UFUNCTION()
    void HandleDodgeRequested()
    {
        ++DodgeRequestCount;
    }

    UFUNCTION()
    void HandleInteractRequested()
    {
        ++InteractRequestCount;
    }

    UFUNCTION()
    void HandleTraversalRequested()
    {
        ++TraversalRequestCount;
    }

    int32 ModeChangeCount = 0;
    int32 DodgeRequestCount = 0;
    int32 InteractRequestCount = 0;
    int32 TraversalRequestCount = 0;
    EDAPlayMode LastPreviousMode = EDAPlayMode::Founder;
    EDAPlayMode LastNewMode = EDAPlayMode::Founder;
};
