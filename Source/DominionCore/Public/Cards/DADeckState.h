#pragma once

#include "Cards/DACollectionState.h"
#include "Math/RandomStream.h"
#include "Misc/Optional.h"

#include "DADeckState.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONCORE_API FDADeckState
{
    GENERATED_BODY()

    static constexpr int32 RequiredDeckSize = 60;
    static constexpr int32 OpeningHandSize = 7;
    static constexpr int32 HandCap = 10;
    static constexpr int32 ReserveCap = 3;

    void BindCollection(const FDACollectionState& InCollection)
    {
        Collection = &InCollection;
    }

    const FDACollectionState* GetCollection() const
    {
        return Collection;
    }

    void SetInstanceIds(const TArray<FGuid>& InInstanceIds)
    {
        InstanceIds = InInstanceIds;
        DrawPile = InInstanceIds;
        Hand.Reset();
        ReserveQueue.Reset();
        Deployed.Reset();
    }

    void AddInstanceId(const FGuid InstanceId)
    {
        InstanceIds.Add(InstanceId);
        DrawPile.Add(InstanceId);
    }

    /** Atomic legal deck edit: membership and the active zone change together, never via 59/61 states. */
    bool TrySwapInstance(const FGuid OutgoingInstanceId, const FGuid IncomingInstanceId,
        FString& OutError)
    {
        const FCardInstance* Incoming = Collection == nullptr
            ? nullptr : Collection->FindInstance(IncomingInstanceId);
        if (!OutgoingInstanceId.IsValid() || !IncomingInstanceId.IsValid()
            || OutgoingInstanceId == IncomingInstanceId
            || InstanceIds.Num() != RequiredDeckSize
            || !InstanceIds.Contains(OutgoingInstanceId)
            || InstanceIds.Contains(IncomingInstanceId)
            || Incoming == nullptr || Incoming->RecoveryState != EDARecoveryState::Available)
        {
            OutError = TEXT("Atomic deck swap requires one member and one available owned non-member.");
            return false;
        }
        const int32 MemberIndex = InstanceIds.Find(OutgoingInstanceId);
        InstanceIds[MemberIndex] = IncomingInstanceId;
        if (Deployed.Contains(OutgoingInstanceId))
        {
            InstanceIds[MemberIndex] = OutgoingInstanceId;
            OutError = TEXT("A deployed card must be recovered before it can leave the deck.");
            return false;
        }
        TArray<FGuid>* Zones[] = {&DrawPile, &Hand, &ReserveQueue, &Deployed};
        int32 ZoneMatches = 0;
        for (TArray<FGuid>* Zone : Zones)
        {
            const int32 ZoneIndex = Zone->Find(OutgoingInstanceId);
            if (ZoneIndex != INDEX_NONE)
            {
                (*Zone)[ZoneIndex] = IncomingInstanceId;
                ++ZoneMatches;
            }
        }
        if (ZoneMatches != 1)
        {
            InstanceIds[MemberIndex] = OutgoingInstanceId;
            for (TArray<FGuid>* Zone : Zones)
            {
                const int32 ZoneIndex = Zone->Find(IncomingInstanceId);
                if (ZoneIndex != INDEX_NONE) (*Zone)[ZoneIndex] = OutgoingInstanceId;
            }
            OutError = TEXT("Atomic deck swap requires exact unique source-zone membership.");
            return false;
        }
        OutError.Reset();
        return true;
    }

    const TArray<FGuid>& GetInstanceIds() const
    {
        return InstanceIds;
    }

    const TArray<FGuid>& GetDrawPile() const
    {
        return DrawPile;
    }

    const TArray<FGuid>& GetHand() const
    {
        return Hand;
    }

    const TArray<FGuid>& GetReserveQueue() const
    {
        return ReserveQueue;
    }

    const TArray<FGuid>& GetDeployed() const
    {
        return Deployed;
    }

    /** Atomically transfers an owned deck card from the playable hand to world deployment. */
    bool TryDeployFromHand(const FGuid InstanceId, FString& OutError)
    {
        if (!InstanceIds.Contains(InstanceId) || Hand.Find(InstanceId) == INDEX_NONE
            || Deployed.Contains(InstanceId))
        {
            OutError = TEXT("Placement requires one unique card in the authoritative hand.");
            return false;
        }
        Hand.RemoveSingle(InstanceId);
        Deployed.Add(InstanceId);
        OutError.Reset();
        return true;
    }

    /** Recovers a demolished/cancelled deployment without ever leaving the 60-card partition. */
    bool TryRecoverDeployedInstance(const FGuid InstanceId, FString& OutError)
    {
        if (!InstanceIds.Contains(InstanceId) || Deployed.Find(InstanceId) == INDEX_NONE)
        {
            OutError = TEXT("Recovery requires one deployed member of the authoritative deck.");
            return false;
        }
        Deployed.RemoveSingle(InstanceId);
        if (Hand.Num() < HandCap)
        {
            Hand.Add(InstanceId);
        }
        else if (ReserveQueue.Num() < ReserveCap)
        {
            ReserveQueue.Add(InstanceId);
        }
        else
        {
            DrawPile.Add(InstanceId);
        }
        OutError.Reset();
        return true;
    }

    /** Schema migration only: transfers a linked member from its legacy zone to Deployed. */
    bool TryRestoreDeployedInstance(const FGuid InstanceId, FString& OutError)
    {
        if (!InstanceIds.Contains(InstanceId) || Deployed.Contains(InstanceId))
        {
            OutError = TEXT("Migration requires a unique undeployed deck member.");
            return false;
        }
        TArray<FGuid>* LegacyZones[] = {&DrawPile, &Hand, &ReserveQueue};
        int32 Matches = 0;
        for (TArray<FGuid>* Zone : LegacyZones)
        {
            Matches += Zone->RemoveSingle(InstanceId);
        }
        if (Matches != 1)
        {
            for (TArray<FGuid>* Zone : LegacyZones)
            {
                Zone->RemoveSingle(InstanceId);
            }
            DrawPile.Add(InstanceId);
            OutError = TEXT("Migration found ambiguous legacy deck-zone membership.");
            return false;
        }
        Deployed.Add(InstanceId);
        OutError.Reset();
        return true;
    }

    void Shuffle(FRandomStream& CampaignRandomStream)
    {
        for (int32 Index = DrawPile.Num() - 1; Index > 0; --Index)
        {
            DrawPile.Swap(Index, CampaignRandomStream.RandRange(0, Index));
        }
    }

    TOptional<FGuid> Draw()
    {
        if (DrawPile.IsEmpty())
        {
            return {};
        }

        return DrawPile.Pop(EAllowShrinking::No);
    }

    TOptional<FGuid> DrawForCycle()
    {
        const TOptional<FGuid> DrawnInstanceId = Draw();
        if (!DrawnInstanceId.IsSet())
        {
            return {};
        }

        if (Hand.Num() < HandCap)
        {
            Hand.Add(DrawnInstanceId.GetValue());
        }
        else if (ReserveQueue.Num() < ReserveCap)
        {
            ReserveQueue.Add(DrawnInstanceId.GetValue());
        }
        else
        {
            const FGuid OldestReserveInstanceId = ReserveQueue[0];
            ReserveQueue.RemoveAt(0, 1, EAllowShrinking::No);
            DrawPile.Insert(OldestReserveInstanceId, 0);
            ReserveQueue.Add(DrawnInstanceId.GetValue());
        }

        return DrawnInstanceId;
    }

    void DrawOpeningHand()
    {
        for (int32 Index = 0; Index < OpeningHandSize; ++Index)
        {
            if (!DrawForCycle().IsSet())
            {
                return;
            }
        }
    }

private:
    // Runtime binding only; save data remains ID-only and rebinds to its owning collection after load.
    const FDACollectionState* Collection = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TArray<FGuid> InstanceIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TArray<FGuid> DrawPile;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TArray<FGuid> Hand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TArray<FGuid> ReserveQueue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TArray<FGuid> Deployed;
};
