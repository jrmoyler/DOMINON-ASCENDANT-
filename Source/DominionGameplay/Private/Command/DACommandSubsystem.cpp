#include "Command/DACommandSubsystem.h"

bool UDACommandSubsystem::RegisterSquad(UDASquadEntity* Squad)
{
    if (Squad == nullptr || !Squad->ClaimCommandOwner(this))
    {
        return false;
    }
    if (!IsRegistered(Squad))
    {
        RegisteredSquads.Add(Squad);
    }
    return true;
}

bool UDACommandSubsystem::UnregisterSquad(UDASquadEntity* Squad)
{
    if (!IsRegistered(Squad))
    {
        return false;
    }

    DirectlySelectedSquads.Remove(Squad);
    if (DirectlySelectedVehicle == Squad)
    {
        DirectlySelectedVehicle = nullptr;
    }
    Squad->ResetToDoctrineFallback();
    RegisteredSquads.Remove(Squad);
    Squad->ReleaseCommandOwner(this);
    return true;
}

bool UDACommandSubsystem::TrySelectDirectly(UDASquadEntity* Squad)
{
    if (!IsRegistered(Squad))
    {
        return false;
    }
    if (Squad->IsDirectlyControlled())
    {
        return true;
    }

    if (Squad->IsVehicle())
    {
        if (DirectlySelectedVehicle != nullptr)
        {
            return false;
        }
        DirectlySelectedVehicle = Squad;
    }
    else
    {
        if (DirectlySelectedSquads.Num() >= MaximumDirectSquads)
        {
            return false;
        }
        DirectlySelectedSquads.Add(Squad);
    }

    Squad->SetDirectlyControlled(true);
    return true;
}

bool UDACommandSubsystem::ReleaseDirectSelection(UDASquadEntity* Squad)
{
    if (!IsRegistered(Squad) || !Squad->IsDirectlyControlled())
    {
        return false;
    }

    if (Squad->IsVehicle())
    {
        if (DirectlySelectedVehicle != Squad)
        {
            return false;
        }
        DirectlySelectedVehicle = nullptr;
    }
    else
    {
        DirectlySelectedSquads.Remove(Squad);
    }

    Squad->ResetToDoctrineFallback();
    return true;
}

bool UDACommandSubsystem::IssueOrder(
    UDASquadEntity* Squad,
    const EDACommandOrder Order,
    const FVector& Destination)
{
    if (!IsRegistered(Squad) || !Squad->IsDirectlyControlled())
    {
        return false;
    }

    Squad->SetActiveOrder(Order, Destination);
    return true;
}

bool UDACommandSubsystem::AwardControlZoneCommandPoints(const int32 Amount)
{
    if (Amount <= 0)
    {
        return false;
    }

    CommandPoints += FMath::Min(Amount, BaselineMaximumCommandPoints - CommandPoints);
    return true;
}

bool UDACommandSubsystem::TrySpendCommandPoints(const int32 Cost)
{
    if (Cost <= 0 || Cost > CommandPoints)
    {
        return false;
    }

    CommandPoints -= Cost;
    return true;
}

bool UDACommandSubsystem::IsRegistered(const UDASquadEntity* Squad) const
{
    return Squad != nullptr
        && Squad->IsCommandOwnedBy(this)
        && RegisteredSquads.Contains(Squad);
}
