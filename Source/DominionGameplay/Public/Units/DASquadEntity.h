#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "DASquadEntity.generated.h"

class AActor;
class UDACommandSubsystem;

UENUM(BlueprintType)
enum class EDACommandOrder : uint8
{
    Move,
    Attack,
    Hold,
    Defend,
    Capture,
    Escort,
    Retreat,
    Ability
};

UENUM(BlueprintType)
enum class EDAMoraleState : uint8
{
    Inspired,
    Steady,
    Shaken,
    Breaking,
    Rout
};

UENUM(BlueprintType)
enum class EDASquadDoctrine : uint8
{
    Hold,
    Patrol,
    DefendCivilians,
    ProtectInfrastructure,
    AdvanceCautiously,
    AggressiveAssault,
    AvoidStructuralDamage
};

UENUM(BlueprintType)
enum class EDASquadFormation : uint8
{
    Line,
    Column,
    Wedge,
    Spread
};

/**
 * Runtime tactical state for one selectable squad or vehicle. The command
 * subsystem alone may mark it directly controlled; every other squad follows
 * its assigned doctrine.
 */
UCLASS(BlueprintType)
class DOMINIONGAMEPLAY_API UDASquadEntity final : public UObject
{
    GENERATED_BODY()

public:
    static constexpr float MinimumMorale = 0.f;
    static constexpr float MaximumMorale = 100.f;

    /** Classification is mutable only before a command subsystem registers this entity. */
    bool Initialize(bool bInVehicle, EDASquadDoctrine InDoctrine = EDASquadDoctrine::Hold);

    bool IsVehicle() const { return bVehicle; }
    bool IsDirectlyControlled() const { return bDirectlyControlled; }
    EDASquadDoctrine GetDoctrine() const { return Doctrine; }
    EDASquadFormation GetFormation() const { return Formation; }
    float GetMorale() const { return Morale; }
    EDAMoraleState GetMoraleState() const;
    float GetSuppression() const { return Suppression; }
    float GetSupply() const { return Supply; }
    EDACommandOrder GetActiveOrder() const { return ActiveOrder; }
    FVector GetDestination() const { return Destination; }
    AActor* GetTarget() const { return Target.Get(); }

    void SetFormation(EDASquadFormation InFormation) { Formation = InFormation; }
    void SetMorale(float InMorale);
    void SetSuppression(float InSuppression);
    void SetSupply(float InSupply);
    void SetTarget(AActor* InTarget) { Target = InTarget; }
    void SetActiveOrder(EDACommandOrder InOrder, const FVector& InDestination);

private:
    friend class UDACommandSubsystem;

    bool ClaimCommandOwner(UDACommandSubsystem* InCommandOwner);
    void ReleaseCommandOwner(UDACommandSubsystem* InCommandOwner);
    bool IsCommandOwnedBy(const UDACommandSubsystem* InCommandOwner) const;
    void ResetToDoctrineFallback();
    void SetDirectlyControlled(bool bInDirectlyControlled) { bDirectlyControlled = bInDirectlyControlled; }

    UPROPERTY(Transient)
    bool bVehicle = false;

    UPROPERTY(Transient)
    bool bDirectlyControlled = false;

    // Store a UObject weak reference here so this public header does not need
    // a complete command-subsystem definition for TWeakObjectPtr operations.
    TWeakObjectPtr<UObject> CommandOwner;

    UPROPERTY(Transient)
    EDASquadDoctrine Doctrine = EDASquadDoctrine::Hold;

    UPROPERTY(Transient)
    EDASquadFormation Formation = EDASquadFormation::Line;

    UPROPERTY(Transient)
    FVector Destination = FVector::ZeroVector;

    UPROPERTY(Transient)
    TObjectPtr<AActor> Target = nullptr;

    UPROPERTY(Transient)
    float Morale = MaximumMorale;

    UPROPERTY(Transient)
    float Suppression = 0.f;

    UPROPERTY(Transient)
    float Supply = MaximumMorale;

    UPROPERTY(Transient)
    EDACommandOrder ActiveOrder = EDACommandOrder::Hold;
};
