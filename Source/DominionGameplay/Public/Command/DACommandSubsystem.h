#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Units/DASquadEntity.h"

#include "DACommandSubsystem.generated.h"

/** Owns player direct-command capacity, tactical orders, and temporary CP. */
UCLASS()
class DOMINIONGAMEPLAY_API UDACommandSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static constexpr int32 MaximumDirectSquads = 3;
    static constexpr int32 MaximumDirectVehicles = 1;
    static constexpr int32 BaselineMaximumCommandPoints = 6;

    bool RegisterSquad(UDASquadEntity* Squad);
    bool UnregisterSquad(UDASquadEntity* Squad);
    bool TrySelectDirectly(UDASquadEntity* Squad);
    bool ReleaseDirectSelection(UDASquadEntity* Squad);
    bool IssueOrder(UDASquadEntity* Squad, EDACommandOrder Order, const FVector& Destination = FVector::ZeroVector);

    int32 GetDirectlySelectedSquadCount() const { return DirectlySelectedSquads.Num(); }
    bool HasDirectlySelectedVehicle() const { return DirectlySelectedVehicle != nullptr; }
    int32 GetCommandPoints() const { return CommandPoints; }
    int32 GetMaximumCommandPoints() const { return BaselineMaximumCommandPoints; }
    const TArray<TObjectPtr<UDASquadEntity>>& GetRegisteredSquads() const { return RegisteredSquads; }
    const TArray<TObjectPtr<UDASquadEntity>>& GetDirectlySelectedSquads() const { return DirectlySelectedSquads; }

    bool AwardControlZoneCommandPoints(int32 Amount);
    bool TrySpendCommandPoints(int32 Cost);

private:
    bool IsRegistered(const UDASquadEntity* Squad) const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UDASquadEntity>> RegisteredSquads;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UDASquadEntity>> DirectlySelectedSquads;

    UPROPERTY(Transient)
    TObjectPtr<UDASquadEntity> DirectlySelectedVehicle = nullptr;

    UPROPERTY(Transient)
    int32 CommandPoints = 0;
};
