#include "Units/DASquadEntity.h"

#include "Command/DACommandSubsystem.h"

bool UDASquadEntity::Initialize(const bool bInVehicle, const EDASquadDoctrine InDoctrine)
{
    if (CommandOwner.IsValid())
    {
        return false;
    }

    bVehicle = bInVehicle;
    Doctrine = InDoctrine;
    return true;
}

EDAMoraleState UDASquadEntity::GetMoraleState() const
{
    if (Morale >= 75.f)
    {
        return EDAMoraleState::Inspired;
    }
    if (Morale >= 50.f)
    {
        return EDAMoraleState::Steady;
    }
    if (Morale >= 25.f)
    {
        return EDAMoraleState::Shaken;
    }
    if (Morale > 0.f)
    {
        return EDAMoraleState::Breaking;
    }
    return EDAMoraleState::Rout;
}

void UDASquadEntity::SetMorale(const float InMorale)
{
    Morale = FMath::Clamp(InMorale, MinimumMorale, MaximumMorale);
}

void UDASquadEntity::SetSuppression(const float InSuppression)
{
    Suppression = FMath::Clamp(InSuppression, 0.f, 100.f);
}

void UDASquadEntity::SetSupply(const float InSupply)
{
    Supply = FMath::Clamp(InSupply, 0.f, 100.f);
}

void UDASquadEntity::SetActiveOrder(const EDACommandOrder InOrder, const FVector& InDestination)
{
    ActiveOrder = InOrder;
    Destination = InDestination;
}

bool UDASquadEntity::ClaimCommandOwner(UDACommandSubsystem* InCommandOwner)
{
    if (InCommandOwner == nullptr)
    {
        return false;
    }
    UObject* CandidateOwner = static_cast<UObject*>(InCommandOwner);
    if (CommandOwner.IsValid())
    {
        return CommandOwner.Get() == CandidateOwner;
    }

    CommandOwner = CandidateOwner;
    return true;
}

void UDASquadEntity::ReleaseCommandOwner(UDACommandSubsystem* InCommandOwner)
{
    if (CommandOwner.Get() == static_cast<UObject*>(InCommandOwner))
    {
        bDirectlyControlled = false;
        CommandOwner.Reset();
    }
}

bool UDASquadEntity::IsCommandOwnedBy(const UDACommandSubsystem* InCommandOwner) const
{
    return InCommandOwner != nullptr
        && CommandOwner.Get() == static_cast<const UObject*>(InCommandOwner);
}

void UDASquadEntity::ResetToDoctrineFallback()
{
    bDirectlyControlled = false;
    ActiveOrder = EDACommandOrder::Hold;
    Destination = FVector::ZeroVector;
    Target = nullptr;
}
