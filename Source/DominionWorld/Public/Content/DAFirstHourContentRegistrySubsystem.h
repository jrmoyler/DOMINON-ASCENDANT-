#pragma once

#include "CoreMinimal.h"
#include "Content/DAFirstHourQuestContent.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DAFirstHourContentRegistrySubsystem.generated.h"

/** Runtime owner for generated first-hour assets, with the canonical JSON as the authoritative fallback. */
UCLASS()
class DOMINIONWORLD_API UDAFirstHourContentRegistrySubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    const FDAFirstHourQuestManifest& GetManifest() const { return Manifest; }
    const FDABuiltFirstHourContent& GetContent() const { return Content; }
    bool UsedGeneratedCache() const { return bUsedGeneratedCache; }
    bool IsReady() const { return bReady; }

private:
    bool RebuildRegistry(TArray<FText>& Errors);

    FDAFirstHourQuestManifest Manifest;
    FDABuiltFirstHourContent Content;
    bool bUsedGeneratedCache = false;
    bool bReady = false;

    // Strong references retain either generated packages or the marked runtime fallback across GC.
    UPROPERTY()
    TArray<TObjectPtr<UDA_FirstHourQuestDefinition>> StoredQuestDefinitions;

    UPROPERTY()
    TObjectPtr<UDA_CitizenDefinition> StoredCitizenDefinition;
};
