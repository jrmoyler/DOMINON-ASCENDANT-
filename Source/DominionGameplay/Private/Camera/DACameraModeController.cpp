#include "Camera/DACameraModeController.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Founder/DAFounderCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"

UDACameraModeController::UDACameraModeController()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    FounderParameters.CameraArmLength = 350.f;
    FounderParameters.CameraRotation = FRotator(-15.f, 0.f, 0.f);
    FounderParameters.FieldOfView = 80.f;
    FounderParameters.bUsePawnControlRotation = true;
    FounderParameters.bEnableCameraCollision = true;
    FounderParameters.BlendDurationSeconds = 0.35f;
    FounderParameters.SelectionRule = EDAModeSelectionRule::Interaction;
    FounderParameters.RequestedSimulationRate = 1.f;

    CityParameters.CameraArmLength = 3000.f;
    CityParameters.CameraRotation = FRotator(-65.f, 0.f, 0.f);
    CityParameters.FieldOfView = 70.f;
    CityParameters.bUsePawnControlRotation = false;
    CityParameters.bEnableCameraCollision = false;
    CityParameters.BlendDurationSeconds = 0.6f;
    CityParameters.SelectionRule = EDAModeSelectionRule::Grid;
    CityParameters.RequestedSimulationRate = 1.f;

    CommandParameters.CameraArmLength = 1600.f;
    CommandParameters.CameraRotation = FRotator(-55.f, 0.f, 0.f);
    CommandParameters.FieldOfView = 75.f;
    CommandParameters.bUsePawnControlRotation = false;
    CommandParameters.bEnableCameraCollision = false;
    CommandParameters.BlendDurationSeconds = 0.45f;
    CommandParameters.SelectionRule = EDAModeSelectionRule::Squad;
    CommandParameters.RequestedSimulationRate = 0.20f;

    FounderMappingContextAsset = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Founder.IMC_Founder")));
    CityMappingContextAsset = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_City.IMC_City")));
    CommandMappingContextAsset = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Command.IMC_Command")));
}

void UDACameraModeController::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
    if (!PlayerController)
    {
        if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
        {
            PlayerController = Cast<APlayerController>(OwnerPawn->GetController());
        }
    }
    if (PlayerController)
    {
        Initialize(PlayerController);
    }
}

void UDACameraModeController::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bTransitioning)
    {
        return;
    }

    TransitionElapsedGameSeconds += FMath::Max(DeltaTime, 0.f);
    const float BlendDuration = GetModeParameters(PendingMode).BlendDurationSeconds;
    const float Alpha = BlendDuration <= 0.f ? 1.f : FMath::Clamp(TransitionElapsedGameSeconds / BlendDuration, 0.f, 1.f);
    ApplyCameraParameters(GetModeParameters(PendingMode), Alpha);
    if (Alpha >= 1.f)
    {
        CompleteCameraTransition();
    }
}

bool UDACameraModeController::Initialize(APlayerController* PlayerController)
{
    if (!PlayerController || !PlayerController->GetPawn() || !PlayerController->GetWorld())
    {
        return false;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = nullptr;
    if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
    {
        InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    }
    return InitializeContext(PlayerController->GetPawn(), PlayerController->GetWorld(), InputSubsystem);
}

bool UDACameraModeController::InitializeContext(APawn* Pawn, UWorld* World, UEnhancedInputLocalPlayerSubsystem* InputSubsystem)
{
    if (!Pawn || !World || (Pawn->GetWorld() && Pawn->GetWorld() != World))
    {
        return false;
    }

    BoundPawn = Pawn;
    BoundWorld = World;
    EnhancedInputSubsystem = InputSubsystem;
    CurrentMode = EDAPlayMode::Founder;
    PreviousMode = CurrentMode;
    PendingMode = CurrentMode;
    bTransitioning = false;
    SetComponentTickEnabled(false);
    ApplyCameraParameters(FounderParameters, 1.f);
    ApplyControlState(CurrentMode);
    return true;
}

bool UDACameraModeController::RequestMode(const EDAPlayMode NewMode)
{
    if (!BoundPawn || !BoundWorld || (BoundPawn->GetWorld() && BoundPawn->GetWorld() != BoundWorld) || bTransitioning)
    {
        return false;
    }
    if (NewMode == CurrentMode)
    {
        return true;
    }

    BeginCameraTransition(NewMode);
    return true;
}

void UDACameraModeController::SetMappingContextForMode(const EDAPlayMode Mode, UInputMappingContext* MappingContext)
{
    switch (Mode)
    {
    case EDAPlayMode::Founder:
        FounderMappingContext = MappingContext;
        break;
    case EDAPlayMode::City:
        CityMappingContext = MappingContext;
        break;
    case EDAPlayMode::Command:
        CommandMappingContext = MappingContext;
        break;
    }
}

EDAPlayMode UDACameraModeController::GetCurrentMode() const
{
    return CurrentMode;
}

bool UDACameraModeController::IsTransitioning() const
{
    return bTransitioning;
}

EDAModeSelectionRule UDACameraModeController::GetSelectionRule() const
{
    return ActiveSelectionRule;
}

float UDACameraModeController::GetRequestedSimulationRate() const
{
    return RequestedSimulationRate;
}

float UDACameraModeController::GetLastTransitionDurationSeconds() const
{
    return LastTransitionDurationSeconds;
}

float UDACameraModeController::GetTransitionBudgetSeconds() const
{
    return TransitionBudgetSeconds;
}

bool UDACameraModeController::WasLastTransitionWithinBudget() const
{
    return LastTransitionDurationSeconds <= TransitionBudgetSeconds;
}

APawn* UDACameraModeController::GetBoundPawn() const
{
    return BoundPawn;
}

UWorld* UDACameraModeController::GetBoundWorld() const
{
    return BoundWorld;
}

UInputMappingContext* UDACameraModeController::GetActiveMappingContext() const
{
    return ActiveMappingContext;
}

const FDACameraModeParameters& UDACameraModeController::GetModeParameters(const EDAPlayMode Mode) const
{
    switch (Mode)
    {
    case EDAPlayMode::City:
        return CityParameters;
    case EDAPlayMode::Command:
        return CommandParameters;
    case EDAPlayMode::Founder:
    default:
        return FounderParameters;
    }
}

void UDACameraModeController::BeginCameraTransition(const EDAPlayMode NewMode)
{
    PreviousMode = CurrentMode;
    PendingMode = NewMode;
    TransitionElapsedGameSeconds = 0.f;
    TransitionStartPlatformSeconds = FPlatformTime::Seconds();

    if (const ADAFounderCharacter* Founder = Cast<ADAFounderCharacter>(BoundPawn))
    {
        StartingCameraArmLength = Founder->GetCameraBoom()->TargetArmLength;
        StartingCameraRotation = Founder->GetCameraBoom()->GetRelativeRotation();
        StartingFieldOfView = Founder->GetFounderCamera()->FieldOfView;
    }
    else
    {
        const FDACameraModeParameters& CurrentParameters = GetModeParameters(CurrentMode);
        StartingCameraArmLength = CurrentParameters.CameraArmLength;
        StartingCameraRotation = CurrentParameters.CameraRotation;
        StartingFieldOfView = CurrentParameters.FieldOfView;
    }

    ApplyControlState(NewMode);
    bTransitioning = true;
    SetComponentTickEnabled(true);
    if (GetModeParameters(NewMode).BlendDurationSeconds <= 0.f)
    {
        ApplyCameraParameters(GetModeParameters(NewMode), 1.f);
        CompleteCameraTransition();
    }
}

void UDACameraModeController::CompleteCameraTransition()
{
    ApplyCameraParameters(GetModeParameters(PendingMode), 1.f);
    CurrentMode = PendingMode;
    LastTransitionDurationSeconds = static_cast<float>(FPlatformTime::Seconds() - TransitionStartPlatformSeconds);
    bTransitioning = false;
    SetComponentTickEnabled(false);
    OnModeChanged.Broadcast(PreviousMode, CurrentMode);
}

void UDACameraModeController::ApplyCameraParameters(const FDACameraModeParameters& Parameters, const float Alpha)
{
    ADAFounderCharacter* Founder = Cast<ADAFounderCharacter>(BoundPawn);
    if (!Founder)
    {
        return;
    }

    Founder->GetCameraBoom()->TargetArmLength = FMath::Lerp(StartingCameraArmLength, Parameters.CameraArmLength, Alpha);
    Founder->GetCameraBoom()->SetRelativeRotation(FMath::Lerp(StartingCameraRotation, Parameters.CameraRotation, Alpha));
    Founder->GetCameraBoom()->bUsePawnControlRotation = Parameters.bUsePawnControlRotation;
    Founder->GetCameraBoom()->bDoCollisionTest = Parameters.bEnableCameraCollision;
    Founder->GetFounderCamera()->SetFieldOfView(FMath::Lerp(StartingFieldOfView, Parameters.FieldOfView, Alpha));
}

void UDACameraModeController::ApplyControlState(const EDAPlayMode Mode)
{
    const FDACameraModeParameters& Parameters = GetModeParameters(Mode);
    ActiveSelectionRule = Parameters.SelectionRule;
    RequestedSimulationRate = Parameters.RequestedSimulationRate;

    if (EnhancedInputSubsystem && ActiveMappingContext)
    {
        EnhancedInputSubsystem->RemoveMappingContext(ActiveMappingContext);
    }
    ActiveMappingContext = ResolveMappingContext(Mode);
    if (EnhancedInputSubsystem && ActiveMappingContext)
    {
        EnhancedInputSubsystem->AddMappingContext(ActiveMappingContext, 0);
    }

    OnSimulationRateRequested.Broadcast(RequestedSimulationRate);
}

UInputMappingContext* UDACameraModeController::ResolveMappingContext(const EDAPlayMode Mode) const
{
    switch (Mode)
    {
    case EDAPlayMode::City:
        return CityMappingContext ? CityMappingContext.Get() : CityMappingContextAsset.LoadSynchronous();
    case EDAPlayMode::Command:
        return CommandMappingContext ? CommandMappingContext.Get() : CommandMappingContextAsset.LoadSynchronous();
    case EDAPlayMode::Founder:
    default:
        return FounderMappingContext ? FounderMappingContext.Get() : FounderMappingContextAsset.LoadSynchronous();
    }
}
