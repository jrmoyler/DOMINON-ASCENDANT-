#pragma once

#include "Campaign/DACampaignAuthority.h"
#include "Components/ActorComponent.h"
#include "Conflict/DAConflictRecords.h"

#include "DAStructuralDamageComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDAStructuralDamageStateChanged,
    EDAConstructionState, CommittedState);

/** Runtime damage facade over Core-owned, stable-key persistent records. */
UCLASS(ClassGroup = (Dominion), BlueprintType, meta = (BlueprintSpawnableComponent))
class DOMINIONGAMEPLAY_API UDAStructuralDamageComponent final : public UActorComponent
{
    GENERATED_BODY()

public:
    bool InitializeFromCampaign(IDACampaignAuthority& InCampaignAuthority, FGuid InWorldAssetId);

    UFUNCTION(BlueprintCallable, Category = "Structural Damage")
    bool ApplyModuleDamage(FName ModuleId, float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Structural Damage")
    bool ApplyStructuralDamage(float DamageAmount);

    UFUNCTION(BlueprintPure, Category = "Structural Damage")
    bool IsProductionEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Structural Damage")
    bool IsRuined() const;

    UFUNCTION(BlueprintPure, Category = "Structural Damage")
    EDAConstructionState GetDamagePresentationState() const;

    /** Copies the committed world record after a presentation delegate fires. */
    bool GetPresentationRecord(FDAWorldAssetRecord& OutRecord) const;

    /** Read-only presentation hook emitted only after the canonical campaign candidate publishes. */
    UPROPERTY(BlueprintAssignable, Category = "Structural Damage|Presentation")
    FDAStructuralDamageStateChanged OnDamageStateChanged;

    const FDAStructureModuleHealthRecord* FindModule(FName ModuleId) const;

private:
    static EDAStructureDamageState GetModuleState(float CurrentHealth, float MaximumHealth);
    bool ResolveRecords(const FDAWorldAssetRecord*& OutAssetRecord,
        const FDAStructuralDamageRecord*& OutDamageRecord) const;
    static void RecalculateDerivedState(FDAWorldAssetRecord& AssetRecord, FDAStructuralDamageRecord& DamageRecord);
    static void SynchronizeOwnedProjections(FDACampaignSnapshot& Campaign,
        const FDAWorldAssetRecord& AssetRecord);

    /** Non-owning; production lifetime is the GameInstance campaign subsystem. */
    IDACampaignAuthority* CampaignAuthority = nullptr;

    UPROPERTY(Transient)
    FGuid WorldAssetId;
};
