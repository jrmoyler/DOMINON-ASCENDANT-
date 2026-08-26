#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "DAFounderAbilityDefinitions.generated.h"

UENUM(BlueprintType)
enum class EDAFounderAbilityKind : uint8
{
    Active,
    Ultimate
};

UENUM(BlueprintType)
enum class EDAFounderAbilityTargetScope : uint8
{
    Self,
    ViewTarget,
    AlliesInRadius
};

USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDAFounderAbilityDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FName AbilityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    EDAFounderAbilityKind Kind = EDAFounderAbilityKind::Active;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float TacticalChargeCost = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (ClampMin = "0.0"))
    float CooldownSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (ClampMin = "0.01"))
    float DurationSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    EDAFounderAbilityTargetScope TargetScope = EDAFounderAbilityTargetScope::Self;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (ClampMin = "0.0"))
    float RangeMeters = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FName EffectId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FGameplayTag GrantedStatusTag;
};

UCLASS(BlueprintType)
class DOMINIONGAMEPLAY_API UDAFounderAbilityDefinitions : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
    TArray<FDAFounderAbilityDefinition> BaselineAbilities;

    // Imports the source-controlled JSON declaration into this runtime/editor data asset.
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Abilities|Import")
    bool ImportFromJsonFile(const FString& SourceFilePath);

    bool ImportFromJsonString(const FString& SourceJson, FText& OutError);

    const TArray<FDAFounderAbilityDefinition>& GetBaselineAbilities() const;
    static bool ContainsAbility(const TArray<FDAFounderAbilityDefinition>& Definitions, FName AbilityId);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities|Import")
    FText LastImportError;
};
