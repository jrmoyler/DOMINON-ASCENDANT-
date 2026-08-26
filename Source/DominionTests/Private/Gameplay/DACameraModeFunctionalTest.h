#pragma once

#include "Camera/DACameraModeController.h"
#include "FunctionalTest.h"

#include "DACameraModeFunctionalTest.generated.h"

class ADAFounderCharacter;

/** Place in /Game/Tests/Maps/L_ModeTransitionBenchmark for real-frame transition timing. */
UCLASS(Blueprintable)
class DOMINIONTESTS_API ADACameraModeFunctionalTest : public AFunctionalTest
{
    GENERATED_BODY()

public:
    virtual void StartTest() override;

private:
    UFUNCTION()
    void HandleModeChanged(EDAPlayMode PreviousMode, EDAPlayMode NewMode);

    bool ValidateContinuityAndBudget();
    void RequestNextMode();

    UPROPERTY(EditInstanceOnly, Category = "Benchmark")
    TObjectPtr<ADAFounderCharacter> FounderPawn;

    UPROPERTY(EditInstanceOnly, Category = "Benchmark")
    TObjectPtr<UDACameraModeController> ModeController;

    UPROPERTY(Transient)
    TObjectPtr<APawn> OriginalPawn;

    UPROPERTY(Transient)
    TObjectPtr<UWorld> OriginalWorld;

    UPROPERTY(Transient)
    TObjectPtr<UPackage> OriginalWorldPackage;

    int32 NextModeIndex = 0;
};
