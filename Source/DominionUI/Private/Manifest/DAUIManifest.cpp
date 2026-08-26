#include "Manifest/DAUIManifest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "DominionUIManifest"

namespace
{
    constexpr TCHAR FrozenFingerprint[] = TEXT("bb3699cbe41260b76a64e24ff6204236768f6a5c");

    class FStrictJsonScanner
    {
    public:
        explicit FStrictJsonScanner(const FString& InText) : Text(InText) {}
        bool Parse(FString& OutError)
        {
            SkipWhitespace();
            if (!ParseValue(OutError)) return false;
            SkipWhitespace();
            if (Index != Text.Len()) { OutError = TEXT("JSON contains trailing content."); return false; }
            return true;
        }
    private:
        static int32 HexDigit(const TCHAR C)
        {
            if (C >= TEXT('0') && C <= TEXT('9')) return C - TEXT('0');
            if (C >= TEXT('a') && C <= TEXT('f')) return C - TEXT('a') + 10;
            if (C >= TEXT('A') && C <= TEXT('F')) return C - TEXT('A') + 10;
            return INDEX_NONE;
        }
        void SkipWhitespace()
        { while (Index < Text.Len() && FChar::IsWhitespace(Text[Index])) ++Index; }
        bool ParseString(FString& Out, FString& Error)
        {
            if (Index >= Text.Len() || Text[Index++] != TEXT('"')) return false;
            while (Index < Text.Len())
            {
                const TCHAR C = Text[Index++];
                if (C == TEXT('"')) return true;
                if (C == TEXT('\\'))
                {
                    if (Index >= Text.Len()) return false;
                    const TCHAR Escaped = Text[Index++];
                    if (Escaped == TEXT('u'))
                    {
                        if (Index + 4 > Text.Len()) return false;
                        uint32 Codepoint = 0;
                        for (int32 DigitIndex = 0; DigitIndex < 4; ++DigitIndex)
                        {
                            const int32 Digit = HexDigit(Text[Index++]);
                            if (Digit == INDEX_NONE) return false;
                            Codepoint = Codepoint * 16 + static_cast<uint32>(Digit);
                        }
                        Out.AppendChar(static_cast<TCHAR>(Codepoint));
                    }
                    else if (Escaped == TEXT('b')) Out.AppendChar(TEXT('\b'));
                    else if (Escaped == TEXT('f')) Out.AppendChar(TEXT('\f'));
                    else if (Escaped == TEXT('n')) Out.AppendChar(TEXT('\n'));
                    else if (Escaped == TEXT('r')) Out.AppendChar(TEXT('\r'));
                    else if (Escaped == TEXT('t')) Out.AppendChar(TEXT('\t'));
                    else if (Escaped == TEXT('"') || Escaped == TEXT('\\') || Escaped == TEXT('/'))
                        Out.AppendChar(Escaped);
                    else return false;
                }
                else if (C < 0x20) { Error = TEXT("JSON string contains a control character."); return false; }
                else Out.AppendChar(C);
            }
            return false;
        }
        bool ParseValue(FString& Error)
        {
            SkipWhitespace();
            if (Index >= Text.Len()) return false;
            if (Text[Index] == TEXT('{')) return ParseObject(Error);
            if (Text[Index] == TEXT('[')) return ParseArray(Error);
            if (Text[Index] == TEXT('"')) { FString Ignored; return ParseString(Ignored, Error); }
            const int32 Start = Index;
            while (Index < Text.Len() && !FChar::IsWhitespace(Text[Index])
                && Text[Index] != TEXT(',') && Text[Index] != TEXT(']') && Text[Index] != TEXT('}')) ++Index;
            return Index > Start;
        }
        bool ParseObject(FString& Error)
        {
            ++Index; SkipWhitespace(); TSet<FString> Keys;
            if (Index < Text.Len() && Text[Index] == TEXT('}')) { ++Index; return true; }
            while (Index < Text.Len())
            {
                FString Key;
                if (!ParseString(Key, Error)) return false;
                if (Keys.Contains(Key))
                { Error = TEXT("JSON object contains duplicate key '") + Key + TEXT("'."); return false; }
                Keys.Add(Key); SkipWhitespace();
                if (Index >= Text.Len() || Text[Index++] != TEXT(':') || !ParseValue(Error)) return false;
                SkipWhitespace();
                if (Index < Text.Len() && Text[Index] == TEXT('}')) { ++Index; return true; }
                if (Index >= Text.Len() || Text[Index++] != TEXT(',')) return false;
                SkipWhitespace();
            }
            return false;
        }
        bool ParseArray(FString& Error)
        {
            ++Index; SkipWhitespace();
            if (Index < Text.Len() && Text[Index] == TEXT(']')) { ++Index; return true; }
            while (Index < Text.Len())
            {
                if (!ParseValue(Error)) return false;
                SkipWhitespace();
                if (Index < Text.Len() && Text[Index] == TEXT(']')) { ++Index; return true; }
                if (Index >= Text.Len() || Text[Index++] != TEXT(',')) return false;
                SkipWhitespace();
            }
            return false;
        }
        const FString& Text;
        int32 Index = 0;
    };

    FString EscapeCanonical(const FString& Value)
    {
        FString Out(TEXT("\""));
        for (const TCHAR C : Value)
        {
            switch (C)
            {
            case TEXT('"'): Out += TEXT("\\\""); break;
            case TEXT('\\'): Out += TEXT("\\\\"); break;
            case TEXT('\b'): Out += TEXT("\\b"); break;
            case TEXT('\f'): Out += TEXT("\\f"); break;
            case TEXT('\n'): Out += TEXT("\\n"); break;
            case TEXT('\r'): Out += TEXT("\\r"); break;
            case TEXT('\t'): Out += TEXT("\\t"); break;
            default: if (C < 0x20) Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(C)); else Out.AppendChar(C);
            }
        }
        return Out + TEXT("\"");
    }

    void CanonicalValue(const TSharedPtr<FJsonValue>& Value, FString& Out, const bool bRoot)
    {
        switch (Value->Type)
        {
        case EJson::Null: Out += TEXT("null"); break;
        case EJson::String: Out += EscapeCanonical(Value->AsString()); break;
        case EJson::Boolean: Out += Value->AsBool() ? TEXT("true") : TEXT("false"); break;
        case EJson::Number:
        {
            const double Number = Value->AsNumber();
            const bool bIntegral = FMath::IsFinite(Number)
                && Number >= -9223372036854775808.0 && Number < 9223372036854775808.0
                && FMath::FloorToDouble(Number) == Number;
            Out += bIntegral ? FString::Printf(TEXT("%lld"), static_cast<long long>(Number))
                : FString::Printf(TEXT("%.17g"), Number); break;
        }
        case EJson::Array:
        {
            Out += TEXT("["); const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
            for (int32 Index = 0; Index < Values.Num(); ++Index)
            { if (Index) Out += TEXT(","); CanonicalValue(Values[Index], Out, false); }
            Out += TEXT("]"); break;
        }
        case EJson::Object:
        {
            Out += TEXT("{"); const TSharedPtr<FJsonObject> Object = Value->AsObject();
            TArray<FString> Keys; Object->Values.GetKeys(Keys); Keys.Sort(); bool bFirst = true;
            for (const FString& Key : Keys)
            {
                if (bRoot && Key == TEXT("fingerprint")) continue;
                if (!bFirst) Out += TEXT(","); bFirst = false;
                Out += EscapeCanonical(Key) + TEXT(":"); CanonicalValue(Object->Values[Key], Out, false);
            }
            Out += TEXT("}"); break;
        }
        default: break;
        }
    }

    FString ComputeCanonicalFingerprint(const TSharedPtr<FJsonObject>& Root)
    {
        FString Canonical; CanonicalValue(MakeShared<FJsonValueObject>(Root), Canonical, true);
        FTCHARToUTF8 Utf8(*Canonical); uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }
    bool OnlyKeys(const TSharedPtr<FJsonObject>& Object, const TSet<FString>& Allowed,
        const FString& Context, TArray<FText>& Errors)
    {
        if (!Object.IsValid())
        {
            Errors.Add(FText::Format(LOCTEXT("MissingObject", "{0} must be an object."), FText::FromString(Context)));
            return false;
        }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
        {
            if (!Allowed.Contains(Pair.Key))
            {
                Errors.Add(FText::Format(LOCTEXT("UnknownField", "{0} has unknown field '{1}'."),
                    FText::FromString(Context), FText::FromString(Pair.Key)));
                return false;
            }
        }
        return true;
    }

    bool ReadName(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FName& Out)
    {
        FString Value;
        if (!Object->TryGetStringField(Key, Value) || Value.IsEmpty()) return false;
        Out = FName(*Value);
        return !Out.IsNone();
    }

    bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32& Out)
    {
        double Value = 0.0;
        if (!Object->TryGetNumberField(Key, Value) || !FMath::IsFinite(Value)
            || Value < static_cast<double>(MIN_int32) || Value > static_cast<double>(MAX_int32)
            || FMath::FloorToDouble(Value) != Value) return false;
        Out = static_cast<int32>(Value);
        return true;
    }

    bool ReadNames(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, TArray<FName>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Key, Values) || Values == nullptr) return false;
        Out.Reset();
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (!Value.IsValid() || !Value->TryGetString(Text) || Text.IsEmpty()) return false;
            Out.Add(FName(*Text));
        }
        return true;
    }

    bool NamesAreUnique(const TArray<FName>& Values)
    {
        TSet<FName> Seen;
        for (const FName Value : Values)
        {
            if (Value.IsNone() || Seen.Contains(Value)) return false;
            Seen.Add(Value);
        }
        return true;
    }

    FString CanonicalObjectPath(const FString& PackagePath)
    {
        return PackagePath.Contains(TEXT(".")) ? PackagePath
            : PackagePath + TEXT(".") + FPaths::GetBaseFilename(PackagePath);
    }

    bool ParseScreen(const TSharedPtr<FJsonObject>& Object, FDAUIScreenDescriptor& Out, TArray<FText>& Errors)
    {
        static const TSet<FString> Keys = {TEXT("id"), TEXT("displayName"), TEXT("assetPath"),
            TEXT("widgetClass"), TEXT("layer"), TEXT("inputContextId"), TEXT("inputMode"),
            TEXT("entryFocusTarget"), TEXT("backTarget"), TEXT("dataChannels"), TEXT("campaignCriticalActions")};
        FString AssetPath; FString WidgetClass; FString Layer; FString InputMode;
        if (!OnlyKeys(Object, Keys, TEXT("screen"), Errors)
            || !ReadName(Object, TEXT("id"), Out.Id)
            || !Object->TryGetStringField(TEXT("displayName"), Out.DisplayName)
            || !Object->TryGetStringField(TEXT("assetPath"), AssetPath)
            || !Object->TryGetStringField(TEXT("widgetClass"), WidgetClass)
            || !Object->TryGetStringField(TEXT("layer"), Layer)
            || !ReadName(Object, TEXT("inputContextId"), Out.InputContextId)
            || !Object->TryGetStringField(TEXT("inputMode"), InputMode)
            || !ReadName(Object, TEXT("entryFocusTarget"), Out.EntryFocusTarget)
            || !ReadName(Object, TEXT("backTarget"), Out.BackTarget)
            || !ReadNames(Object, TEXT("dataChannels"), Out.DataChannels)
            || !ReadNames(Object, TEXT("campaignCriticalActions"), Out.CampaignCriticalActions)) return false;
        if (Layer == TEXT("menu")) Out.Layer = EDAUIScreenLayer::Menu;
        else if (Layer == TEXT("hud")) Out.Layer = EDAUIScreenLayer::HUD;
        else if (Layer == TEXT("panel")) Out.Layer = EDAUIScreenLayer::Panel;
        else if (Layer == TEXT("modal")) Out.Layer = EDAUIScreenLayer::Modal;
        else return false;
        if (InputMode == TEXT("game_only")) Out.InputMode = EDAUIScreenInputMode::GameOnly;
        else if (InputMode == TEXT("ui_only")) Out.InputMode = EDAUIScreenInputMode::UIOnly;
        else if (InputMode == TEXT("game_and_ui")) Out.InputMode = EDAUIScreenInputMode::GameAndUI;
        else return false;
        Out.AssetPath = FSoftObjectPath(CanonicalObjectPath(AssetPath));
        Out.WidgetClass = FSoftClassPath(WidgetClass);
        return true;
    }

    const TArray<FName>& FrozenScreens()
    {
        static const TArray<FName> Values = {TEXT("main_menu"), TEXT("new_campaign"), TEXT("settings"),
            TEXT("accessibility"), TEXT("pause"), TEXT("city_hud"), TEXT("founder_hud"), TEXT("command_hud"),
            TEXT("card_collection"), TEXT("deck_builder"), TEXT("card_inspect"), TEXT("blueprint_crafting"),
            TEXT("building_inspect"), TEXT("citizen_inspect"), TEXT("faction_panel"), TEXT("diplomacy"),
            TEXT("treaty_builder"), TEXT("world_map"), TEXT("quest_journal"), TEXT("history_timeline"),
            TEXT("research"), TEXT("city_metrics"), TEXT("conquest_dashboard"), TEXT("leader_resolution"),
            TEXT("ascension_reward"), TEXT("save_load"), TEXT("returning_player_recap")};
        return Values;
    }

    const TArray<FName>& FrozenOverlays()
    {
        static const TArray<FName> Values = {TEXT("power"), TEXT("water"), TEXT("data"), TEXT("employment"),
            TEXT("housing"), TEXT("happiness"), TEXT("dependency"), TEXT("adjacency")};
        return Values;
    }

    const TArray<FName>& FrozenAccessibility()
    {
        static const TArray<FName> Values = {TEXT("keyboard_rebinding"), TEXT("controller_remapping"),
            TEXT("text_scale"), TEXT("subtitles"), TEXT("speaker_labels"), TEXT("subtitle_background"),
            TEXT("color_independent_markers"), TEXT("color_vision_preset"), TEXT("camera_shake"),
            TEXT("field_of_view"), TEXT("motion_blur"), TEXT("reduced_motion"), TEXT("reduced_flash"),
            TEXT("tactical_pause"), TEXT("hold_toggle"), TEXT("aim_assist"), TEXT("build_snap_strength"),
            TEXT("tutorial_recall"), TEXT("tooltip_mode")};
        return Values;
    }

    const TArray<FName>& FrozenAccessibilityTypes()
    {
        static const TArray<FName> Values = {TEXT("binding_map"), TEXT("binding_map"), TEXT("float"),
            TEXT("boolean"), TEXT("boolean"), TEXT("float"), TEXT("boolean"), TEXT("enum"),
            TEXT("float"), TEXT("float"), TEXT("boolean"), TEXT("boolean"), TEXT("boolean"),
            TEXT("boolean"), TEXT("hold_toggle_map"), TEXT("float"), TEXT("float"), TEXT("boolean"), TEXT("enum")};
        return Values;
    }

    void AppendSemantic(FString& Out, const FString& Value)
    { Out += FString::Printf(TEXT("%d:"), Value.Len()) + Value + TEXT("|"); }
    void AppendSemantic(FString& Out, const FName Value) { AppendSemantic(Out, Value.ToString()); }
    void AppendSemantic(FString& Out, const int64 Value)
    { Out += FString::Printf(TEXT("%lld|"), static_cast<long long>(Value)); }
    void AppendNamesSemantic(FString& Out, const TArray<FName>& Values)
    { AppendSemantic(Out, Values.Num()); for (const FName Value : Values) AppendSemantic(Out, Value); }

    FString ComputeSemanticFingerprint(const FDAUIManifest& Manifest)
    {
        FString Out; AppendSemantic(Out, Manifest.SchemaVersion); AppendSemantic(Out, Manifest.VisualDirection.Tone);
        AppendSemantic(Out, Manifest.VisualDirection.bLowChromeHUD ? 1 : 0);
        AppendSemantic(Out, Manifest.VisualDirection.bKeepCenterAndLowerMiddleClear ? 1 : 0);
        AppendSemantic(Out, Manifest.VisualDirection.MotionSettingId); AppendSemantic(Out, Manifest.VisualDirection.FlashSettingId);
        AppendSemantic(Out, Manifest.InputContexts.Num());
        for (const FDAUIInputContextDescriptor& Row : Manifest.InputContexts)
        { AppendSemantic(Out, Row.Id); AppendSemantic(Out, Row.AssetPath.ToString()); AppendSemantic(Out, Row.Priority); }
        AppendSemantic(Out, Manifest.Screens.Num());
        for (const FDAUIScreenDescriptor& Row : Manifest.Screens)
        {
            AppendSemantic(Out, Row.Id); AppendSemantic(Out, Row.DisplayName); AppendSemantic(Out, Row.AssetPath.ToString());
            AppendSemantic(Out, Row.WidgetClass.ToString()); AppendSemantic(Out, static_cast<int64>(Row.Layer));
            AppendSemantic(Out, Row.InputContextId); AppendSemantic(Out, static_cast<int64>(Row.InputMode));
            AppendSemantic(Out, Row.EntryFocusTarget); AppendSemantic(Out, Row.BackTarget);
            AppendNamesSemantic(Out, Row.DataChannels); AppendNamesSemantic(Out, Row.CampaignCriticalActions);
        }
        AppendSemantic(Out, Manifest.Overlays.Num());
        for (const FDAUIOverlayDescriptor& Row : Manifest.Overlays)
        { AppendSemantic(Out, Row.Id); AppendSemantic(Out, Row.DisplayName); AppendSemantic(Out, Row.AssetPath.ToString()); AppendSemantic(Out, Row.WidgetClass.ToString()); AppendSemantic(Out, Row.MarkerShape); }
        AppendSemantic(Out, Manifest.CampaignCriticalActions.Num());
        for (const FDAUICampaignActionDescriptor& Row : Manifest.CampaignCriticalActions)
        { AppendSemantic(Out, Row.Id); AppendSemantic(Out, Row.CommandId); AppendNamesSemantic(Out, Row.Surfaces); AppendSemantic(Out, Row.KeyboardInput); AppendSemantic(Out, Row.ControllerInput); AppendSemantic(Out, Row.bRequiresHover ? 1 : 0); }
        AppendSemantic(Out, Manifest.AccessibilityOptions.Num());
        for (const FDAUIAccessibilityOptionDescriptor& Row : Manifest.AccessibilityOptions)
        { AppendSemantic(Out, Row.Id); AppendSemantic(Out, Row.Type); AppendSemantic(Out, Row.DefaultValueJson);
          AppendSemantic(Out, Row.MinimumJson); AppendSemantic(Out, Row.MaximumJson); AppendNamesSemantic(Out, Row.Values); }
        FTCHARToUTF8 Utf8(*Out); uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }
}

FString FDAUIManifest::GetCanonicalPath()
{
    return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI/Manifests/VerticalSliceUI.json"));
}

bool FDAUIManifest::LoadCanonical(FDAUIManifest& OutManifest, TArray<FText>& OutErrors)
{
    return LoadFile(GetCanonicalPath(), OutManifest, OutErrors);
}

bool FDAUIManifest::LoadFile(const FString& Path, FDAUIManifest& OutManifest, TArray<FText>& OutErrors)
{
    OutErrors.Reset();
    struct FLoadErrorGuard
    {
        TArray<FText>& Errors;
        bool bSucceeded = false;
        ~FLoadErrorGuard()
        {
            if (!bSucceeded && Errors.IsEmpty())
                Errors.Add(FText::FromString(TEXT("Canonical UI manifest does not match its strict frozen schema.")));
        }
    } LoadErrorGuard{OutErrors};
    FString Json;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Json, *Path))
    {
        OutErrors.Add(LOCTEXT("Unreadable", "Canonical UI manifest is missing."));
        return false;
    }
    FString StrictError;
    if (!FStrictJsonScanner(Json).Parse(StrictError))
    {
        OutErrors.Add(FText::FromString(StrictError.IsEmpty() ? TEXT("Canonical UI manifest is invalid JSON.") : StrictError));
        return false;
    }
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
    {
        OutErrors.Add(LOCTEXT("InvalidJson", "Canonical UI manifest is invalid JSON."));
        return false;
    }
    static const TSet<FString> RootKeys = {TEXT("fingerprint"), TEXT("schemaVersion"), TEXT("visualDirection"),
        TEXT("inputContexts"), TEXT("screens"), TEXT("overlays"), TEXT("campaignCriticalActions"),
        TEXT("accessibilityOptions")};
    if (!OnlyKeys(Root, RootKeys, TEXT("manifest"), OutErrors)) return false;
    OutManifest = {};
    if (!Root->TryGetStringField(TEXT("fingerprint"), OutManifest.Fingerprint)
        || OutManifest.Fingerprint != ComputeCanonicalFingerprint(Root)
        || OutManifest.Fingerprint != FrozenFingerprint
        || !ReadInt(Root, TEXT("schemaVersion"), OutManifest.SchemaVersion)) return false;

    const TSharedPtr<FJsonObject>* Visual = nullptr;
    static const TSet<FString> VisualKeys = {TEXT("tone"), TEXT("lowChromeHUD"),
        TEXT("keepCenterAndLowerMiddleClear"), TEXT("motionSettingId"), TEXT("flashSettingId")};
    if (!Root->TryGetObjectField(TEXT("visualDirection"), Visual) || Visual == nullptr
        || !OnlyKeys(*Visual, VisualKeys, TEXT("visualDirection"), OutErrors)
        || !ReadName(*Visual, TEXT("tone"), OutManifest.VisualDirection.Tone)
        || !(*Visual)->TryGetBoolField(TEXT("lowChromeHUD"), OutManifest.VisualDirection.bLowChromeHUD)
        || !(*Visual)->TryGetBoolField(TEXT("keepCenterAndLowerMiddleClear"), OutManifest.VisualDirection.bKeepCenterAndLowerMiddleClear)
        || !ReadName(*Visual, TEXT("motionSettingId"), OutManifest.VisualDirection.MotionSettingId)
        || !ReadName(*Visual, TEXT("flashSettingId"), OutManifest.VisualDirection.FlashSettingId)) return false;

    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!Root->TryGetArrayField(TEXT("inputContexts"), Rows) || Rows == nullptr) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Rows)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        static const TSet<FString> Keys = {TEXT("id"), TEXT("assetPath"), TEXT("priority")};
        FDAUIInputContextDescriptor Row; FString Asset;
        if (!OnlyKeys(Object, Keys, TEXT("inputContext"), OutErrors) || !ReadName(Object, TEXT("id"), Row.Id)
            || !Object->TryGetStringField(TEXT("assetPath"), Asset)
            || !ReadInt(Object, TEXT("priority"), Row.Priority)) return false;
        Row.AssetPath = FSoftObjectPath(CanonicalObjectPath(Asset)); OutManifest.InputContexts.Add(Row);
    }
    if (!Root->TryGetArrayField(TEXT("screens"), Rows) || Rows == nullptr) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Rows)
    {
        FDAUIScreenDescriptor Row;
        if (!ParseScreen(Value->AsObject(), Row, OutErrors)) return false;
        OutManifest.Screens.Add(MoveTemp(Row));
    }
    if (!Root->TryGetArrayField(TEXT("overlays"), Rows) || Rows == nullptr) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Rows)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        static const TSet<FString> Keys = {TEXT("id"), TEXT("displayName"), TEXT("assetPath"), TEXT("widgetClass"), TEXT("markerShape")};
        FDAUIOverlayDescriptor Row; FString Asset; FString Widget;
        if (!OnlyKeys(Object, Keys, TEXT("overlay"), OutErrors) || !ReadName(Object, TEXT("id"), Row.Id)
            || !Object->TryGetStringField(TEXT("displayName"), Row.DisplayName)
            || !Object->TryGetStringField(TEXT("assetPath"), Asset)
            || !Object->TryGetStringField(TEXT("widgetClass"), Widget)
            || !ReadName(Object, TEXT("markerShape"), Row.MarkerShape)) return false;
        Row.AssetPath = FSoftObjectPath(CanonicalObjectPath(Asset)); Row.WidgetClass = FSoftClassPath(Widget);
        OutManifest.Overlays.Add(MoveTemp(Row));
    }
    if (!Root->TryGetArrayField(TEXT("campaignCriticalActions"), Rows) || Rows == nullptr) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Rows)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        static const TSet<FString> Keys = {TEXT("id"), TEXT("commandId"), TEXT("surfaces"),
            TEXT("keyboardInput"), TEXT("controllerInput"), TEXT("requiresHover")};
        FDAUICampaignActionDescriptor Row;
        if (!OnlyKeys(Object, Keys, TEXT("campaignAction"), OutErrors) || !ReadName(Object, TEXT("id"), Row.Id)
            || !ReadName(Object, TEXT("commandId"), Row.CommandId) || !ReadNames(Object, TEXT("surfaces"), Row.Surfaces)
            || !Object->TryGetStringField(TEXT("keyboardInput"), Row.KeyboardInput)
            || !Object->TryGetStringField(TEXT("controllerInput"), Row.ControllerInput)
            || !Object->TryGetBoolField(TEXT("requiresHover"), Row.bRequiresHover)) return false;
        OutManifest.CampaignCriticalActions.Add(MoveTemp(Row));
    }
    if (!Root->TryGetArrayField(TEXT("accessibilityOptions"), Rows) || Rows == nullptr) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Rows)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        static const TSet<FString> Keys = {TEXT("id"), TEXT("type"), TEXT("defaultValue"),
            TEXT("minimum"), TEXT("maximum"), TEXT("values")};
        FDAUIAccessibilityOptionDescriptor Row;
        if (!OnlyKeys(Object, Keys, TEXT("accessibilityOption"), OutErrors)
            || !ReadName(Object, TEXT("id"), Row.Id) || !ReadName(Object, TEXT("type"), Row.Type)
            || !Object->HasField(TEXT("defaultValue"))) return false;
        const TSharedPtr<FJsonValue> DefaultValue = Object->TryGetField(TEXT("defaultValue"));
        CanonicalValue(DefaultValue, Row.DefaultValueJson, false);
        if (Row.Type == TEXT("boolean"))
        {
            bool Parsed = false;
            if (Object->Values.Num() != 3 || !DefaultValue->TryGetBool(Parsed)) return false;
        }
        else if (Row.Type == TEXT("float"))
        {
            double Default = 0.0; double Minimum = 0.0; double Maximum = 0.0;
            const TSharedPtr<FJsonValue> MinimumValue = Object->TryGetField(TEXT("minimum"));
            const TSharedPtr<FJsonValue> MaximumValue = Object->TryGetField(TEXT("maximum"));
            if (Object->Values.Num() != 5 || !DefaultValue->TryGetNumber(Default)
                || !MinimumValue.IsValid() || !MinimumValue->TryGetNumber(Minimum)
                || !MaximumValue.IsValid() || !MaximumValue->TryGetNumber(Maximum)
                || !FMath::IsFinite(Default) || !FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum)
                || Minimum > Default || Default > Maximum) return false;
            CanonicalValue(MinimumValue, Row.MinimumJson, false);
            CanonicalValue(MaximumValue, Row.MaximumJson, false);
        }
        else if (Row.Type == TEXT("enum"))
        {
            FString Default;
            if (Object->Values.Num() != 4 || !DefaultValue->TryGetString(Default)
                || !ReadNames(Object, TEXT("values"), Row.Values) || !NamesAreUnique(Row.Values)
                || !Row.Values.Contains(FName(*Default))) return false;
        }
        else if (Row.Type == TEXT("binding_map") || Row.Type == TEXT("hold_toggle_map"))
        {
            FString Default;
            if (Object->Values.Num() != 3 || !DefaultValue->TryGetString(Default) || Default.IsEmpty()) return false;
        }
        else return false;
        OutManifest.AccessibilityOptions.Add(Row);
    }
    LoadErrorGuard.bSucceeded = OutManifest.Validate(OutErrors);
    return LoadErrorGuard.bSucceeded;
}

bool FDAUIManifest::Validate(TArray<FText>& OutErrors) const
{
    if (SchemaVersion != 1 || Fingerprint != FrozenFingerprint || Screens.Num() != FrozenScreens().Num()
        || Overlays.Num() != FrozenOverlays().Num() || AccessibilityOptions.Num() != FrozenAccessibility().Num()
        || InputContexts.Num() != 5 || VisualDirection.Tone != TEXT("industrial_civic_synara")
        || !VisualDirection.bLowChromeHUD || !VisualDirection.bKeepCenterAndLowerMiddleClear
        || VisualDirection.MotionSettingId != TEXT("reduced_motion")
        || VisualDirection.FlashSettingId != TEXT("reduced_flash"))
    {
        OutErrors.Add(LOCTEXT("FrozenShape", "UI manifest does not match the frozen vertical-slice shape."));
        return false;
    }
    if (ComputeSemanticFingerprint(*this) != TEXT("e9e7adc0e4e8dea0a4e60138ccfbafa8c341fe23"))
    { OutErrors.Add(LOCTEXT("FrozenSemantics", "UI manifest semantic projection differs from the frozen contract.")); return false; }
    TSet<FName> ContextIds;
    for (const FDAUIInputContextDescriptor& Context : InputContexts)
    {
        if (Context.Id.IsNone() || ContextIds.Contains(Context.Id) || Context.AssetPath.IsNull() || Context.Priority < 0
            || !Context.AssetPath.ToString().StartsWith(TEXT("/Game/UI/Input/")))
        { OutErrors.Add(LOCTEXT("InvalidContext", "UI input contexts require unique stable IDs and canonical asset paths.")); return false; }
        ContextIds.Add(Context.Id);
    }
    TSet<FName> ScreenIds;
    for (int32 Index = 0; Index < Screens.Num(); ++Index)
    {
        const FDAUIScreenDescriptor& Screen = Screens[Index];
        const FString Asset = Screen.AssetPath.ToString();
        if (Screen.Id != FrozenScreens()[Index] || ScreenIds.Contains(Screen.Id) || Screen.DisplayName.IsEmpty()
            || !ContextIds.Contains(Screen.InputContextId)
            || Screen.EntryFocusTarget.IsNone() || Screen.BackTarget.IsNone() || Screen.WidgetClass.IsNull()
            || !NamesAreUnique(Screen.DataChannels) || !NamesAreUnique(Screen.CampaignCriticalActions)
            || !(Asset.StartsWith(TEXT("/Game/UI/Screens/")) || Asset.StartsWith(TEXT("/Game/UI/HUD/"))))
        { OutErrors.Add(LOCTEXT("InvalidScreen", "A frozen screen descriptor is invalid or duplicated.")); return false; }
        ScreenIds.Add(Screen.Id);
    }
    for (const FDAUIScreenDescriptor& Screen : Screens)
        if (Screen.BackTarget != TEXT("__exit__") && !ScreenIds.Contains(Screen.BackTarget))
        { OutErrors.Add(LOCTEXT("InvalidBackTarget", "Every screen back target must resolve or explicitly exit.")); return false; }
    TSet<FName> OverlayIds;
    for (int32 Index = 0; Index < Overlays.Num(); ++Index)
        if (Overlays[Index].Id != FrozenOverlays()[Index]
            || OverlayIds.Contains(Overlays[Index].Id) || Overlays[Index].DisplayName.IsEmpty()
            || Overlays[Index].WidgetClass.IsNull()
            || Overlays[Index].MarkerShape.IsNone()
            || !Overlays[Index].AssetPath.ToString().StartsWith(TEXT("/Game/UI/Overlays/")))
        { OutErrors.Add(LOCTEXT("InvalidOverlay", "A frozen overlay descriptor is invalid.")); return false; }
        else OverlayIds.Add(Overlays[Index].Id);
    for (int32 Index = 0; Index < AccessibilityOptions.Num(); ++Index)
        if (AccessibilityOptions[Index].Id != FrozenAccessibility()[Index]
            || AccessibilityOptions[Index].Type != FrozenAccessibilityTypes()[Index]
            || AccessibilityOptions[Index].DefaultValueJson.IsEmpty())
        { OutErrors.Add(LOCTEXT("InvalidAccessibility", "Accessibility option set is not the frozen ordered set.")); return false; }
    TSet<FName> ActionIds;
    TSet<FName> CommandIds;
    for (const FDAUICampaignActionDescriptor& Action : CampaignCriticalActions)
    {
        if (Action.Id.IsNone() || ActionIds.Contains(Action.Id)
            || Action.CommandId.IsNone() || CommandIds.Contains(Action.CommandId) || Action.Surfaces.IsEmpty()
            || !NamesAreUnique(Action.Surfaces)
            || Action.KeyboardInput.IsEmpty() || Action.ControllerInput.IsEmpty() || Action.bRequiresHover)
        { OutErrors.Add(LOCTEXT("InvalidAction", "Campaign actions require keyboard/controller service routes without hover.")); return false; }
        ActionIds.Add(Action.Id);
        CommandIds.Add(Action.CommandId);
        for (const FName SurfaceId : Action.Surfaces)
        {
            const FDAUIScreenDescriptor* Screen = FindScreen(SurfaceId);
            if (Screen == nullptr || !Screen->CampaignCriticalActions.Contains(Action.Id))
            { OutErrors.Add(LOCTEXT("UnlinkedAction", "Campaign action surface links must be bidirectional.")); return false; }
        }
    }
    for (const FDAUIScreenDescriptor& Screen : Screens)
        for (const FName ActionId : Screen.CampaignCriticalActions)
        {
            const FDAUICampaignActionDescriptor* Action = FindAction(ActionId);
            if (Action == nullptr || !Action->Surfaces.Contains(Screen.Id))
            { OutErrors.Add(LOCTEXT("UnlinkedScreen", "Screen campaign action links must be bidirectional.")); return false; }
        }
    return true;
}

const FDAUIScreenDescriptor* FDAUIManifest::FindScreen(const FName Id) const
{ return Screens.FindByPredicate([Id](const FDAUIScreenDescriptor& Row) { return Row.Id == Id; }); }
const FDAUIInputContextDescriptor* FDAUIManifest::FindInputContext(const FName Id) const
{ return InputContexts.FindByPredicate([Id](const FDAUIInputContextDescriptor& Row) { return Row.Id == Id; }); }
const FDAUICampaignActionDescriptor* FDAUIManifest::FindAction(const FName Id) const
{ return CampaignCriticalActions.FindByPredicate([Id](const FDAUICampaignActionDescriptor& Row) { return Row.Id == Id; }); }
const FDAUIOverlayDescriptor* FDAUIManifest::FindOverlay(const FName Id) const
{ return Overlays.FindByPredicate([Id](const FDAUIOverlayDescriptor& Row) { return Row.Id == Id; }); }

#undef LOCTEXT_NAMESPACE
