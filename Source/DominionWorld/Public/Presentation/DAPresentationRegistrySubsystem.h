#pragma once

#include "Conflict/DAConflictRecords.h"
#include "Presentation/DAPresentationContent.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DAPresentationRegistrySubsystem.generated.h"

/** Strong runtime owner for complete generated presentation cache or canonical packaged fallback. */
UCLASS()
class DOMINIONWORLD_API UDAPresentationRegistrySubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool RebuildFromCanonicalAndGenerated(
        const TArray<UDAPresentationDefinition*>& GeneratedDefinitions,
        TArray<FText>& OutErrors);

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool IsReady() const { return bReady; }

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool UsedGeneratedCache() const { return bUsedGeneratedCache; }

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool HasDegradedCacheWarning() const { return !DegradedCacheWarnings.IsEmpty(); }

    const TArray<FText>& GetDegradedCacheWarnings() const { return DegradedCacheWarnings; }

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool Resolve(EDAPresentationDefinitionKind Kind, FName Id,
        FDAPresentationDefinitionSource& OutDefinition) const;

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool BuildConstructionPlan(FName Faction, EDAConstructionState State,
        FDAPresentationPlaybackPlan& OutPlan) const;

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool BuildDamagePlan(FName Faction, const FDAWorldAssetRecord& Asset,
        FDAPresentationPlaybackPlan& OutPlan) const;

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool BuildCapturePlan(const FDAWorldAssetRecord& Asset, const FDACaptureRecord& Capture,
        FDAPresentationPlaybackPlan& OutPlan) const;

    bool BuildCaptureSnapshotPlan(const FDACapturePresentationSnapshot& Snapshot,
        FDAPresentationPlaybackPlan& OutPlan) const;

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool BuildDaxtonPlan(const FDADaxtonCampaignState& Daxton,
        FDAPresentationPlaybackPlan& OutPlan) const;

    UFUNCTION(BlueprintPure, Category = "Presentation")
    bool BuildAscensionPlan(const FDAAscensionPresentationState& Ascension,
        FDAPresentationPlaybackPlan& OutPlan) const;

    UFUNCTION(BlueprintCallable, Category = "Presentation")
    void SkipPlaybackPlan(UPARAM(ref) FDAPresentationPlaybackPlan& InOutPlan) const;

    const FDAPresentationContentManifest& GetManifest() const { return Manifest; }

private:
    FDAPresentationContentManifest Manifest;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UDAPresentationDefinition>> StrongDefinitions;

    bool bReady = false;
    bool bUsedGeneratedCache = false;
    TArray<FText> DegradedCacheWarnings;
};
