#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "DAStarterDeckDefinition.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDAStarterDeckEntry
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deck")
    FName DefinitionId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deck", meta = (ClampMin = "1"))
    int32 Quantity = 1;
};

UCLASS(BlueprintType)
class DOMINIONCORE_API UDA_DeckDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    static const FPrimaryAssetType AssetType;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(AssetType, DeckId);
    }

    int32 GetInstanceCount() const
    {
        int32 Count = 0;
        for (const FDAStarterDeckEntry& Entry : Entries)
        {
            Count += Entry.Quantity;
        }
        return Count;
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deck")
    FName DeckId;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Deck")
    FString SourceManifestFingerprint;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deck")
    TArray<FDAStarterDeckEntry> Entries;
};
