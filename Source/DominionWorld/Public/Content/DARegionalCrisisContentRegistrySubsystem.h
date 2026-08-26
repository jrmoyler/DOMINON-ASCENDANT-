#pragma once

#include "Content/DARegionalCrisisContent.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DARegionalCrisisContentRegistrySubsystem.generated.h"

UENUM(BlueprintType)
enum class EDARegionalGeneratedCacheState : uint8
{
    NoCache,
    Valid,
    Stale
};

/** Strong runtime cache owner; generated assets are accepted only at full manifest/fingerprint parity. */
UCLASS()
class DOMINIONWORLD_API UDARegionalCrisisContentRegistrySubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    const FDARegionalCrisisManifest& GetManifest() const { return Manifest; }
    bool IsReady() const { return bReady; }
    bool UsedGeneratedCache() const { return bUsedGeneratedCache; }
    EDARegionalGeneratedCacheState GetGeneratedCacheState() const { return GeneratedCacheState; }
private:
    FDARegionalCrisisManifest Manifest;
    bool bReady = false;
    bool bUsedGeneratedCache = false;
    EDARegionalGeneratedCacheState GeneratedCacheState = EDARegionalGeneratedCacheState::NoCache;
    UPROPERTY() TArray<TObjectPtr<UDARegionalWorldEventDefinition>> Events;
    UPROPERTY() TArray<TObjectPtr<UDARegionalQuestDefinition>> Quests;
};
