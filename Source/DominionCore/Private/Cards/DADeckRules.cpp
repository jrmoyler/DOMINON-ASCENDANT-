#include "Cards/DADeckRules.h"

#include "Cards/DACollectionState.h"
#include "Cards/DADeckState.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentRegistrySubsystem.h"

#define LOCTEXT_NAMESPACE "DADeckRules"

bool FDADeckRules::Validate(
    const FDADeckState& Deck,
    const UDAContentRegistrySubsystem& ContentRegistry,
    TArray<FText>& Errors)
{
    const int32 InitialErrorCount = Errors.Num();

    if (Deck.GetInstanceIds().Num() != FDADeckState::RequiredDeckSize)
    {
        Errors.Add(FText::Format(
            LOCTEXT("InvalidDeckSize", "A city deck must contain exactly {0} card instances; it contains {1}."),
            FDADeckState::RequiredDeckSize,
            Deck.GetInstanceIds().Num()));
    }

    const FDACollectionState* Collection = Deck.GetCollection();
    if (!Collection)
    {
        Errors.Add(LOCTEXT("UnboundCollection", "Deck validation requires the deck to be bound to its owning collection."));
        return false;
    }

    TSet<FGuid> SeenInstanceIds;
    TMap<FName, int32> CopiesByDefinitionId;
    for (const FGuid InstanceId : Deck.GetInstanceIds())
    {
        if (SeenInstanceIds.Contains(InstanceId))
        {
            Errors.Add(FText::Format(
                LOCTEXT("DuplicateInstance", "Card instance '{0}' appears more than once in this deck."),
                FText::FromString(InstanceId.ToString())));
            continue;
        }
        SeenInstanceIds.Add(InstanceId);

        const FCardInstance* Instance = Collection->FindInstance(InstanceId);
        if (!Instance)
        {
            Errors.Add(FText::Format(
                LOCTEXT("UnownedInstance", "Card instance '{0}' is not owned by the deck's collection."),
                FText::FromString(InstanceId.ToString())));
            continue;
        }

        UDA_CardDefinition* Definition = ContentRegistry.GetCardDefinition(Instance->DefinitionId);
        if (!Definition)
        {
            Errors.Add(FText::Format(
                LOCTEXT("MissingDefinition", "Card instance '{0}' references missing definition '{1}'."),
                FText::FromString(InstanceId.ToString()),
                FText::FromName(Instance->DefinitionId)));
            continue;
        }

        const int32 CopyCount = CopiesByDefinitionId.FindOrAdd(Instance->DefinitionId) + 1;
        CopiesByDefinitionId[Instance->DefinitionId] = CopyCount;

        const int32 CopyLimit = GetCopyLimit(*Definition);
        if (CopyLimit <= 0)
        {
            Errors.Add(FText::Format(
                LOCTEXT("UnsupportedRarity", "Card definition '{0}' has a rarity without a frozen deck copy limit."),
                FText::FromName(Instance->DefinitionId)));
            continue;
        }

        if (CopyCount > CopyLimit)
        {
            Errors.Add(FText::Format(
                LOCTEXT("CopyLimitExceeded", "Card definition '{0}' allows at most {1} deck copies but has {2}."),
                FText::FromName(Instance->DefinitionId),
                CopyLimit,
                CopyCount));
        }
    }

    return Errors.Num() == InitialErrorCount;
}

int32 FDADeckRules::GetCopyLimit(const UDA_CardDefinition& Definition)
{
    switch (Definition.Rarity)
    {
    case EDARarity::Common:
    case EDARarity::Specialized:
        return 3;
    case EDARarity::Elite:
        return 2;
    case EDARarity::Legendary:
    case EDARarity::Mythic:
    case EDARarity::Wonder:
    case EDARarity::Leader:
        return 1;
    default:
        return 0;
    }
}

#undef LOCTEXT_NAMESPACE
