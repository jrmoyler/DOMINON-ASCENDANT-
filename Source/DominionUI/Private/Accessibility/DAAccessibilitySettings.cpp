#include "Accessibility/DAAccessibilitySettings.h"

#include "Camera/PlayerCameraManager.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Kismet/GameplayStatics.h"
#include "Manifest/DAUIGeneratedMetadata.h"
#include "Manifest/DAUIManifest.h"
#include "Rendering/SlateRenderer.h"

namespace
{
    bool UsesPersistentToggleState(const FName ActionId)
    {
        return ActionId == TEXT("ui.tactical_pause");
    }
}

const FString UDAAccessibilitySettingsSubsystem::SlotName = TEXT("DominionAccessibility");

bool UDAAccessibilityRuntimeBridgeSubsystem::RegisterRuntime(UObject* Runtime, FString& OutError)
{
    if (Runtime == nullptr || (Cast<IDASubtitleAccessibilityRuntime>(Runtime) == nullptr
        && Cast<IDAFlashAccessibilityRuntime>(Runtime) == nullptr
        && Cast<IDACameraAccessibilityRuntime>(Runtime) == nullptr
        && Cast<IDAGameplayAccessibilityRuntime>(Runtime) == nullptr
        && Cast<IDATutorialAccessibilityRuntime>(Runtime) == nullptr
        && Cast<IDAInputAccessibilityRuntime>(Runtime) == nullptr))
    { OutError = TEXT("Accessibility runtime implements no downstream contract."); return false; }
    Runtimes.AddUnique(TWeakObjectPtr<UObject>(Runtime)); OutError.Reset(); return true;
}

void UDAAccessibilityRuntimeBridgeSubsystem::UnregisterRuntime(UObject* Runtime)
{
    Runtimes.RemoveAll([Runtime](const TWeakObjectPtr<UObject>& Row) { return Row.Get() == Runtime; });
}

bool UDAAccessibilityRuntimeBridgeSubsystem::ApplyToRegisteredRuntimes(
    const FDAAccessibilityRuntimePolicy& Policy, FString& OutError)
{
    return ApplyToRegisteredRuntimes(Policy,
        FDAAccessibilityRuntimePolicy::FromSettings(FDAAccessibilitySettings::MakeDefaults()), OutError);
}

bool UDAAccessibilityRuntimeBridgeSubsystem::ApplyToRegisteredRuntimes(
    const FDAAccessibilityRuntimePolicy& Policy,
    const FDAAccessibilityRuntimePolicy& RollbackPolicy, FString& OutError)
{
    bool bSubtitle = false; bool bFlash = false; bool bCamera = false;
    bool bGameplay = false; bool bTutorial = false; bool bInput = false;
    for (int32 Index = Runtimes.Num() - 1; Index >= 0; --Index)
    {
        UObject* Runtime = Runtimes[Index].Get();
        if (Runtime == nullptr) { Runtimes.RemoveAt(Index); continue; }
        bSubtitle |= Cast<IDASubtitleAccessibilityRuntime>(Runtime) != nullptr;
        bFlash |= Cast<IDAFlashAccessibilityRuntime>(Runtime) != nullptr;
        bCamera |= Cast<IDACameraAccessibilityRuntime>(Runtime) != nullptr;
        bGameplay |= Cast<IDAGameplayAccessibilityRuntime>(Runtime) != nullptr;
        bTutorial |= Cast<IDATutorialAccessibilityRuntime>(Runtime) != nullptr;
        bInput |= Cast<IDAInputAccessibilityRuntime>(Runtime) != nullptr;
    }
    if (!bSubtitle || !bFlash || !bCamera || !bGameplay || !bTutorial || !bInput)
    { OutError = TEXT("ServiceUnavailable: every accessibility downstream runtime must be registered."); return false; }
    const auto ApplyPolicy = [this](const FDAAccessibilityRuntimePolicy& Applied,
        const bool bStopOnFailure, FString& Error)
    {
        bool bSucceeded = true;
        const auto RecordFailure = [&Error, &bSucceeded](const FString& RuntimeError)
        {
            bSucceeded = false;
            if (!RuntimeError.IsEmpty())
            {
                if (!Error.IsEmpty()) Error += TEXT(" ");
                Error += RuntimeError;
            }
        };
        for (const TWeakObjectPtr<UObject>& Row : Runtimes)
        {
            UObject* Runtime = Row.Get(); if (Runtime == nullptr) continue;
            if (IDASubtitleAccessibilityRuntime* Typed = Cast<IDASubtitleAccessibilityRuntime>(Runtime))
            {
                FString RuntimeError;
                if (!Typed->ApplySubtitleAccessibility(Applied.Settings.bSubtitles, Applied.Settings.bSpeakerLabels,
                    Applied.Settings.SubtitleBackgroundOpacity, RuntimeError))
                { RecordFailure(RuntimeError); if (bStopOnFailure) return false; }
            }
            if (IDAFlashAccessibilityRuntime* Typed = Cast<IDAFlashAccessibilityRuntime>(Runtime))
            {
                FString RuntimeError;
                if (!Typed->ApplyFlashAccessibility(Applied.Settings.bReducedFlash, RuntimeError))
                { RecordFailure(RuntimeError); if (bStopOnFailure) return false; }
            }
            if (IDACameraAccessibilityRuntime* Typed = Cast<IDACameraAccessibilityRuntime>(Runtime))
            {
                FString RuntimeError;
                if (!Typed->ApplyCameraAccessibility(Applied.EffectiveFieldOfView, Applied.bAllowMotionBlur,
                    Applied.Effects.CameraShakeScale, Applied.Settings.bReducedMotion, RuntimeError))
                { RecordFailure(RuntimeError); if (bStopOnFailure) return false; }
            }
            if (IDAGameplayAccessibilityRuntime* Typed = Cast<IDAGameplayAccessibilityRuntime>(Runtime))
            {
                FString RuntimeError;
                if (!Typed->ApplyGameplayAccessibility(Applied.Settings.bTacticalPause,
                    Applied.Settings.AimAssist, Applied.Settings.BuildSnapStrength, RuntimeError))
                { RecordFailure(RuntimeError); if (bStopOnFailure) return false; }
            }
            if (IDATutorialAccessibilityRuntime* Typed = Cast<IDATutorialAccessibilityRuntime>(Runtime))
            {
                FString RuntimeError;
                if (!Typed->ApplyTutorialAccessibility(Applied.Settings.bTutorialRecall,
                    Applied.Settings.TooltipMode, RuntimeError))
                { RecordFailure(RuntimeError); if (bStopOnFailure) return false; }
            }
            if (IDAInputAccessibilityRuntime* Typed = Cast<IDAInputAccessibilityRuntime>(Runtime))
            {
                FString RuntimeError;
                if (!Typed->ApplyInputAccessibility(Applied.Settings.KeyboardBindings,
                    Applied.Settings.ControllerBindings, Applied.Settings.HoldToggleModes, RuntimeError))
                { RecordFailure(RuntimeError); if (bStopOnFailure) return false; }
            }
        }
        return bSucceeded;
    };
    FString CandidateError;
    if (!ApplyPolicy(Policy, true, CandidateError))
    {
        FString RollbackError;
        // Rollback deliberately visits every concrete consumer even if one rollback call fails.
        ApplyPolicy(RollbackPolicy, false, RollbackError);
        OutError = CandidateError.IsEmpty() ? TEXT("Accessibility downstream rejected the atomic policy.") : CandidateError;
        if (!RollbackError.IsEmpty()) OutError += TEXT(" Rollback failed: ") + RollbackError;
        return false;
    }
    OutError.Reset(); return true;
}

void UDAUIVisualAccessibilityAdapter::ApplyAccessibilityPolicy(
    const FDAAccessibilityRuntimePolicy& InPolicy)
{
    Super::ApplyAccessibilityPolicy(InPolicy);
    AppliedTextScale = InPolicy.EffectiveTextScale;
    SubtitleBackgroundOpacity = InPolicy.Settings.SubtitleBackgroundOpacity;
    ColorVisionPreset = InPolicy.Settings.ColorVisionPreset;
    bUseHighContrast = ColorVisionPreset == EDAColorVisionPreset::HighContrast;
    bRenderSubtitles = InPolicy.bRenderSubtitles;
    bRenderSpeakerLabels = InPolicy.bRenderSpeakerLabels;
    bUseColorIndependentMarkers = InPolicy.bUseColorIndependentMarkers;
    bAllowNonEssentialMotion = InPolicy.Effects.bAllowNonEssentialMotion;
    bAllowFullIntensityFlash = InPolicy.Effects.bAllowFullIntensityFlash;
    if (!FSlateApplication::IsInitialized()) return;
    FSlateApplication& Slate = FSlateApplication::Get();
    Slate.SetApplicationScale(AppliedTextScale);
    Slate.EnableMenuAnimations(bAllowNonEssentialMotion);
    EColorVisionDeficiency Deficiency = EColorVisionDeficiency::NormalVision;
    if (ColorVisionPreset == EDAColorVisionPreset::Deuteranopia) Deficiency = EColorVisionDeficiency::Deuteranope;
    else if (ColorVisionPreset == EDAColorVisionPreset::Protanopia) Deficiency = EColorVisionDeficiency::Protanope;
    else if (ColorVisionPreset == EDAColorVisionPreset::Tritanopia) Deficiency = EColorVisionDeficiency::Tritanope;
    if (Slate.GetRenderer().IsValid())
        Slate.GetRenderer()->SetColorVisionDeficiencyType(
            Deficiency, bUseHighContrast ? 10 : (Deficiency == EColorVisionDeficiency::NormalVision ? 0 : 10),
            Deficiency != EColorVisionDeficiency::NormalVision || bUseHighContrast, false);
}

bool UDAUIVisualAccessibilityAdapter::ApplySubtitleAccessibility(const bool bSubtitles,
    const bool bSpeakerLabels, const float BackgroundOpacity, FString& OutError)
{
    bRenderSubtitles = bSubtitles; bRenderSpeakerLabels = bSpeakerLabels;
    SubtitleBackgroundOpacity = BackgroundOpacity; OutError.Reset(); return true;
}

bool UDAUIVisualAccessibilityAdapter::ApplyFlashAccessibility(
    const bool bReducedFlash, FString& OutError)
{
    bAllowFullIntensityFlash = !bReducedFlash; OutError.Reset(); return true;
}

void UDACameraAccessibilityAdapter::BindPlayerController(APlayerController* InPlayerController)
{
    PlayerController = InPlayerController;
    ApplyToBoundPlayer();
}

void UDACameraAccessibilityAdapter::ApplyAccessibilityPolicy(
    const FDAAccessibilityRuntimePolicy& InPolicy)
{
    Super::ApplyAccessibilityPolicy(InPolicy);
    AppliedFieldOfView = InPolicy.EffectiveFieldOfView;
    AppliedShakeScale = InPolicy.Effects.CameraShakeScale;
    bAllowMotionBlur = InPolicy.bAllowMotionBlur;
    ApplyToBoundPlayer();
}

bool UDACameraAccessibilityAdapter::ApplyCameraAccessibility(const float FieldOfView,
    const bool bMotionBlur, const float CameraShakeScale, const bool, FString& OutError)
{
    AppliedFieldOfView = FieldOfView; AppliedShakeScale = CameraShakeScale;
    bAllowMotionBlur = bMotionBlur; ApplyToBoundPlayer(); OutError.Reset(); return true;
}

bool UDACameraAccessibilityAdapter::StartAccessibleCameraShake(
    const TSubclassOf<UCameraShakeBase> ShakeClass, const float AuthoredAmplitude)
{
    APlayerController* Controller = PlayerController.Get();
    if (Controller == nullptr || Controller->PlayerCameraManager == nullptr || ShakeClass == nullptr) return false;
    const float Scale = ScaleCameraShakeAmplitude(AuthoredAmplitude);
    if (Scale <= 0.f) return false;
    Controller->PlayerCameraManager->StartCameraShake(ShakeClass, Scale); return true;
}

bool UDAInputAccessibilityAdapter::ApplyInputAccessibility(
    const TMap<FName, FKey>& KeyboardBindings, const TMap<FName, FKey>& ControllerBindings,
    const TMap<FName, EDAHoldToggleMode>& HoldToggleModes, FString& OutError)
{
    AppliedKeyboardBindings = KeyboardBindings; AppliedControllerBindings = ControllerBindings;
    AppliedHoldToggleModes = HoldToggleModes; ToggleStates.Reset();
    OnRemappingsApplied.Broadcast(); OutError.Reset(); return true;
}

bool UDAGameplayAccessibilityAdapter::ApplyGameplayAccessibility(const bool bInTacticalPause,
    const float InAimAssist, const float InBuildSnapStrength, FString& OutError)
{
    bTacticalPause = bInTacticalPause; AimAssist = InAimAssist;
    BuildSnapStrength = InBuildSnapStrength; OutError.Reset(); return true;
}

bool UDATutorialAccessibilityAdapter::ApplyTutorialAccessibility(const bool bTutorialRecall,
    const EDATooltipMode InTooltipMode, FString& OutError)
{
    bRecallEnabled = bTutorialRecall; TooltipMode = InTooltipMode; OutError.Reset(); return true;
}

void UDACameraAccessibilityAdapter::ApplyToBoundPlayer()
{
    APlayerController* Controller = PlayerController.Get();
    if (Controller == nullptr) return;
    if (APlayerCameraManager* Camera = Controller->PlayerCameraManager)
    {
        Camera->SetFOV(AppliedFieldOfView);
        if (AppliedShakeScale <= 0.f) Camera->StopAllCameraShakes(true);
    }
    Controller->ConsoleCommand(
        FString::Printf(TEXT("r.MotionBlurQuality %d"), bAllowMotionBlur ? 4 : 0), true);
}

UInputMappingContext* UDAInputAccessibilityAdapter::BuildPlayerMappingContext(
    const UInputMappingContext& FrozenContext, const FDAUIManifest& Manifest,
    const FDAUIScreenDescriptor& ActiveScreen, FString& OutError)
{
    UInputMappingContext* PlayerContext = DuplicateObject<UInputMappingContext>(&FrozenContext, this);
    if (PlayerContext == nullptr)
    { OutError = TEXT("Could not create the player-specific Enhanced Input remap context."); return nullptr; }
    for (const FName ActionId : ActiveScreen.CampaignCriticalActions)
    {
        const FDAUICampaignActionDescriptor* Action = Manifest.FindAction(ActionId);
        if (Action == nullptr)
        { OutError = TEXT("Active screen remap graph contains an unknown action."); return nullptr; }
        const TArray<FDAUIInputBindingDescriptor> Bindings =
            FDAUIGeneratedCache::GetBindingDescriptors(*Action, OutError);
        if (!OutError.IsEmpty()) return nullptr;
        for (const FDAUIInputBindingDescriptor& Binding : Bindings)
        {
            if (Binding.PayloadIndex != 0) continue;
            const TMap<FName, FKey>& Overrides = Binding.bController
                ? AppliedControllerBindings : AppliedKeyboardBindings;
            const FKey* Override = Overrides.Find(Action->Id);
            if (Override == nullptr) continue;
            if (!Override->IsValid() || Override->IsGamepadKey() != Binding.bController)
            { OutError = TEXT("Accessibility remap key has the wrong input-device type."); return nullptr; }
            bool bReplaced = false;
            for (int32 MappingIndex = 0; MappingIndex < PlayerContext->GetMappings().Num(); ++MappingIndex)
            {
                FEnhancedActionKeyMapping& Mapping = PlayerContext->GetMapping(MappingIndex);
                if (Mapping.Key != Binding.Key) continue;
                Mapping.Key = *Override; Mapping.Modifiers.Reset();
                if (Binding.bNegate)
                    Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(PlayerContext));
                bReplaced = true;
            }
            if (!bReplaced)
            { OutError = TEXT("Accessibility remap did not find its exact frozen input mapping."); return nullptr; }
        }
    }
    for (int32 MappingIndex = 0; MappingIndex < PlayerContext->GetMappings().Num(); ++MappingIndex)
    {
        FEnhancedActionKeyMapping& Mapping = PlayerContext->GetMapping(MappingIndex);
        if (Mapping.Action == nullptr) continue;
        FName StableActionId = NAME_None;
        for (const FName ActionId : ActiveScreen.CampaignCriticalActions)
        {
            const FDAUICampaignActionDescriptor* Action = Manifest.FindAction(ActionId);
            if (Action == nullptr) continue;
            FString BindingError;
            const TArray<FDAUIInputBindingDescriptor> Bindings =
                FDAUIGeneratedCache::GetBindingDescriptors(*Action, BindingError);
            if (!BindingError.IsEmpty()) { OutError = BindingError; return nullptr; }
            if (Bindings.ContainsByPredicate([&Mapping](const FDAUIInputBindingDescriptor& Binding)
                {
                    const FName GeneratedName(*Binding.AssetPath.GetAssetName());
                    const FName RuntimeName(*Binding.BindingId.ToString().Replace(TEXT("."), TEXT("_")));
                    return Mapping.Action->GetFName() == GeneratedName || Mapping.Action->GetFName() == RuntimeName;
                }))
            { StableActionId = ActionId; break; }
        }
        const EDAHoldToggleMode* Mode = AppliedHoldToggleModes.Find(StableActionId);
        if (Mode == nullptr) continue;
        Mapping.Triggers.Reset();
        if (*Mode == EDAHoldToggleMode::Hold)
        {
            UInputTriggerHold* Hold = NewObject<UInputTriggerHold>(PlayerContext);
            Hold->bIsOneShot = true; Mapping.Triggers.Add(Hold);
        }
        else
            Mapping.Triggers.Add(NewObject<UInputTriggerPressed>(PlayerContext));
    }
    OutError.Reset();
    return PlayerContext;
}

bool UDAInputAccessibilityAdapter::ConsumeAccessibleAction(
    const FName ActionId, const bool bTriggered)
{
    const EDAHoldToggleMode Mode = GetAppliedMode(ActionId);
    if (Mode != EDAHoldToggleMode::Toggle || !UsesPersistentToggleState(ActionId))
        return bTriggered;
    if (!bTriggered) return ToggleStates.FindRef(ActionId);
    const bool bNext = !ToggleStates.FindRef(ActionId);
    ToggleStates.Add(ActionId, bNext);
    return bNext;
}

FDAAccessibilitySettings FDAAccessibilitySettings::MakeDefaults()
{
    FDAAccessibilitySettings Result;
    Result.HoldToggleModes = {
        {TEXT("city.place_building"), EDAHoldToggleMode::Press},
        {TEXT("command.issue_order"), EDAHoldToggleMode::Hold},
        {TEXT("founder.activate_ability"), EDAHoldToggleMode::Press},
        {TEXT("ui.open_command"), EDAHoldToggleMode::Hold},
        {TEXT("ui.tactical_pause"), EDAHoldToggleMode::Press}
    };
    return Result;
}

bool FDAAccessibilitySettings::Validate(FString& OutError) const
{
    OutError.Reset();
    static const TSet<FName> HoldToggleActions = {TEXT("city.place_building"), TEXT("command.issue_order"),
        TEXT("founder.activate_ability"), TEXT("ui.open_command"), TEXT("ui.tactical_pause")};
    if (!FMath::IsFinite(TextScale) || TextScale < 0.75f || TextScale > 2.f
        || !FMath::IsFinite(SubtitleBackgroundOpacity) || SubtitleBackgroundOpacity < 0.f || SubtitleBackgroundOpacity > 1.f
        || !FMath::IsFinite(CameraShake) || CameraShake < 0.f || CameraShake > 1.f
        || !FMath::IsFinite(FieldOfView) || FieldOfView < 70.f || FieldOfView > 110.f
        || !FMath::IsFinite(AimAssist) || AimAssist < 0.f || AimAssist > 1.f
        || !FMath::IsFinite(BuildSnapStrength) || BuildSnapStrength < 0.f || BuildSnapStrength > 1.f
        || static_cast<uint8>(ColorVisionPreset) > static_cast<uint8>(EDAColorVisionPreset::HighContrast)
        || static_cast<uint8>(TooltipMode) > static_cast<uint8>(EDATooltipMode::Advanced))
    {
        OutError = TEXT("Accessibility settings contain an out-of-range frozen option.");
        return false;
    }
    if (HoldToggleModes.Num() != HoldToggleActions.Num())
    {
        OutError = TEXT("Every frozen hold/toggle action requires a persisted mode.");
        return false;
    }
    for (const TPair<FName, EDAHoldToggleMode>& Pair : HoldToggleModes)
        if (!HoldToggleActions.Contains(Pair.Key)
            || static_cast<uint8>(Pair.Value) > static_cast<uint8>(EDAHoldToggleMode::Press))
        {
            OutError = TEXT("Hold/toggle settings contain an unknown action or mode.");
            return false;
        }
    FDAUIManifest Manifest;
    TArray<FText> ManifestErrors;
    if (!FDAUIManifest::LoadCanonical(Manifest, ManifestErrors))
    { OutError = TEXT("Canonical action graph is unavailable for remap validation."); return false; }
    const auto ValidateBindings = [&OutError, &Manifest](
        const TMap<FName, FKey>& Bindings, const bool bController)
    {
        for (const TPair<FName, FKey>& Pair : Bindings)
            if (Pair.Key.IsNone() || Manifest.FindAction(Pair.Key) == nullptr || !Pair.Value.IsValid()
                || Pair.Value.IsGamepadKey() != bController)
            { OutError = TEXT("Rebinding maps require stable actions and device-correct valid keys."); return false; }
        for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
        {
            TSet<FKey> UsedKeys;
            for (const FName ActionId : Screen.CampaignCriticalActions)
            {
                const FDAUICampaignActionDescriptor* Action = Manifest.FindAction(ActionId);
                if (Action == nullptr) return false;
                TArray<FKey> Keyboard;
                TArray<FKey> Controller;
                FString KeyError;
                if (!FDAUIGeneratedCache::GetExpectedInputKeys(*Action, Keyboard, Controller, KeyError))
                { OutError = KeyError; return false; }
                TArray<FKey>& Defaults = bController ? Controller : Keyboard;
                if (const FKey* Override = Bindings.Find(ActionId))
                {
                    if (!Defaults.IsEmpty()) Defaults[0] = *Override;
                }
                for (const FKey Key : Defaults)
                {
                    if (UsedKeys.Contains(Key))
                    { OutError = TEXT("Rebinding conflicts with another action on the same active screen."); return false; }
                    UsedKeys.Add(Key);
                }
            }
        }
        return true;
    };
    return ValidateBindings(KeyboardBindings, false) && ValidateBindings(ControllerBindings, true);
}

FDAUIEffectsPolicy FDAUIEffectsPolicy::FromSettings(const FDAAccessibilitySettings& Settings)
{
    FDAUIEffectsPolicy Result;
    Result.bAllowNonEssentialMotion = !Settings.bReducedMotion;
    Result.bAllowFullIntensityFlash = !Settings.bReducedFlash;
    Result.CameraShakeScale = Settings.bReducedMotion ? 0.f : Settings.CameraShake;
    return Result;
}

FDAAccessibilityRuntimePolicy FDAAccessibilityRuntimePolicy::FromSettings(
    const FDAAccessibilitySettings& Settings)
{
    FDAAccessibilityRuntimePolicy Result;
    Result.Settings = Settings; Result.Effects = FDAUIEffectsPolicy::FromSettings(Settings);
    Result.EffectiveTextScale = Settings.TextScale; Result.EffectiveFieldOfView = Settings.FieldOfView;
    Result.bRenderSubtitles = Settings.bSubtitles; Result.bRenderSpeakerLabels = Settings.bSpeakerLabels;
    Result.bUseColorIndependentMarkers = Settings.bColorIndependentMarkers;
    Result.bAllowMotionBlur = Settings.bMotionBlur && !Settings.bReducedMotion;
    Result.bAllowReducedFlashOnly = Settings.bReducedFlash;
    return Result;
}

void UDAAccessibilitySettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDAAccessibilityRuntimeBridgeSubsystem>();
    RuntimeBridge = GetGameInstance() == nullptr ? nullptr
        : GetGameInstance()->GetSubsystem<UDAAccessibilityRuntimeBridgeSubsystem>();
    FString Error;
    if (!Reload(Error))
    {
        Settings = FDAAccessibilitySettings::MakeDefaults();
        RuntimePolicy = FDAAccessibilityRuntimePolicy::FromSettings(Settings);
        PublishRuntimePolicy();
    }
}

bool UDAAccessibilitySettingsSubsystem::ApplyAndPersist(
    const FDAAccessibilitySettings& Candidate, FString& OutError)
{
    if (!Candidate.Validate(OutError)) return false;
    const FDAAccessibilityRuntimePolicy CandidatePolicy =
        FDAAccessibilityRuntimePolicy::FromSettings(Candidate);
    if (RuntimeBridge == nullptr || !RuntimeBridge->ApplyToRegisteredRuntimes(
        CandidatePolicy, RuntimePolicy, OutError))
        return false;
    const auto RollbackRuntime = [this]()
    {
        if (RuntimeBridge != nullptr)
        {
            FString Ignored;
            RuntimeBridge->ApplyToRegisteredRuntimes(RuntimePolicy, RuntimePolicy, Ignored);
        }
    };
    UDAAccessibilitySettingsSaveGame* Save = Cast<UDAAccessibilitySettingsSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UDAAccessibilitySettingsSaveGame::StaticClass()));
    if (Save == nullptr)
    {
        RollbackRuntime();
        OutError = TEXT("Could not allocate accessibility save state.");
        return false;
    }
    Save->Settings = Candidate;
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))
    {
        RollbackRuntime();
        OutError = TEXT("Could not persist accessibility settings.");
        return false;
    }
    Settings = Candidate;
    RuntimePolicy = FDAAccessibilityRuntimePolicy::FromSettings(Settings);
    PublishRuntimePolicy();
    return true;
}

void UDAAccessibilitySettingsSubsystem::PublishRuntimePolicy()
{
    for (int32 Index = Consumers.Num() - 1; Index >= 0; --Index)
        if (UObject* Consumer = Consumers[Index].Get())
        {
            if (IDAAccessibilityConsumer* Typed = Cast<IDAAccessibilityConsumer>(Consumer))
                Typed->ApplyAccessibilityPolicy(RuntimePolicy);
        }
        else Consumers.RemoveAt(Index);
    OnSettingsChanged.Broadcast(RuntimePolicy);
}

bool UDAAccessibilitySettingsSubsystem::Reload(FString& OutError)
{
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        Settings = FDAAccessibilitySettings::MakeDefaults();
        RuntimePolicy = FDAAccessibilityRuntimePolicy::FromSettings(Settings);
        const bool bValid = Settings.Validate(OutError);
        if (bValid) PublishRuntimePolicy();
        return bValid;
    }
    const UDAAccessibilitySettingsSaveGame* Save = Cast<UDAAccessibilitySettingsSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (Save == nullptr || Save->SchemaVersion != 1 || !Save->Settings.Validate(OutError))
    {
        Settings = FDAAccessibilitySettings::MakeDefaults();
        UDAAccessibilitySettingsSaveGame* Recovery = Cast<UDAAccessibilitySettingsSaveGame>(
            UGameplayStatics::CreateSaveGameObject(UDAAccessibilitySettingsSaveGame::StaticClass()));
        if (Recovery == nullptr) { OutError = TEXT("Invalid accessibility slot could not be recovered."); return false; }
        Recovery->Settings = Settings;
        if (!UGameplayStatics::SaveGameToSlot(Recovery, SlotName, 0))
        { OutError = TEXT("Invalid accessibility slot could not be replaced with defaults."); return false; }
        OutError.Reset();
    }
    else Settings = Save->Settings;
    RuntimePolicy = FDAAccessibilityRuntimePolicy::FromSettings(Settings);
    PublishRuntimePolicy();
    return true;
}

bool UDAAccessibilitySettingsSubsystem::RegisterConsumer(UObject* Consumer, FString& OutError)
{
    IDAAccessibilityConsumer* Typed = Consumer == nullptr ? nullptr : Cast<IDAAccessibilityConsumer>(Consumer);
    if (Typed == nullptr) { OutError = TEXT("Accessibility consumer must implement the typed policy interface."); return false; }
    Consumers.AddUnique(TWeakObjectPtr<UObject>(Consumer));
    if (RuntimeBridge != nullptr)
    {
        FString RuntimeError;
        RuntimeBridge->RegisterRuntime(Consumer, RuntimeError);
    }
    Typed->ApplyAccessibilityPolicy(RuntimePolicy); OutError.Reset(); return true;
}

void UDAAccessibilitySettingsSubsystem::UnregisterConsumer(UObject* Consumer)
{
    Consumers.RemoveAll([Consumer](const TWeakObjectPtr<UObject>& Candidate) { return Candidate.Get() == Consumer; });
    if (RuntimeBridge != nullptr) RuntimeBridge->UnregisterRuntime(Consumer);
}
