#include "Narrative/DAPromiseLedger.h"

#include "Narrative/DAHistoryLedger.h"

namespace
{
    FString StableGuidKey(const FGuid& Guid)
    {
        return Guid.ToString(EGuidFormats::Digits);
    }

    void SortNames(TArray<FName>& Names)
    {
        Names.Sort(
            [](const FName Left, const FName Right)
            {
                return Left.LexicalLess(Right);
            });
    }

    bool AreNamesValid(const TArray<FName>& Names)
    {
        if (Names.IsEmpty())
        {
            return false;
        }
        TSet<FName> Seen;
        for (const FName Name : Names)
        {
            if (Name.IsNone() || Seen.Contains(Name))
            {
                return false;
            }
            Seen.Add(Name);
        }
        return true;
    }

    bool PromiseDefinitionsEqual(const FDAPromiseRecord& Left, const FDAPromiseRecord& Right)
    {
        return Left.PromiseId == Right.PromiseId
            && Left.PromiseDefinitionId == Right.PromiseDefinitionId
            && Left.PromiserId == Right.PromiserId
            && Left.ConflictActionTags == Right.ConflictActionTags
            && Left.FulfillmentActionTags == Right.FulfillmentActionTags
            && Left.CreatedWorldTick == Right.CreatedWorldTick;
    }

    bool ContainsExactTag(const TArray<FName>& CandidateTags, const TSet<FName>& ActionTags)
    {
        for (const FName Candidate : CandidateTags)
        {
            if (ActionTags.Contains(Candidate))
            {
                return true;
            }
        }
        return false;
    }

    FName FirstExactTag(const TArray<FName>& CandidateTags, const TSet<FName>& ActionTags)
    {
        TArray<FName> Matches;
        for (const FName Candidate : CandidateTags)
        {
            if (ActionTags.Contains(Candidate))
            {
                Matches.Add(Candidate);
            }
        }
        SortNames(Matches);
        return Matches.IsEmpty() ? NAME_None : Matches[0];
    }
}

bool FDAHistoryLedger::HasTag(const FDACampaignSnapshot& Campaign, const FName HistoryTag)
{
    return !HistoryTag.IsNone() && Campaign.HistoryTags.Contains(HistoryTag);
}

bool FDAHistoryLedger::RecordTags(
    FDACampaignSnapshot& InOutCampaign,
    const TArray<FName>& HistoryTags)
{
    if (!AreNamesValid(HistoryTags))
    {
        return false;
    }
    TArray<FName> CandidateTags = InOutCampaign.HistoryTags;
    for (const FName HistoryTag : HistoryTags)
    {
        CandidateTags.AddUnique(HistoryTag);
    }
    SortNames(CandidateTags);
    FDACampaignSnapshot CandidateCampaign = InOutCampaign;
    CandidateCampaign.HistoryTags = MoveTemp(CandidateTags);
    FString Error;
    if (!CandidateCampaign.Validate(Error))
    {
        return false;
    }
    InOutCampaign = MoveTemp(CandidateCampaign);
    return true;
}

FDAPromiseLedger::FDAPromiseLedger(FDACampaignSnapshot& InCampaign)
    : Campaign(InCampaign)
{
}

int64 FDAPromiseLedger::GetRevision() const
{
    return Campaign.NarrativeState.MutationRevision;
}

EDAPromiseMutationResult FDAPromiseLedger::RegisterPromise(const FDAPromiseRecord& Promise)
{
    FString CampaignError;
    if (!Campaign.Validate(CampaignError))
    {
        return EDAPromiseMutationResult::InvalidInput;
    }
    FDAPromiseRecord Normalized = Promise;
    SortNames(Normalized.ConflictActionTags);
    SortNames(Normalized.FulfillmentActionTags);
    if (Normalized.State != EDAPromiseState::Active
        || Normalized.ResolvedWorldTick != 0
        || !Normalized.ResolutionActionTag.IsNone()
        || Normalized.ResolutionActionId.IsValid()
        || Normalized.bLegacyResolutionWithoutAction
        || Normalized.LegacyResolutionSourceSchemaVersion != 0)
    {
        return EDAPromiseMutationResult::InvalidInput;
    }

    const FDAPromiseRecord* Existing = Campaign.NarrativeState.FindPromiseRecord(Normalized.PromiseId);
    if (Existing != nullptr)
    {
        FDAPromiseRecord NormalizedExisting = *Existing;
        SortNames(NormalizedExisting.ConflictActionTags);
        SortNames(NormalizedExisting.FulfillmentActionTags);
        return PromiseDefinitionsEqual(NormalizedExisting, Normalized)
            ? EDAPromiseMutationResult::AlreadyApplied
            : EDAPromiseMutationResult::InvalidInput;
    }

    if (Campaign.NarrativeState.MutationRevision == MAX_int64)
    {
        return EDAPromiseMutationResult::InvalidInput;
    }
    FDACampaignSnapshot Candidate = Campaign;
    Candidate.NarrativeState.PromiseRecords.Add(MoveTemp(Normalized));
    Candidate.NarrativeState.PromiseRecords.Sort(
        [](const FDAPromiseRecord& Left, const FDAPromiseRecord& Right)
        {
            return StableGuidKey(Left.PromiseId) < StableGuidKey(Right.PromiseId);
        });
    ++Candidate.NarrativeState.MutationRevision;
    FString Error;
    if (!Candidate.Validate(Error))
    {
        return EDAPromiseMutationResult::InvalidInput;
    }
    Campaign = MoveTemp(Candidate);
    return EDAPromiseMutationResult::Applied;
}

TArray<FDAPromiseRecord> FDAPromiseLedger::GetBrokenPromises(const TArray<FName>& ActionTags) const
{
    TSet<FName> ExactActionTags;
    for (const FName Tag : ActionTags)
    {
        if (!Tag.IsNone())
        {
            ExactActionTags.Add(Tag);
        }
    }

    TArray<FDAPromiseRecord> Broken;
    for (const FDAPromiseRecord& Promise : Campaign.NarrativeState.PromiseRecords)
    {
        if (Promise.State == EDAPromiseState::Active
            && ContainsExactTag(Promise.ConflictActionTags, ExactActionTags))
        {
            Broken.Add(Promise);
        }
    }
    Broken.Sort(
        [](const FDAPromiseRecord& Left, const FDAPromiseRecord& Right)
        {
            return StableGuidKey(Left.PromiseId) < StableGuidKey(Right.PromiseId);
        });
    return Broken;
}

EDAPromiseMutationResult FDAPromiseLedger::CommitAction(
    const FGuid ActionId,
    const TArray<FName>& ActionTags,
    const int64 WorldTick,
    const int64 ExpectedRevision,
    const bool bConfirmBrokenPromises)
{
    FString CampaignError;
    if (!Campaign.Validate(CampaignError))
    {
        return EDAPromiseMutationResult::InvalidInput;
    }
    if (!ActionId.IsValid() || WorldTick < 0 || !AreNamesValid(ActionTags))
    {
        return EDAPromiseMutationResult::InvalidInput;
    }
    TArray<FName> NormalizedActionTags = ActionTags;
    SortNames(NormalizedActionTags);
    if (const FDANarrativeActionRecord* ExistingAction = Campaign.NarrativeState.FindActionRecord(ActionId))
    {
        return !ExistingAction->bLegacyIdentityOnly
            && ExistingAction->WorldTick == WorldTick
            && ExistingAction->NormalizedActionTags == NormalizedActionTags
            ? EDAPromiseMutationResult::AlreadyApplied
            : EDAPromiseMutationResult::InvalidInput;
    }
    if (ExpectedRevision != Campaign.NarrativeState.MutationRevision)
    {
        return EDAPromiseMutationResult::StaleRevision;
    }
    TSet<FName> ExactActionTags;
    for (const FName ActionTag : NormalizedActionTags)
    {
        ExactActionTags.Add(ActionTag);
    }
    for (const FDAPromiseRecord& Promise : Campaign.NarrativeState.PromiseRecords)
    {
        if (Promise.State == EDAPromiseState::Active
            && ContainsExactTag(Promise.ConflictActionTags, ExactActionTags)
            && ContainsExactTag(Promise.FulfillmentActionTags, ExactActionTags))
        {
            return EDAPromiseMutationResult::InvalidInput;
        }
    }
    if (!bConfirmBrokenPromises && !GetBrokenPromises(ActionTags).IsEmpty())
    {
        return EDAPromiseMutationResult::RequiresConfirmation;
    }
    if (Campaign.NarrativeState.MutationRevision == MAX_int64)
    {
        return EDAPromiseMutationResult::InvalidInput;
    }

    FDACampaignSnapshot Candidate = Campaign;
    FDANarrativeActionRecord ActionRecord;
    ActionRecord.ActionId = ActionId;
    ActionRecord.NormalizedActionTags = NormalizedActionTags;
    ActionRecord.WorldTick = WorldTick;
    for (FDAPromiseRecord& Promise : Candidate.NarrativeState.PromiseRecords)
    {
        if (Promise.State != EDAPromiseState::Active)
        {
            continue;
        }
        const FName FulfillmentTag = FirstExactTag(Promise.FulfillmentActionTags, ExactActionTags);
        const FName ConflictTag = FirstExactTag(Promise.ConflictActionTags, ExactActionTags);
        if (!FulfillmentTag.IsNone())
        {
            Promise.State = EDAPromiseState::Fulfilled;
            Promise.ResolutionActionTag = FulfillmentTag;
            Promise.ResolutionActionId = ActionId;
            Promise.ResolvedWorldTick = WorldTick;
            ActionRecord.FulfilledPromiseIds.Add(Promise.PromiseId);
        }
        else if (!ConflictTag.IsNone())
        {
            Promise.State = EDAPromiseState::Breached;
            Promise.ResolutionActionTag = ConflictTag;
            Promise.ResolutionActionId = ActionId;
            Promise.ResolvedWorldTick = WorldTick;
            ActionRecord.BreachedPromiseIds.Add(Promise.PromiseId);
        }
    }

    ActionRecord.FulfilledPromiseIds.Sort([](const FGuid& Left, const FGuid& Right)
    {
        return StableGuidKey(Left) < StableGuidKey(Right);
    });
    ActionRecord.BreachedPromiseIds.Sort([](const FGuid& Left, const FGuid& Right)
    {
        return StableGuidKey(Left) < StableGuidKey(Right);
    });
    Candidate.NarrativeState.ActionRecords.Add(MoveTemp(ActionRecord));
    Candidate.NarrativeState.ActionRecords.Sort(
        [](const FDANarrativeActionRecord& Left, const FDANarrativeActionRecord& Right)
        {
            return StableGuidKey(Left.ActionId) < StableGuidKey(Right.ActionId);
        });
    ++Candidate.NarrativeState.MutationRevision;
    if (!FDAHistoryLedger::RecordTags(Candidate, ActionTags))
    {
        return EDAPromiseMutationResult::InvalidInput;
    }
    FString Error;
    if (!Candidate.Validate(Error))
    {
        return EDAPromiseMutationResult::InvalidInput;
    }

    Campaign = MoveTemp(Candidate);
    return EDAPromiseMutationResult::Applied;
}
