#include "Content/DAContentManifest.h"

#include "Content/DACardDefinition.h"
#include "Content/DAStarterDeckDefinition.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Containers/StringConv.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

#include <initializer_list>

#define LOCTEXT_NAMESPACE "DAContentManifest"

namespace
{
    struct FDAValueFieldDescriptor
    {
        const TCHAR* JsonName;
        EDAAuthoredCardValue Field;
        EJson JsonType;
    };

    const FDAValueFieldDescriptor ValueFields[] = {
        { TEXT("deploymentCapital"), EDAAuthoredCardValue::DeploymentCapital, EJson::Number },
        { TEXT("deploymentInsight"), EDAAuthoredCardValue::DeploymentInsight, EJson::Number },
        { TEXT("deploymentInfluence"), EDAAuthoredCardValue::DeploymentInfluence, EJson::Number },
        { TEXT("craftCapital"), EDAAuthoredCardValue::CraftCapital, EJson::Number },
        { TEXT("craftInsight"), EDAAuthoredCardValue::CraftInsight, EJson::Number },
        { TEXT("craftProductionThroughput"), EDAAuthoredCardValue::CraftProductionThroughput, EJson::Number },
        { TEXT("requiredCraftingFacilityId"), EDAAuthoredCardValue::RequiredCraftingFacilityId, EJson::String },
        { TEXT("maintenanceCapitalPerCycle"), EDAAuthoredCardValue::MaintenanceCapitalPerCycle, EJson::Number },
        { TEXT("baseCapitalPerCycle"), EDAAuthoredCardValue::BaseCapitalPerCycle, EJson::Number },
        { TEXT("baseInsightPerCycle"), EDAAuthoredCardValue::BaseInsightPerCycle, EJson::Number },
        { TEXT("baseInfluencePerCycle"), EDAAuthoredCardValue::BaseInfluencePerCycle, EJson::Number },
        { TEXT("synaraDependencyPerCycle"), EDAAuthoredCardValue::SynaraDependencyPerCycle, EJson::Number },
        { TEXT("forgeweaveResourceHungerPerCycle"), EDAAuthoredCardValue::ForgeweaveResourceHungerPerCycle, EJson::Number },
        { TEXT("workforceRequirementModifier"), EDAAuthoredCardValue::WorkforceRequirementModifier, EJson::Number },
        { TEXT("industrialThroughputModifier"), EDAAuthoredCardValue::IndustrialThroughputModifier, EJson::Number },
        { TEXT("adjacentIndustrialConstructionSpeedModifier"), EDAAuthoredCardValue::AdjacentIndustrialConstructionSpeedModifier, EJson::Number },
        { TEXT("constructionCycles"), EDAAuthoredCardValue::ConstructionCycles, EJson::Number },
        { TEXT("utilityPower"), EDAAuthoredCardValue::UtilityPower, EJson::Number },
        { TEXT("utilityWater"), EDAAuthoredCardValue::UtilityWater, EJson::Number },
        { TEXT("utilityData"), EDAAuthoredCardValue::UtilityData, EJson::Number },
        { TEXT("housingCapacity"), EDAAuthoredCardValue::HousingCapacity, EJson::Number }
    };

    const FDAValueFieldDescriptor* FindValueField(const FString& JsonName)
    {
        for (const FDAValueFieldDescriptor& Descriptor : ValueFields)
        {
            if (JsonName == Descriptor.JsonName)
            {
                return &Descriptor;
            }
        }
        return nullptr;
    }

    TSet<FString> MakeKeySet(std::initializer_list<const TCHAR*> Keys)
    {
        TSet<FString> Result;
        for (const TCHAR* Key : Keys) { Result.Add(Key); }
        return Result;
    }

    bool ValidateObjectKeys(
        const TSharedPtr<FJsonObject>& Object,
        const FString& Context,
        const TSet<FString>& Required,
        const TSet<FString>& Allowed,
        TArray<FText>& Errors)
    {
        if (!Object.IsValid())
        {
            Errors.Add(FText::FromString(FString::Printf(TEXT("%s must be an object."), *Context)));
            return false;
        }
        for (const FString& Key : Required)
        {
            if (!Object->HasField(Key))
            {
                Errors.Add(FText::FromString(FString::Printf(TEXT("%s is missing required key '%s'."), *Context, *Key)));
            }
        }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
        {
            if (!Allowed.Contains(Pair.Key))
            {
                Errors.Add(FText::FromString(FString::Printf(TEXT("%s contains unknown key '%s'."), *Context, *Pair.Key)));
            }
        }
        return true;
    }

    bool ReadStrictValue(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const EJson ExpectedType,
        const FString& Context,
        TSharedPtr<FJsonValue>& OutValue,
        TArray<FText>& Errors)
    {
        OutValue.Reset();
        if (Object.IsValid())
        {
            const TSharedPtr<FJsonValue>* Found = Object->Values.Find(Field);
            if (Found)
            {
                OutValue = *Found;
            }
        }
        if (!OutValue.IsValid() || OutValue->Type != ExpectedType)
        {
            Errors.Add(FText::FromString(FString::Printf(TEXT("%s key '%s' has a missing or wrong JSON type."), *Context, Field)));
            return false;
        }
        return true;
    }

    bool ReadStrictString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Context, FString& Out, TArray<FText>& Errors)
    {
        TSharedPtr<FJsonValue> Value;
        if (!ReadStrictValue(Object, Field, EJson::String, Context, Value, Errors)) { return false; }
        if (Value->AsString().IsEmpty())
        {
            Errors.Add(FText::FromString(FString::Printf(TEXT("%s key '%s' must be non-empty."), *Context, Field)));
            return false;
        }
        Out = Value->AsString();
        return true;
    }

    bool ReadStrictBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Context, bool& Out, TArray<FText>& Errors)
    {
        TSharedPtr<FJsonValue> Value;
        if (!ReadStrictValue(Object, Field, EJson::Boolean, Context, Value, Errors)) { return false; }
        Out = Value->AsBool();
        return true;
    }

    bool ReadStrictNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Context, double& Out, TArray<FText>& Errors)
    {
        TSharedPtr<FJsonValue> Value;
        if (!ReadStrictValue(Object, Field, EJson::Number, Context, Value, Errors)) { return false; }
        Out = Value->AsNumber();
        if (!FMath::IsFinite(Out))
        {
            Errors.Add(FText::FromString(FString::Printf(TEXT("%s key '%s' must be finite."), *Context, Field)));
            return false;
        }
        return true;
    }

    bool ReadStrictInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Context, int32& Out, TArray<FText>& Errors)
    {
        double Number = 0.0;
        if (!ReadStrictNumber(Object, Field, Context, Number, Errors)
            || Number < MIN_int32 || Number > MAX_int32
            || Number != static_cast<double>(static_cast<int32>(Number)))
        {
            Errors.Add(FText::FromString(FString::Printf(TEXT("%s key '%s' must be an int32."), *Context, Field)));
            return false;
        }
        Out = static_cast<int32>(Number);
        return true;
    }

    bool ReadStrictNameArray(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        const FString& Context,
        TArray<FName>& Out,
        TArray<FText>& Errors)
    {
        TSharedPtr<FJsonValue> Value;
        if (!ReadStrictValue(Object, Field, EJson::Array, Context, Value, Errors))
        {
            return false;
        }
        TSet<FName> Seen;
        for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
        {
            if (!Entry.IsValid() || Entry->Type != EJson::String || Entry->AsString().IsEmpty())
            {
                Errors.Add(FText::FromString(FString::Printf(TEXT("%s key '%s' must contain non-empty strings."), *Context, Field)));
                continue;
            }
            const FName Name(*Entry->AsString());
            if (Seen.Contains(Name))
            {
                Errors.Add(FText::FromString(FString::Printf(TEXT("%s key '%s' contains duplicate '%s'."), *Context, Field, *Name.ToString())));
                continue;
            }
            Seen.Add(Name);
            Out.Add(Name);
        }
        Out.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        return true;
    }

    FString FingerprintJson(const FString& Json)
    {
        FString NormalizedJson = Json;
        NormalizedJson.ReplaceInline(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
        NormalizedJson.ReplaceInline(TEXT("\r"), TEXT("\n"), ESearchCase::CaseSensitive);
        const FTCHARToUTF8 Utf8(*NormalizedJson);
        uint8 Hash[FSHA1::DigestSize];
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash));
    }

    TArray<FName> NormalizedNames(const TArray<FName>& Values)
    {
        TArray<FName> Result = Values;
        Result.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        return Result;
    }

    TArray<FName> NormalizedTagNames(const TArray<FGameplayTag>& Values)
    {
        TArray<FName> Result;
        Result.Reserve(Values.Num());
        for (const FGameplayTag Tag : Values)
        {
            Result.Add(Tag.GetTagName());
        }
        Result.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        return Result;
    }

    bool IsLowerSnakeIdentifier(const FString& Value, const bool bAllowDigits)
    {
        if (Value.IsEmpty())
        {
            return false;
        }
        for (const TCHAR Character : Value)
        {
            if (Character != TEXT('_')
                && !FChar::IsLower(Character)
                && !(bAllowDigits && FChar::IsDigit(Character)))
            {
                return false;
            }
        }
        return true;
    }

    bool IsStableDefinitionId(const FName DefinitionId)
    {
        const FString Value = DefinitionId.ToString();
        FString Namespace;
        FString Name;
        return Value.Split(TEXT("."), &Namespace, &Name)
            && !Name.Contains(TEXT("."))
            && IsLowerSnakeIdentifier(Namespace, false)
            && IsLowerSnakeIdentifier(Name, true);
    }

    bool IsDominionGameplayClassPath(const FString& Value)
    {
        constexpr const TCHAR* Prefix = TEXT("/Script/DominionGameplay.");
        if (!Value.StartsWith(Prefix))
        {
            return false;
        }
        const FString ClassName = Value.RightChop(FCString::Strlen(Prefix));
        if (ClassName.IsEmpty())
        {
            return false;
        }
        for (const TCHAR Character : ClassName)
        {
            if (Character != TEXT('_') && !FChar::IsAlnum(Character))
            {
                return false;
            }
        }
        return true;
    }

    bool ParseFaction(const FString& Value, EDAContentFaction& Out)
    {
        if (Value == TEXT("Synara")) { Out = EDAContentFaction::Synara; return true; }
        if (Value == TEXT("Forgeweave")) { Out = EDAContentFaction::Forgeweave; return true; }
        if (Value == TEXT("EdenCircuit")) { Out = EDAContentFaction::EdenCircuit; return true; }
        if (Value == TEXT("Universal")) { Out = EDAContentFaction::Universal; return true; }
        if (Value == TEXT("Fusion")) { Out = EDAContentFaction::Fusion; return true; }
        if (Value == TEXT("Special")) { Out = EDAContentFaction::Special; return true; }
        return false;
    }

    bool ParseCardType(const FString& Value, EDACardType& Out)
    {
        if (Value == TEXT("Residential")) { Out = EDACardType::Residential; return true; }
        if (Value == TEXT("Retail")) { Out = EDACardType::Retail; return true; }
        if (Value == TEXT("Office")) { Out = EDACardType::Office; return true; }
        if (Value == TEXT("Research")) { Out = EDACardType::Research; return true; }
        if (Value == TEXT("Industrial")) { Out = EDACardType::Industrial; return true; }
        if (Value == TEXT("Infrastructure")) { Out = EDACardType::Infrastructure; return true; }
        if (Value == TEXT("Civic")) { Out = EDACardType::Civic; return true; }
        if (Value == TEXT("Defense")) { Out = EDACardType::Defense; return true; }
        if (Value == TEXT("Unit")) { Out = EDACardType::Unit; return true; }
        if (Value == TEXT("Leader")) { Out = EDACardType::Leader; return true; }
        if (Value == TEXT("Wonder")) { Out = EDACardType::Wonder; return true; }
        if (Value == TEXT("Special")) { Out = EDACardType::Special; return true; }
        return false;
    }

    bool ParseRarity(const FString& Value, EDARarity& Out)
    {
        if (Value == TEXT("Common")) { Out = EDARarity::Common; return true; }
        if (Value == TEXT("Specialized")) { Out = EDARarity::Specialized; return true; }
        if (Value == TEXT("Elite")) { Out = EDARarity::Elite; return true; }
        if (Value == TEXT("Legendary")) { Out = EDARarity::Legendary; return true; }
        if (Value == TEXT("Mythic")) { Out = EDARarity::Mythic; return true; }
        if (Value == TEXT("Wonder")) { Out = EDARarity::Wonder; return true; }
        if (Value == TEXT("Leader")) { Out = EDARarity::Leader; return true; }
        if (Value == TEXT("Dominion")) { Out = EDARarity::Dominion; return true; }
        return false;
    }

    FString FactionStableName(const EDAContentFaction Faction)
    {
        switch (Faction)
        {
        case EDAContentFaction::Synara: return TEXT("synara");
        case EDAContentFaction::Forgeweave: return TEXT("forgeweave");
        case EDAContentFaction::EdenCircuit: return TEXT("eden_circuit");
        case EDAContentFaction::Universal: return TEXT("universal");
        case EDAContentFaction::Fusion: return TEXT("fusion");
        case EDAContentFaction::Special: default: return TEXT("special");
        }
    }

    FGuid MakeStarterInstanceId(const FName DefinitionId, const int32 CopyIndex)
    {
        const FString Seed = FString::Printf(TEXT("DA.SynaraStarter60|%s|%d"), *DefinitionId.ToString(), CopyIndex);
        return FGuid(
            FCrc::StrCrc32(*(Seed + TEXT("|A"))),
            FCrc::StrCrc32(*(Seed + TEXT("|B"))),
            FCrc::StrCrc32(*(Seed + TEXT("|C"))),
            FCrc::StrCrc32(*(Seed + TEXT("|D"))));
    }

}

UDA_CardDefinition* FDABuiltManifestContent::FindDefinition(const FName DefinitionId) const
{
    UDA_CardDefinition* const* Found = Definitions.FindByPredicate([DefinitionId](const UDA_CardDefinition* Definition)
    {
        return Definition && Definition->DefinitionId == DefinitionId;
    });
    return Found ? *Found : nullptr;
}

FString FDAContentManifestPipeline::GetCanonicalManifestPath()
{
    return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DA/Manifests/VerticalSliceContent.json"));
}

bool FDAContentManifestPipeline::LoadCanonical(FDAVerticalSliceContentManifest& OutManifest, TArray<FText>& Errors)
{
    return LoadFile(GetCanonicalManifestPath(), OutManifest, Errors);
}

bool FDAContentManifestPipeline::LoadFile(
    const FString& Filename,
    FDAVerticalSliceContentManifest& OutManifest,
    TArray<FText>& Errors)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Filename))
    {
        Errors.Add(FText::Format(LOCTEXT("UnreadableManifest", "Could not read content manifest '{0}'."), FText::FromString(Filename)));
        return false;
    }
    return ParseJson(Json, OutManifest, Errors);
}

bool FDAContentManifestPipeline::ParseJson(
    const FString& Json,
    FDAVerticalSliceContentManifest& OutManifest,
    TArray<FText>& Errors)
{
    const int32 InitialErrorCount = Errors.Num();
    OutManifest = FDAVerticalSliceContentManifest();
    OutManifest.SourceFingerprint = FingerprintJson(Json);

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        Errors.Add(LOCTEXT("MalformedJson", "Vertical-slice content manifest is not valid JSON."));
        return false;
    }

    const TSet<FString> RootKeys = MakeKeySet({ TEXT("schemaVersion"), TEXT("authority"), TEXT("expectedCounts"), TEXT("definitions"), TEXT("starterDeck") });
    ValidateObjectKeys(Root, TEXT("manifest root"), RootKeys, RootKeys, Errors);
    ReadStrictInteger(Root, TEXT("schemaVersion"), TEXT("manifest root"), OutManifest.SchemaVersion, Errors);

    TSharedPtr<FJsonValue> AuthorityValue;
    if (ReadStrictValue(Root, TEXT("authority"), EJson::Object, TEXT("manifest root"), AuthorityValue, Errors))
    {
        const TSharedPtr<FJsonObject> Authority = AuthorityValue->AsObject();
        const TSet<FString> AuthorityKeys = MakeKeySet({ TEXT("definitionNamesAndCounts"), TEXT("starterDeck"), TEXT("gameplayValues"), TEXT("prefabPolicy") });
        ValidateObjectKeys(Authority, TEXT("authority"), AuthorityKeys, AuthorityKeys, Errors);
        for (const FString& Key : AuthorityKeys)
        {
            FString Ignored;
            ReadStrictString(Authority, *Key, TEXT("authority"), Ignored, Errors);
        }
    }

    TSharedPtr<FJsonValue> CountsValue;
    if (ReadStrictValue(Root, TEXT("expectedCounts"), EJson::Object, TEXT("manifest root"), CountsValue, Errors))
    {
        const TSharedPtr<FJsonObject> Counts = CountsValue->AsObject();
        const TMap<FString, int32> Expected = {
            { TEXT("Synara"), 15 }, { TEXT("Forgeweave"), 15 }, { TEXT("EdenCircuit"), 15 },
            { TEXT("Universal"), 17 }, { TEXT("Fusion"), 1 }, { TEXT("Special"), 1 },
            { TEXT("total"), 64 }, { TEXT("starterInstances"), 60 }
        };
        TSet<FString> CountKeys;
        for (const TPair<FString, int32>& Pair : Expected) { CountKeys.Add(Pair.Key); }
        ValidateObjectKeys(Counts, TEXT("expectedCounts"), CountKeys, CountKeys, Errors);
        for (const TPair<FString, int32>& Pair : Expected)
        {
            int32 Actual = 0;
            if (ReadStrictInteger(Counts, *Pair.Key, TEXT("expectedCounts"), Actual, Errors) && Actual != Pair.Value)
            {
                Errors.Add(FText::FromString(FString::Printf(TEXT("expectedCounts.%s must equal %d."), *Pair.Key, Pair.Value)));
            }
        }
    }

    TSharedPtr<FJsonValue> DefinitionsValue;
    if (!ReadStrictValue(Root, TEXT("definitions"), EJson::Array, TEXT("manifest root"), DefinitionsValue, Errors))
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>& Definitions = DefinitionsValue->AsArray();
    for (int32 DefinitionIndex = 0; DefinitionIndex < Definitions.Num(); ++DefinitionIndex)
    {
        const TSharedPtr<FJsonValue>& Value = Definitions[DefinitionIndex];
        const FString Context = FString::Printf(TEXT("definitions[%d]"), DefinitionIndex);
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            Errors.Add(FText::FromString(Context + TEXT(" must be an object.")));
            continue;
        }
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        TSet<FString> RequiredKeys = MakeKeySet({
            TEXT("id"), TEXT("displayName"), TEXT("faction"), TEXT("cardType"), TEXT("rarity"),
            TEXT("placeable"), TEXT("footprint"), TEXT("footprintAuthority"), TEXT("worldPrefab"),
            TEXT("randomCacheEligible"), TEXT("authoredValues"), TEXT("tags"), TEXT("upgradeBranchIds")
        });
        TSet<FString> AllowedKeys = RequiredKeys;
        AllowedKeys.Add(TEXT("valueAuthority"));
        AllowedKeys.Add(TEXT("combat"));
        for (const FDAValueFieldDescriptor& Descriptor : ValueFields) { AllowedKeys.Add(Descriptor.JsonName); }
        ValidateObjectKeys(Object, Context, RequiredKeys, AllowedKeys, Errors);

        FDAManifestCardDefinition Definition;
        FString Id;
        FString Faction;
        FString CardType;
        FString Rarity;
        ReadStrictString(Object, TEXT("id"), Context, Id, Errors);
        ReadStrictString(Object, TEXT("displayName"), Context, Definition.DisplayName, Errors);
        ReadStrictString(Object, TEXT("faction"), Context, Faction, Errors);
        ReadStrictString(Object, TEXT("cardType"), Context, CardType, Errors);
        ReadStrictString(Object, TEXT("rarity"), Context, Rarity, Errors);
        Definition.DefinitionId = FName(*Id);

        if (!ParseFaction(Faction, Definition.Faction))
        {
            Errors.Add(FText::Format(LOCTEXT("InvalidFaction", "Definition '{0}' has unknown faction '{1}'."), FText::FromString(Id), FText::FromString(Faction)));
        }
        if (!ParseCardType(CardType, Definition.CardType))
        {
            Errors.Add(FText::Format(LOCTEXT("InvalidCardType", "Definition '{0}' has unknown card type '{1}'."), FText::FromString(Id), FText::FromString(CardType)));
        }
        if (!ParseRarity(Rarity, Definition.Rarity))
        {
            Errors.Add(FText::Format(LOCTEXT("InvalidRarity", "Definition '{0}' has unknown rarity '{1}'."), FText::FromString(Id), FText::FromString(Rarity)));
        }

        ReadStrictBool(Object, TEXT("placeable"), Context, Definition.bPlaceable, Errors);
        ReadStrictString(Object, TEXT("worldPrefab"), Context, Definition.WorldPrefabClassPath, Errors);
        ReadStrictBool(Object, TEXT("randomCacheEligible"), Context, Definition.bRandomCacheEligible, Errors);
        FString IgnoredAuthority;
        ReadStrictString(Object, TEXT("footprintAuthority"), Context, IgnoredAuthority, Errors);
        if (Object->HasField(TEXT("valueAuthority")))
        {
            ReadStrictString(Object, TEXT("valueAuthority"), Context, IgnoredAuthority, Errors);
        }

        Definition.bTagsProvided = ReadStrictNameArray(Object, TEXT("tags"), Context, Definition.Tags, Errors);
        Definition.bUpgradeBranchIdsProvided = ReadStrictNameArray(
            Object, TEXT("upgradeBranchIds"), Context, Definition.UpgradeBranchIds, Errors);

        if (Object->HasField(TEXT("combat")))
        {
            Definition.bCombatProvided = true;
            const int32 CombatErrorCount = Errors.Num();
            TSharedPtr<FJsonValue> CombatValue;
            if (ReadStrictValue(Object, TEXT("combat"), EJson::Object, Context, CombatValue, Errors))
            {
                const TSharedPtr<FJsonObject> CombatObject = CombatValue->AsObject();
                const FString CombatContext = Context + TEXT(".combat");
                const TSet<FString> CombatKeys = MakeKeySet({
                    TEXT("structuralIntegrity"), TEXT("armor"), TEXT("cyberIntegrity"), TEXT("capturable")
                });
                ValidateObjectKeys(CombatObject, CombatContext, CombatKeys, CombatKeys, Errors);
                double StructuralIntegrity = 0.0;
                double Armor = 0.0;
                double CyberIntegrity = 0.0;
                ReadStrictNumber(CombatObject, TEXT("structuralIntegrity"), CombatContext, StructuralIntegrity, Errors);
                ReadStrictNumber(CombatObject, TEXT("armor"), CombatContext, Armor, Errors);
                ReadStrictNumber(CombatObject, TEXT("cyberIntegrity"), CombatContext, CyberIntegrity, Errors);
                ReadStrictBool(CombatObject, TEXT("capturable"), CombatContext, Definition.Combat.bCapturable, Errors);
                if (StructuralIntegrity < -MAX_flt || StructuralIntegrity > MAX_flt
                    || Armor < -MAX_flt || Armor > MAX_flt
                    || CyberIntegrity < -MAX_flt || CyberIntegrity > MAX_flt)
                {
                    Errors.Add(FText::FromString(CombatContext + TEXT(" numeric values must fit in finite floats.")));
                }
                Definition.Combat.StructuralIntegrity = static_cast<float>(StructuralIntegrity);
                Definition.Combat.Armor = static_cast<float>(Armor);
                Definition.Combat.CyberIntegrity = static_cast<float>(CyberIntegrity);
            }
            Definition.bHasAuthoredCombat = Errors.Num() == CombatErrorCount;
        }

        TSharedPtr<FJsonValue> FootprintValue;
        if (ReadStrictValue(Object, TEXT("footprint"), EJson::Array, Context, FootprintValue, Errors))
        {
            const TArray<TSharedPtr<FJsonValue>>& Footprint = FootprintValue->AsArray();
            if (Footprint.Num() != 2 || !Footprint[0].IsValid() || !Footprint[1].IsValid()
                || Footprint[0]->Type != EJson::Number || Footprint[1]->Type != EJson::Number)
            {
                Errors.Add(FText::FromString(Context + TEXT(" footprint must contain exactly two integers.")));
            }
            else
            {
                const double X = Footprint[0]->AsNumber();
                const double Y = Footprint[1]->AsNumber();
                if (!FMath::IsFinite(X) || !FMath::IsFinite(Y)
                    || X < MIN_int32 || X > MAX_int32 || Y < MIN_int32 || Y > MAX_int32
                    || X != static_cast<double>(static_cast<int32>(X))
                    || Y != static_cast<double>(static_cast<int32>(Y)))
                {
                    Errors.Add(FText::FromString(Context + TEXT(" footprint must contain exactly two integers.")));
                }
                else
                {
                    Definition.Footprint = FIntPoint(static_cast<int32>(X), static_cast<int32>(Y));
                }
            }
        }

        TSharedPtr<FJsonValue> AuthoredValues;
        if (ReadStrictValue(Object, TEXT("authoredValues"), EJson::Array, Context, AuthoredValues, Errors))
        {
            for (const TSharedPtr<FJsonValue>& AuthoredValue : AuthoredValues->AsArray())
            {
                if (!AuthoredValue.IsValid() || AuthoredValue->Type != EJson::String)
                {
                    Errors.Add(FText::FromString(Context + TEXT(" authoredValues entries must be strings.")));
                    continue;
                }
                const FString Name = AuthoredValue->AsString();
                const FDAValueFieldDescriptor* Descriptor = FindValueField(Name);
                if (!Descriptor || (Definition.AuthoredValueMask & DAAuthoredValueBit(Descriptor->Field)) != 0)
                {
                    Errors.Add(FText::FromString(FString::Printf(TEXT("%s authoredValues contains unknown or duplicate '%s'."), *Context, *Name)));
                    continue;
                }
                Definition.AuthoredValueMask |= DAAuthoredValueBit(Descriptor->Field);
            }
        }

        for (const FDAValueFieldDescriptor& Descriptor : ValueFields)
        {
            if (!Object->HasField(Descriptor.JsonName)) { continue; }
            TSharedPtr<FJsonValue> FieldValue;
            if (!ReadStrictValue(Object, Descriptor.JsonName, Descriptor.JsonType, Context, FieldValue, Errors)) { continue; }
            Definition.ProvidedValueMask |= DAAuthoredValueBit(Descriptor.Field);
            int32 IntegerValue = 0;
            double NumberValue = 0.0;
            switch (Descriptor.Field)
            {
            case EDAAuthoredCardValue::RequiredCraftingFacilityId:
            {
                FString FacilityId;
                if (ReadStrictString(Object, Descriptor.JsonName, Context, FacilityId, Errors))
                {
                    Definition.RequiredCraftingFacilityId = FName(*FacilityId);
                }
                break;
            }
            case EDAAuthoredCardValue::MaintenanceCapitalPerCycle:
            case EDAAuthoredCardValue::BaseCapitalPerCycle:
            case EDAAuthoredCardValue::BaseInsightPerCycle:
            case EDAAuthoredCardValue::BaseInfluencePerCycle:
            case EDAAuthoredCardValue::SynaraDependencyPerCycle:
            case EDAAuthoredCardValue::ForgeweaveResourceHungerPerCycle:
            case EDAAuthoredCardValue::WorkforceRequirementModifier:
            case EDAAuthoredCardValue::IndustrialThroughputModifier:
            case EDAAuthoredCardValue::AdjacentIndustrialConstructionSpeedModifier:
                if (!ReadStrictNumber(Object, Descriptor.JsonName, Context, NumberValue, Errors)
                    || NumberValue < -MAX_flt || NumberValue > MAX_flt)
                {
                    Errors.Add(FText::FromString(FString::Printf(TEXT("%s key '%s' must fit in a finite float."), *Context, Descriptor.JsonName)));
                    break;
                }
                switch (Descriptor.Field)
                {
                case EDAAuthoredCardValue::MaintenanceCapitalPerCycle: Definition.MaintenanceCapitalPerCycle = NumberValue; break;
                case EDAAuthoredCardValue::BaseCapitalPerCycle: Definition.BaseCapitalPerCycle = NumberValue; break;
                case EDAAuthoredCardValue::BaseInsightPerCycle: Definition.BaseInsightPerCycle = NumberValue; break;
                case EDAAuthoredCardValue::BaseInfluencePerCycle: Definition.BaseInfluencePerCycle = NumberValue; break;
                case EDAAuthoredCardValue::SynaraDependencyPerCycle: Definition.SynaraDependencyPerCycle = NumberValue; break;
                case EDAAuthoredCardValue::ForgeweaveResourceHungerPerCycle: Definition.ForgeweaveResourceHungerPerCycle = NumberValue; break;
                case EDAAuthoredCardValue::WorkforceRequirementModifier: Definition.WorkforceRequirementModifier = NumberValue; break;
                case EDAAuthoredCardValue::IndustrialThroughputModifier: Definition.IndustrialThroughputModifier = NumberValue; break;
                case EDAAuthoredCardValue::AdjacentIndustrialConstructionSpeedModifier: Definition.AdjacentIndustrialConstructionSpeedModifier = NumberValue; break;
                default: break;
                }
                break;
            default:
                if (!ReadStrictInteger(Object, Descriptor.JsonName, Context, IntegerValue, Errors)) { break; }
                switch (Descriptor.Field)
                {
                case EDAAuthoredCardValue::DeploymentCapital: Definition.DeploymentCapital = IntegerValue; break;
                case EDAAuthoredCardValue::DeploymentInsight: Definition.DeploymentInsight = IntegerValue; break;
                case EDAAuthoredCardValue::DeploymentInfluence: Definition.DeploymentInfluence = IntegerValue; break;
                case EDAAuthoredCardValue::CraftCapital: Definition.CraftCapital = IntegerValue; break;
                case EDAAuthoredCardValue::CraftInsight: Definition.CraftInsight = IntegerValue; break;
                case EDAAuthoredCardValue::CraftProductionThroughput: Definition.CraftProductionThroughput = IntegerValue; break;
                case EDAAuthoredCardValue::ConstructionCycles: Definition.ConstructionCycles = IntegerValue; break;
                case EDAAuthoredCardValue::UtilityPower: Definition.UtilityPower = IntegerValue; break;
                case EDAAuthoredCardValue::UtilityWater: Definition.UtilityWater = IntegerValue; break;
                case EDAAuthoredCardValue::UtilityData: Definition.UtilityData = IntegerValue; break;
                case EDAAuthoredCardValue::HousingCapacity: Definition.HousingCapacity = IntegerValue; break;
                default: break;
                }
                break;
            }
        }
        OutManifest.Definitions.Add(MoveTemp(Definition));
    }

    TSharedPtr<FJsonValue> StarterDeckValue;
    if (!ReadStrictValue(Root, TEXT("starterDeck"), EJson::Array, TEXT("manifest root"), StarterDeckValue, Errors))
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>& StarterDeck = StarterDeckValue->AsArray();
    for (int32 DeckIndex = 0; DeckIndex < StarterDeck.Num(); ++DeckIndex)
    {
        const TSharedPtr<FJsonValue>& Value = StarterDeck[DeckIndex];
        const FString Context = FString::Printf(TEXT("starterDeck[%d]"), DeckIndex);
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            Errors.Add(FText::FromString(Context + TEXT(" must be an object.")));
            continue;
        }
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        const TSet<FString> DeckKeys = MakeKeySet({ TEXT("definitionId"), TEXT("quantity") });
        ValidateObjectKeys(Object, Context, DeckKeys, DeckKeys, Errors);
        FString Id;
        FDAManifestDeckEntry Entry;
        ReadStrictString(Object, TEXT("definitionId"), Context, Id, Errors);
        Entry.DefinitionId = FName(*Id);
        ReadStrictInteger(Object, TEXT("quantity"), Context, Entry.Quantity, Errors);
        OutManifest.StarterDeck.Add(Entry);
    }

    if (Errors.Num() != InitialErrorCount)
    {
        return false;
    }
    return ValidateManifest(OutManifest, Errors);
}

bool FDAContentManifestPipeline::ValidateManifest(
    const FDAVerticalSliceContentManifest& Manifest,
    TArray<FText>& Errors)
{
    const int32 InitialErrorCount = Errors.Num();
    if (Manifest.SchemaVersion != 1)
    {
        Errors.Add(FText::Format(LOCTEXT("SchemaVersion", "Expected content manifest schema 1, found {0}."), Manifest.SchemaVersion));
    }
    if (Manifest.SourceFingerprint.IsEmpty())
    {
        Errors.Add(LOCTEXT("MissingFingerprint", "Manifest source fingerprint is required."));
    }
    if (Manifest.Definitions.Num() != 64)
    {
        Errors.Add(FText::Format(LOCTEXT("DefinitionCount", "Vertical slice requires exactly 64 definitions, found {0}."), Manifest.Definitions.Num()));
    }

    const TMap<EDAContentFaction, int32> RequiredCounts = {
        { EDAContentFaction::Synara, 15 }, { EDAContentFaction::Forgeweave, 15 },
        { EDAContentFaction::EdenCircuit, 15 }, { EDAContentFaction::Universal, 17 },
        { EDAContentFaction::Fusion, 1 }, { EDAContentFaction::Special, 1 }
    };
    TMap<EDAContentFaction, int32> ActualCounts;
    TSet<FName> DefinitionIds;
    TSet<FString> DisplayNames;
    for (const FDAManifestCardDefinition& Definition : Manifest.Definitions)
    {
        ++ActualCounts.FindOrAdd(Definition.Faction);
        if (!IsStableDefinitionId(Definition.DefinitionId) || DefinitionIds.Contains(Definition.DefinitionId))
        {
            Errors.Add(FText::Format(LOCTEXT("DuplicateDefinitionId", "Invalid or duplicate manifest definition ID '{0}'."), FText::FromName(Definition.DefinitionId)));
        }
        DefinitionIds.Add(Definition.DefinitionId);
        if (Definition.DisplayName.IsEmpty() || DisplayNames.Contains(Definition.DisplayName))
        {
            Errors.Add(FText::Format(LOCTEXT("DuplicateDisplayName", "Missing or duplicate manifest display name '{0}'."), FText::FromString(Definition.DisplayName)));
        }
        DisplayNames.Add(Definition.DisplayName);
        if (!Definition.bTagsProvided || !Definition.bUpgradeBranchIdsProvided)
        {
            Errors.Add(FText::Format(
                LOCTEXT("MissingExplicitArrays", "Definition '{0}' must explicitly provide tags and upgradeBranchIds arrays."),
                FText::FromName(Definition.DefinitionId)));
        }
        TSet<FName> SeenTags;
        for (const FName Tag : Definition.Tags)
        {
            if (Tag.IsNone() || SeenTags.Contains(Tag))
            {
                Errors.Add(FText::Format(
                    LOCTEXT("InvalidManifestTag", "Definition '{0}' has an empty or duplicate gameplay tag."),
                    FText::FromName(Definition.DefinitionId)));
            }
            SeenTags.Add(Tag);
        }
        TSet<FName> SeenUpgradeBranchIds;
        for (const FName UpgradeBranchId : Definition.UpgradeBranchIds)
        {
            if (UpgradeBranchId.IsNone() || SeenUpgradeBranchIds.Contains(UpgradeBranchId))
            {
                Errors.Add(FText::Format(
                    LOCTEXT("InvalidManifestUpgrade", "Definition '{0}' has an empty or duplicate upgrade branch ID."),
                    FText::FromName(Definition.DefinitionId)));
            }
            SeenUpgradeBranchIds.Add(UpgradeBranchId);
        }
        if (Definition.bHasAuthoredCombat != Definition.bCombatProvided)
        {
            Errors.Add(FText::Format(
                LOCTEXT("CombatPresenceMismatch", "Definition '{0}' has combat data without valid authored presence."),
                FText::FromName(Definition.DefinitionId)));
        }
        const bool bHasRawCombat = Definition.Combat.StructuralIntegrity != 0.f
            || Definition.Combat.Armor != 0.f
            || Definition.Combat.CyberIntegrity != 0.f
            || Definition.Combat.bCapturable;
        if (bHasRawCombat && !Definition.bCombatProvided)
        {
            Errors.Add(FText::Format(
                LOCTEXT("UnprovidedCombat", "Definition '{0}' has raw combat data without a combat property."),
                FText::FromName(Definition.DefinitionId)));
        }
        if ((Definition.bHasAuthoredCombat || bHasRawCombat)
            && (!FMath::IsFinite(Definition.Combat.StructuralIntegrity)
                || !FMath::IsFinite(Definition.Combat.Armor)
                || !FMath::IsFinite(Definition.Combat.CyberIntegrity)
                || Definition.Combat.StructuralIntegrity < 0.f
                || Definition.Combat.Armor < 0.f
                || Definition.Combat.CyberIntegrity < 0.f))
        {
            Errors.Add(FText::Format(
                LOCTEXT("InvalidManifestCombat", "Definition '{0}' has invalid authored combat values."),
                FText::FromName(Definition.DefinitionId)));
        }
        if (Definition.Footprint.X < 0 || Definition.Footprint.Y < 0)
        {
            Errors.Add(FText::Format(LOCTEXT("NegativeFootprint", "Definition '{0}' has a negative footprint."), FText::FromName(Definition.DefinitionId)));
        }
        if (!IsDominionGameplayClassPath(Definition.WorldPrefabClassPath))
        {
            Errors.Add(FText::Format(LOCTEXT("InvalidPrefabPath", "Definition '{0}' has an invalid DominionGameplay prefab class path."), FText::FromName(Definition.DefinitionId)));
        }
        if (Definition.bPlaceable && (Definition.Footprint.X <= 0 || Definition.Footprint.Y <= 0))
        {
            Errors.Add(FText::Format(LOCTEXT("PlaceableRepresentation", "Placeable definition '{0}' needs a positive footprint and prefab class path."), FText::FromName(Definition.DefinitionId)));
        }
        if (Definition.AuthoredValueMask != Definition.ProvidedValueMask)
        {
            Errors.Add(FText::Format(
                LOCTEXT("AuthoredPresenceMismatch", "Definition '{0}' supplied gameplay values do not exactly match authoredValues."),
                FText::FromName(Definition.DefinitionId)));
        }
        constexpr uint64 KnownValueMask = (uint64(1) << (static_cast<uint8>(EDAAuthoredCardValue::HousingCapacity) + 1)) - 1;
        if (((Definition.AuthoredValueMask | Definition.ProvidedValueMask) & ~KnownValueMask) != 0)
        {
            Errors.Add(FText::Format(LOCTEXT("UnknownValueMask", "Definition '{0}' has unknown gameplay-value presence bits."), FText::FromName(Definition.DefinitionId)));
        }
        const auto RejectUnprovidedRaw = [&Errors, &Definition](const EDAAuthoredCardValue Field, const bool bHasRaw, const TCHAR* Name)
        {
            if (bHasRaw && (Definition.ProvidedValueMask & DAAuthoredValueBit(Field)) == 0)
            {
                Errors.Add(FText::Format(
                    LOCTEXT("UnprovidedRaw", "Definition '{0}' has raw value '{1}' without JSON property presence."),
                    FText::FromName(Definition.DefinitionId),
                    FText::FromString(Name)));
            }
        };
        RejectUnprovidedRaw(EDAAuthoredCardValue::DeploymentCapital, Definition.DeploymentCapital != 0, TEXT("deploymentCapital"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::DeploymentInsight, Definition.DeploymentInsight != 0, TEXT("deploymentInsight"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::DeploymentInfluence, Definition.DeploymentInfluence != 0, TEXT("deploymentInfluence"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::CraftCapital, Definition.CraftCapital != 0, TEXT("craftCapital"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::CraftInsight, Definition.CraftInsight != 0, TEXT("craftInsight"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::CraftProductionThroughput, Definition.CraftProductionThroughput != 0, TEXT("craftProductionThroughput"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::RequiredCraftingFacilityId, !Definition.RequiredCraftingFacilityId.IsNone(), TEXT("requiredCraftingFacilityId"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::MaintenanceCapitalPerCycle, Definition.MaintenanceCapitalPerCycle != 0.f, TEXT("maintenanceCapitalPerCycle"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::BaseCapitalPerCycle, Definition.BaseCapitalPerCycle != 0.f, TEXT("baseCapitalPerCycle"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::BaseInsightPerCycle, Definition.BaseInsightPerCycle != 0.f, TEXT("baseInsightPerCycle"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::BaseInfluencePerCycle, Definition.BaseInfluencePerCycle != 0.f, TEXT("baseInfluencePerCycle"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::SynaraDependencyPerCycle, Definition.SynaraDependencyPerCycle != 0.f, TEXT("synaraDependencyPerCycle"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::ForgeweaveResourceHungerPerCycle, Definition.ForgeweaveResourceHungerPerCycle != 0.f, TEXT("forgeweaveResourceHungerPerCycle"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::WorkforceRequirementModifier, Definition.WorkforceRequirementModifier != 0.f, TEXT("workforceRequirementModifier"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::IndustrialThroughputModifier, Definition.IndustrialThroughputModifier != 0.f, TEXT("industrialThroughputModifier"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::AdjacentIndustrialConstructionSpeedModifier, Definition.AdjacentIndustrialConstructionSpeedModifier != 0.f, TEXT("adjacentIndustrialConstructionSpeedModifier"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::ConstructionCycles, Definition.ConstructionCycles != 0, TEXT("constructionCycles"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::UtilityPower, Definition.UtilityPower != 0, TEXT("utilityPower"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::UtilityWater, Definition.UtilityWater != 0, TEXT("utilityWater"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::UtilityData, Definition.UtilityData != 0, TEXT("utilityData"));
        RejectUnprovidedRaw(EDAAuthoredCardValue::HousingCapacity, Definition.HousingCapacity != 0, TEXT("housingCapacity"));
        if (!FMath::IsFinite(Definition.MaintenanceCapitalPerCycle)
            || !FMath::IsFinite(Definition.BaseCapitalPerCycle)
            || !FMath::IsFinite(Definition.BaseInsightPerCycle)
            || !FMath::IsFinite(Definition.BaseInfluencePerCycle)
            || !FMath::IsFinite(Definition.SynaraDependencyPerCycle)
            || !FMath::IsFinite(Definition.ForgeweaveResourceHungerPerCycle)
            || !FMath::IsFinite(Definition.WorkforceRequirementModifier)
            || !FMath::IsFinite(Definition.IndustrialThroughputModifier)
            || !FMath::IsFinite(Definition.AdjacentIndustrialConstructionSpeedModifier)
            || Definition.DeploymentCapital < 0 || Definition.DeploymentInsight < 0 || Definition.DeploymentInfluence < 0
            || Definition.CraftCapital < 0 || Definition.CraftInsight < 0
            || Definition.CraftProductionThroughput < 0 || Definition.MaintenanceCapitalPerCycle < 0.f
            || Definition.BaseCapitalPerCycle < 0.f || Definition.BaseInsightPerCycle < 0.f
            || Definition.BaseInfluencePerCycle < 0.f || Definition.UtilityPower < 0
            || Definition.UtilityWater < 0 || Definition.UtilityData < 0 || Definition.HousingCapacity < 0)
        {
            Errors.Add(FText::Format(LOCTEXT("InvalidCost", "Definition '{0}' has an invalid negative cost."), FText::FromName(Definition.DefinitionId)));
        }
        if ((Definition.ProvidedValueMask & DAAuthoredValueBit(EDAAuthoredCardValue::ConstructionCycles)) != 0
            && Definition.ConstructionCycles <= 0)
        {
            Errors.Add(FText::Format(LOCTEXT("InvalidCycles", "Definition '{0}' has invalid authored construction cycles."), FText::FromName(Definition.DefinitionId)));
        }
        if ((Definition.AuthoredValueMask & DAAuthoredValueBit(EDAAuthoredCardValue::RequiredCraftingFacilityId)) != 0
            && !IsStableDefinitionId(Definition.RequiredCraftingFacilityId))
        {
            Errors.Add(FText::Format(LOCTEXT("InvalidCraftFacility", "Definition '{0}' has an invalid authored crafting facility ID."), FText::FromName(Definition.DefinitionId)));
        }
        if (Definition.Rarity == EDARarity::Dominion && Definition.bRandomCacheEligible)
        {
            Errors.Add(FText::Format(LOCTEXT("DominionCache", "Dominion definition '{0}' cannot be cache eligible."), FText::FromName(Definition.DefinitionId)));
        }
    }
    for (const TPair<EDAContentFaction, int32>& Required : RequiredCounts)
    {
        if (ActualCounts.FindRef(Required.Key) != Required.Value)
        {
            Errors.Add(LOCTEXT("FactionCount", "Manifest faction counts must be exactly 15/15/15/17/1/1."));
            break;
        }
    }

    if (Manifest.StarterDeck.Num() != 32)
    {
        Errors.Add(FText::Format(LOCTEXT("DeckRowCount", "Starter deck requires exactly 32 definition rows, found {0}."), Manifest.StarterDeck.Num()));
    }
    int32 DeckSize = 0;
    TSet<FName> DeckDefinitionIds;
    for (const FDAManifestDeckEntry& Entry : Manifest.StarterDeck)
    {
        DeckSize += Entry.Quantity;
        if (Entry.Quantity <= 0 || Entry.Quantity > 3 || !IsStableDefinitionId(Entry.DefinitionId)
            || !DefinitionIds.Contains(Entry.DefinitionId) || DeckDefinitionIds.Contains(Entry.DefinitionId))
        {
            Errors.Add(FText::Format(LOCTEXT("InvalidDeckComposition", "Starter deck entry '{0}' is missing, duplicated, or has invalid quantity."), FText::FromName(Entry.DefinitionId)));
        }
        DeckDefinitionIds.Add(Entry.DefinitionId);
    }
    if (DeckSize != FDADeckState::RequiredDeckSize)
    {
        Errors.Add(FText::Format(LOCTEXT("DeckCount", "Starter deck requires exactly 60 instances, found {0}."), DeckSize));
    }
    if (DeckDefinitionIds.Contains(TEXT("special.founder_hall")))
    {
        Errors.Add(LOCTEXT("FounderHallInDeck", "Founder Hall is placed at campaign start and cannot appear in the starter deck."));
    }
    return Errors.Num() == InitialErrorCount;
}

FString FDAContentManifestPipeline::GetGeneratedPackageName(const UDA_CardDefinition& Definition)
{
    if (Definition.DefinitionId == TEXT("special.founder_hall"))
    {
        return TEXT("/Game/DA/Buildings/DA_FounderHall");
    }

    FString Folder = TEXT("Universal");
    const FString StableId = Definition.DefinitionId.ToString();
    if (StableId.StartsWith(TEXT("synara."))) { Folder = TEXT("Synara"); }
    else if (StableId.StartsWith(TEXT("forgeweave."))) { Folder = TEXT("Forgeweave"); }
    else if (StableId.StartsWith(TEXT("eden."))) { Folder = TEXT("Eden"); }
    else if (StableId.StartsWith(TEXT("fusion."))) { Folder = TEXT("Fusion"); }

    FString Suffix;
    for (const TCHAR Character : Definition.DisplayName.ToString())
    {
        if (FChar::IsAlnum(Character))
        {
            Suffix.AppendChar(Character);
        }
    }
    return FString::Printf(TEXT("/Game/DA/Cards/%s/DA_Card_%s"), *Folder, *Suffix);
}

bool FDAContentManifestPipeline::ValidateGeneratedCache(
    const FDAVerticalSliceContentManifest& Manifest,
    const TArray<UDA_CardDefinition*>& CandidateDefinitions,
    const UDA_DeckDefinition* CandidateDeck,
    TArray<FText>& Errors)
{
    const int32 InitialErrorCount = Errors.Num();
    FDABuiltManifestContent Canonical;
    if (!BuildRuntimeContent(Manifest, Canonical, Errors))
    {
        return false;
    }
    if (CandidateDefinitions.Num() != Canonical.Definitions.Num())
    {
        Errors.Add(LOCTEXT("CacheDefinitionCount", "Generated content cache does not contain exactly 64 definitions."));
        return false;
    }

    TMap<FName, const UDA_CardDefinition*> CandidatesById;
    for (const UDA_CardDefinition* Candidate : CandidateDefinitions)
    {
        if (!Candidate || Candidate->DefinitionId.IsNone() || CandidatesById.Contains(Candidate->DefinitionId))
        {
            Errors.Add(LOCTEXT("CacheIdentity", "Generated content cache contains a null, empty, or duplicate stable definition ID."));
            return false;
        }
        CandidatesById.Add(Candidate->DefinitionId, Candidate);
    }

    for (const UDA_CardDefinition* Expected : Canonical.Definitions)
    {
        const UDA_CardDefinition* const* CandidatePtr = CandidatesById.Find(Expected->DefinitionId);
        const UDA_CardDefinition* Candidate = CandidatePtr ? *CandidatePtr : nullptr;
        if (!Candidate
            || Candidate->GetPrimaryAssetId() != Expected->GetPrimaryAssetId()
            || Candidate->DisplayName.ToString() != Expected->DisplayName.ToString()
            || Candidate->CivilizationId != Expected->CivilizationId
            || Candidate->CardType != Expected->CardType
            || Candidate->Rarity != Expected->Rarity
            || Candidate->Footprint != Expected->Footprint
            || Candidate->bPlaceable != Expected->bPlaceable
            || Candidate->WorldPrefab.ToSoftObjectPath() != Expected->WorldPrefab.ToSoftObjectPath()
            || Candidate->bRandomCacheEligible != Expected->bRandomCacheEligible
            || NormalizedTagNames(Candidate->Tags) != NormalizedTagNames(Expected->Tags)
            || NormalizedNames(Candidate->UpgradeBranchIds) != NormalizedNames(Expected->UpgradeBranchIds)
            || Candidate->bHasAuthoredCombat != Expected->bHasAuthoredCombat
            || Candidate->Combat.StructuralIntegrity != Expected->Combat.StructuralIntegrity
            || Candidate->Combat.Armor != Expected->Combat.Armor
            || Candidate->Combat.CyberIntegrity != Expected->Combat.CyberIntegrity
            || Candidate->Combat.bCapturable != Expected->Combat.bCapturable
            || Candidate->SourceManifestFingerprint != Manifest.SourceFingerprint
            || Candidate->AuthoredValueMask != Expected->AuthoredValueMask
            || Candidate->DeploymentCapital != Expected->DeploymentCapital
            || Candidate->DeploymentInsight != Expected->DeploymentInsight
            || Candidate->DeploymentInfluence != Expected->DeploymentInfluence
            || Candidate->CraftCapital != Expected->CraftCapital
            || Candidate->CraftInsight != Expected->CraftInsight
            || Candidate->CraftProductionThroughput != Expected->CraftProductionThroughput
            || Candidate->RequiredCraftingFacilityId != Expected->RequiredCraftingFacilityId
            || Candidate->MaintenanceCapitalPerCycle != Expected->MaintenanceCapitalPerCycle
            || Candidate->BaseCapitalPerCycle != Expected->BaseCapitalPerCycle
            || Candidate->BaseInsightPerCycle != Expected->BaseInsightPerCycle
            || Candidate->BaseInfluencePerCycle != Expected->BaseInfluencePerCycle
            || Candidate->SynaraDependencyPerCycle != Expected->SynaraDependencyPerCycle
            || Candidate->ForgeweaveResourceHungerPerCycle != Expected->ForgeweaveResourceHungerPerCycle
            || Candidate->WorkforceRequirementModifier != Expected->WorkforceRequirementModifier
            || Candidate->IndustrialThroughputModifier != Expected->IndustrialThroughputModifier
            || Candidate->AdjacentIndustrialConstructionSpeedModifier != Expected->AdjacentIndustrialConstructionSpeedModifier
            || Candidate->ConstructionCycles != Expected->ConstructionCycles
            || Candidate->UtilityDemand.Power != Expected->UtilityDemand.Power
            || Candidate->UtilityDemand.Water != Expected->UtilityDemand.Water
            || Candidate->UtilityDemand.Data != Expected->UtilityDemand.Data
            || Candidate->HousingCapacity != Expected->HousingCapacity)
        {
            Errors.Add(FText::Format(
                LOCTEXT("StaleCacheDefinition", "Generated cache definition '{0}' does not exactly match the canonical manifest."),
                FText::FromName(Expected->DefinitionId)));
        }
    }

    if (!CandidateDeck
        || CandidateDeck->DeckId != Canonical.DeckAsset->DeckId
        || CandidateDeck->GetPrimaryAssetId() != Canonical.DeckAsset->GetPrimaryAssetId()
        || CandidateDeck->SourceManifestFingerprint != Manifest.SourceFingerprint
        || CandidateDeck->Entries.Num() != Canonical.DeckAsset->Entries.Num())
    {
        Errors.Add(LOCTEXT("StaleCacheDeck", "Generated starter deck cache does not exactly match the canonical manifest."));
    }
    else
    {
        for (int32 Index = 0; Index < CandidateDeck->Entries.Num(); ++Index)
        {
            if (CandidateDeck->Entries[Index].DefinitionId != Canonical.DeckAsset->Entries[Index].DefinitionId
                || CandidateDeck->Entries[Index].Quantity != Canonical.DeckAsset->Entries[Index].Quantity)
            {
                Errors.Add(LOCTEXT("StaleCacheDeckEntry", "Generated starter deck cache entries do not exactly match the canonical manifest."));
                break;
            }
        }
    }
    return Errors.Num() == InitialErrorCount;
}

bool FDAContentManifestPipeline::BuildRuntimeContent(
    const FDAVerticalSliceContentManifest& Manifest,
    FDABuiltManifestContent& OutContent,
    TArray<FText>& Errors)
{
    const int32 InitialErrorCount = Errors.Num();
    if (!ValidateManifest(Manifest, Errors))
    {
        return false;
    }

    OutContent = FDABuiltManifestContent();
    UObject* ContentOuter = NewObject<UObject>(GetTransientPackage(), NAME_None, RF_Transient);
    for (const FDAManifestCardDefinition& Source : Manifest.Definitions)
    {
        FString ObjectName = Source.DefinitionId.ToString().Replace(TEXT("."), TEXT("_"));
        UDA_CardDefinition* Definition = NewObject<UDA_CardDefinition>(ContentOuter, FName(*ObjectName));
        Definition->DefinitionId = Source.DefinitionId;
        Definition->DisplayName = FText::FromString(Source.DisplayName);
        Definition->SourceManifestFingerprint = Manifest.SourceFingerprint;
        Definition->CivilizationId = FPrimaryAssetId(FPrimaryAssetType(TEXT("CivilizationDefinition")), FName(*FactionStableName(Source.Faction)));
        Definition->CardType = Source.CardType;
        Definition->Rarity = Source.Rarity;
        Definition->Footprint = Source.Footprint;
        Definition->bPlaceable = Source.bPlaceable;
        Definition->WorldPrefab = TSoftClassPtr<AActor>(FSoftObjectPath(Source.WorldPrefabClassPath));
        Definition->bRandomCacheEligible = Source.bRandomCacheEligible;
        for (const FName TagName : Source.Tags)
        {
            const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
            if (!Tag.IsValid())
            {
                Errors.Add(FText::Format(
                    LOCTEXT("UnknownGameplayTag", "Definition '{0}' references unknown gameplay tag '{1}'."),
                    FText::FromName(Source.DefinitionId),
                    FText::FromName(TagName)));
                continue;
            }
            Definition->Tags.Add(Tag);
        }
        Definition->Tags.Sort([](const FGameplayTag Left, const FGameplayTag Right)
        {
            return Left.GetTagName().LexicalLess(Right.GetTagName());
        });
        Definition->UpgradeBranchIds = NormalizedNames(Source.UpgradeBranchIds);
        Definition->bHasAuthoredCombat = Source.bHasAuthoredCombat;
        Definition->Combat = Source.Combat;
        Definition->AuthoredValueMask = Source.AuthoredValueMask;
        Definition->DeploymentCapital = Source.DeploymentCapital;
        Definition->DeploymentInsight = Source.DeploymentInsight;
        Definition->DeploymentInfluence = Source.DeploymentInfluence;
        Definition->CraftCapital = Source.CraftCapital;
        Definition->CraftInsight = Source.CraftInsight;
        Definition->CraftProductionThroughput = Source.CraftProductionThroughput;
        Definition->RequiredCraftingFacilityId = Source.RequiredCraftingFacilityId;
        Definition->MaintenanceCapitalPerCycle = Source.MaintenanceCapitalPerCycle;
        Definition->ConstructionCycles = Source.ConstructionCycles;
        Definition->BaseCapitalPerCycle = Source.BaseCapitalPerCycle;
        Definition->BaseInsightPerCycle = Source.BaseInsightPerCycle;
        Definition->BaseInfluencePerCycle = Source.BaseInfluencePerCycle;
        Definition->SynaraDependencyPerCycle = Source.SynaraDependencyPerCycle;
        Definition->ForgeweaveResourceHungerPerCycle = Source.ForgeweaveResourceHungerPerCycle;
        Definition->WorkforceRequirementModifier = Source.WorkforceRequirementModifier;
        Definition->IndustrialThroughputModifier = Source.IndustrialThroughputModifier;
        Definition->AdjacentIndustrialConstructionSpeedModifier = Source.AdjacentIndustrialConstructionSpeedModifier;
        Definition->UtilityDemand.Power = Source.UtilityPower;
        Definition->UtilityDemand.Water = Source.UtilityWater;
        Definition->UtilityDemand.Data = Source.UtilityData;
        Definition->HousingCapacity = Source.HousingCapacity;
        Definition->Validate(Errors);
        OutContent.Definitions.Add(Definition);
    }

    TArray<FGuid> InstanceIds;
    for (const FDAManifestDeckEntry& Entry : Manifest.StarterDeck)
    {
        for (int32 CopyIndex = 0; CopyIndex < Entry.Quantity; ++CopyIndex)
        {
            const FGuid InstanceId = MakeStarterInstanceId(Entry.DefinitionId, CopyIndex);
            if (!OutContent.Collection.AddInstanceWithId(InstanceId, Entry.DefinitionId, EDAAcquisitionSource::StarterDeck))
            {
                Errors.Add(FText::Format(LOCTEXT("DuplicateInstanceId", "Deterministic starter instance collision for '{0}' copy {1}."), FText::FromName(Entry.DefinitionId), CopyIndex));
                continue;
            }
            InstanceIds.Add(InstanceId);
        }
    }
    OutContent.Deck.BindCollection(OutContent.Collection);
    OutContent.Deck.SetInstanceIds(InstanceIds);
    OutContent.DeckAsset = NewObject<UDA_DeckDefinition>(ContentOuter, TEXT("DA_Deck_SynaraStarter60"));
    OutContent.DeckAsset->DeckId = TEXT("synara.starter_60");
    OutContent.DeckAsset->SourceManifestFingerprint = Manifest.SourceFingerprint;
    for (const FDAManifestDeckEntry& SourceEntry : Manifest.StarterDeck)
    {
        FDAStarterDeckEntry Entry;
        Entry.DefinitionId = SourceEntry.DefinitionId;
        Entry.Quantity = SourceEntry.Quantity;
        OutContent.DeckAsset->Entries.Add(Entry);
    }
    return Errors.Num() == InitialErrorCount;
}

#undef LOCTEXT_NAMESPACE
