#include "Manifest/DAUIGeneratedMetadata.h"

#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputModifiers.h"
#include "Misc/SecureHash.h"
#include "Widgets/DAGrayboxWidgets.h"

namespace
{
    FString Sha1(const FString& Value)
    {
        FTCHARToUTF8 Utf8(*Value); uint8 Hash[FSHA1::DigestSize];
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }
    void AddKeyboardKeys(const FString& Token, TArray<FKey>& Out)
    {
        if (Token == TEXT("1-0")) Out = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
            EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero};
        else if (Token == TEXT("1-4")) Out = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
        else if (Token == TEXT("1-3")) Out = {EKeys::One, EKeys::Two, EKeys::Three};
        else if (Token == TEXT("ArrowLeftRight")) Out = {EKeys::Left, EKeys::Right};
        else Out.Add(FKey(FName(*Token)));
    }

    void AddControllerKeys(const FString& Token, TArray<FKey>& Out)
    {
        static const TMap<FString, FKey> Keys = {
            {TEXT("FaceButtonBottom"), EKeys::Gamepad_FaceButton_Bottom},
            {TEXT("FaceButtonRight"), EKeys::Gamepad_FaceButton_Right},
            {TEXT("FaceButtonLeft"), EKeys::Gamepad_FaceButton_Left},
            {TEXT("FaceButtonTop"), EKeys::Gamepad_FaceButton_Top},
            {TEXT("SpecialRight"), EKeys::Gamepad_Special_Right},
            {TEXT("SpecialLeft"), EKeys::Gamepad_Special_Left},
            {TEXT("DPadDown"), EKeys::Gamepad_DPad_Down}, {TEXT("DPadUp"), EKeys::Gamepad_DPad_Up},
            {TEXT("DPadLeft"), EKeys::Gamepad_DPad_Left}, {TEXT("DPadRight"), EKeys::Gamepad_DPad_Right},
            {TEXT("LeftTrigger"), EKeys::Gamepad_LeftTrigger},
            {TEXT("RightTrigger"), EKeys::Gamepad_RightTrigger},
            {TEXT("LeftThumbstick"), EKeys::Gamepad_LeftThumbstick},
            {TEXT("RightThumbstick"), EKeys::Gamepad_RightThumbstick},
            {TEXT("RightShoulder"), EKeys::Gamepad_RightShoulder},
            {TEXT("LeftShoulder"), EKeys::Gamepad_LeftShoulder}, {TEXT("RightStick"), EKeys::Gamepad_RightX}
        };
        if (Token == TEXT("DPadLeftRight")) Out = {EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right};
        else if (const FKey* Key = Keys.Find(Token)) Out.Add(*Key);
    }
}

bool FDAUIGeneratedCache::ValidateActiveScreenInputGraph(
    const FDAUIManifest& Manifest, FString& OutError)
{
    for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
    {
        TSet<FKey> KeyboardKeys;
        TSet<FKey> ControllerKeys;
        for (const FName ActionId : Screen.CampaignCriticalActions)
        {
            const FDAUICampaignActionDescriptor* Action = Manifest.FindAction(ActionId);
            if (Action == nullptr)
            {
                OutError = TEXT("Active screen input graph references an unknown action ID.");
                return false;
            }
            TArray<FKey> Keyboard;
            TArray<FKey> Controller;
            if (!GetExpectedInputKeys(*Action, Keyboard, Controller, OutError)) return false;
            for (const FKey Key : Keyboard)
            {
                if (KeyboardKeys.Contains(Key))
                {
                    OutError = FString::Printf(TEXT("Active screen '%s' has keyboard collision '%s'."),
                        *Screen.Id.ToString(), *Key.ToString());
                    return false;
                }
                KeyboardKeys.Add(Key);
            }
            for (const FKey Key : Controller)
            {
                if (ControllerKeys.Contains(Key))
                {
                    OutError = FString::Printf(TEXT("Active screen '%s' has controller collision '%s'."),
                        *Screen.Id.ToString(), *Key.ToString());
                    return false;
                }
                ControllerKeys.Add(Key);
            }
        }
    }
    OutError.Reset();
    return true;
}

bool FDAUIGeneratedCache::GetExpectedInputKeys(const FDAUICampaignActionDescriptor& Action,
    TArray<FKey>& OutKeyboard, TArray<FKey>& OutController, FString& OutError)
{
    OutKeyboard.Reset(); OutController.Reset();
    AddKeyboardKeys(Action.KeyboardInput, OutKeyboard);
    AddControllerKeys(Action.ControllerInput, OutController);
    const bool bInvalidKeyboard = OutKeyboard.IsEmpty()
        || OutKeyboard.ContainsByPredicate([](const FKey Key) { return !Key.IsValid() || Key.IsGamepadKey(); });
    const bool bInvalidController = OutController.IsEmpty()
        || OutController.ContainsByPredicate([](const FKey Key) { return !Key.IsValid() || !Key.IsGamepadKey(); });
    if (bInvalidKeyboard || bInvalidController)
    { OutError = TEXT("Frozen action has an invalid keyboard/controller input token."); return false; }
    OutError.Reset(); return true;
}

bool FDAUIGeneratedCache::MapFrozenAction(UInputMappingContext& Context, UInputAction& InputAction,
    const FDAUICampaignActionDescriptor& Action, FString& OutError)
{
    TArray<FKey> Keyboard; TArray<FKey> Controller;
    if (!GetExpectedInputKeys(Action, Keyboard, Controller, OutError)) return false;
    InputAction.ValueType = GetExpectedValueType(Action);
    TSet<FKey> Unique;
    const auto MapKey = [&Context, &InputAction, &Unique](const FKey Key)
    {
        if (Unique.Contains(Key)) return;
        FEnhancedActionKeyMapping& Mapping = Context.MapKey(&InputAction, Key);
        if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
            Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(&Context));
        Unique.Add(Key);
    };
    for (const FKey Key : Keyboard) if (!Unique.Contains(Key))
        MapKey(Key);
    for (const FKey Key : Controller) if (!Unique.Contains(Key))
        MapKey(Key);
    return true;
}

bool FDAUIGeneratedCache::MapFrozenBinding(UInputMappingContext& Context, UInputAction& InputAction,
    const FDAUICampaignActionDescriptor& Action, const FDAUIInputBindingDescriptor& Binding,
    FString& OutError)
{
    const TArray<FDAUIInputBindingDescriptor> Expected = GetBindingDescriptors(Action, OutError);
    if (!OutError.IsEmpty()) return false;
    if (!Expected.ContainsByPredicate([&Binding](const FDAUIInputBindingDescriptor& Row)
        { return Row.BindingId == Binding.BindingId && Row.AssetPath == Binding.AssetPath
            && Row.Key == Binding.Key && Row.PayloadIndex == Binding.PayloadIndex
            && Row.bController == Binding.bController && Row.bNegate == Binding.bNegate; }))
    {
        OutError = TEXT("Per-key input binding does not belong to the frozen action.");
        return false;
    }
    InputAction.ValueType = GetExpectedValueType(Action);
    FEnhancedActionKeyMapping& Mapping = Context.MapKey(&InputAction, Binding.Key);
    if (Binding.bNegate) Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(&Context));
    OutError.Reset();
    return true;
}

EInputActionValueType FDAUIGeneratedCache::GetExpectedValueType(
    const FDAUICampaignActionDescriptor& Action)
{
    return Action.KeyboardInput == TEXT("ArrowLeftRight")
        || Action.ControllerInput == TEXT("DPadLeftRight")
        || Action.ControllerInput == TEXT("RightStick")
        ? EInputActionValueType::Axis1D : EInputActionValueType::Boolean;
}

const TCHAR* FDAUIGeneratedCache::MetadataObjectPath()
{
    return TEXT("/Game/UI/Generated/DAUIManifestMetadata.DAUIManifestMetadata");
}

FString FDAUIGeneratedCache::InputActionObjectPath(const FName ActionId)
{
    const FString Name = TEXT("IA_") + ActionId.ToString().Replace(TEXT("."), TEXT("_")) + TEXT("_K0");
    return TEXT("/Game/UI/Input/Actions/") + Name + TEXT(".") + Name;
}

TArray<FDAUIInputBindingDescriptor> FDAUIGeneratedCache::GetBindingDescriptors(
    const FDAUICampaignActionDescriptor& Action, FString& OutError)
{
    TArray<FKey> Keyboard; TArray<FKey> Controller;
    if (!GetExpectedInputKeys(Action, Keyboard, Controller, OutError)) return {};
    TArray<FDAUIInputBindingDescriptor> Result;
    const FString BaseName = TEXT("IA_") + Action.Id.ToString().Replace(TEXT("."), TEXT("_"));
    const auto Append = [&Result, &Action, &BaseName](const TArray<FKey>& Keys, const bool bController)
    {
        for (int32 Index = 0; Index < Keys.Num(); ++Index)
        {
            FDAUIInputBindingDescriptor& Row = Result.Emplace_GetRef();
            const FString Suffix = FString::Printf(TEXT("_%c%d"), bController ? TEXT('C') : TEXT('K'), Index);
            const FString AssetName = BaseName + Suffix;
            Row.BindingId = FName(*(Action.Id.ToString() + (bController ? TEXT(".controller.") : TEXT(".keyboard."))
                + FString::FromInt(Index)));
            Row.AssetPath = FSoftObjectPath(TEXT("/Game/UI/Input/Actions/") + AssetName + TEXT(".") + AssetName);
            Row.Key = Keys[Index]; Row.PayloadIndex = Index; Row.bController = bController;
            Row.bNegate = Row.Key == EKeys::Left || Row.Key == EKeys::Gamepad_DPad_Left;
        }
    };
    Append(Keyboard, false); Append(Controller, true);
    OutError.Reset();
    return Result;
}

FString FDAUIGeneratedCache::ScreenSemanticHash(const FDAUIScreenDescriptor& Screen)
{
    FString Value = Screen.Id.ToString() + TEXT("|") + Screen.AssetPath.ToString() + TEXT("|")
        + Screen.WidgetClass.ToString() + TEXT("|") + Screen.DisplayName + TEXT("|")
        + FString::FromInt(static_cast<uint8>(Screen.Layer)) + TEXT("|")
        + Screen.InputContextId.ToString() + TEXT("|")
        + FString::FromInt(static_cast<uint8>(Screen.InputMode)) + TEXT("|")
        + Screen.EntryFocusTarget.ToString() + TEXT("|") + Screen.BackTarget.ToString();
    for (const FName Channel : Screen.DataChannels) Value += TEXT("|d:") + Channel.ToString();
    for (const FName Action : Screen.CampaignCriticalActions) Value += TEXT("|a:") + Action.ToString();
    return Sha1(Value);
}

FString FDAUIGeneratedCache::OverlaySemanticHash(const FDAUIOverlayDescriptor& Overlay)
{
    return Sha1(Overlay.Id.ToString() + TEXT("|") + Overlay.AssetPath.ToString() + TEXT("|")
        + Overlay.WidgetClass.ToString() + TEXT("|") + Overlay.DisplayName + TEXT("|")
        + Overlay.MarkerShape.ToString());
}

FString FDAUIGeneratedCache::ActionSemanticHash(const FDAUICampaignActionDescriptor& Action)
{
    FString Value = Action.Id.ToString() + TEXT("|") + Action.CommandId.ToString() + TEXT("|")
        + InputActionObjectPath(Action.Id) + TEXT("|")
        + FString::FromInt(static_cast<uint8>(GetExpectedValueType(Action))) + TEXT("|")
        + Action.KeyboardInput + TEXT("|") + Action.ControllerInput + TEXT("|")
        + (Action.bRequiresHover ? TEXT("1") : TEXT("0"));
    for (const FName Surface : Action.Surfaces) Value += TEXT("|") + Surface.ToString();
    return Sha1(Value);
}

FString FDAUIGeneratedCache::BindingSemanticHash(const FDAUICampaignActionDescriptor& Action,
    const FDAUIInputBindingDescriptor& Binding)
{
    return Sha1(ActionSemanticHash(Action) + TEXT("|")
        + Binding.BindingId.ToString() + TEXT("|") + Binding.AssetPath.ToString() + TEXT("|")
        + Binding.Key.ToString() + TEXT("|") + FString::FromInt(Binding.PayloadIndex) + TEXT("|")
        + (Binding.bController ? TEXT("controller") : TEXT("keyboard")) + TEXT("|")
        + (Binding.bNegate ? TEXT("negate") : TEXT("identity")));
}

bool FDAUIGeneratedCache::ValidateMetadata(const FDAUIManifest& Manifest,
    const UDAUIGeneratedManifestMetadata& Metadata, FString& OutError)
{
    int32 ExpectedBindingCount = 0;
    for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
    {
        FString BindingError;
        ExpectedBindingCount += GetBindingDescriptors(Action, BindingError).Num();
        if (!BindingError.IsEmpty()) { OutError = BindingError; return false; }
    }
    if (Metadata.SourceFingerprint != Manifest.Fingerprint
        || Metadata.ManifestContentHash != Manifest.Fingerprint
        || Metadata.Screens.Num() != Manifest.Screens.Num()
        || Metadata.Overlays.Num() != Manifest.Overlays.Num()
        || Metadata.InputContexts.Num() != Manifest.InputContexts.Num()
        || Metadata.InputActions.Num() != ExpectedBindingCount)
    { OutError = TEXT("Generated UI metadata is stale or has the wrong frozen shape."); return false; }
    for (int32 Index = 0; Index < Manifest.Screens.Num(); ++Index)
    {
        const FDAUIScreenDescriptor& Expected = Manifest.Screens[Index];
        const FDAUIGeneratedAssetRecord& Actual = Metadata.Screens[Index];
        if (Actual.Id != Expected.Id || Actual.AssetPath != Expected.AssetPath || Actual.SourceClass != Expected.WidgetClass
            || Actual.SemanticHash != ScreenSemanticHash(Expected))
        { OutError = TEXT("Generated screen metadata differs from its frozen ID/path/class."); return false; }
    }
    for (int32 Index = 0; Index < Manifest.Overlays.Num(); ++Index)
    {
        const FDAUIOverlayDescriptor& Expected = Manifest.Overlays[Index];
        const FDAUIGeneratedAssetRecord& Actual = Metadata.Overlays[Index];
        if (Actual.Id != Expected.Id || Actual.AssetPath != Expected.AssetPath || Actual.SourceClass != Expected.WidgetClass
            || Actual.SemanticHash != OverlaySemanticHash(Expected))
        { OutError = TEXT("Generated overlay metadata differs from its frozen ID/path/class."); return false; }
    }
    int32 BindingRecordIndex = 0;
    for (const FDAUICampaignActionDescriptor& Expected : Manifest.CampaignCriticalActions)
    {
        FString BindingError;
        const TArray<FDAUIInputBindingDescriptor> Bindings = GetBindingDescriptors(Expected, BindingError);
        if (!BindingError.IsEmpty()) { OutError = BindingError; return false; }
        for (const FDAUIInputBindingDescriptor& Binding : Bindings)
        {
            const FDAUIGeneratedActionRecord& Actual = Metadata.InputActions[BindingRecordIndex++];
            if (Actual.Id != Binding.BindingId || Actual.ActionId != Expected.Id
                || Actual.AssetPath != Binding.AssetPath
                || Actual.ValueType != static_cast<uint8>(GetExpectedValueType(Expected))
                || Actual.Key != Binding.Key || Actual.PayloadIndex != Binding.PayloadIndex
                || Actual.bController != Binding.bController || Actual.bNegate != Binding.bNegate
                || Actual.SemanticHash != BindingSemanticHash(Expected, Binding))
            { OutError = TEXT("Generated input-action metadata differs from exact per-key payload binding."); return false; }
        }
    }
    for (int32 Index = 0; Index < Manifest.InputContexts.Num(); ++Index)
    {
        const FDAUIInputContextDescriptor& Expected = Manifest.InputContexts[Index];
        const FDAUIGeneratedInputRecord& Actual = Metadata.InputContexts[Index];
        TArray<FString> ExpectedMappings;
        for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
            if (Action.Surfaces.ContainsByPredicate([&Manifest, &Expected](const FName SurfaceId)
                { const FDAUIScreenDescriptor* Screen = Manifest.FindScreen(SurfaceId); return Screen != nullptr && Screen->InputContextId == Expected.Id; }))
            {
                FString BindingError;
                for (const FDAUIInputBindingDescriptor& Binding : GetBindingDescriptors(Action, BindingError))
                    ExpectedMappings.Add(Binding.BindingId.ToString() + TEXT("|") + Binding.Key.ToString()
                        + TEXT("|") + FString::FromInt(Binding.PayloadIndex));
                if (!BindingError.IsEmpty()) { OutError = BindingError; return false; }
            }
        if (Actual.ContextId != Expected.Id || Actual.AssetPath != Expected.AssetPath
            || Actual.MappingSignatures != ExpectedMappings)
        { OutError = TEXT("Generated input metadata differs from the frozen action mapping graph."); return false; }
    }
    OutError.Reset(); return true;
}

bool FDAUIGeneratedCache::ValidateInstalledCache(const FDAUIManifest& Manifest, FString& OutError)
{
    const UDAUIGeneratedManifestMetadata* Metadata = LoadObject<UDAUIGeneratedManifestMetadata>(
        nullptr, MetadataObjectPath());
    if (Metadata == nullptr || !ValidateMetadata(Manifest, *Metadata, OutError)) return false;
    for (const FDAUIScreenDescriptor& Screen : Manifest.Screens)
    {
        const UClass* Generated = FSoftClassPath(Screen.AssetPath.ToString() + TEXT("_C"))
            .TryLoadClass<UDAActivatableScreen>();
        const UClass* Source = Screen.WidgetClass.TryLoadClass<UDAActivatableScreen>();
        if (Generated == nullptr || Source == nullptr || Generated->GetSuperClass() != Source)
        { OutError = TEXT("Generated screen cache is missing or has the wrong exact source class."); return false; }
    }
    for (const FDAUIOverlayDescriptor& Overlay : Manifest.Overlays)
    {
        const UClass* Generated = FSoftClassPath(Overlay.AssetPath.ToString() + TEXT("_C"))
            .TryLoadClass<UDAOverlayWidget>();
        const UClass* Source = Overlay.WidgetClass.TryLoadClass<UDAOverlayWidget>();
        if (Generated == nullptr || Source == nullptr || Generated->GetSuperClass() != Source)
        { OutError = TEXT("Generated overlay cache is missing or has the wrong exact source class."); return false; }
    }
    for (const FDAUIInputContextDescriptor& Context : Manifest.InputContexts)
    {
        const UInputMappingContext* Mapping = Cast<UInputMappingContext>(Context.AssetPath.TryLoad());
        if (Mapping == nullptr || Mapping->GetMappings().IsEmpty())
        { OutError = TEXT("Generated input context is missing real Enhanced Input mappings."); return false; }
        int32 ExpectedContextMappingCount = 0;
        for (const FDAUICampaignActionDescriptor& Action : Manifest.CampaignCriticalActions)
        {
            const bool bUsesContext = Action.Surfaces.ContainsByPredicate([&Manifest, &Context](const FName SurfaceId)
                { const FDAUIScreenDescriptor* Screen = Manifest.FindScreen(SurfaceId); return Screen != nullptr && Screen->InputContextId == Context.Id; });
            if (!bUsesContext) continue;
            const TArray<FDAUIInputBindingDescriptor> Bindings = GetBindingDescriptors(Action, OutError);
            if (!OutError.IsEmpty()) return false;
            ExpectedContextMappingCount += Bindings.Num();
            for (const FDAUIInputBindingDescriptor& Binding : Bindings)
            {
                const UInputAction* InputAction = Cast<UInputAction>(Binding.AssetPath.TryLoad());
                if (InputAction == nullptr || InputAction->ValueType != GetExpectedValueType(Action)
                    || !InputAction->Triggers.IsEmpty() || !InputAction->Modifiers.IsEmpty()
                    || !InputAction->bConsumeInput || InputAction->bConsumesActionAndAxisMappings
                    || InputAction->bTriggerWhenPaused || InputAction->bReserveAllMappings
                    || InputAction->AccumulationBehavior
                        != EInputActionAccumulationBehavior::TakeHighestAbsoluteValue)
                { OutError = TEXT("Generated per-key input action has stale type, flags, triggers, or modifiers."); return false; }
                int32 ActualMappingCount = 0;
                for (const FEnhancedActionKeyMapping& Row : Mapping->GetMappings())
                    if (Row.Action == InputAction)
                    {
                        if (Row.Key != Binding.Key || !Row.Triggers.IsEmpty()
                            || Row.Modifiers.Num() != (Binding.bNegate ? 1 : 0)
                            || (Binding.bNegate && (Row.Modifiers[0] == nullptr
                                || !Row.Modifiers[0]->IsA<UInputModifierNegate>())))
                        { OutError = TEXT("Generated mapping has stale key, triggers, or directional modifiers."); return false; }
                        ++ActualMappingCount;
                    }
                if (ActualMappingCount != 1)
                {
                    OutError = TEXT("Generated context must map every per-key input action exactly once.");
                    return false;
                }
            }
        }
        if (Mapping->GetMappings().Num() != ExpectedContextMappingCount)
        { OutError = TEXT("Generated context contains an extra action or key mapping."); return false; }
    }
    OutError.Reset(); return true;
}

bool FDAUIGeneratedCache::CanUseGeneratedScreen(
    const FDAUIManifest& Manifest, const FDAUIScreenDescriptor& Screen)
{
    FString Error;
    if (!ValidateInstalledCache(Manifest, Error)) return false;
    const UDAUIGeneratedManifestMetadata* Metadata = LoadObject<UDAUIGeneratedManifestMetadata>(
        nullptr, MetadataObjectPath());
    return Metadata != nullptr && Metadata->Screens.ContainsByPredicate([&Screen](const FDAUIGeneratedAssetRecord& Row)
        { return Row.Id == Screen.Id && Row.AssetPath == Screen.AssetPath && Row.SourceClass == Screen.WidgetClass; });
}
