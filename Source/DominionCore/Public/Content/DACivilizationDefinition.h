#pragma once

#include "Engine/DataAsset.h"

#include "DACivilizationDefinition.generated.h"

UCLASS(BlueprintType)
class DOMINIONCORE_API UDA_CivilizationDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    static const FPrimaryAssetType AssetType;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(AssetType, GetFName());
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FName DefinitionId;
};
