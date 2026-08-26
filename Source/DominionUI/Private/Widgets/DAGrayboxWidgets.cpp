#include "Widgets/DAGrayboxWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Navigation/DAUIScreenRouter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UDAActivatableScreen::UDAActivatableScreen(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Back is exclusively dispatched by the remappable Enhanced Input action graph.
    bIsBackHandler = false;
    SetIsFocusable(true);
    AppliedAccessibility = FDAAccessibilitySettings::MakeDefaults();
    EffectsPolicy = FDAUIEffectsPolicy::FromSettings(AppliedAccessibility);
    RuntimePolicy = FDAAccessibilityRuntimePolicy::FromSettings(AppliedAccessibility);
}

void UDAActivatableScreen::BindRouter(UDAUIScreenRouter* InRouter, const FName InScreenId)
{
    Router = InRouter;
    StableScreenId = InScreenId;
}

void UDAActivatableScreen::ConfigureScreen(
    const FDAUIScreenDescriptor& Descriptor, UDAUIScreenRouter* InRouter)
{
    ScreenDescriptor = Descriptor;
    BindRouter(InRouter, Descriptor.Id);
    RebuildManifestSlots();
}

void UDAActivatableScreen::SetProjection(const FDAUIScreenProjection& InProjection)
{
    Projection = InProjection;
    RebuildManifestSlots();
}

FString UDAActivatableScreen::BuildPayloadForAction(const FName ActionId,
    const int32 PayloadIndex, const bool bController, const float AxisValue, const bool bHasAxis) const
{
    if (ActionId == TEXT("accessibility.apply"))
        return FDAUIViewModelFactory::BuildAccessibilityPayload(
            StableScreenId, ActionId, Projection.AccessibilityScreen.Draft);
    if (ActionId == TEXT("settings.apply"))
        return FDAUIViewModelFactory::BuildAccessibilityPayload(
            StableScreenId, ActionId, Projection.Settings.PersistedSettings);
    FString Payload;
    if (const FDAUIIndexedActionPayloads* Indexed = Projection.IndexedActionPayloads.Find(ActionId))
    {
        if (bController && !Indexed->ControllerValue.IsEmpty()) Payload = Indexed->ControllerValue;
        else if (Indexed->Values.IsValidIndex(PayloadIndex)) Payload = Indexed->Values[PayloadIndex];
    }
    if (Payload.IsEmpty())
        if (const FString* DefaultPayload = Projection.ActionPayloads.Find(ActionId)) Payload = *DefaultPayload;
    if (Payload.IsEmpty()) Payload = FString::Printf(TEXT("{\"screenId\":\"%s\",\"actionId\":\"%s\"}"),
        *StableScreenId.ToString(), *ActionId.ToString());
    TSharedPtr<FJsonObject> Object;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Payload), Object) || !Object.IsValid())
        return FString();
    Object->SetNumberField(TEXT("bindingIndex"), PayloadIndex);
    Object->SetStringField(TEXT("inputDevice"), bController ? TEXT("controller") : TEXT("keyboard"));
    if (bHasAxis) Object->SetNumberField(TEXT("axis"), AxisValue);
    FString Result;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
    return Result;
}

TSharedRef<SWidget> UDAActivatableScreen::RebuildWidget()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GrayboxRoot"));
    UVerticalBox* Chrome = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LowChromeRail"));
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ScreenTitle"));
    Title->SetText(FText::FromName(StableScreenId));
    Title->SetColorAndOpacity(FSlateColor(CivicAccent));
    Chrome->AddChildToVerticalBox(Title);
    DataList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AuthoritativeDataSlots"));
    ActionList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CampaignActionList"));
    Chrome->AddChildToVerticalBox(DataList);
    Chrome->AddChildToVerticalBox(ActionList);
    UOverlaySlot* ChromeSlot = Root->AddChildToOverlay(Chrome);
    ChromeSlot->SetHorizontalAlignment(HAlign_Left);
    ChromeSlot->SetVerticalAlignment(VAlign_Top);
    WidgetTree->RootWidget = Root;
    RebuildManifestSlots();
    return Root->TakeWidget();
}

void UDAActivatableScreen::RebuildManifestSlots()
{
    ActionButtons.Reset(); FocusWidgets.Reset(); AccessibilityOptionButtons.Reset();
    if (ActionList == nullptr || DataList == nullptr || WidgetTree == nullptr) return;
    ActionList->ClearChildren(); DataList->ClearChildren();
    const int32 RenderedRowCount = FMath::Max(ScreenDescriptor.DataChannels.Num(), Projection.RenderedValues.Num());
    for (int32 Index = 0; Index < RenderedRowCount; ++Index)
    {
        UTextBlock* Slot = WidgetTree->ConstructWidget<UTextBlock>();
        const FString Value = Projection.RenderedValues.IsValidIndex(Index)
            ? Projection.RenderedValues[Index] : TEXT("Awaiting authoritative data");
        Slot->SetText(FText::FromString(Value));
        DataList->AddChildToVerticalBox(Slot);
    }
    for (int32 Index = 0; Index < ScreenDescriptor.CampaignCriticalActions.Num(); ++Index)
    {
        const FName ActionId = ScreenDescriptor.CampaignCriticalActions[Index];
        UDAActionButton* Button = WidgetTree->ConstructWidget<UDAActionButton>();
        const FName FocusId = Index == 0 ? ScreenDescriptor.EntryFocusTarget : ActionId;
        Button->Configure(this, ActionId, FocusId);
        UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>();
        ButtonLabel->SetText(Button->GetLabelText()); ButtonLabel->SetColorAndOpacity(FSlateColor(CivicAccent));
        ActionList->AddChildToVerticalBox(ButtonLabel);
        ActionList->AddChildToVerticalBox(Button);
        ActionButtons.Add(ActionId, Button);
        FocusWidgets.Add(FocusId, Button);
    }
    if (ScreenDescriptor.Id == TEXT("accessibility"))
    {
        FDAUIManifest Manifest;
        TArray<FText> Errors;
        if (FDAUIManifest::LoadCanonical(Manifest, Errors))
            for (const FDAUIAccessibilityOptionDescriptor& Option : Manifest.AccessibilityOptions)
            {
                UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
                Label->SetText(FText::FromName(Option.Id));
                UDAAccessibilityOptionButton* Control = WidgetTree->ConstructWidget<UDAAccessibilityOptionButton>();
                Control->Configure(this, Option.Id);
                DataList->AddChildToVerticalBox(Label); DataList->AddChildToVerticalBox(Control);
                AccessibilityOptionButtons.Add(Option.Id, Control);
                FocusWidgets.Add(Option.Id, Control);
                FocusWidgets.Add(FName(*(TEXT("option.") + Option.Id.ToString())), Control);
            }
    }
}

TArray<FName> UDAActivatableScreen::GetRegisteredActionIds() const
{
    TArray<FName> Result; ActionButtons.GetKeys(Result);
    Result.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
    return Result;
}

int32 UDAActivatableScreen::GetDataSlotCount() const
{
    return DataList == nullptr ? 0
        : FMath::Max(ScreenDescriptor.DataChannels.Num(), Projection.RenderedValues.Num());
}

UDAActionButton* UDAActivatableScreen::FindActionButton(const FName ActionId) const
{
    const TObjectPtr<UDAActionButton>* Found = ActionButtons.Find(ActionId);
    return Found == nullptr ? nullptr : Found->Get();
}

UWidget* UDAActivatableScreen::NativeGetDesiredFocusTarget() const
{
    if (UWidget* Resolved = GetResolvedFocusWidget()) return Resolved;
    return Super::NativeGetDesiredFocusTarget();
}

bool UDAActivatableScreen::NativeOnHandleBackAction()
{
    return false;
}

UWidget* UDAActivatableScreen::GetResolvedFocusWidget() const
{
    const TObjectPtr<UWidget>* Found = FocusWidgets.Find(ScreenDescriptor.EntryFocusTarget);
    return Found == nullptr ? nullptr : Found->Get();
}

void UDAActivatableScreen::ApplyAccessibilitySettings(const FDAAccessibilitySettings& Settings)
{
    FString Error;
    if (!Settings.Validate(Error)) return;
    ApplyAccessibilityPolicy(FDAAccessibilityRuntimePolicy::FromSettings(Settings));
}

void UDAActivatableScreen::ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& Policy)
{
    RuntimePolicy = Policy; AppliedAccessibility = Policy.Settings; EffectsPolicy = Policy.Effects;
    // Text scale is applied once at the Slate application boundary by the visual adapter.
}

bool UDAActivatableScreen::SetAccessibilityBinding(
    const FName ActionId, const FKey Key, const bool bController, FString& OutError)
{
    FDAAccessibilitySettings Candidate = Projection.AccessibilityScreen.Draft;
    (bController ? Candidate.ControllerBindings : Candidate.KeyboardBindings).Add(ActionId, Key);
    if (!Candidate.Validate(OutError)) return false;
    Projection.AccessibilityScreen.Draft = Candidate; Projection.Accessibility = Candidate;
    OutError.Reset(); return true;
}

bool UDAActivatableScreen::SetHoldToggleMode(
    const FName ActionId, const EDAHoldToggleMode Mode, FString& OutError)
{
    FDAAccessibilitySettings Candidate = Projection.AccessibilityScreen.Draft;
    Candidate.HoldToggleModes.Add(ActionId, Mode);
    if (!Candidate.Validate(OutError)) return false;
    Projection.AccessibilityScreen.Draft = Candidate; Projection.Accessibility = Candidate;
    OutError.Reset(); return true;
}

bool UDAActivatableScreen::CycleAccessibilityOption(const FName OptionId)
{
    FDAAccessibilitySettings Candidate = Projection.AccessibilityScreen.Draft;
    if (OptionId == TEXT("text_scale")) Candidate.TextScale = Candidate.TextScale >= 2.f ? 0.75f : Candidate.TextScale + 0.25f;
    else if (OptionId == TEXT("subtitles")) Candidate.bSubtitles = !Candidate.bSubtitles;
    else if (OptionId == TEXT("speaker_labels")) Candidate.bSpeakerLabels = !Candidate.bSpeakerLabels;
    else if (OptionId == TEXT("subtitle_background")) Candidate.SubtitleBackgroundOpacity =
        Candidate.SubtitleBackgroundOpacity >= 1.f ? 0.f : Candidate.SubtitleBackgroundOpacity + 0.25f;
    else if (OptionId == TEXT("color_independent_markers")) Candidate.bColorIndependentMarkers = !Candidate.bColorIndependentMarkers;
    else if (OptionId == TEXT("color_vision_preset")) Candidate.ColorVisionPreset = static_cast<EDAColorVisionPreset>(
        (static_cast<int32>(Candidate.ColorVisionPreset) + 1) % 5);
    else if (OptionId == TEXT("camera_shake")) Candidate.CameraShake = Candidate.CameraShake <= 0.f ? 1.f : Candidate.CameraShake - 0.25f;
    else if (OptionId == TEXT("field_of_view")) Candidate.FieldOfView = Candidate.FieldOfView >= 110.f ? 70.f : Candidate.FieldOfView + 5.f;
    else if (OptionId == TEXT("motion_blur")) Candidate.bMotionBlur = !Candidate.bMotionBlur;
    else if (OptionId == TEXT("reduced_motion")) Candidate.bReducedMotion = !Candidate.bReducedMotion;
    else if (OptionId == TEXT("reduced_flash")) Candidate.bReducedFlash = !Candidate.bReducedFlash;
    else if (OptionId == TEXT("tactical_pause")) Candidate.bTacticalPause = !Candidate.bTacticalPause;
    else if (OptionId == TEXT("aim_assist")) Candidate.AimAssist = Candidate.AimAssist >= 1.f ? 0.f : Candidate.AimAssist + 0.25f;
    else if (OptionId == TEXT("build_snap_strength")) Candidate.BuildSnapStrength =
        Candidate.BuildSnapStrength >= 1.f ? 0.f : Candidate.BuildSnapStrength + 0.25f;
    else if (OptionId == TEXT("tutorial_recall")) Candidate.bTutorialRecall = !Candidate.bTutorialRecall;
    else if (OptionId == TEXT("tooltip_mode")) Candidate.TooltipMode = Candidate.TooltipMode == EDATooltipMode::Simple
        ? EDATooltipMode::Advanced : EDATooltipMode::Simple;
    else if (OptionId == TEXT("keyboard_rebinding"))
    {
        if (Candidate.KeyboardBindings.Contains(TEXT("ui.back")))
            Candidate.KeyboardBindings.Remove(TEXT("ui.back"));
        else Candidate.KeyboardBindings.Add(TEXT("ui.back"), EKeys::BackSpace);
    }
    else if (OptionId == TEXT("controller_remapping"))
    {
        if (Candidate.ControllerBindings.Contains(TEXT("ui.back")))
            Candidate.ControllerBindings.Remove(TEXT("ui.back"));
        else Candidate.ControllerBindings.Add(TEXT("ui.back"), EKeys::Gamepad_LeftY);
    }
    else if (OptionId == TEXT("hold_toggle"))
    {
        const EDAHoldToggleMode Current = Candidate.HoldToggleModes.FindRef(TEXT("city.place_building"));
        Candidate.HoldToggleModes.Add(TEXT("city.place_building"),
            Current == EDAHoldToggleMode::Press ? EDAHoldToggleMode::Hold
                : Current == EDAHoldToggleMode::Hold ? EDAHoldToggleMode::Toggle
                : EDAHoldToggleMode::Press);
    }
    else return false;
    FString Error;
    if (!Candidate.Validate(Error)) return false;
    Projection.AccessibilityScreen.Draft = Candidate; Projection.Accessibility = Candidate;
    return true;
}

bool UDAAccessibilityOptionButton::Trigger()
{
    return OwnerScreen != nullptr && OwnerScreen->CycleAccessibilityOption(OptionId);
}

void UDAAccessibilityOptionButton::NativeOnClicked()
{
    Super::NativeOnClicked(); Trigger();
}

bool UDAActivatableScreen::RequestCampaignAction(const FName ActionId, const FString& PayloadJson)
{
    FString Error;
    return Router != nullptr && Router->DispatchAction(ActionId, PayloadJson, Error);
}

void UDAActionButton::Configure(
    UDAActivatableScreen* InOwner, const FName InActionId, const FName InFocusId)
{
    OwnerScreen = InOwner; ActionId = InActionId; FocusId = InFocusId;
    FString Label = ActionId.ToString().Replace(TEXT("."), TEXT(" "));
    LabelText = FText::FromString(Label);
    SetIsFocusable(true);
}

FString UDAActionButton::GetPayloadJson() const
{
    return OwnerScreen == nullptr ? FString() : OwnerScreen->BuildPayloadForAction(ActionId);
}

bool UDAActionButton::Trigger()
{
    return OwnerScreen != nullptr && OwnerScreen->RequestCampaignAction(ActionId, GetPayloadJson());
}

void UDAActionButton::NativeOnClicked()
{
    Super::NativeOnClicked();
    Trigger();
}

void UDAOverlayWidget::ConfigureOverlay(const FName OverlayId,
    const FDAUIOverlayReadModel& InReadModel, UDAUIScreenRouter* InRouter)
{
    OverlayReadModel = InReadModel;
    FDAUIScreenDescriptor Descriptor; Descriptor.Id = OverlayId; Descriptor.EntryFocusTarget = TEXT("overlay.marker");
    Descriptor.BackTarget = TEXT("__exit__"); Descriptor.DataChannels = {TEXT("overlay.authoritative_values")};
    ConfigureScreen(Descriptor, InRouter);
    FDAUIScreenProjection OverlayProjection; OverlayProjection.ScreenId = OverlayId;
    OverlayProjection.bAuthoritative = InReadModel.bAuthoritative;
    OverlayProjection.DataChannels = Descriptor.DataChannels;
    OverlayProjection.RenderedValues = InReadModel.RenderedValues;
    SetProjection(OverlayProjection);
}
