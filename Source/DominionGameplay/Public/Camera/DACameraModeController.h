#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "DACameraModeController.generated.h"

class APawn;
class APlayerController;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;

UENUM(BlueprintType)
enum class EDAPlayMode : uint8
{
    Founder,
    City,
    Command
};

UENUM(BlueprintType)
enum class EDAModeSelectionRule : uint8
{
    Interaction,
    Grid,
    Squad
};

USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDACameraModeParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraArmLength = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FRotator CameraRotation = FRotator(-15.f, 0.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float FieldOfView = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    bool bUsePawnControlRotation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    bool bEnableCameraCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "1.5"))
    float BlendDurationSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    EDAModeSelectionRule SelectionRule = EDAModeSelectionRule::Interaction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RequestedSimulationRate = 1.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDAOnModeChanged, EDAPlayMode, PreviousMode, EDAPlayMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDAOnSimulationRateRequested, float, RequestedRate);

UCLASS(BlueprintType, ClassGroup = (Dominion), meta = (BlueprintSpawnableComponent))
class DOMINIONGAMEPLAY_API UDACameraModeController : public UActorComponent
{
    GENERATED_BODY()

public:
    UDACameraModeController();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Play Mode")
    bool Initialize(APlayerController* PlayerController);

    bool InitializeContext(APawn* Pawn, UWorld* World, UEnhancedInputLocalPlayerSubsystem* InputSubsystem);

    UFUNCTION(BlueprintCallable, Category = "Play Mode")
    bool RequestMode(EDAPlayMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Play Mode")
    void SetMappingContextForMode(EDAPlayMode Mode, UInputMappingContext* MappingContext);

    UFUNCTION(BlueprintPure, Category = "Play Mode")
    EDAPlayMode GetCurrentMode() const;

    UFUNCTION(BlueprintPure, Category = "Play Mode")
    bool IsTransitioning() const;

    UFUNCTION(BlueprintPure, Category = "Play Mode")
    EDAModeSelectionRule GetSelectionRule() const;

    UFUNCTION(BlueprintPure, Category = "Play Mode")
    float GetRequestedSimulationRate() const;

    UFUNCTION(BlueprintPure, Category = "Play Mode|Performance")
    float GetLastTransitionDurationSeconds() const;

    UFUNCTION(BlueprintPure, Category = "Play Mode|Performance")
    float GetTransitionBudgetSeconds() const;

    UFUNCTION(BlueprintPure, Category = "Play Mode|Performance")
    bool WasLastTransitionWithinBudget() const;

    APawn* GetBoundPawn() const;
    UWorld* GetBoundWorld() const;
    UInputMappingContext* GetActiveMappingContext() const;
    const FDACameraModeParameters& GetModeParameters(EDAPlayMode Mode) const;

    UPROPERTY(BlueprintAssignable, Category = "Play Mode")
    FDAOnModeChanged OnModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Play Mode")
    FDAOnSimulationRateRequested OnSimulationRateRequested;

private:
    void BeginCameraTransition(EDAPlayMode NewMode);
    void CompleteCameraTransition();
    void ApplyCameraParameters(const FDACameraModeParameters& Parameters, float Alpha);
    void ApplyControlState(EDAPlayMode Mode);
    UInputMappingContext* ResolveMappingContext(EDAPlayMode Mode) const;

    UPROPERTY(EditDefaultsOnly, Category = "Play Mode|Camera")
    FDACameraModeParameters FounderParameters;

    UPROPERTY(EditDefaultsOnly, Category = "Play Mode|Camera")
    FDACameraModeParameters CityParameters;

    UPROPERTY(EditDefaultsOnly, Category = "Play Mode|Camera")
    FDACameraModeParameters CommandParameters;

    UPROPERTY(EditDefaultsOnly, Category = "Play Mode|Input")
    TSoftObjectPtr<UInputMappingContext> FounderMappingContextAsset;

    UPROPERTY(EditDefaultsOnly, Category = "Play Mode|Input")
    TSoftObjectPtr<UInputMappingContext> CityMappingContextAsset;

    UPROPERTY(EditDefaultsOnly, Category = "Play Mode|Input")
    TSoftObjectPtr<UInputMappingContext> CommandMappingContextAsset;

    UPROPERTY(EditDefaultsOnly, Category = "Play Mode|Performance", meta = (ClampMin = "0.0"))
    float TransitionBudgetSeconds = 1.5f;

    UPROPERTY(Transient)
    TObjectPtr<APawn> BoundPawn;

    UPROPERTY(Transient)
    TObjectPtr<UWorld> BoundWorld;

    UPROPERTY(Transient)
    TObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> FounderMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> CityMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> CommandMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> ActiveMappingContext;

    EDAPlayMode CurrentMode = EDAPlayMode::Founder;
    EDAPlayMode PreviousMode = EDAPlayMode::Founder;
    EDAPlayMode PendingMode = EDAPlayMode::Founder;
    EDAModeSelectionRule ActiveSelectionRule = EDAModeSelectionRule::Interaction;
    float RequestedSimulationRate = 1.f;
    float TransitionElapsedGameSeconds = 0.f;
    float LastTransitionDurationSeconds = 0.f;
    double TransitionStartPlatformSeconds = 0.0;
    bool bTransitioning = false;
    float StartingCameraArmLength = 350.f;
    float StartingFieldOfView = 80.f;
    FRotator StartingCameraRotation = FRotator(-15.f, 0.f, 0.f);
};
