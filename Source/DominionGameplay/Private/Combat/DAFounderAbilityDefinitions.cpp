#include "Combat/DAFounderAbilityDefinitions.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool UDAFounderAbilityDefinitions::ImportFromJsonFile(const FString& SourceFilePath)
{
    FString SourceJson;
    if (!FFileHelper::LoadFileToString(SourceJson, *SourceFilePath))
    {
        LastImportError = FText::FromString(FString::Printf(TEXT("Could not read ability definition file: %s"), *SourceFilePath));
        return false;
    }

    return ImportFromJsonString(SourceJson, LastImportError);
}

bool UDAFounderAbilityDefinitions::ImportFromJsonString(const FString& SourceJson, FText& OutError)
{
    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SourceJson);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutError = FText::FromString(TEXT("Ability definition source is not valid JSON."));
        return false;
    }

    FString AssetType;
    const TArray<TSharedPtr<FJsonValue>>* JsonDefinitions = nullptr;
    if (!RootObject->TryGetStringField(TEXT("assetType"), AssetType)
        || AssetType != TEXT("DAFounderAbilityDefinitions")
        || !RootObject->TryGetArrayField(TEXT("definitions"), JsonDefinitions))
    {
        OutError = FText::FromString(TEXT("Ability definition source has an unsupported schema."));
        return false;
    }

    TArray<FDAFounderAbilityDefinition> ImportedDefinitions;
    TSet<FName> SeenAbilityIds;
    for (const TSharedPtr<FJsonValue>& JsonDefinition : *JsonDefinitions)
    {
        const TSharedPtr<FJsonObject> Object = JsonDefinition.IsValid() ? JsonDefinition->AsObject() : nullptr;
        if (!Object.IsValid())
        {
            OutError = FText::FromString(TEXT("Ability definition entry is not an object."));
            return false;
        }

        FString AbilityId;
        FString DisplayName;
        FString Kind;
        FString TargetScope;
        FString EffectId;
        FString GrantedStatusTag;
        double TacticalChargeCost = 0.0;
        double CooldownSeconds = 0.0;
        double DurationSeconds = 0.0;
        double RangeMeters = 0.0;
        if (!Object->TryGetStringField(TEXT("abilityId"), AbilityId)
            || !Object->TryGetStringField(TEXT("displayName"), DisplayName)
            || !Object->TryGetStringField(TEXT("kind"), Kind)
            || !Object->TryGetNumberField(TEXT("tacticalChargeCost"), TacticalChargeCost)
            || !Object->TryGetNumberField(TEXT("cooldownSeconds"), CooldownSeconds)
            || !Object->TryGetNumberField(TEXT("durationSeconds"), DurationSeconds)
            || !Object->TryGetStringField(TEXT("targetScope"), TargetScope)
            || !Object->TryGetNumberField(TEXT("rangeMeters"), RangeMeters)
            || !Object->TryGetStringField(TEXT("effectId"), EffectId)
            || !Object->TryGetStringField(TEXT("grantedStatusTag"), GrantedStatusTag))
        {
            OutError = FText::FromString(TEXT("Ability definition entry is missing a required field."));
            return false;
        }

        const FName ParsedAbilityId(*AbilityId);
        const FGameplayTag ParsedStatusTag = FGameplayTag::RequestGameplayTag(FName(*GrantedStatusTag), false);
        if (ParsedAbilityId.IsNone()
            || SeenAbilityIds.Contains(ParsedAbilityId)
            || !ParsedStatusTag.IsValid()
            || !FMath::IsFinite(TacticalChargeCost)
            || !FMath::IsFinite(CooldownSeconds)
            || !FMath::IsFinite(DurationSeconds)
            || !FMath::IsFinite(RangeMeters)
            || TacticalChargeCost < 0.0
            || TacticalChargeCost > 100.0
            || CooldownSeconds <= 0.0
            || DurationSeconds <= 0.0
            || RangeMeters < 0.0
            || FName(*EffectId).IsNone())
        {
            OutError = FText::FromString(TEXT("Ability definition entry contains invalid data."));
            return false;
        }

        EDAFounderAbilityKind ParsedKind;
        if (Kind == TEXT("Active"))
        {
            ParsedKind = EDAFounderAbilityKind::Active;
        }
        else if (Kind == TEXT("Ultimate"))
        {
            ParsedKind = EDAFounderAbilityKind::Ultimate;
        }
        else
        {
            OutError = FText::FromString(TEXT("Ability definition entry has an invalid kind."));
            return false;
        }

        EDAFounderAbilityTargetScope ParsedTargetScope;
        if (TargetScope == TEXT("Self"))
        {
            ParsedTargetScope = EDAFounderAbilityTargetScope::Self;
        }
        else if (TargetScope == TEXT("ViewTarget"))
        {
            ParsedTargetScope = EDAFounderAbilityTargetScope::ViewTarget;
        }
        else if (TargetScope == TEXT("AlliesInRadius"))
        {
            ParsedTargetScope = EDAFounderAbilityTargetScope::AlliesInRadius;
        }
        else
        {
            OutError = FText::FromString(TEXT("Ability definition entry has an invalid target scope."));
            return false;
        }
        if (ParsedTargetScope != EDAFounderAbilityTargetScope::Self && RangeMeters <= 0.0)
        {
            OutError = FText::FromString(TEXT("A non-self Founder ability requires a positive authored range."));
            return false;
        }

        FDAFounderAbilityDefinition Definition;
        Definition.AbilityId = ParsedAbilityId;
        Definition.DisplayName = FText::FromString(DisplayName);
        Definition.Kind = ParsedKind;
        Definition.TacticalChargeCost = static_cast<float>(TacticalChargeCost);
        Definition.CooldownSeconds = static_cast<float>(CooldownSeconds);
        Definition.DurationSeconds = static_cast<float>(DurationSeconds);
        Definition.TargetScope = ParsedTargetScope;
        Definition.RangeMeters = static_cast<float>(RangeMeters);
        Definition.EffectId = FName(*EffectId);
        Definition.GrantedStatusTag = ParsedStatusTag;
        ImportedDefinitions.Add(MoveTemp(Definition));
        SeenAbilityIds.Add(ParsedAbilityId);
    }

    BaselineAbilities = MoveTemp(ImportedDefinitions);
    OutError = FText::GetEmpty();
    LastImportError = FText::GetEmpty();
    return true;
}

const TArray<FDAFounderAbilityDefinition>& UDAFounderAbilityDefinitions::GetBaselineAbilities() const
{
    return BaselineAbilities;
}

bool UDAFounderAbilityDefinitions::ContainsAbility(const TArray<FDAFounderAbilityDefinition>& Definitions, const FName AbilityId)
{
    return Definitions.ContainsByPredicate([AbilityId](const FDAFounderAbilityDefinition& Definition)
    {
        return Definition.AbilityId == AbilityId;
    });
}
