#include "DACameraModeFunctionalTest.h"

#include "Founder/DAFounderCharacter.h"

#include "EngineUtils.h"
#include "UObject/Package.h"

void ADACameraModeFunctionalTest::StartTest()
{
    Super::StartTest();

    if (!FounderPawn)
    {
        for (TActorIterator<ADAFounderCharacter> It(GetWorld()); It; ++It)
        {
            FounderPawn = *It;
            break;
        }
    }
    if (!ModeController && FounderPawn)
    {
        ModeController = FounderPawn->FindComponentByClass<UDACameraModeController>();
    }
    if (!FounderPawn || !ModeController || !ModeController->InitializeContext(FounderPawn, GetWorld(), nullptr))
    {
        FinishTest(EFunctionalTestResult::Failed, TEXT("Benchmark map requires one Founder pawn with a DA Camera Mode Controller."));
        return;
    }

    OriginalPawn = FounderPawn;
    OriginalWorld = GetWorld();
    OriginalWorldPackage = OriginalWorld->GetOutermost();
    NextModeIndex = 0;
    ModeController->OnModeChanged.AddDynamic(this, &ADACameraModeFunctionalTest::HandleModeChanged);
    RequestNextMode();
}

void ADACameraModeFunctionalTest::HandleModeChanged(const EDAPlayMode PreviousMode, const EDAPlayMode NewMode)
{
    static_cast<void>(PreviousMode);
    static_cast<void>(NewMode);
    if (!ValidateContinuityAndBudget())
    {
        return;
    }

    ++NextModeIndex;
    if (NextModeIndex >= 3)
    {
        FinishTest(EFunctionalTestResult::Succeeded,
            FString::Printf(TEXT("Founder/City/Command transitions preserved the world and remained within %.2fs."), ModeController->GetTransitionBudgetSeconds()));
        return;
    }
    RequestNextMode();
}

bool ADACameraModeFunctionalTest::ValidateContinuityAndBudget()
{
    if (ModeController->GetBoundPawn() != OriginalPawn || ModeController->GetBoundWorld() != OriginalWorld
        || ModeController->GetBoundWorld()->GetOutermost() != OriginalWorldPackage)
    {
        FinishTest(EFunctionalTestResult::Failed, TEXT("A mode transition replaced the Founder pawn, world, or loaded level package."));
        return false;
    }
    if (!ModeController->WasLastTransitionWithinBudget())
    {
        FinishTest(EFunctionalTestResult::Failed,
            FString::Printf(TEXT("Mode transition took %.3fs; budget is %.3fs."),
                ModeController->GetLastTransitionDurationSeconds(), ModeController->GetTransitionBudgetSeconds()));
        return false;
    }
    return true;
}

void ADACameraModeFunctionalTest::RequestNextMode()
{
    static const EDAPlayMode Sequence[] = { EDAPlayMode::City, EDAPlayMode::Command, EDAPlayMode::Founder };
    if (!ModeController->RequestMode(Sequence[NextModeIndex]))
    {
        FinishTest(EFunctionalTestResult::Failed, TEXT("Camera mode controller rejected the benchmark transition."));
    }
}
