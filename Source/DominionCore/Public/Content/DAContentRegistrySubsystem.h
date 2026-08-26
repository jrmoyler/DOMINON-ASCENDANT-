#pragma once

#include "Cards/DACollectionState.h"
#include "Cards/DADeckState.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DAContentRegistrySubsystem.generated.h"

class UDA_CardDefinition;
class UDA_CivilizationDefinition;
class UDA_DeckDefinition;
class FDAContentRegistrySpec;

UCLASS()
class DOMINIONCORE_API UDAContentRegistrySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UDA_CardDefinition* GetCardDefinition(const FPrimaryAssetId& AssetId) const;
    UDA_CardDefinition* GetCardDefinition(const FName DefinitionId) const;
    bool ValidateRegistry(TArray<FText>& Errors) const;
    bool RebuildFromDefinitions(const TArray<UDA_CardDefinition*>& Definitions, TArray<FText>& Errors);
    bool RebuildFromGeneratedCache(
        const TArray<UDA_CardDefinition*>& CandidateDefinitions,
        const UDA_DeckDefinition* CandidateDeck,
        bool& bOutUsedGeneratedCache,
        TArray<FText>& Errors);
    int32 GetRegisteredCardCount() const { return RegisteredCardDefinitions.Num(); }
    /** Copies the deterministic authored 60-instance starter collection/deck from this registry build. */
    bool BuildStarterCampaignContent(
        FDACollectionState& OutCollection,
        FDADeckState& OutDeck,
        FString& OutError) const;

private:
    void RebuildRegistry();
    void IndexCardDefinition(UDA_CardDefinition* Definition, const FPrimaryAssetId& AssetId);

    friend class FDAContentRegistrySpec;
    friend class FDeckRulesSpecFixture;

    TArray<TSoftObjectPtr<UDA_CardDefinition>> RegisteredCardDefinitions;
    TMap<FName, TSoftObjectPtr<UDA_CardDefinition>> CardDefinitionsByStableId;
    TMap<FPrimaryAssetId, TSoftObjectPtr<UDA_CardDefinition>> CardDefinitionsByPrimaryAssetId;
    TMap<FName, TSoftObjectPtr<UDA_CivilizationDefinition>> CivilizationDefinitionsByStableId;
    TArray<FText> IndexingErrors;

    // Keeps transient definitions alive when the packaged JSON manifest is the
    // runtime authority because generated editor assets have not been cooked.
    UPROPERTY(Transient)
    TArray<TObjectPtr<UDA_CardDefinition>> RuntimeManifestDefinitions;

    FDACollectionState StarterCollectionTemplate;
    FDADeckState StarterDeckTemplate;
    FString StarterContentFingerprint;
    bool bHasStarterCampaignContent = false;
};
