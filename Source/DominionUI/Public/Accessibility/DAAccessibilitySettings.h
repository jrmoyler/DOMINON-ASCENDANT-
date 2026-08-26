#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Interface.h"

#include "DAAccessibilitySettings.generated.h"

class APlayerController;
class UCameraShakeBase;
class UInputMappingContext;
struct FDAUIManifest;

UENUM(BlueprintType)
enum class EDAColorVisionPreset : uint8
{
    Off,
    Deuteranopia,
    Protanopia,
    Tritanopia,
    HighContrast
};

UENUM(BlueprintType)
enum class EDAHoldToggleMode : uint8
{
    Hold,
    Toggle,
    Press
};

UENUM(BlueprintType)
enum class EDATooltipMode : uint8
{
    Simple,
    Advanced
};

/** Complete frozen accessibility state; all values are validated before persistence. */
USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAAccessibilitySettings
{
    GENERATED_BODY()

    static FDAAccessibilitySettings MakeDefaults();
    bool Validate(FString& OutError) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FKey> KeyboardBindings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FKey> ControllerBindings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TextScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSubtitles = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSpeakerLabels = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SubtitleBackgroundOpacity = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bColorIndependentMarkers = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDAColorVisionPreset ColorVisionPreset = EDAColorVisionPreset::Off;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CameraShake = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FieldOfView = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bMotionBlur = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReducedMotion = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReducedFlash = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTacticalPause = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, EDAHoldToggleMode> HoldToggleModes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimAssist = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BuildSnapStrength = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTutorialRecall = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDATooltipMode TooltipMode = EDATooltipMode::Simple;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIEffectsPolicy
{
    GENERATED_BODY()
    static FDAUIEffectsPolicy FromSettings(const FDAAccessibilitySettings& Settings);
    UPROPERTY(BlueprintReadOnly) bool bAllowNonEssentialMotion = true;
    UPROPERTY(BlueprintReadOnly) bool bAllowFullIntensityFlash = true;
    UPROPERTY(BlueprintReadOnly) float CameraShakeScale = 1.f;
};

/** Typed readback contract consumed by UI, camera, input, combat and tutorial services. */
USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAAccessibilityRuntimePolicy
{
    GENERATED_BODY()
    static FDAAccessibilityRuntimePolicy FromSettings(const FDAAccessibilitySettings& Settings);
    UPROPERTY(BlueprintReadOnly) FDAAccessibilitySettings Settings;
    UPROPERTY(BlueprintReadOnly) FDAUIEffectsPolicy Effects;
    UPROPERTY(BlueprintReadOnly) float EffectiveTextScale = 1.f;
    UPROPERTY(BlueprintReadOnly) float EffectiveFieldOfView = 90.f;
    UPROPERTY(BlueprintReadOnly) bool bRenderSubtitles = true;
    UPROPERTY(BlueprintReadOnly) bool bRenderSpeakerLabels = true;
    UPROPERTY(BlueprintReadOnly) bool bUseColorIndependentMarkers = true;
    UPROPERTY(BlueprintReadOnly) bool bAllowMotionBlur = true;
    UPROPERTY(BlueprintReadOnly) bool bAllowReducedFlashOnly = false;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDASubtitleAccessibilityRuntime : public UInterface
{
    GENERATED_BODY()
};
class DOMINIONUI_API IDASubtitleAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual bool ApplySubtitleAccessibility(bool bSubtitles, bool bSpeakerLabels,
        float BackgroundOpacity, FString& OutError) = 0;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDAFlashAccessibilityRuntime : public UInterface
{
    GENERATED_BODY()
};
class DOMINIONUI_API IDAFlashAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual bool ApplyFlashAccessibility(bool bReducedFlash, FString& OutError) = 0;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDACameraAccessibilityRuntime : public UInterface
{
    GENERATED_BODY()
};
class DOMINIONUI_API IDACameraAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual bool ApplyCameraAccessibility(float FieldOfView, bool bMotionBlur,
        float CameraShakeScale, bool bReducedMotion, FString& OutError) = 0;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDAGameplayAccessibilityRuntime : public UInterface
{
    GENERATED_BODY()
};
class DOMINIONUI_API IDAGameplayAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual bool ApplyGameplayAccessibility(bool bTacticalPause, float AimAssist,
        float BuildSnapStrength, FString& OutError) = 0;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDATutorialAccessibilityRuntime : public UInterface
{
    GENERATED_BODY()
};
class DOMINIONUI_API IDATutorialAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual bool ApplyTutorialAccessibility(bool bTutorialRecall, EDATooltipMode TooltipMode,
        FString& OutError) = 0;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDAInputAccessibilityRuntime : public UInterface
{
    GENERATED_BODY()
};
class DOMINIONUI_API IDAInputAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual bool ApplyInputAccessibility(const TMap<FName, FKey>& KeyboardBindings,
        const TMap<FName, FKey>& ControllerBindings,
        const TMap<FName, EDAHoldToggleMode>& HoldToggleModes, FString& OutError) = 0;
};

/** Fans a candidate into live runtime subsystems and rejects incomplete downstream wiring. */
UCLASS()
class DOMINIONUI_API UDAAccessibilityRuntimeBridgeSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool RegisterRuntime(UObject* Runtime, FString& OutError);
    void UnregisterRuntime(UObject* Runtime);
    bool ApplyToRegisteredRuntimes(const FDAAccessibilityRuntimePolicy& Policy, FString& OutError);
    bool ApplyToRegisteredRuntimes(const FDAAccessibilityRuntimePolicy& Policy,
        const FDAAccessibilityRuntimePolicy& RollbackPolicy, FString& OutError);
private:
    UPROPERTY(Transient) TArray<TWeakObjectPtr<UObject>> Runtimes;
};

UINTERFACE(BlueprintType, meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDAAccessibilityConsumer : public UInterface
{
    GENERATED_BODY()
};

class DOMINIONUI_API IDAAccessibilityConsumer
{
    GENERATED_BODY()
public:
    virtual void ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& Policy) = 0;
};

/** Strongly owned production adapters fan the typed policy into each runtime concern. */
UCLASS(Abstract)
class DOMINIONUI_API UDAAccessibilityAdapterBase : public UObject, public IDAAccessibilityConsumer
{
    GENERATED_BODY()
public:
    virtual void ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& InPolicy) override { Policy = InPolicy; }
    const FDAAccessibilityRuntimePolicy& GetPolicy() const { return Policy; }
protected:
    UPROPERTY(Transient) FDAAccessibilityRuntimePolicy Policy;
};

UCLASS()
class DOMINIONUI_API UDAUIVisualAccessibilityAdapter final : public UDAAccessibilityAdapterBase,
    public IDASubtitleAccessibilityRuntime, public IDAFlashAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual void ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& InPolicy) override;
    virtual bool ApplySubtitleAccessibility(bool bSubtitles, bool bSpeakerLabels,
        float BackgroundOpacity, FString& OutError) override;
    virtual bool ApplyFlashAccessibility(bool bReducedFlash, FString& OutError) override;
    float GetAppliedTextScale() const { return AppliedTextScale; }
    bool ShouldRenderSubtitles() const { return bRenderSubtitles; }
    bool ShouldRenderSpeakerLabels() const { return bRenderSpeakerLabels; }
    bool UsesColorIndependentMarkers() const { return bUseColorIndependentMarkers; }
    float GetSubtitleBackgroundOpacity() const { return SubtitleBackgroundOpacity; }
    bool UsesHighContrast() const { return bUseHighContrast; }
    bool AllowsNonEssentialMotion() const { return bAllowNonEssentialMotion; }
    bool AllowsFullIntensityFlash() const { return bAllowFullIntensityFlash; }
    bool ShouldRenderSubtitleCue() const { return bRenderSubtitles; }
    FString RenderSubtitleCue(FName SpeakerId, const FText& Cue) const
    {
        if (!bRenderSubtitles) return FString();
        return bRenderSpeakerLabels && !SpeakerId.IsNone()
            ? FString::Printf(TEXT("%s: %s"), *SpeakerId.ToString(), *Cue.ToString()) : Cue.ToString();
    }
    bool ShouldPlayFlashEffect(bool bEssential) const { return bEssential || bAllowFullIntensityFlash; }
private:
    UPROPERTY(Transient) float AppliedTextScale = 1.f;
    UPROPERTY(Transient) float SubtitleBackgroundOpacity = 0.75f;
    UPROPERTY(Transient) EDAColorVisionPreset ColorVisionPreset = EDAColorVisionPreset::Off;
    UPROPERTY(Transient) bool bRenderSubtitles = true;
    UPROPERTY(Transient) bool bRenderSpeakerLabels = true;
    UPROPERTY(Transient) bool bUseColorIndependentMarkers = true;
    UPROPERTY(Transient) bool bUseHighContrast = false;
    UPROPERTY(Transient) bool bAllowNonEssentialMotion = true;
    UPROPERTY(Transient) bool bAllowFullIntensityFlash = true;
};
UCLASS()
class DOMINIONUI_API UDACameraAccessibilityAdapter final : public UDAAccessibilityAdapterBase,
    public IDACameraAccessibilityRuntime
{
    GENERATED_BODY()
public:
    void BindPlayerController(APlayerController* InPlayerController);
    virtual void ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& InPolicy) override;
    virtual bool ApplyCameraAccessibility(float FieldOfView, bool bMotionBlur,
        float CameraShakeScale, bool bReducedMotion, FString& OutError) override;
    float GetAppliedFieldOfView() const { return AppliedFieldOfView; }
    float GetAppliedShakeScale() const { return AppliedShakeScale; }
    float ScaleCameraShakeAmplitude(float AuthoredAmplitude) const
    { return FMath::Max(0.f, AuthoredAmplitude) * AppliedShakeScale; }
    bool StartAccessibleCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float AuthoredAmplitude);
    bool ShouldAllowMotionBlur() const { return bAllowMotionBlur; }
private:
    void ApplyToBoundPlayer();
    UPROPERTY(Transient) TWeakObjectPtr<APlayerController> PlayerController;
    UPROPERTY(Transient) float AppliedFieldOfView = 90.f;
    UPROPERTY(Transient) float AppliedShakeScale = 1.f;
    UPROPERTY(Transient) bool bAllowMotionBlur = true;
};
UCLASS()
class DOMINIONUI_API UDAInputAccessibilityAdapter final : public UDAAccessibilityAdapterBase,
    public IDAInputAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual void ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& InPolicy) override
    {
        Super::ApplyAccessibilityPolicy(InPolicy);
        AppliedKeyboardBindings = InPolicy.Settings.KeyboardBindings;
        AppliedControllerBindings = InPolicy.Settings.ControllerBindings;
        AppliedHoldToggleModes = InPolicy.Settings.HoldToggleModes;
        ToggleStates.Reset();
        OnRemappingsApplied.Broadcast();
    }
    UInputMappingContext* BuildPlayerMappingContext(
        const UInputMappingContext& FrozenContext, const FDAUIManifest& Manifest,
        const struct FDAUIScreenDescriptor& ActiveScreen, FString& OutError);
    EDAHoldToggleMode GetAppliedMode(const FName ActionId) const
    {
        const EDAHoldToggleMode* Mode = AppliedHoldToggleModes.Find(ActionId);
        return Mode == nullptr ? EDAHoldToggleMode::Press : *Mode;
    }
    /** Consumes an Enhanced Input trigger pulse using the persisted per-action hold/toggle state. */
    bool ConsumeAccessibleAction(FName ActionId, bool bTriggered);
    DECLARE_MULTICAST_DELEGATE(FDAInputRemappingsApplied);
    FDAInputRemappingsApplied OnRemappingsApplied;
    virtual bool ApplyInputAccessibility(const TMap<FName, FKey>& KeyboardBindings,
        const TMap<FName, FKey>& ControllerBindings,
        const TMap<FName, EDAHoldToggleMode>& HoldToggleModes, FString& OutError) override;
private:
    UPROPERTY(Transient) TMap<FName, FKey> AppliedKeyboardBindings;
    UPROPERTY(Transient) TMap<FName, FKey> AppliedControllerBindings;
    UPROPERTY(Transient) TMap<FName, EDAHoldToggleMode> AppliedHoldToggleModes;
    UPROPERTY(Transient) TMap<FName, bool> ToggleStates;
};
UCLASS()
class DOMINIONUI_API UDAGameplayAccessibilityAdapter final : public UDAAccessibilityAdapterBase,
    public IDAGameplayAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual void ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& InPolicy) override
    {
        Super::ApplyAccessibilityPolicy(InPolicy);
        bTacticalPause = InPolicy.Settings.bTacticalPause;
        AimAssist = InPolicy.Settings.AimAssist;
        BuildSnapStrength = InPolicy.Settings.BuildSnapStrength;
    }
    bool IsTacticalPauseEnabled() const { return bTacticalPause; }
    float GetAimAssist() const { return AimAssist; }
    float GetBuildSnapStrength() const { return BuildSnapStrength; }
    FVector ApplyAimAssist(const FVector& RawDirection, const FVector& TargetDirection) const
    { return FMath::Lerp(RawDirection.GetSafeNormal(), TargetDirection.GetSafeNormal(), AimAssist).GetSafeNormal(); }
    FVector SnapBuildCoordinate(const FVector& RawCoordinate) const
    { return FMath::Lerp(RawCoordinate, FVector(FMath::RoundToDouble(RawCoordinate.X),
        FMath::RoundToDouble(RawCoordinate.Y), FMath::RoundToDouble(RawCoordinate.Z)), BuildSnapStrength); }
    static FIntPoint ResolveBuildGridOrigin(const FVector2D& RawCoordinate, float SnapStrength)
    {
        const double Bias = 0.5 * static_cast<double>(FMath::Clamp(SnapStrength, 0.f, 1.f));
        return FIntPoint(FMath::FloorToInt(RawCoordinate.X + Bias),
            FMath::FloorToInt(RawCoordinate.Y + Bias));
    }
    virtual bool ApplyGameplayAccessibility(bool bInTacticalPause, float InAimAssist,
        float InBuildSnapStrength, FString& OutError) override;
private:
    UPROPERTY(Transient) bool bTacticalPause = true;
    UPROPERTY(Transient) float AimAssist = 0.5f;
    UPROPERTY(Transient) float BuildSnapStrength = 1.f;
};
UCLASS()
class DOMINIONUI_API UDATutorialAccessibilityAdapter final : public UDAAccessibilityAdapterBase,
    public IDATutorialAccessibilityRuntime
{
    GENERATED_BODY()
public:
    virtual void ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& InPolicy) override
    {
        Super::ApplyAccessibilityPolicy(InPolicy);
        bRecallEnabled = InPolicy.Settings.bTutorialRecall;
        TooltipMode = InPolicy.Settings.TooltipMode;
    }
    bool IsRecallEnabled() const { return bRecallEnabled; }
    EDATooltipMode GetTooltipMode() const { return TooltipMode; }
    bool CanRecallTutorial(FName TutorialId) const { return bRecallEnabled && !TutorialId.IsNone(); }
    bool ShouldRenderAdvancedTooltip() const { return TooltipMode == EDATooltipMode::Advanced; }
    virtual bool ApplyTutorialAccessibility(bool bTutorialRecall, EDATooltipMode InTooltipMode,
        FString& OutError) override;
private:
    UPROPERTY(Transient) bool bRecallEnabled = true;
    UPROPERTY(Transient) EDATooltipMode TooltipMode = EDATooltipMode::Simple;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDAAccessibilitySettingsChanged, const FDAAccessibilityRuntimePolicy& /* Policy */);

UCLASS()
class DOMINIONUI_API UDAAccessibilitySettingsSaveGame final : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere) int32 SchemaVersion = 1;
    UPROPERTY(VisibleAnywhere) FDAAccessibilitySettings Settings;
};

UCLASS()
class DOMINIONUI_API UDAAccessibilitySettingsSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    FDAAccessibilitySettingsChanged OnSettingsChanged;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    const FDAAccessibilitySettings& GetSettings() const { return Settings; }
    bool ApplyAndPersist(const FDAAccessibilitySettings& Candidate, FString& OutError);
    bool Reload(FString& OutError);
    const FDAAccessibilityRuntimePolicy& GetRuntimePolicy() const { return RuntimePolicy; }
    bool RegisterConsumer(UObject* Consumer, FString& OutError);
    void UnregisterConsumer(UObject* Consumer);

private:
    void PublishRuntimePolicy();
    static const FString SlotName;
    UPROPERTY(Transient) FDAAccessibilitySettings Settings;
    UPROPERTY(Transient) FDAAccessibilityRuntimePolicy RuntimePolicy;
    UPROPERTY(Transient) TArray<TWeakObjectPtr<UObject>> Consumers;
    UPROPERTY(Transient) TObjectPtr<UDAAccessibilityRuntimeBridgeSubsystem> RuntimeBridge;
};
