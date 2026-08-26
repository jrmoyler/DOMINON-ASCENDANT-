#include "Combat/DACoverSubsystem.h"

bool FDACoverSocket::Matches(const FDACoverSocket& Other) const
{
    return Id == Other.Id
        && CoverType == Other.CoverType
        && Source == Other.Source
        && Location.Equals(Other.Location);
}

bool UDACoverSubsystem::RegisterAuthoredCoverSocket(
    const FName Id,
    const EDACoverType CoverType,
    const FVector& Location)
{
    return RegisterCover(Id, CoverType, EDACoverSource::Authored, Location);
}

bool UDACoverSubsystem::RegisterRuinCover(
    const FName Id,
    const EDACoverType CoverType,
    const FVector& Location)
{
    return RegisterCover(Id, CoverType, EDACoverSource::Ruin, Location);
}

bool UDACoverSubsystem::RegisterDeployableCover(
    const FName Id,
    const EDACoverType CoverType,
    const FVector& Location)
{
    return RegisterCover(Id, CoverType, EDACoverSource::Deployable, Location);
}

bool UDACoverSubsystem::UnregisterAuthoredCoverSocket(const FName Id)
{
    return UnregisterCover(Id, EDACoverSource::Authored);
}

bool UDACoverSubsystem::UnregisterRuinCover(const FName Id)
{
    return UnregisterCover(Id, EDACoverSource::Ruin);
}

bool UDACoverSubsystem::UnregisterDeployableCover(const FName Id)
{
    return UnregisterCover(Id, EDACoverSource::Deployable);
}

const FDACoverSocket* UDACoverSubsystem::FindCoverSocket(const FName Id) const
{
    return CoverSockets.FindByPredicate([Id](const FDACoverSocket& Socket)
    {
        return Socket.Id == Id;
    });
}

bool UDACoverSubsystem::RegisterCover(
    const FName Id,
    const EDACoverType CoverType,
    const EDACoverSource Source,
    const FVector& Location)
{
    if (Id.IsNone() || static_cast<uint8>(CoverType) > static_cast<uint8>(EDACoverType::Destructible))
    {
        return false;
    }

    FDACoverSocket Candidate;
    Candidate.Id = Id;
    Candidate.CoverType = CoverType;
    Candidate.Source = Source;
    Candidate.Location = Location;
    const FDACoverSocket* Existing = FindCoverSocket(Id);
    if (Existing != nullptr)
    {
        return Existing->Matches(Candidate);
    }

    CoverSockets.Add(Candidate);
    return true;
}

bool UDACoverSubsystem::UnregisterCover(const FName Id, const EDACoverSource Source)
{
    const int32 ExistingIndex = CoverSockets.IndexOfByPredicate([Id](const FDACoverSocket& Socket)
    {
        return Socket.Id == Id;
    });
    if (ExistingIndex == INDEX_NONE || CoverSockets[ExistingIndex].Source != Source)
    {
        return false;
    }

    CoverSockets.RemoveAt(ExistingIndex);
    return true;
}
