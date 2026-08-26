#include "Presentation/DAPresentationContent.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    TSet<FString> Keys(std::initializer_list<const TCHAR*> Values)
    {
        TSet<FString> Result;
        for (const TCHAR* Value : Values) Result.Add(Value);
        return Result;
    }

    void AddError(TArray<FText>& Errors, const FString& Message)
    {
        Errors.Add(FText::FromString(Message));
    }

    bool ExactKeys(const TSharedPtr<FJsonObject>& Object, const TSet<FString>& Expected,
        const FString& Where, TArray<FText>& Errors)
    {
        if (!Object.IsValid())
        {
            AddError(Errors, Where + TEXT(" must be an object."));
            return false;
        }
        TSet<FString> Actual;
        for (const auto& Pair : Object->Values) Actual.Add(Pair.Key);
        if (Actual != Expected)
        {
            AddError(Errors, Where + TEXT(" has missing or unknown fields."));
            return false;
        }
        return true;
    }

    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        FString& Out, const FString& Where, TArray<FText>& Errors)
    {
        if (!Object.IsValid() || !Object->TryGetStringField(Field, Out) || Out.IsEmpty())
        {
            AddError(Errors, Where + TEXT(".") + Field + TEXT(" must be a non-empty string."));
            return false;
        }
        return true;
    }

    bool ReadName(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        FName& Out, const FString& Where, TArray<FText>& Errors)
    {
        FString Value;
        if (!ReadString(Object, Field, Value, Where, Errors)) return false;
        Out = FName(*Value);
        return true;
    }

    bool ReadInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        int32& Out, const FString& Where, TArray<FText>& Errors)
    {
        double Value = 0.0;
        if (!Object.IsValid() || !Object->TryGetNumberField(Field, Value)
            || !FMath::IsFinite(Value) || Value != FMath::RoundToDouble(Value)
            || Value < static_cast<double>(MIN_int32)
            || Value > static_cast<double>(MAX_int32))
        {
            AddError(Errors, Where + TEXT(".") + Field + TEXT(" must be an integer."));
            return false;
        }
        Out = static_cast<int32>(Value);
        return true;
    }

    bool ReadBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        bool& Out, const FString& Where, TArray<FText>& Errors)
    {
        if (!Object.IsValid() || !Object->TryGetBoolField(Field, Out))
        {
            AddError(Errors, Where + TEXT(".") + Field + TEXT(" must be a boolean."));
            return false;
        }
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ReadArray(const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field, const FString& Where, TArray<FText>& Errors)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || Values == nullptr)
        {
            AddError(Errors, Where + TEXT(".") + Field + TEXT(" must be an array."));
            return nullptr;
        }
        return Values;
    }

    bool ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        TArray<FName>& Out, const FString& Where, TArray<FText>& Errors)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = ReadArray(Object, Field, Where, Errors);
        if (Values == nullptr) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (!Value.IsValid() || !Value->TryGetString(Text) || Text.IsEmpty())
            {
                AddError(Errors, Where + TEXT(".") + Field + TEXT(" entries must be strings."));
                return false;
            }
            Out.Add(FName(*Text));
        }
        return true;
    }

    bool LoadJson(const FString& Filename, TSharedPtr<FJsonObject>& Out, TArray<FText>& Errors)
    {
        FString Json;
        if (!FFileHelper::LoadFileToString(Json, *Filename))
        {
            AddError(Errors, TEXT("Could not read presentation source: ") + Filename);
            return false;
        }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
        {
            AddError(Errors, TEXT("Presentation source is not valid JSON: ") + Filename);
            return false;
        }
        return true;
    }

    FString EscapeJson(const FString& Value)
    {
        FString Out(TEXT("\""));
        for (const TCHAR Character : Value)
        {
            if (Character == TEXT('"')) Out += TEXT("\\\"");
            else if (Character == TEXT('\\')) Out += TEXT("\\\\");
            else if (Character == TEXT('\n')) Out += TEXT("\\n");
            else if (Character == TEXT('\r')) Out += TEXT("\\r");
            else if (Character == TEXT('\t')) Out += TEXT("\\t");
            else Out.AppendChar(Character);
        }
        return Out + TEXT("\"");
    }

    void CanonicalJson(const TSharedPtr<FJsonValue>& Value, FString& Out)
    {
        if (!Value.IsValid()) return;
        switch (Value->Type)
        {
        case EJson::Null: Out += TEXT("null"); break;
        case EJson::String: Out += EscapeJson(Value->AsString()); break;
        case EJson::Boolean: Out += Value->AsBool() ? TEXT("true") : TEXT("false"); break;
        case EJson::Number:
        {
            const double Number = Value->AsNumber();
            const int64 Integer = static_cast<int64>(Number);
            Out += Number == static_cast<double>(Integer)
                ? FString::Printf(TEXT("%lld"), static_cast<long long>(Integer))
                : FString::Printf(TEXT("%.15g"), Number);
            break;
        }
        case EJson::Array:
        {
            Out += TEXT("[");
            const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
            for (int32 Index = 0; Index < Values.Num(); ++Index)
            {
                if (Index > 0) Out += TEXT(",");
                CanonicalJson(Values[Index], Out);
            }
            Out += TEXT("]");
            break;
        }
        case EJson::Object:
        {
            Out += TEXT("{");
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            TArray<FString> Names;
            Object->Values.GetKeys(Names);
            Names.Sort();
            for (int32 Index = 0; Index < Names.Num(); ++Index)
            {
                if (Index > 0) Out += TEXT(",");
                Out += EscapeJson(Names[Index]) + TEXT(":");
                CanonicalJson(Object->Values[Names[Index]], Out);
            }
            Out += TEXT("}");
            break;
        }
        default: break;
        }
    }

    FString FingerprintValue(const TSharedPtr<FJsonValue>& Value)
    {
        FString Canonical;
        CanonicalJson(Value, Canonical);
        FTCHARToUTF8 Utf8(*Canonical);
        uint8 Hash[FSHA1::DigestSize];
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }

    FString CombinedFingerprint(const TSharedPtr<FJsonObject>& Root,
        const TMap<FString, TSharedPtr<FJsonObject>>& Sources)
    {
        TSharedPtr<FJsonObject> RootWithoutFingerprint = MakeShared<FJsonObject>();
        RootWithoutFingerprint->Values = Root->Values;
        RootWithoutFingerprint->RemoveField(TEXT("fingerprint"));
        TSharedPtr<FJsonObject> SourceObject = MakeShared<FJsonObject>();
        for (const auto& Pair : Sources)
            SourceObject->SetObjectField(Pair.Key, Pair.Value);
        TSharedPtr<FJsonObject> Material = MakeShared<FJsonObject>();
        Material->SetObjectField(TEXT("manifest"), RootWithoutFingerprint);
        Material->SetObjectField(TEXT("sources"), SourceObject);
        return FingerprintValue(MakeShared<FJsonValueObject>(Material));
    }

    bool StringArrayIsValid(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        const FString& Where, const int32 Minimum, TArray<FText>& Errors)
    {
        TArray<FName> Values;
        return ReadStringArray(Object, Field, Values, Where, Errors)
            && Values.Num() >= Minimum;
    }

    bool ParsePrimary(const TSharedPtr<FJsonObject>& Root, const bool bCharacter,
        TArray<FDAPresentationDefinitionSource>& Out, TArray<int32>& OutNumbers,
        TArray<FText>& Errors)
    {
        const FString Where = bCharacter ? TEXT("characters") : TEXT("buildings");
        if (!ExactKeys(Root, Keys({TEXT("schemaVersion"), TEXT("sourceId"), TEXT("assets")}),
            Where, Errors)) return false;
        int32 Version = 0;
        FString SourceId;
        if (!ReadInteger(Root, TEXT("schemaVersion"), Version, Where, Errors)
            || !ReadString(Root, TEXT("sourceId"), SourceId, Where, Errors)
            || Version != 1
            || SourceId != (bCharacter ? TEXT("presentation.primary.characters.v06")
                                      : TEXT("presentation.primary.buildings.v06")))
        {
            AddError(Errors, Where + TEXT(" identity is not canonical."));
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>* Rows = ReadArray(Root, TEXT("assets"), Where, Errors);
        if (Rows == nullptr) return false;
        for (int32 Index = 0; Index < Rows->Num(); ++Index)
        {
            const FString Label = FString::Printf(TEXT("%s.assets[%d]"), *Where, Index);
            const TSharedPtr<FJsonObject> Row = (*Rows)[Index].IsValid()
                ? (*Rows)[Index]->AsObject() : nullptr;
            if (!ExactKeys(Row, Keys({TEXT("number"), TEXT("id"), TEXT("displayName"),
                    TEXT("faction"), TEXT("kind"), TEXT("assetPath"), TEXT("recipe")}),
                Label, Errors)) return false;
            int32 Number = 0;
            FString Id;
            FString Kind;
            FDAPresentationDefinitionSource& Source = Out.Emplace_GetRef();
            Source.Kind = EDAPresentationDefinitionKind::PrimaryAsset;
            if (!ReadInteger(Row, TEXT("number"), Number, Label, Errors)
                || !ReadString(Row, TEXT("id"), Id, Label, Errors)
                || !ReadString(Row, TEXT("displayName"), Source.DisplayName, Label, Errors)
                || !ReadName(Row, TEXT("faction"), Source.Faction, Label, Errors)
                || !ReadString(Row, TEXT("kind"), Kind, Label, Errors)
                || !ReadString(Row, TEXT("assetPath"), Source.AssetPath, Label, Errors)
                || Number < 1 || Number > 50
                || !Id.StartsWith(FString::Printf(TEXT("primary.%02d."), Number))
                || !Source.AssetPath.StartsWith(bCharacter
                    ? TEXT("/Game/Characters/") : TEXT("/Game/Buildings/")))
            {
                AddError(Errors, Label + TEXT(" has invalid identity or path."));
                return false;
            }
            Source.Id = FName(*Id);
            const TSharedPtr<FJsonObject>* Recipe = nullptr;
            if (!Row->TryGetObjectField(TEXT("recipe"), Recipe) || Recipe == nullptr)
            {
                AddError(Errors, Label + TEXT(".recipe must be an object."));
                return false;
            }
            const TSet<FString> RecipeKeys = bCharacter
                ? Keys({TEXT("silhouette"), TEXT("materials"), TEXT("requiredVariants"),
                    TEXT("readabilityHook"), TEXT("rigProfile")})
                : Keys({TEXT("silhouette"), TEXT("materials"), TEXT("signatureElements"),
                    TEXT("constructionGrammar"), TEXT("damageLanguage"), TEXT("requiredStates")});
            if (!ExactKeys(*Recipe, RecipeKeys, Label + TEXT(".recipe"), Errors)
                || !StringArrayIsValid(*Recipe, TEXT("materials"), Label + TEXT(".recipe"), 1, Errors)
                || !(bCharacter
                    ? StringArrayIsValid(*Recipe, TEXT("requiredVariants"), Label + TEXT(".recipe"), 1, Errors)
                    : StringArrayIsValid(*Recipe, TEXT("signatureElements"), Label + TEXT(".recipe"), 1, Errors)))
                return false;
            FString Silhouette;
            if (!ReadString(*Recipe, TEXT("silhouette"), Silhouette, Label + TEXT(".recipe"), Errors))
                return false;
            if (bCharacter)
            {
                FString Readability;
                FString Rig;
                if (!ReadString(*Recipe, TEXT("readabilityHook"), Readability, Label + TEXT(".recipe"), Errors)
                    || !ReadString(*Recipe, TEXT("rigProfile"), Rig, Label + TEXT(".recipe"), Errors))
                    return false;
            }
            else
            {
                FString Grammar;
                FString Damage;
                TArray<FName> RequiredStates;
                if (!ReadString(*Recipe, TEXT("constructionGrammar"), Grammar, Label + TEXT(".recipe"), Errors)
                    || !ReadString(*Recipe, TEXT("damageLanguage"), Damage, Label + TEXT(".recipe"), Errors)
                    || !ReadStringArray(*Recipe, TEXT("requiredStates"), RequiredStates,
                        Label + TEXT(".recipe"), Errors)
                    || !Grammar.StartsWith(TEXT("construction."))
                    || !Damage.StartsWith(TEXT("damage."))
                    || RequiredStates != TArray<FName>({TEXT("construction"), TEXT("operational"),
                        TEXT("damaged"), TEXT("ruined")}))
                    return false;
            }
            CanonicalJson(MakeShared<FJsonValueObject>(*Recipe), Source.RecipePayload);
            Source.RecipeFingerprint = FingerprintValue(MakeShared<FJsonValueObject>(*Recipe));
            OutNumbers.Add(Number);
        }
        return true;
    }

    bool ParseVfx(const TSharedPtr<FJsonObject>& Root,
        TArray<FDAPresentationDefinitionSource>& Out, TArray<FText>& Errors)
    {
        if (!ExactKeys(Root, Keys({TEXT("schemaVersion"), TEXT("sourceId"), TEXT("effects")}),
            TEXT("vfx"), Errors)) return false;
        int32 Version = 0;
        FString SourceId;
        if (!ReadInteger(Root, TEXT("schemaVersion"), Version, TEXT("vfx"), Errors)
            || !ReadString(Root, TEXT("sourceId"), SourceId, TEXT("vfx"), Errors)
            || Version != 1 || SourceId != TEXT("presentation.core_vfx.v11")) return false;
        const TArray<TSharedPtr<FJsonValue>>* Rows = ReadArray(Root, TEXT("effects"), TEXT("vfx"), Errors);
        if (Rows == nullptr || Rows->Num() != 25) return false;
        for (int32 Index = 0; Index < Rows->Num(); ++Index)
        {
            const FString Label = FString::Printf(TEXT("vfx.effects[%d]"), Index);
            const TSharedPtr<FJsonObject> Row = (*Rows)[Index].IsValid()
                ? (*Rows)[Index]->AsObject() : nullptr;
            if (!ExactKeys(Row, Keys({TEXT("id"), TEXT("faction"), TEXT("family"),
                    TEXT("assetPath"), TEXT("recipe")}), Label, Errors)) return false;
            FDAPresentationDefinitionSource& Source = Out.Emplace_GetRef();
            Source.Kind = EDAPresentationDefinitionKind::CoreVFX;
            FString Id;
            if (!ReadString(Row, TEXT("id"), Id, Label, Errors)
                || !ReadName(Row, TEXT("faction"), Source.Faction, Label, Errors)
                || !ReadString(Row, TEXT("family"), Source.DisplayName, Label, Errors)
                || !ReadString(Row, TEXT("assetPath"), Source.AssetPath, Label, Errors)
                || !Id.StartsWith(TEXT("vfx."))
                || !Source.AssetPath.StartsWith(TEXT("/Game/VFX/"))) return false;
            Source.Id = FName(*Id);
            const TSharedPtr<FJsonObject>* Recipe = nullptr;
            if (!Row->TryGetObjectField(TEXT("recipe"), Recipe) || Recipe == nullptr
                || !ExactKeys(*Recipe, Keys({TEXT("anticipation"), TEXT("active"), TEXT("resolution"),
                    TEXT("signature"), TEXT("gameplayRadiusMeters")}), Label + TEXT(".recipe"), Errors))
                return false;
            double Radius = 0.0;
            FString Value;
            if (!ReadString(*Recipe, TEXT("anticipation"), Value, Label, Errors)
                || !ReadString(*Recipe, TEXT("active"), Value, Label, Errors)
                || !ReadString(*Recipe, TEXT("resolution"), Value, Label, Errors)
                || !ReadString(*Recipe, TEXT("signature"), Value, Label, Errors)
                || !(*Recipe)->TryGetNumberField(TEXT("gameplayRadiusMeters"), Radius)
                || !FMath::IsFinite(Radius) || Radius <= 0.0) return false;
            CanonicalJson(MakeShared<FJsonValueObject>(*Recipe), Source.RecipePayload);
            Source.RecipeFingerprint = FingerprintValue(MakeShared<FJsonValueObject>(*Recipe));
        }
        return true;
    }

    bool ParseAudioRows(const TArray<TSharedPtr<FJsonValue>>& Rows,
        const EDAPresentationDefinitionKind Kind, const FString& Prefix,
        const FString& AssetPrefix, TArray<FDAPresentationDefinitionSource>& Out,
        TArray<FText>& Errors)
    {
        const bool bSfx = Kind == EDAPresentationDefinitionKind::SFX;
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            const FString Label = FString::Printf(TEXT("audio.%s[%d]"), *Prefix.LeftChop(1), Index);
            const TSharedPtr<FJsonObject> Row = Rows[Index].IsValid() ? Rows[Index]->AsObject() : nullptr;
            if (!ExactKeys(Row, bSfx
                    ? Keys({TEXT("id"), TEXT("family"), TEXT("assetPath"), TEXT("recipe")})
                    : Keys({TEXT("id"), TEXT("displayName"), TEXT("assetPath"), TEXT("recipe")}),
                Label, Errors)) return false;
            FDAPresentationDefinitionSource& Source = Out.Emplace_GetRef();
            Source.Kind = Kind;
            FString Id;
            if (!ReadString(Row, TEXT("id"), Id, Label, Errors)
                || !ReadString(Row, bSfx ? TEXT("family") : TEXT("displayName"),
                    Source.DisplayName, Label, Errors)
                || !ReadString(Row, TEXT("assetPath"), Source.AssetPath, Label, Errors)
                || !Id.StartsWith(Prefix) || !Source.AssetPath.StartsWith(AssetPrefix)) return false;
            Source.Id = FName(*Id);
            const TSharedPtr<FJsonObject>* Recipe = nullptr;
            if (!Row->TryGetObjectField(TEXT("recipe"), Recipe) || Recipe == nullptr) return false;
            const TSet<FString> Expected = Kind == EDAPresentationDefinitionKind::Music
                ? Keys({TEXT("palette"), TEXT("playback"), TEXT("purpose")})
                : Kind == EDAPresentationDefinitionKind::Ambient
                    ? Keys({TEXT("layers"), TEXT("spatialized"), TEXT("loopSeconds")})
                    : Keys({TEXT("transient"), TEXT("palette"), TEXT("event"), TEXT("variationLayers")});
            if (!ExactKeys(*Recipe, Expected, Label + TEXT(".recipe"), Errors)) return false;
            if (Kind == EDAPresentationDefinitionKind::Music)
            {
                FString Value;
                if (!StringArrayIsValid(*Recipe, TEXT("palette"), Label, 1, Errors)
                    || !ReadString(*Recipe, TEXT("playback"), Value, Label, Errors)
                    || !ReadString(*Recipe, TEXT("purpose"), Value, Label, Errors)) return false;
            }
            else if (Kind == EDAPresentationDefinitionKind::Ambient)
            {
                bool bSpatialized = false;
                int32 Seconds = 0;
                if (!StringArrayIsValid(*Recipe, TEXT("layers"), Label, 1, Errors)
                    || !ReadBool(*Recipe, TEXT("spatialized"), bSpatialized, Label, Errors)
                    || !ReadInteger(*Recipe, TEXT("loopSeconds"), Seconds, Label, Errors)
                    || Seconds <= 0) return false;
            }
            else
            {
                FString Value;
                int32 Variations = 0;
                if (!ReadString(*Recipe, TEXT("transient"), Value, Label, Errors)
                    || !ReadString(*Recipe, TEXT("palette"), Value, Label, Errors)
                    || !ReadString(*Recipe, TEXT("event"), Value, Label, Errors)
                    || !ReadInteger(*Recipe, TEXT("variationLayers"), Variations, Label, Errors)
                    || Variations != 0) return false;
            }
            CanonicalJson(MakeShared<FJsonValueObject>(*Recipe), Source.RecipePayload);
            Source.RecipeFingerprint = FingerprintValue(MakeShared<FJsonValueObject>(*Recipe));
        }
        return true;
    }

    bool ParseAudio(const TSharedPtr<FJsonObject>& Root, FDAPresentationContentManifest& Out,
        TArray<FText>& Errors)
    {
        if (!ExactKeys(Root, Keys({TEXT("schemaVersion"), TEXT("sourceId"), TEXT("music"),
                TEXT("ambient"), TEXT("sfx")}), TEXT("audio"), Errors)) return false;
        int32 Version = 0;
        FString SourceId;
        if (!ReadInteger(Root, TEXT("schemaVersion"), Version, TEXT("audio"), Errors)
            || !ReadString(Root, TEXT("sourceId"), SourceId, TEXT("audio"), Errors)
            || Version != 1 || SourceId != TEXT("presentation.audio.v11")) return false;
        const auto* Music = ReadArray(Root, TEXT("music"), TEXT("audio"), Errors);
        const auto* Ambient = ReadArray(Root, TEXT("ambient"), TEXT("audio"), Errors);
        const auto* Sfx = ReadArray(Root, TEXT("sfx"), TEXT("audio"), Errors);
        return Music != nullptr && Ambient != nullptr && Sfx != nullptr
            && Music->Num() == 9 && Ambient->Num() == 12 && Sfx->Num() >= 60
            && ParseAudioRows(*Music, EDAPresentationDefinitionKind::Music,
                TEXT("music."), TEXT("/Game/Audio/Music/"), Out.Music, Errors)
            && ParseAudioRows(*Ambient, EDAPresentationDefinitionKind::Ambient,
                TEXT("ambient."), TEXT("/Game/Audio/Ambient/"), Out.Ambient, Errors)
            && ParseAudioRows(*Sfx, EDAPresentationDefinitionKind::SFX,
                TEXT("sfx."), TEXT("/Game/Audio/SFX/"), Out.Sfx, Errors);
    }

    bool ParseCueBinding(const TSharedPtr<FJsonObject>& Row, const FString& Label,
        FDAPresentationStateBinding& Out, const TCHAR* StateField, TArray<FText>& Errors)
    {
        if (!ExactKeys(Row, Keys({StateField, TEXT("geometryHook"), TEXT("vfxId"), TEXT("sfxId")}),
            Label, Errors)) return false;
        return ReadName(Row, StateField, Out.State, Label, Errors)
            && ReadName(Row, TEXT("geometryHook"), Out.GeometryHook, Label, Errors)
            && ReadName(Row, TEXT("vfxId"), Out.VfxId, Label, Errors)
            && ReadName(Row, TEXT("sfxId"), Out.SfxId, Label, Errors);
    }

    bool ParseBindings(const TSharedPtr<FJsonObject>& Root, FDAPresentationContentManifest& Out,
        TArray<FText>& Errors)
    {
        if (!ExactKeys(Root, Keys({TEXT("schemaVersion"), TEXT("sourceId"),
                TEXT("constructionSourceHook"), TEXT("damageSourceHook"),
                TEXT("constructionGrammars"), TEXT("damageLanguages"), TEXT("capturePolicy"),
                TEXT("daxton"), TEXT("ascension")}), TEXT("bindings"), Errors)) return false;
        int32 Version = 0;
        FString SourceId;
        if (!ReadInteger(Root, TEXT("schemaVersion"), Version, TEXT("bindings"), Errors)
            || !ReadString(Root, TEXT("sourceId"), SourceId, TEXT("bindings"), Errors)
            || Version != 1 || SourceId != TEXT("presentation.bindings.v11")
            || !ReadName(Root, TEXT("constructionSourceHook"), Out.ConstructionSourceHook,
                TEXT("bindings"), Errors)
            || !ReadName(Root, TEXT("damageSourceHook"), Out.DamageSourceHook,
                TEXT("bindings"), Errors)) return false;

        const auto* Grammars = ReadArray(Root, TEXT("constructionGrammars"), TEXT("bindings"), Errors);
        if (Grammars == nullptr || Grammars->Num() != 3) return false;
        for (int32 Index = 0; Index < Grammars->Num(); ++Index)
        {
            const FString Label = FString::Printf(TEXT("constructionGrammars[%d]"), Index);
            const TSharedPtr<FJsonObject> Row = (*Grammars)[Index].IsValid()
                ? (*Grammars)[Index]->AsObject() : nullptr;
            if (!ExactKeys(Row, Keys({TEXT("faction"), TEXT("grammarId"), TEXT("stages")}),
                Label, Errors)) return false;
            FDAConstructionPresentationGrammar& Grammar = Out.ConstructionGrammars.Emplace_GetRef();
            if (!ReadName(Row, TEXT("faction"), Grammar.Faction, Label, Errors)
                || !ReadName(Row, TEXT("grammarId"), Grammar.GrammarId, Label, Errors)) return false;
            const auto* Stages = ReadArray(Row, TEXT("stages"), Label, Errors);
            if (Stages == nullptr || Stages->Num() != 5) return false;
            for (int32 StageIndex = 0; StageIndex < Stages->Num(); ++StageIndex)
            {
                const TSharedPtr<FJsonObject> Stage = (*Stages)[StageIndex].IsValid()
                    ? (*Stages)[StageIndex]->AsObject() : nullptr;
                if (!ExactKeys(Stage, Keys({TEXT("state"), TEXT("geometryHook"),
                        TEXT("vfxId"), TEXT("sfxId")}), Label + TEXT(".stages"), Errors))
                    return false;
                FString State;
                FDAConstructionPresentationStage& Parsed = Grammar.Stages.Emplace_GetRef();
                if (!ReadString(Stage, TEXT("state"), State, Label, Errors)
                    || !ReadName(Stage, TEXT("geometryHook"), Parsed.GeometryHook, Label, Errors)
                    || !ReadName(Stage, TEXT("vfxId"), Parsed.VfxId, Label, Errors)
                    || !ReadName(Stage, TEXT("sfxId"), Parsed.SfxId, Label, Errors)) return false;
                if (State == TEXT("Foundation")) Parsed.State = EDAConstructionState::Foundation;
                else if (State == TEXT("Frame")) Parsed.State = EDAConstructionState::Frame;
                else if (State == TEXT("Shell")) Parsed.State = EDAConstructionState::Shell;
                else if (State == TEXT("Systems")) Parsed.State = EDAConstructionState::Systems;
                else if (State == TEXT("Operational")) Parsed.State = EDAConstructionState::Operational;
                else return false;
            }
        }

        const auto* Damage = ReadArray(Root, TEXT("damageLanguages"), TEXT("bindings"), Errors);
        if (Damage == nullptr || Damage->Num() != 3) return false;
        for (int32 Index = 0; Index < Damage->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Row = (*Damage)[Index].IsValid()
                ? (*Damage)[Index]->AsObject() : nullptr;
            if (!ExactKeys(Row, Keys({TEXT("faction"), TEXT("materialHook"),
                    TEXT("vfxId"), TEXT("sfxId")}), TEXT("damageLanguages[]"), Errors)) return false;
            FDADamagePresentationLanguage& Parsed = Out.DamageLanguages.Emplace_GetRef();
            if (!ReadName(Row, TEXT("faction"), Parsed.Faction, TEXT("damage"), Errors)
                || !ReadName(Row, TEXT("materialHook"), Parsed.MaterialHook, TEXT("damage"), Errors)
                || !ReadName(Row, TEXT("vfxId"), Parsed.VfxId, TEXT("damage"), Errors)
                || !ReadName(Row, TEXT("sfxId"), Parsed.SfxId, TEXT("damage"), Errors)) return false;
        }

        const TSharedPtr<FJsonObject>* Capture = nullptr;
        if (!Root->TryGetObjectField(TEXT("capturePolicy"), Capture) || Capture == nullptr
            || !ExactKeys(*Capture, Keys({TEXT("sourceHook"), TEXT("allowsInstantFactionRecolor"),
                TEXT("stages")}), TEXT("capturePolicy"), Errors)
            || !ReadName(*Capture, TEXT("sourceHook"), Out.CaptureSourceHook, TEXT("capturePolicy"), Errors)
            || !ReadBool(*Capture, TEXT("allowsInstantFactionRecolor"),
                Out.bAllowsInstantFactionRecolor, TEXT("capturePolicy"), Errors)
            || !ReadStringArray(*Capture, TEXT("stages"), Out.CaptureStages,
                TEXT("capturePolicy"), Errors)) return false;

        const TSharedPtr<FJsonObject>* Daxton = nullptr;
        if (!Root->TryGetObjectField(TEXT("daxton"), Daxton) || Daxton == nullptr
            || !ExactKeys(*Daxton, Keys({TEXT("sourceAuthority"), TEXT("states")}),
                TEXT("daxton"), Errors)
            || !ReadName(*Daxton, TEXT("sourceAuthority"), Out.DaxtonSourceAuthority,
                TEXT("daxton"), Errors)) return false;
        const auto* DaxtonStates = ReadArray(*Daxton, TEXT("states"), TEXT("daxton"), Errors);
        if (DaxtonStates == nullptr || DaxtonStates->Num() != 4) return false;
        for (int32 Index = 0; Index < DaxtonStates->Num(); ++Index)
        {
            FDAPresentationStateBinding& Parsed = Out.DaxtonStates.Emplace_GetRef();
            if (!ParseCueBinding((*DaxtonStates)[Index]->AsObject(), TEXT("daxton.states[]"),
                Parsed, TEXT("phase"), Errors)) return false;
        }

        const TSharedPtr<FJsonObject>* Ascension = nullptr;
        if (!Root->TryGetObjectField(TEXT("ascension"), Ascension) || Ascension == nullptr
            || !ExactKeys(*Ascension, Keys({TEXT("sourceAuthority"), TEXT("cinematicAssetPath"),
                TEXT("gameplayGate"), TEXT("cinematicMayBeSkipped"), TEXT("beats")}),
                TEXT("ascension"), Errors)
            || !ReadName(*Ascension, TEXT("sourceAuthority"), Out.AscensionSourceAuthority,
                TEXT("ascension"), Errors)) return false;
        FString CinematicPath;
        if (!ReadString(*Ascension, TEXT("cinematicAssetPath"), CinematicPath,
                TEXT("ascension"), Errors)
            || !ReadBool(*Ascension, TEXT("gameplayGate"), Out.bAscensionGameplayGate,
                TEXT("ascension"), Errors)
            || !ReadBool(*Ascension, TEXT("cinematicMayBeSkipped"),
                Out.bAscensionCinematicMayBeSkipped, TEXT("ascension"), Errors)) return false;
        Out.AscensionCinematicAsset = FSoftObjectPath(CinematicPath);
        const auto* Beats = ReadArray(*Ascension, TEXT("beats"), TEXT("ascension"), Errors);
        if (Beats == nullptr || Beats->Num() != 5) return false;
        for (int32 Index = 0; Index < Beats->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Beat = (*Beats)[Index].IsValid()
                ? (*Beats)[Index]->AsObject() : nullptr;
            if (!ExactKeys(Beat, Keys({TEXT("beat"), TEXT("vfxId"), TEXT("sfxId")}),
                TEXT("ascension.beats[]"), Errors)) return false;
            FDAPresentationStateBinding& Parsed = Out.AscensionBeats.Emplace_GetRef();
            if (!ReadName(Beat, TEXT("beat"), Parsed.State, TEXT("ascension.beats[]"), Errors)
                || !ReadName(Beat, TEXT("vfxId"), Parsed.VfxId, TEXT("ascension.beats[]"), Errors)
                || !ReadName(Beat, TEXT("sfxId"), Parsed.SfxId, TEXT("ascension.beats[]"), Errors))
                return false;
            Parsed.GeometryHook = TEXT("forgeweave_authority_transfer");
        }
        return true;
    }

    bool ValidateSourceFile(const FString& RelativePath, const FString& ExpectedSha1,
        TArray<FText>& Errors)
    {
#if WITH_EDITOR
        TArray<uint8> Bytes;
        const FString Filename = FPaths::ProjectDir() / RelativePath;
        if (!RelativePath.StartsWith(TEXT("ContentSource/"))
            || ExpectedSha1.Len() != 40
            || !FFileHelper::LoadFileToArray(Bytes, *Filename))
        {
            AddError(Errors, TEXT("Presentation artifact source is missing or invalid: ")
                + RelativePath);
            return false;
        }
        uint8 Hash[FSHA1::DigestSize];
        FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash);
        if (BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower() != ExpectedSha1)
        {
            AddError(Errors, TEXT("Presentation artifact source fingerprint drift: ")
                + RelativePath);
            return false;
        }
        return true;
#else
        return RelativePath.StartsWith(TEXT("ContentSource/")) && ExpectedSha1.Len() == 40;
#endif
    }

    bool ParseArtifactSources(const TSharedPtr<FJsonObject>& Root,
        FDAPresentationContentManifest& Out, TArray<FText>& Errors)
    {
        if (!ExactKeys(Root, Keys({TEXT("schemaVersion"), TEXT("sourceId"), TEXT("primary"),
                TEXT("vfx"), TEXT("audio"), TEXT("sequences")}), TEXT("artifacts"), Errors))
            return false;
        int32 Version = 0;
        FString SourceId;
        if (!ReadInteger(Root, TEXT("schemaVersion"), Version, TEXT("artifacts"), Errors)
            || !ReadString(Root, TEXT("sourceId"), SourceId, TEXT("artifacts"), Errors)
            || Version != 1 || SourceId != TEXT("presentation.artifact_sources.v11"))
            return false;

        const TSharedPtr<FJsonObject>* Primary = nullptr;
        const TSharedPtr<FJsonObject>* Vfx = nullptr;
        const TSharedPtr<FJsonObject>* Audio = nullptr;
        if (!Root->TryGetObjectField(TEXT("primary"), Primary) || Primary == nullptr
            || !ExactKeys(*Primary, Keys({TEXT("meshSourcePath"), TEXT("meshSourceSha1"),
                TEXT("meshClass"), TEXT("materialSourcePath"), TEXT("materialSourceSha1"),
                TEXT("materialClass")}), TEXT("artifacts.primary"), Errors)
            || !Root->TryGetObjectField(TEXT("vfx"), Vfx) || Vfx == nullptr
            || !ExactKeys(*Vfx, Keys({TEXT("systemSourcePath"), TEXT("systemSourceSha1"),
                TEXT("systemClass")}), TEXT("artifacts.vfx"), Errors)
            || !Root->TryGetObjectField(TEXT("audio"), Audio) || Audio == nullptr
            || !ExactKeys(*Audio, Keys({TEXT("waveSourcePath"), TEXT("waveSourceSha1"),
                TEXT("waveClass"), TEXT("cueClass")}), TEXT("artifacts.audio"), Errors))
            return false;

        FString MeshSource, MeshHash, MeshClass, MaterialSource, MaterialHash, MaterialClass;
        FString SystemSource, SystemHash, SystemClass, WaveSource, WaveHash, WaveClass, CueClass;
        if (!ReadString(*Primary, TEXT("meshSourcePath"), MeshSource, TEXT("artifacts.primary"), Errors)
            || !ReadString(*Primary, TEXT("meshSourceSha1"), MeshHash, TEXT("artifacts.primary"), Errors)
            || !ReadString(*Primary, TEXT("meshClass"), MeshClass, TEXT("artifacts.primary"), Errors)
            || !ReadString(*Primary, TEXT("materialSourcePath"), MaterialSource, TEXT("artifacts.primary"), Errors)
            || !ReadString(*Primary, TEXT("materialSourceSha1"), MaterialHash, TEXT("artifacts.primary"), Errors)
            || !ReadString(*Primary, TEXT("materialClass"), MaterialClass, TEXT("artifacts.primary"), Errors)
            || !ReadString(*Vfx, TEXT("systemSourcePath"), SystemSource, TEXT("artifacts.vfx"), Errors)
            || !ReadString(*Vfx, TEXT("systemSourceSha1"), SystemHash, TEXT("artifacts.vfx"), Errors)
            || !ReadString(*Vfx, TEXT("systemClass"), SystemClass, TEXT("artifacts.vfx"), Errors)
            || !ReadString(*Audio, TEXT("waveSourcePath"), WaveSource, TEXT("artifacts.audio"), Errors)
            || !ReadString(*Audio, TEXT("waveSourceSha1"), WaveHash, TEXT("artifacts.audio"), Errors)
            || !ReadString(*Audio, TEXT("waveClass"), WaveClass, TEXT("artifacts.audio"), Errors)
            || !ReadString(*Audio, TEXT("cueClass"), CueClass, TEXT("artifacts.audio"), Errors)
            || MeshClass != TEXT("StaticMesh")
            || MaterialClass != TEXT("MaterialInstanceConstant")
            || SystemClass != TEXT("NiagaraSystem")
            || WaveClass != TEXT("SoundWave") || CueClass != TEXT("SoundCue")
            || !ValidateSourceFile(MeshSource, MeshHash, Errors)
            || !ValidateSourceFile(MaterialSource, MaterialHash, Errors)
            || !ValidateSourceFile(SystemSource, SystemHash, Errors)
            || !ValidateSourceFile(WaveSource, WaveHash, Errors))
            return false;

        TSharedPtr<FJsonObject> PaletteSource;
        TSharedPtr<FJsonObject> NiagaraSource;
        if (!LoadJson(FPaths::ProjectDir() / MaterialSource, PaletteSource, Errors)
            || !ExactKeys(PaletteSource, Keys({TEXT("schemaVersion"), TEXT("palettes")}),
                TEXT("material source"), Errors)
            || !LoadJson(FPaths::ProjectDir() / SystemSource, NiagaraSource, Errors)
            || !ExactKeys(NiagaraSource, Keys({TEXT("schemaVersion"), TEXT("systemTemplate"),
                TEXT("emitter"), TEXT("parameters"), TEXT("bounds"),
                TEXT("warmupSeconds"), TEXT("autoDeactivate")}),
                TEXT("Niagara source"), Errors))
            return false;

        int32 PaletteVersion = 0;
        const TSharedPtr<FJsonObject>* Palettes = nullptr;
        int32 NiagaraVersion = 0;
        FString NiagaraTemplate;
        const TSharedPtr<FJsonObject>* Emitter = nullptr;
        const TSharedPtr<FJsonObject>* Lifecycle = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Parameters = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Bounds = nullptr;
        bool bAutoDeactivate = false;
        double WarmupSeconds = -1.0;
        if (!ReadInteger(PaletteSource, TEXT("schemaVersion"), PaletteVersion,
                TEXT("material source"), Errors)
            || PaletteVersion != 1
            || !PaletteSource->TryGetObjectField(TEXT("palettes"), Palettes)
            || Palettes == nullptr || (*Palettes)->Values.Num() != 6
            || !ReadInteger(NiagaraSource, TEXT("schemaVersion"), NiagaraVersion,
                TEXT("Niagara source"), Errors)
            || NiagaraVersion != 1
            || !ReadString(NiagaraSource, TEXT("systemTemplate"), NiagaraTemplate,
                TEXT("Niagara source"), Errors)
            || NiagaraTemplate.Contains(TEXT("empty"), ESearchCase::IgnoreCase)
            || !NiagaraSource->TryGetObjectField(TEXT("emitter"), Emitter)
            || Emitter == nullptr
            || !ExactKeys(*Emitter, Keys({TEXT("templateAssetPath"),
                TEXT("expectedEmitterName"), TEXT("simulationTarget"),
                TEXT("rendererClass"), TEXT("minimumEmitterCount"),
                TEXT("burstCount"), TEXT("lifetimeSeconds"), TEXT("spriteSize"),
                TEXT("lifecycle")}),
                TEXT("Niagara source.emitter"), Errors)
            || !(*Emitter)->TryGetObjectField(TEXT("lifecycle"), Lifecycle)
            || Lifecycle == nullptr
            || !ExactKeys(*Lifecycle, Keys({TEXT("mode"), TEXT("loopBehavior")}),
                TEXT("Niagara source.emitter.lifecycle"), Errors)
            || (Parameters = ReadArray(NiagaraSource, TEXT("parameters"),
                TEXT("Niagara source"), Errors)) == nullptr || Parameters->Num() < 1
            || (Bounds = ReadArray(NiagaraSource, TEXT("bounds"),
                TEXT("Niagara source"), Errors)) == nullptr || Bounds->Num() != 6
            || !NiagaraSource->TryGetNumberField(TEXT("warmupSeconds"), WarmupSeconds)
            || WarmupSeconds < 0.0
            || !ReadBool(NiagaraSource, TEXT("autoDeactivate"), bAutoDeactivate,
                TEXT("Niagara source"), Errors))
        {
            AddError(Errors, TEXT("Presentation artifact authored content is incomplete."));
            return false;
        }

        FString TemplateAssetPath;
        FString ExpectedEmitterName;
        FString SimulationTarget;
        FString RendererClass;
        int32 MinimumEmitterCount = 0;
        int32 BurstCount = 0;
        double LifetimeSeconds = 0.0;
        const TArray<TSharedPtr<FJsonValue>>* SpriteSize = nullptr;
        FString LifecycleMode;
        FString LoopBehavior;
        if (!ReadString(*Lifecycle, TEXT("mode"), LifecycleMode,
                TEXT("Niagara source.emitter.lifecycle"), Errors)
            || !ReadString(*Lifecycle, TEXT("loopBehavior"), LoopBehavior,
                TEXT("Niagara source.emitter.lifecycle"), Errors)
            || LifecycleMode != TEXT("Self") || LoopBehavior != TEXT("Once"))
        {
            AddError(Errors, TEXT("Niagara source must define a Self/Once emitter lifecycle."));
            return false;
        }
        if (!ReadString(*Emitter, TEXT("templateAssetPath"), TemplateAssetPath,
                TEXT("Niagara source.emitter"), Errors)
            || !TemplateAssetPath.StartsWith(TEXT("/Niagara/"))
            || !ReadString(*Emitter, TEXT("expectedEmitterName"), ExpectedEmitterName,
                TEXT("Niagara source.emitter"), Errors)
            || !ReadString(*Emitter, TEXT("simulationTarget"), SimulationTarget,
                TEXT("Niagara source.emitter"), Errors)
            || SimulationTarget != TEXT("CPUSim")
            || !ReadString(*Emitter, TEXT("rendererClass"), RendererClass,
                TEXT("Niagara source.emitter"), Errors)
            || RendererClass != TEXT("NiagaraSpriteRendererProperties")
            || !ReadInteger(*Emitter, TEXT("minimumEmitterCount"), MinimumEmitterCount,
                TEXT("Niagara source.emitter"), Errors) || MinimumEmitterCount < 1
            || !ReadInteger(*Emitter, TEXT("burstCount"), BurstCount,
                TEXT("Niagara source.emitter"), Errors) || BurstCount < 1
            || !(*Emitter)->TryGetNumberField(TEXT("lifetimeSeconds"), LifetimeSeconds)
            || LifetimeSeconds <= 0.0
            || (SpriteSize = ReadArray(*Emitter, TEXT("spriteSize"),
                TEXT("Niagara source.emitter"), Errors)) == nullptr
            || SpriteSize->Num() != 2)
        {
            AddError(Errors, TEXT("Niagara source must define a non-empty CPU sprite burst."));
            return false;
        }

        const TSet<FString> ExpectedFactions = {TEXT("Synara"), TEXT("Forgeweave"),
            TEXT("EdenCircuit"), TEXT("Universal"), TEXT("Player"), TEXT("Fusion")};
        TSet<FString> ActualFactions;
        for (const auto& Pair : (*Palettes)->Values) ActualFactions.Add(Pair.Key);
        if (ActualFactions != ExpectedFactions)
        {
            AddError(Errors, TEXT("Material source must cover all primary faction palettes."));
            return false;
        }

        TSet<FString> NiagaraParameterNames;
        for (const TSharedPtr<FJsonValue>& ParameterValue : *Parameters)
        {
            const TSharedPtr<FJsonObject> Parameter = ParameterValue.IsValid()
                ? ParameterValue->AsObject() : nullptr;
            FString ParameterName;
            if (!ReadString(Parameter, TEXT("name"), ParameterName,
                    TEXT("Niagara source parameter"), Errors))
                return false;
            NiagaraParameterNames.Add(ParameterName);
        }
        if (NiagaraParameterNames != TSet<FString>({
                TEXT("User.DA.GameplayRadiusMeters"), TEXT("User.DA.BurstCount"),
                TEXT("User.DA.LifetimeSeconds"), TEXT("User.DA.SpriteSize")}))
        {
            AddError(Errors, TEXT("Niagara source parameters are not canonical."));
            return false;
        }

        auto AddRequirement = [&Out](FDAPresentationDefinitionSource& Definition,
            const TCHAR* Role, const FString& Class, const TCHAR* Suffix,
            const FString& Source, const FString& SourceHash,
            const TSharedPtr<FJsonObject>& SourceContent = nullptr)
        {
            FDAPresentationContentManifest::FArtifactRequirement& Requirement =
                Out.ArtifactRequirements.Emplace_GetRef();
            Requirement.DefinitionId = Definition.Id;
            Requirement.DefinitionKind = Definition.Kind;
            Requirement.Role = Role;
            Requirement.AssetClass = Class;
            Requirement.AssetPath = Definition.AssetPath + Suffix;
            Requirement.SourcePath = Source;
            Requirement.SourceSha1 = SourceHash;
            if (SourceContent.IsValid())
            {
                Requirement.SourceContentPayload.Reset();
                CanonicalJson(MakeShared<FJsonValueObject>(SourceContent),
                    Requirement.SourceContentPayload);
                Requirement.SourceContentFingerprint = FingerprintValue(
                    MakeShared<FJsonValueObject>(SourceContent));
            }
            Requirement.GeneratorOwner = TEXT("DAPresentationContent");
            FDAPresentationArtifactBinding& Binding = Definition.Artifacts.Emplace_GetRef();
            Binding.Role = Requirement.Role;
            Binding.AssetClass = Requirement.AssetClass;
            Binding.Asset = FSoftObjectPath(Requirement.AssetPath + TEXT(".")
                + FPackageName::GetLongPackageAssetName(Requirement.AssetPath));
        };
        for (FDAPresentationDefinitionSource& Definition : Out.PrimaryAssets)
        {
            AddRequirement(Definition, TEXT("mesh"), MeshClass, TEXT("_Mesh"), MeshSource, MeshHash);
            const TSharedPtr<FJsonObject>* Palette = nullptr;
            if (!(*Palettes)->TryGetObjectField(Definition.Faction.ToString(), Palette)
                || Palette == nullptr
                || !ExactKeys(*Palette, Keys({TEXT("baseColor"), TEXT("emissiveColor"),
                    TEXT("metallic"), TEXT("roughness")}),
                    TEXT("material source palette"), Errors))
                return false;
            AddRequirement(Definition, TEXT("material"), MaterialClass, TEXT("_Material"),
                MaterialSource, MaterialHash, *Palette);
        }
        for (FDAPresentationDefinitionSource& Definition : Out.CoreVfx)
        {
            TSharedPtr<FJsonObject> Recipe;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
                Definition.RecipePayload);
            if (!FJsonSerializer::Deserialize(Reader, Recipe) || !Recipe.IsValid())
                return false;
            TSharedPtr<FJsonObject> DefinitionNiagara = MakeShared<FJsonObject>();
            DefinitionNiagara->Values = NiagaraSource->Values;
            DefinitionNiagara->SetObjectField(TEXT("definitionParameters"), Recipe);
            AddRequirement(Definition, TEXT("system"), SystemClass, TEXT("_System"),
                SystemSource, SystemHash, DefinitionNiagara);
        }
        auto AddAudioRequirements = [&AddRequirement, &WaveClass, &WaveSource,
            &WaveHash, &CueClass](TArray<FDAPresentationDefinitionSource>& Definitions)
        {
            for (FDAPresentationDefinitionSource& Definition : Definitions)
            {
                AddRequirement(Definition, TEXT("wave"), WaveClass, TEXT("_Wave"), WaveSource, WaveHash);
                AddRequirement(Definition, TEXT("cue"), CueClass, TEXT("_Cue"), WaveSource, WaveHash);
            }
        };
        AddAudioRequirements(Out.Music);
        AddAudioRequirements(Out.Ambient);
        AddAudioRequirements(Out.Sfx);

        const TArray<TSharedPtr<FJsonValue>>* Sequences = ReadArray(
            Root, TEXT("sequences"), TEXT("artifacts"), Errors);
        if (Sequences == nullptr || Sequences->Num() != 1) return false;
        const TSharedPtr<FJsonObject> Sequence = (*Sequences)[0].IsValid()
            ? (*Sequences)[0]->AsObject() : nullptr;
        if (!ExactKeys(Sequence, Keys({TEXT("id"), TEXT("assetPath"), TEXT("assetClass"),
                TEXT("sourcePath"), TEXT("sourceSha1"), TEXT("generatorOwner")}),
                TEXT("artifacts.sequences[0]"), Errors)) return false;
        FDAPresentationContentManifest::FArtifactRequirement& Requirement =
            Out.ArtifactRequirements.Emplace_GetRef();
        FString Id;
        if (!ReadString(Sequence, TEXT("id"), Id, TEXT("artifacts.sequences[0]"), Errors)
            || !ReadString(Sequence, TEXT("assetPath"), Requirement.AssetPath, TEXT("artifacts.sequences[0]"), Errors)
            || !ReadString(Sequence, TEXT("assetClass"), Requirement.AssetClass, TEXT("artifacts.sequences[0]"), Errors)
            || !ReadString(Sequence, TEXT("sourcePath"), Requirement.SourcePath, TEXT("artifacts.sequences[0]"), Errors)
            || !ReadString(Sequence, TEXT("sourceSha1"), Requirement.SourceSha1, TEXT("artifacts.sequences[0]"), Errors)
            || !ReadName(Sequence, TEXT("generatorOwner"), Requirement.GeneratorOwner, TEXT("artifacts.sequences[0]"), Errors)
            || Requirement.AssetClass != TEXT("LevelSequence")
            || !ValidateSourceFile(Requirement.SourcePath, Requirement.SourceSha1, Errors))
            return false;
        Requirement.DefinitionId = FName(*Id);
        Requirement.Role = TEXT("sequence");
        Requirement.bExternalGenerator = true;
        return true;
    }

    bool HasCue(const TSet<FName>& Ids, const FName VfxId, const FName SfxId)
    {
        return Ids.Contains(VfxId) && Ids.Contains(SfxId);
    }
}

FString FDAPresentationContentPipeline::GetCanonicalManifestPath()
{
    return FPaths::ProjectContentDir() / TEXT("DA/Manifests/VerticalSlicePresentation.json");
}

bool FDAPresentationContentPipeline::LoadCanonical(
    FDAPresentationContentManifest& OutManifest, TArray<FText>& OutErrors)
{
    return LoadRootFile(GetCanonicalManifestPath(), OutManifest, OutErrors);
}

bool FDAPresentationContentPipeline::LoadRootFile(const FString& Filename,
    FDAPresentationContentManifest& OutManifest, TArray<FText>& OutErrors)
{
    OutManifest = FDAPresentationContentManifest();
    TSharedPtr<FJsonObject> Root;
    if (!LoadJson(Filename, Root, OutErrors)
        || !ExactKeys(Root, Keys({TEXT("schemaVersion"), TEXT("contentId"), TEXT("fingerprint"),
            TEXT("authority"), TEXT("sources"), TEXT("expectedCounts")}), TEXT("manifest"), OutErrors)
        || !ReadInteger(Root, TEXT("schemaVersion"), OutManifest.SchemaVersion,
            TEXT("manifest"), OutErrors)
        || !ReadName(Root, TEXT("contentId"), OutManifest.ContentId, TEXT("manifest"), OutErrors)
        || !ReadString(Root, TEXT("fingerprint"), OutManifest.Fingerprint,
            TEXT("manifest"), OutErrors)) return false;

    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TSharedPtr<FJsonObject>* SourcesObject = nullptr;
    const TSharedPtr<FJsonObject>* Counts = nullptr;
    if (!Root->TryGetObjectField(TEXT("authority"), Authority) || Authority == nullptr
        || !ExactKeys(*Authority, Keys({TEXT("frozenScope"), TEXT("primaryAssetsAndFactionLanguage"),
            TEXT("gameplayOwnership"), TEXT("cachePolicy")}), TEXT("authority"), OutErrors)
        || !Root->TryGetObjectField(TEXT("sources"), SourcesObject) || SourcesObject == nullptr
        || !ExactKeys(*SourcesObject, Keys({TEXT("buildings"), TEXT("characters"), TEXT("vfx"),
            TEXT("audio"), TEXT("bindings"), TEXT("artifacts")}), TEXT("sources"), OutErrors)
        || !Root->TryGetObjectField(TEXT("expectedCounts"), Counts) || Counts == nullptr
        || !ExactKeys(*Counts, Keys({TEXT("primaryAssets"), TEXT("coreVfx"), TEXT("musicCues"),
            TEXT("ambientLoops"), TEXT("minimumSfxEvents"), TEXT("constructionGrammars"),
            TEXT("minimumGeneratedDefinitions")}), TEXT("expectedCounts"), OutErrors)) return false;

    const TMap<FString, FString> ExpectedSources = {
        {TEXT("buildings"), TEXT("Buildings/Presentation/PrimaryAssets.json")},
        {TEXT("characters"), TEXT("Characters/Presentation/PrimaryAssets.json")},
        {TEXT("vfx"), TEXT("VFX/Presentation/CoreVFX.json")},
        {TEXT("audio"), TEXT("Audio/Presentation/VerticalSliceAudio.json")},
        {TEXT("bindings"), TEXT("Buildings/Presentation/FactionBindings.json")},
        {TEXT("artifacts"), TEXT("DA/Manifests/PresentationArtifactSources.json")}};
    TMap<FString, TSharedPtr<FJsonObject>> SourceDocuments;
    for (const auto& Pair : ExpectedSources)
    {
        FString Relative;
        if (!ReadString(*SourcesObject, *Pair.Key, Relative, TEXT("sources"), OutErrors)
            || Relative != Pair.Value
            || !LoadJson(FPaths::ProjectContentDir() / Relative,
                SourceDocuments.FindOrAdd(Pair.Key), OutErrors)) return false;
    }
    const TMap<FString, int32> ExpectedCounts = {
        {TEXT("primaryAssets"), 50}, {TEXT("coreVfx"), 25}, {TEXT("musicCues"), 9},
        {TEXT("ambientLoops"), 12}, {TEXT("minimumSfxEvents"), 60},
        {TEXT("constructionGrammars"), 3}, {TEXT("minimumGeneratedDefinitions"), 156}};
    for (const auto& Pair : ExpectedCounts)
    {
        int32 Value = 0;
        if (!ReadInteger(*Counts, *Pair.Key, Value, TEXT("expectedCounts"), OutErrors)
            || Value != Pair.Value) return false;
    }

    TArray<int32> Numbers;
    if (!ParsePrimary(SourceDocuments[TEXT("buildings")], false,
            OutManifest.PrimaryAssets, Numbers, OutErrors)
        || !ParsePrimary(SourceDocuments[TEXT("characters")], true,
            OutManifest.PrimaryAssets, Numbers, OutErrors)
        || !ParseVfx(SourceDocuments[TEXT("vfx")], OutManifest.CoreVfx, OutErrors)
        || !ParseAudio(SourceDocuments[TEXT("audio")], OutManifest, OutErrors)
        || !ParseBindings(SourceDocuments[TEXT("bindings")], OutManifest, OutErrors)
        || !ParseArtifactSources(SourceDocuments[TEXT("artifacts")], OutManifest, OutErrors))
        return false;
    OutManifest.PrimaryAssets.Sort([](const FDAPresentationDefinitionSource& Left,
        const FDAPresentationDefinitionSource& Right)
    { return Left.Id.LexicalLess(Right.Id); });
    Numbers.Sort();
    for (int32 Index = 0; Index < Numbers.Num(); ++Index)
        if (Numbers[Index] != Index + 1) return false;

    const FString ComputedFingerprint = CombinedFingerprint(Root, SourceDocuments);
    if (OutManifest.Fingerprint != ComputedFingerprint)
    {
        AddError(OutErrors, TEXT("Presentation source fingerprint is stale."));
        return false;
    }
    return Validate(OutManifest, OutErrors);
}

bool FDAPresentationContentPipeline::Validate(
    const FDAPresentationContentManifest& Manifest, TArray<FText>& OutErrors)
{
    bool bValid = Manifest.SchemaVersion == 1
        && Manifest.ContentId == TEXT("presentation.vertical_slice.v11")
        && Manifest.Fingerprint.Len() == 40
        && Manifest.PrimaryAssets.Num() == 50 && Manifest.CoreVfx.Num() == 25
        && Manifest.Music.Num() == 9 && Manifest.Ambient.Num() == 12
        && Manifest.Sfx.Num() >= 60
        && Manifest.AllDefinitions().Num() >= 156
        && Manifest.ConstructionSourceHook == TEXT("UDAConstructionComponent.OnStageChanged")
        && Manifest.DamageSourceHook == TEXT("UDAStructuralDamageComponent.OnDamageStateChanged")
        && Manifest.CaptureSourceHook == TEXT("UDACaptureComponent.OnCaptureStateChanged")
        && Manifest.DaxtonSourceAuthority == TEXT("FDADaxtonCampaignState")
        && Manifest.AscensionSourceAuthority == TEXT("FDAAscensionPresentationState")
        && Manifest.AscensionCinematicAsset.ToString()
            == TEXT("/Game/Cinematics/CS_ForgeweaveAscension.CS_ForgeweaveAscension")
        && !Manifest.bAscensionGameplayGate && Manifest.bAscensionCinematicMayBeSkipped
        && !Manifest.bAllowsInstantFactionRecolor
        && Manifest.CaptureStages == TArray<FName>({TEXT("original_architecture"),
            TEXT("remove_old_signage"), TEXT("integration_scaffold"),
            TEXT("install_new_signage"), TEXT("integrated_operation")});
    const int32 ExpectedArtifactCount = Manifest.PrimaryAssets.Num() * 2
        + Manifest.CoreVfx.Num()
        + (Manifest.Music.Num() + Manifest.Ambient.Num() + Manifest.Sfx.Num()) * 2 + 1;
    if (Manifest.ArtifactRequirements.Num() != ExpectedArtifactCount)
        bValid = false;
    TSet<FString> ArtifactPaths;
    for (const FDAPresentationContentManifest::FArtifactRequirement& Requirement
        : Manifest.ArtifactRequirements)
    {
        if (Requirement.DefinitionId.IsNone() || Requirement.Role.IsNone()
            || Requirement.AssetClass.IsEmpty()
            || !Requirement.AssetPath.StartsWith(TEXT("/Game/"))
            || !Requirement.SourcePath.StartsWith(TEXT("ContentSource/"))
            || Requirement.SourceSha1.Len() != 40
            || ArtifactPaths.Contains(Requirement.AssetPath))
            bValid = false;
        if ((Requirement.Role == TEXT("material") || Requirement.Role == TEXT("system"))
            && (Requirement.SourceContentPayload.IsEmpty()
                || Requirement.SourceContentFingerprint.Len() != 40))
            bValid = false;
        ArtifactPaths.Add(Requirement.AssetPath);
    }
    TSet<FName> Ids;
    TSet<FString> Paths;
    for (const FDAPresentationDefinitionSource& Definition : Manifest.AllDefinitions())
    {
        const int32 ExpectedBindings = Definition.Kind == EDAPresentationDefinitionKind::CoreVFX
            ? 1 : 2;
        TSet<FName> ArtifactRoles;
        if (Definition.Id.IsNone() || Ids.Contains(Definition.Id)
            || Definition.AssetPath.IsEmpty() || !Definition.AssetPath.StartsWith(TEXT("/Game/"))
            || Paths.Contains(Definition.AssetPath) || Definition.RecipePayload.IsEmpty()
            || Definition.RecipeFingerprint.Len() != 40
            || Definition.bGeneratedCache || Definition.Artifacts.Num() != ExpectedBindings)
            bValid = false;
        for (const FDAPresentationArtifactBinding& Binding : Definition.Artifacts)
        {
            if (Binding.Role.IsNone() || Binding.AssetClass.IsEmpty()
                || !Binding.Asset.IsValid() || ArtifactRoles.Contains(Binding.Role))
                bValid = false;
            ArtifactRoles.Add(Binding.Role);
        }
        Ids.Add(Definition.Id);
        Paths.Add(Definition.AssetPath);
    }

    const TArray<FName> ExpectedFactions = {TEXT("Synara"), TEXT("Forgeweave"), TEXT("EdenCircuit")};
    TSet<FName> GrammarIds;
    TSet<FString> GrammarSignatures;
    if (Manifest.ConstructionGrammars.Num() != 3 || Manifest.DamageLanguages.Num() != 3)
        bValid = false;
    for (int32 Index = 0; Index < Manifest.ConstructionGrammars.Num(); ++Index)
    {
        const FDAConstructionPresentationGrammar& Grammar = Manifest.ConstructionGrammars[Index];
        if (!ExpectedFactions.IsValidIndex(Index) || Grammar.Faction != ExpectedFactions[Index]
            || Grammar.GrammarId.IsNone() || Grammar.Stages.Num() != 5) bValid = false;
        GrammarIds.Add(Grammar.GrammarId);
        FString Signature;
        for (const FDAConstructionPresentationStage& Stage : Grammar.Stages)
        {
            Signature += Stage.GeometryHook.ToString() + TEXT("|");
            if (!HasCue(Ids, Stage.VfxId, Stage.SfxId)) bValid = false;
        }
        GrammarSignatures.Add(Signature);
    }
    if (GrammarIds.Num() != 3 || GrammarSignatures.Num() != 3) bValid = false;
    for (int32 Index = 0; Index < Manifest.DamageLanguages.Num(); ++Index)
    {
        const FDADamagePresentationLanguage& Damage = Manifest.DamageLanguages[Index];
        if (!ExpectedFactions.IsValidIndex(Index) || Damage.Faction != ExpectedFactions[Index]
            || Damage.MaterialHook.IsNone() || !HasCue(Ids, Damage.VfxId, Damage.SfxId))
            bValid = false;
    }
    const TArray<FName> ExpectedDaxton = {TEXT("PhaseOne"), TEXT("PhaseTwo"),
        TEXT("PhaseThree"), TEXT("Resolved")};
    const TArray<FName> ExpectedAscension = {TEXT("SystemsHaltAndReact"),
        TEXT("ForgeRelicEmerges"), TEXT("WorldTransit"),
        TEXT("FounderHallReceivesRelic"), TEXT("Unlocks")};
    if (Manifest.DaxtonStates.Num() != ExpectedDaxton.Num()
        || Manifest.AscensionBeats.Num() != ExpectedAscension.Num()) bValid = false;
    for (int32 Index = 0; Index < Manifest.DaxtonStates.Num(); ++Index)
        if (Manifest.DaxtonStates[Index].State != ExpectedDaxton[Index]
            || !HasCue(Ids, Manifest.DaxtonStates[Index].VfxId,
                Manifest.DaxtonStates[Index].SfxId)) bValid = false;
    for (int32 Index = 0; Index < Manifest.AscensionBeats.Num(); ++Index)
        if (Manifest.AscensionBeats[Index].State != ExpectedAscension[Index]
            || !HasCue(Ids, Manifest.AscensionBeats[Index].VfxId,
                Manifest.AscensionBeats[Index].SfxId)) bValid = false;
    if (!bValid) AddError(OutErrors,
        TEXT("Presentation manifest differs from the frozen v1.1/v0.6 authority."));
    return bValid;
}
