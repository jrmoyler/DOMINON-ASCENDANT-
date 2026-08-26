#include "Content/DACardDefinition.h"

#define LOCTEXT_NAMESPACE "DACardDefinition"

const FPrimaryAssetType UDA_CardDefinition::AssetType(TEXT("CardDefinition"));

FPrimaryAssetId UDA_CardDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(AssetType, DefinitionId);
}

bool UDA_CardDefinition::HasAuthoredValue(const EDAAuthoredCardValue Value) const
{
    return (AuthoredValueMask & DAAuthoredValueBit(Value)) != 0;
}

bool UDA_CardDefinition::TryGetIntValue(
    const EDAAuthoredCardValue Field,
    const int32 RawValue,
    int32& OutValue) const
{
    if (!HasAuthoredValue(Field))
    {
        return false;
    }
    OutValue = RawValue;
    return true;
}

bool UDA_CardDefinition::TryGetFloatValue(
    const EDAAuthoredCardValue Field,
    const float RawValue,
    float& OutValue) const
{
    if (!HasAuthoredValue(Field))
    {
        return false;
    }
    OutValue = RawValue;
    return true;
}

#define DA_TRY_INT(Name, Field, Raw) \
    bool UDA_CardDefinition::TryGet##Name(int32& OutValue) const \
    { return TryGetIntValue(EDAAuthoredCardValue::Field, Raw, OutValue); }
#define DA_TRY_FLOAT(Name, Field, Raw) \
    bool UDA_CardDefinition::TryGet##Name(float& OutValue) const \
    { return TryGetFloatValue(EDAAuthoredCardValue::Field, Raw, OutValue); }

DA_TRY_INT(DeploymentCapital, DeploymentCapital, DeploymentCapital)
DA_TRY_INT(DeploymentInsight, DeploymentInsight, DeploymentInsight)
DA_TRY_INT(DeploymentInfluence, DeploymentInfluence, DeploymentInfluence)
DA_TRY_INT(CraftCapital, CraftCapital, CraftCapital)
DA_TRY_INT(CraftInsight, CraftInsight, CraftInsight)
DA_TRY_INT(CraftProductionThroughput, CraftProductionThroughput, CraftProductionThroughput)
DA_TRY_FLOAT(MaintenanceCapitalPerCycle, MaintenanceCapitalPerCycle, MaintenanceCapitalPerCycle)
DA_TRY_FLOAT(BaseCapitalPerCycle, BaseCapitalPerCycle, BaseCapitalPerCycle)
DA_TRY_FLOAT(BaseInsightPerCycle, BaseInsightPerCycle, BaseInsightPerCycle)
DA_TRY_FLOAT(BaseInfluencePerCycle, BaseInfluencePerCycle, BaseInfluencePerCycle)
DA_TRY_FLOAT(SynaraDependencyPerCycle, SynaraDependencyPerCycle, SynaraDependencyPerCycle)
DA_TRY_FLOAT(ForgeweaveResourceHungerPerCycle, ForgeweaveResourceHungerPerCycle, ForgeweaveResourceHungerPerCycle)
DA_TRY_FLOAT(WorkforceRequirementModifier, WorkforceRequirementModifier, WorkforceRequirementModifier)
DA_TRY_FLOAT(IndustrialThroughputModifier, IndustrialThroughputModifier, IndustrialThroughputModifier)
DA_TRY_FLOAT(AdjacentIndustrialConstructionSpeedModifier, AdjacentIndustrialConstructionSpeedModifier, AdjacentIndustrialConstructionSpeedModifier)
DA_TRY_INT(ConstructionCycles, ConstructionCycles, ConstructionCycles)
DA_TRY_INT(UtilityPower, UtilityPower, UtilityDemand.Power)
DA_TRY_INT(UtilityWater, UtilityWater, UtilityDemand.Water)
DA_TRY_INT(UtilityData, UtilityData, UtilityDemand.Data)
DA_TRY_INT(HousingCapacity, HousingCapacity, HousingCapacity)

#undef DA_TRY_INT
#undef DA_TRY_FLOAT

bool UDA_CardDefinition::TryGetRequiredCraftingFacilityId(FName& OutValue) const
{
    if (!HasAuthoredValue(EDAAuthoredCardValue::RequiredCraftingFacilityId))
    {
        return false;
    }
    OutValue = RequiredCraftingFacilityId;
    return true;
}

bool UDA_CardDefinition::TryGetCombatDefinition(FDACombatDefinition& OutValue) const
{
    if (!bHasAuthoredCombat)
    {
        return false;
    }
    OutValue = Combat;
    return true;
}

bool UDA_CardDefinition::Validate(TArray<FText>& Errors) const
{
    const int32 InitialErrorCount = Errors.Num();

    if (DefinitionId.IsNone())
    {
        Errors.Add(LOCTEXT("EmptyDefinitionId", "Card definition has an empty stable DefinitionId."));
    }

    if (bPlaceable && (Footprint.X <= 0 || Footprint.Y <= 0))
    {
        Errors.Add(FText::Format(
            LOCTEXT("InvalidFootprint", "Placeable card definition '{0}' has a non-positive footprint."),
            FText::FromName(DefinitionId)));
    }

    if (bPlaceable && WorldPrefab.IsNull())
    {
        Errors.Add(FText::Format(
            LOCTEXT("MissingWorldPrefab", "Placeable card definition '{0}' has no world prefab."),
            FText::FromName(DefinitionId)));
    }
    else if (bPlaceable && WorldPrefab.LoadSynchronous() == nullptr)
    {
        Errors.Add(FText::Format(
            LOCTEXT("UnresolvedWorldPrefab", "Placeable card definition '{0}' has a world prefab class that cannot be resolved."),
            FText::FromName(DefinitionId)));
    }

    constexpr uint64 KnownAuthoredMask = (uint64(1) << (static_cast<uint8>(EDAAuthoredCardValue::HousingCapacity) + 1)) - 1;
    if ((AuthoredValueMask & ~KnownAuthoredMask) != 0)
    {
        Errors.Add(FText::Format(
            LOCTEXT("UnknownAuthoredValue", "Card definition '{0}' contains unknown authored-value bits."),
            FText::FromName(DefinitionId)));
    }

    const auto ValidateRawPresence = [this, &Errors](const EDAAuthoredCardValue Field, const bool bRawHasValue, const TCHAR* FieldName)
    {
        if (bRawHasValue && !HasAuthoredValue(Field))
        {
            Errors.Add(FText::Format(
                LOCTEXT("UnauthoredRawValue", "Card definition '{0}' supplies raw gameplay value '{1}' without authored presence."),
                FText::FromName(DefinitionId),
                FText::FromString(FieldName)));
        }
    };
    ValidateRawPresence(EDAAuthoredCardValue::DeploymentCapital, DeploymentCapital != 0, TEXT("deploymentCapital"));
    ValidateRawPresence(EDAAuthoredCardValue::DeploymentInsight, DeploymentInsight != 0, TEXT("deploymentInsight"));
    ValidateRawPresence(EDAAuthoredCardValue::DeploymentInfluence, DeploymentInfluence != 0, TEXT("deploymentInfluence"));
    ValidateRawPresence(EDAAuthoredCardValue::CraftCapital, CraftCapital != 0, TEXT("craftCapital"));
    ValidateRawPresence(EDAAuthoredCardValue::CraftInsight, CraftInsight != 0, TEXT("craftInsight"));
    ValidateRawPresence(EDAAuthoredCardValue::CraftProductionThroughput, CraftProductionThroughput != 0, TEXT("craftProductionThroughput"));
    ValidateRawPresence(EDAAuthoredCardValue::RequiredCraftingFacilityId, !RequiredCraftingFacilityId.IsNone(), TEXT("requiredCraftingFacilityId"));
    ValidateRawPresence(EDAAuthoredCardValue::MaintenanceCapitalPerCycle, MaintenanceCapitalPerCycle != 0.f, TEXT("maintenanceCapitalPerCycle"));
    ValidateRawPresence(EDAAuthoredCardValue::BaseCapitalPerCycle, BaseCapitalPerCycle != 0.f, TEXT("baseCapitalPerCycle"));
    ValidateRawPresence(EDAAuthoredCardValue::BaseInsightPerCycle, BaseInsightPerCycle != 0.f, TEXT("baseInsightPerCycle"));
    ValidateRawPresence(EDAAuthoredCardValue::BaseInfluencePerCycle, BaseInfluencePerCycle != 0.f, TEXT("baseInfluencePerCycle"));
    ValidateRawPresence(EDAAuthoredCardValue::SynaraDependencyPerCycle, SynaraDependencyPerCycle != 0.f, TEXT("synaraDependencyPerCycle"));
    ValidateRawPresence(EDAAuthoredCardValue::ForgeweaveResourceHungerPerCycle, ForgeweaveResourceHungerPerCycle != 0.f, TEXT("forgeweaveResourceHungerPerCycle"));
    ValidateRawPresence(EDAAuthoredCardValue::WorkforceRequirementModifier, WorkforceRequirementModifier != 0.f, TEXT("workforceRequirementModifier"));
    ValidateRawPresence(EDAAuthoredCardValue::IndustrialThroughputModifier, IndustrialThroughputModifier != 0.f, TEXT("industrialThroughputModifier"));
    ValidateRawPresence(EDAAuthoredCardValue::AdjacentIndustrialConstructionSpeedModifier, AdjacentIndustrialConstructionSpeedModifier != 0.f, TEXT("adjacentIndustrialConstructionSpeedModifier"));
    ValidateRawPresence(EDAAuthoredCardValue::ConstructionCycles, ConstructionCycles != 0, TEXT("constructionCycles"));
    ValidateRawPresence(EDAAuthoredCardValue::UtilityPower, UtilityDemand.Power != 0, TEXT("utilityPower"));
    ValidateRawPresence(EDAAuthoredCardValue::UtilityWater, UtilityDemand.Water != 0, TEXT("utilityWater"));
    ValidateRawPresence(EDAAuthoredCardValue::UtilityData, UtilityDemand.Data != 0, TEXT("utilityData"));
    ValidateRawPresence(EDAAuthoredCardValue::HousingCapacity, HousingCapacity != 0, TEXT("housingCapacity"));

    const bool bHasRawCombat = Combat.StructuralIntegrity != 0.f
        || Combat.Armor != 0.f
        || Combat.CyberIntegrity != 0.f
        || Combat.bCapturable;
    if (bHasRawCombat && !bHasAuthoredCombat)
    {
        Errors.Add(FText::Format(
            LOCTEXT("UnauthoredCombat", "Card definition '{0}' supplies raw combat values without authored presence."),
            FText::FromName(DefinitionId)));
    }
    if ((bHasAuthoredCombat || bHasRawCombat)
        && (!FMath::IsFinite(Combat.StructuralIntegrity)
            || !FMath::IsFinite(Combat.Armor)
            || !FMath::IsFinite(Combat.CyberIntegrity)
            || Combat.StructuralIntegrity < 0.f
            || Combat.Armor < 0.f
            || Combat.CyberIntegrity < 0.f))
    {
        Errors.Add(FText::Format(
            LOCTEXT("InvalidCombat", "Card definition '{0}' has invalid combat values."),
            FText::FromName(DefinitionId)));
    }

    if (!FMath::IsFinite(MaintenanceCapitalPerCycle)
        || !FMath::IsFinite(BaseCapitalPerCycle)
        || !FMath::IsFinite(BaseInsightPerCycle)
        || !FMath::IsFinite(BaseInfluencePerCycle)
        || !FMath::IsFinite(SynaraDependencyPerCycle)
        || !FMath::IsFinite(ForgeweaveResourceHungerPerCycle)
        || !FMath::IsFinite(WorkforceRequirementModifier)
        || !FMath::IsFinite(IndustrialThroughputModifier)
        || !FMath::IsFinite(AdjacentIndustrialConstructionSpeedModifier)
        || DeploymentCapital < 0
        || DeploymentInsight < 0
        || DeploymentInfluence < 0
        || CraftCapital < 0
        || CraftInsight < 0
        || CraftProductionThroughput < 0
        || MaintenanceCapitalPerCycle < 0.f
        || BaseCapitalPerCycle < 0.f
        || BaseInsightPerCycle < 0.f
        || BaseInfluencePerCycle < 0.f
        || UtilityDemand.HasNegativeDemand())
    {
        Errors.Add(FText::Format(
            LOCTEXT("NegativeCosts", "Card definition '{0}' has a negative cost or utility demand."),
            FText::FromName(DefinitionId)));
    }

    if (HousingCapacity < 0)
    {
        Errors.Add(FText::Format(
            LOCTEXT("NegativeHousing", "Card definition '{0}' has negative housing capacity."),
            FText::FromName(DefinitionId)));
    }

    if (HasAuthoredValue(EDAAuthoredCardValue::ConstructionCycles) && ConstructionCycles <= 0)
    {
        Errors.Add(FText::Format(
            LOCTEXT("InvalidConstructionCycles", "Placeable card definition '{0}' has a non-positive construction-cycle count."),
            FText::FromName(DefinitionId)));
    }

    TSet<FName> SeenUpgradeBranchIds;
    for (const FName UpgradeBranchId : UpgradeBranchIds)
    {
        if (SeenUpgradeBranchIds.Contains(UpgradeBranchId))
        {
            Errors.Add(FText::Format(
                LOCTEXT("DuplicateUpgradeBranch", "Card definition '{0}' contains duplicate upgrade branch ID '{1}'."),
                FText::FromName(DefinitionId),
                FText::FromName(UpgradeBranchId)));
        }
        else
        {
            SeenUpgradeBranchIds.Add(UpgradeBranchId);
        }
    }

    TSet<FGameplayTag> SeenTags;
    for (const FGameplayTag Tag : Tags)
    {
        if (!Tag.IsValid() || SeenTags.Contains(Tag))
        {
            Errors.Add(FText::Format(
                LOCTEXT("InvalidGameplayTag", "Card definition '{0}' contains an invalid or duplicate gameplay tag."),
                FText::FromName(DefinitionId)));
        }
        SeenTags.Add(Tag);
    }

    if (Rarity == EDARarity::Dominion && bRandomCacheEligible)
    {
        Errors.Add(FText::Format(
            LOCTEXT("DominionRandomCache", "Dominion card definition '{0}' cannot be random-cache eligible."),
            FText::FromName(DefinitionId)));
    }

    return Errors.Num() == InitialErrorCount;
}

#undef LOCTEXT_NAMESPACE
