#pragma once

#include "Commands/DAUIAuthoritativeService.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DAUIAuthoritativeFeatureSubsystem.generated.h"

/** Production adapter from shipped UI commands to the canonical World/Narrative owners. */
UCLASS()
class DOMINIONUI_API UDAUIAuthoritativeFeatureSubsystem final
    : public UGameInstanceSubsystem, public IDAUIAuthoritativeFeatureService
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ExecuteAuthoritativeUICommand(FName CommandId, FName SourceScreenId,
        const FString& PayloadJson, FString& OutError) override;
    virtual bool CaptureAuthoritativeUIState(
        FDAUIAuthoritativeFeatureSnapshot& OutState, FString& OutError) const override;

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<class UDAWorldStateSubsystem> WorldStateSubsystem;

    UPROPERTY(Transient)
    TWeakObjectPtr<class UDAFirstHourCampaignCoordinatorSubsystem> FirstHourCoordinator;
};
