#include "Camera/DACameraModeController.h"
#include "Founder/DAFounderCharacter.h"

#include "CameraModeTestObserver.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDACameraModeSpec, "Dominion.Gameplay.CameraModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDACameraModeSpec)

namespace
{
void CompleteCameraTransition(UDACameraModeController& Controller)
{
    for (int32 Step = 0; Step < 40 && Controller.IsTransitioning(); ++Step)
    {
        Controller.TickComponent(0.05f, LEVELTICK_All, nullptr);
    }
}
}

void FDACameraModeSpec::Define()
{
    It("preserves the Founder pawn and world while switching Founder to City to Command to Founder", [this]()
    {
        UWorld* BenchmarkWorld = NewObject<UWorld>(GetTransientPackage(), FName("DA_ModeTransitionBenchmark"));
        ADAFounderCharacter* Founder = NewObject<ADAFounderCharacter>();
        UDACameraModeController* Controller = NewObject<UDACameraModeController>(Founder);
        UInputMappingContext* FounderContext = NewObject<UInputMappingContext>();
        UInputMappingContext* CityContext = NewObject<UInputMappingContext>();
        UInputMappingContext* CommandContext = NewObject<UInputMappingContext>();
        Controller->SetMappingContextForMode(EDAPlayMode::Founder, FounderContext);
        Controller->SetMappingContextForMode(EDAPlayMode::City, CityContext);
        Controller->SetMappingContextForMode(EDAPlayMode::Command, CommandContext);
        TestTrue("Controller accepts the existing pawn/world context", Controller->InitializeContext(Founder, BenchmarkWorld, nullptr));

        APawn* OriginalPawn = Controller->GetBoundPawn();
        UWorld* OriginalWorld = Controller->GetBoundWorld();
        UPackage* OriginalWorldPackage = OriginalWorld->GetOutermost();
        UCameraModeTestObserver* Observer = NewObject<UCameraModeTestObserver>();
        Controller->OnModeChanged.AddDynamic(Observer, &UCameraModeTestObserver::HandleModeChanged);

        struct FExpectedMode
        {
            EDAPlayMode Mode;
            UInputMappingContext* Context;
            EDAModeSelectionRule SelectionRule;
            float RequestedSimulationRate;
            float CameraArmLength;
            float CameraPitch;
            float FieldOfView;
            bool bUsesPawnControlRotation;
            bool bUsesCameraCollision;
        };
        const FExpectedMode Sequence[] = {
            { EDAPlayMode::City, CityContext, EDAModeSelectionRule::Grid, 1.f, 3000.f, -65.f, 70.f, false, false },
            { EDAPlayMode::Command, CommandContext, EDAModeSelectionRule::Squad, 0.20f, 1600.f, -55.f, 75.f, false, false },
            { EDAPlayMode::Founder, FounderContext, EDAModeSelectionRule::Interaction, 1.f, 350.f, -15.f, 80.f, true, true }
        };

        for (const FExpectedMode& Expected : Sequence)
        {
            TestTrue("Mode request starts a transition", Controller->RequestMode(Expected.Mode));
            TestTrue("Transition is observable before it completes", Controller->IsTransitioning());
            CompleteCameraTransition(*Controller);
            TestFalse("Configured camera blend completes", Controller->IsTransitioning());
            TestTrue("Requested mode becomes current", Controller->GetCurrentMode() == Expected.Mode);
            TestTrue("Mode selects its Enhanced Input context", Controller->GetActiveMappingContext() == Expected.Context);
            TestTrue("Mode selects its targeting rules", Controller->GetSelectionRule() == Expected.SelectionRule);
            TestEqual("Mode requests its simulation rate", Controller->GetRequestedSimulationRate(), Expected.RequestedSimulationRate);
            TestEqual("Mode applies its camera distance", Founder->GetCameraBoom()->TargetArmLength, Expected.CameraArmLength);
            TestEqual("Mode applies its camera pitch", Founder->GetCameraBoom()->GetRelativeRotation().Pitch, Expected.CameraPitch);
            TestEqual("Mode applies its field of view", Founder->GetFounderCamera()->FieldOfView, Expected.FieldOfView);
            TestEqual("Mode applies its camera control scheme", Founder->GetCameraBoom()->bUsePawnControlRotation, Expected.bUsesPawnControlRotation);
            TestEqual("Mode applies its camera collision policy", Founder->GetCameraBoom()->bDoCollisionTest, Expected.bUsesCameraCollision);
            TestTrue("Transition stays within the configured benchmark budget", Controller->WasLastTransitionWithinBudget());
            TestTrue("Transition preserves the exact pawn", Controller->GetBoundPawn() == OriginalPawn);
            TestTrue("Transition preserves the exact world", Controller->GetBoundWorld() == OriginalWorld);
            TestTrue("Transition does not load a different level package", Controller->GetBoundWorld()->GetOutermost() == OriginalWorldPackage);
        }

        TestEqual("Every completed transition emits one mode change", Observer->ModeChangeCount, 3);
        TestTrue("Final callback reports Command to Founder", Observer->LastPreviousMode == EDAPlayMode::Command && Observer->LastNewMode == EDAPlayMode::Founder);
        TestEqual("Performance budget is the architecture target", Controller->GetTransitionBudgetSeconds(), 1.5f);
    });

    It("applies Founder move and sprint input to character movement state", [this]()
    {
        ADAFounderCharacter* Founder = NewObject<ADAFounderCharacter>();

        Founder->HandleMoveInput(FInputActionValue(FVector2D(0.f, 1.f)));
        TestTrue("Forward input reaches the Character movement accumulator", Founder->GetPendingMovementInputVector().X > 0.f);

        Founder->HandleSprintStarted();
        TestTrue("Sprint state is active", Founder->IsSprinting());
        TestEqual("Sprint selects the configured sprint speed", Founder->GetCharacterMovement()->MaxWalkSpeed, 900.f);

        Founder->HandleSprintCompleted();
        TestFalse("Sprint state clears", Founder->IsSprinting());
        TestEqual("Stopping sprint restores walk speed", Founder->GetCharacterMovement()->MaxWalkSpeed, 600.f);
    });

    It("routes Founder dodge interact and traversal input through gameplay hooks", [this]()
    {
        ADAFounderCharacter* Founder = NewObject<ADAFounderCharacter>();
        UCameraModeTestObserver* Observer = NewObject<UCameraModeTestObserver>();
        Founder->OnDodgeRequested.AddDynamic(Observer, &UCameraModeTestObserver::HandleDodgeRequested);
        Founder->OnInteractRequested.AddDynamic(Observer, &UCameraModeTestObserver::HandleInteractRequested);
        Founder->OnTraversalRequested.AddDynamic(Observer, &UCameraModeTestObserver::HandleTraversalRequested);

        Founder->HandleDodgeInput();
        Founder->HandleInteractInput();
        Founder->HandleTraversalInput();

        TestEqual("Dodge input emits one gameplay request", Observer->DodgeRequestCount, 1);
        TestEqual("Interact input emits one gameplay request", Observer->InteractRequestCount, 1);
        TestEqual("Traversal input emits one gameplay request", Observer->TraversalRequestCount, 1);
    });
}
