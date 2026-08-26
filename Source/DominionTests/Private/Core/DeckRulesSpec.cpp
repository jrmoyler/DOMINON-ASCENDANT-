#include "Cards/DACollectionState.h"
#include "Cards/DADeckRules.h"
#include "Cards/DADeckState.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDADeckRulesSpec, "Dominion.Core.DeckRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDADeckRulesSpec)

struct FDeckDefinitionFixture
{
    UDA_CardDefinition* Definition = nullptr;
    FPrimaryAssetId AssetId;
    UDAContentRegistrySubsystem* Registry = nullptr;
};

class FDeckRulesSpecFixture
{
public:
    static TMap<UDAContentRegistrySubsystem*, TArray<UDA_CardDefinition*>>& DefinitionsByRegistry()
    {
        static TMap<UDAContentRegistrySubsystem*, TArray<UDA_CardDefinition*>> Definitions;
        return Definitions;
    }

    static FDeckDefinitionFixture AddDefinition(
        UDAContentRegistrySubsystem& Registry,
        const FName DefinitionId,
        const EDARarity Rarity,
        const int32 Suffix)
    {
        const FName ObjectName(*FString::Printf(TEXT("DeckRuleDefinition_%d"), Suffix));
        UDA_CardDefinition* Definition = NewObject<UDA_CardDefinition>(GetTransientPackage(), ObjectName);
        Definition->DefinitionId = DefinitionId;
        Definition->Rarity = Rarity;
        Definition->bPlaceable = false;

        const FPrimaryAssetId AssetId = Definition->GetPrimaryAssetId();
        TArray<UDA_CardDefinition*>& Definitions = DefinitionsByRegistry().FindOrAdd(&Registry);
        Definitions.Add(Definition);
        TArray<FText> Errors;
        Registry.RebuildFromDefinitions(Definitions, Errors);

        return { Definition, AssetId, &Registry };
    }

    static void RemoveDefinition(const FDeckDefinitionFixture& Fixture)
    {
        if (Fixture.Registry)
        {
            DefinitionsByRegistry().Remove(Fixture.Registry);
        }
    }

    static bool ValidateDeck(const FDADeckState& Deck, const UDAContentRegistrySubsystem& Registry)
    {
        TArray<FText> Errors;
        return FDADeckRules::Validate(Deck, Registry, Errors);
    }

    static bool HasCopyLimitError(const FDADeckState& Deck, const UDAContentRegistrySubsystem& Registry)
    {
        TArray<FText> Errors;
        FDADeckRules::Validate(Deck, Registry, Errors);
        return Errors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("allows at most"));
        });
    }

    static FDADeckState CreateDeck(const FDACollectionState& Collection, const TArray<FGuid>& InstanceIds)
    {
        FDADeckState Deck;
        Deck.BindCollection(Collection);
        Deck.SetInstanceIds(InstanceIds);
        return Deck;
    }
};

void FDADeckRulesSpec::Define()
{
    Describe("validation", [this]()
    {
        It("rejects a fourth Common instance of one definition", [this]()
        {
            UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
            const FDeckDefinitionFixture Fixture = FDeckRulesSpecFixture::AddDefinition(*Registry, FName("test.common"), EDARarity::Common, 1);

            FDACollectionState Collection;
            TArray<FGuid> InstanceIds;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                InstanceIds.Add(Collection.AddInstance(Fixture.Definition->DefinitionId, EDAAcquisitionSource::StarterDeck));
            }

            TestFalse("Fourth Common copy is invalid", FDeckRulesSpecFixture::ValidateDeck(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            TestTrue("Common copy limit reported", FDeckRulesSpecFixture::HasCopyLimitError(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            FDeckRulesSpecFixture::RemoveDefinition(Fixture);
        });

        It("rejects a fourth Specialized instance of one definition", [this]()
        {
            UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
            const FDeckDefinitionFixture Fixture = FDeckRulesSpecFixture::AddDefinition(*Registry, FName("test.specialized"), EDARarity::Specialized, 2);

            FDACollectionState Collection;
            TArray<FGuid> InstanceIds;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                InstanceIds.Add(Collection.AddInstance(Fixture.Definition->DefinitionId, EDAAcquisitionSource::StarterDeck));
            }

            TestFalse("Fourth Specialized copy is invalid", FDeckRulesSpecFixture::ValidateDeck(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            TestTrue("Specialized copy limit reported", FDeckRulesSpecFixture::HasCopyLimitError(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            FDeckRulesSpecFixture::RemoveDefinition(Fixture);
        });

        It("rejects a third Elite instance of one definition", [this]()
        {
            UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
            const FDeckDefinitionFixture Fixture = FDeckRulesSpecFixture::AddDefinition(*Registry, FName("test.elite"), EDARarity::Elite, 3);

            FDACollectionState Collection;
            TArray<FGuid> InstanceIds;
            for (int32 Index = 0; Index < 3; ++Index)
            {
                InstanceIds.Add(Collection.AddInstance(Fixture.Definition->DefinitionId, EDAAcquisitionSource::StarterDeck));
            }

            TestFalse("Third Elite copy is invalid", FDeckRulesSpecFixture::ValidateDeck(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            TestTrue("Elite copy limit reported", FDeckRulesSpecFixture::HasCopyLimitError(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            FDeckRulesSpecFixture::RemoveDefinition(Fixture);
        });

        It("rejects second copies of single-copy rarities", [this]()
        {
            const TArray<EDARarity> SingleCopyRarities = {
                EDARarity::Legendary,
                EDARarity::Mythic,
                EDARarity::Wonder,
                EDARarity::Leader
            };

            for (int32 Index = 0; Index < SingleCopyRarities.Num(); ++Index)
            {
                UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
                const FDeckDefinitionFixture Fixture = FDeckRulesSpecFixture::AddDefinition(
                    *Registry,
                    FName(*FString::Printf(TEXT("test.single_copy_%d"), Index)),
                    SingleCopyRarities[Index],
                    10 + Index);

                FDACollectionState Collection;
                const TArray<FGuid> InstanceIds = {
                    Collection.AddInstance(Fixture.Definition->DefinitionId, EDAAcquisitionSource::StarterDeck),
                    Collection.AddInstance(Fixture.Definition->DefinitionId, EDAAcquisitionSource::StarterDeck)
                };

                TestFalse("Second single-copy rarity instance is invalid", FDeckRulesSpecFixture::ValidateDeck(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
                TestTrue("Single-copy rarity limit reported", FDeckRulesSpecFixture::HasCopyLimitError(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
                FDeckRulesSpecFixture::RemoveDefinition(Fixture);
            }
        });

        It("accepts exactly 60 owned instances within copy limits", [this]()
        {
            UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
            FDACollectionState Collection;
            TArray<FGuid> InstanceIds;
            TArray<FDeckDefinitionFixture> Fixtures;

            for (int32 DefinitionIndex = 0; DefinitionIndex < 20; ++DefinitionIndex)
            {
                const FDeckDefinitionFixture Fixture = FDeckRulesSpecFixture::AddDefinition(
                    *Registry,
                    FName(*FString::Printf(TEXT("test.sixty_%d"), DefinitionIndex)),
                    EDARarity::Common,
                    100 + DefinitionIndex);
                Fixtures.Add(Fixture);

                for (int32 CopyIndex = 0; CopyIndex < 3; ++CopyIndex)
                {
                    InstanceIds.Add(Collection.AddInstance(Fixture.Definition->DefinitionId, EDAAcquisitionSource::StarterDeck));
                }
            }

            TestTrue("Exactly 60 cards is valid", FDeckRulesSpecFixture::ValidateDeck(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            for (const FDeckDefinitionFixture& Fixture : Fixtures)
            {
                FDeckRulesSpecFixture::RemoveDefinition(Fixture);
            }
        });

        It("rejects a 59-card deck", [this]()
        {
            UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
            FDACollectionState Collection;
            TArray<FGuid> InstanceIds;
            TArray<FDeckDefinitionFixture> Fixtures;

            for (int32 DefinitionIndex = 0; DefinitionIndex < 20; ++DefinitionIndex)
            {
                const FDeckDefinitionFixture Fixture = FDeckRulesSpecFixture::AddDefinition(
                    *Registry,
                    FName(*FString::Printf(TEXT("test.fifty_nine_%d"), DefinitionIndex)),
                    EDARarity::Common,
                    200 + DefinitionIndex);
                Fixtures.Add(Fixture);

                const int32 Copies = DefinitionIndex == 19 ? 2 : 3;
                for (int32 CopyIndex = 0; CopyIndex < Copies; ++CopyIndex)
                {
                    InstanceIds.Add(Collection.AddInstance(Fixture.Definition->DefinitionId, EDAAcquisitionSource::StarterDeck));
                }
            }

            TestFalse("59 cards is invalid", FDeckRulesSpecFixture::ValidateDeck(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            for (const FDeckDefinitionFixture& Fixture : Fixtures)
            {
                FDeckRulesSpecFixture::RemoveDefinition(Fixture);
            }
        });

        It("rejects a 61-card deck", [this]()
        {
            UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
            FDACollectionState Collection;
            TArray<FGuid> InstanceIds;
            TArray<FDeckDefinitionFixture> Fixtures;

            for (int32 DefinitionIndex = 0; DefinitionIndex < 21; ++DefinitionIndex)
            {
                const FDeckDefinitionFixture Fixture = FDeckRulesSpecFixture::AddDefinition(
                    *Registry,
                    FName(*FString::Printf(TEXT("test.sixty_one_%d"), DefinitionIndex)),
                    EDARarity::Common,
                    300 + DefinitionIndex);
                Fixtures.Add(Fixture);

                const int32 Copies = DefinitionIndex == 20 ? 1 : 3;
                for (int32 CopyIndex = 0; CopyIndex < Copies; ++CopyIndex)
                {
                    InstanceIds.Add(Collection.AddInstance(Fixture.Definition->DefinitionId, EDAAcquisitionSource::StarterDeck));
                }
            }

            TestFalse("61 cards is invalid", FDeckRulesSpecFixture::ValidateDeck(FDeckRulesSpecFixture::CreateDeck(Collection, InstanceIds), *Registry));
            for (const FDeckDefinitionFixture& Fixture : Fixtures)
            {
                FDeckRulesSpecFixture::RemoveDefinition(Fixture);
            }
        });
    });

    Describe("draw flow", [this]()
    {
        It("uses the campaign-seeded random stream for deterministic shuffles", [this]()
        {
            TArray<FGuid> InstanceIds;
            for (int32 Index = 0; Index < 10; ++Index)
            {
                InstanceIds.Add(FGuid::NewGuid());
            }

            FDADeckState FirstDeck;
            FirstDeck.SetInstanceIds(InstanceIds);
            FRandomStream FirstCampaignStream(91357);
            FirstDeck.Shuffle(FirstCampaignStream);

            FDADeckState SecondDeck;
            SecondDeck.SetInstanceIds(InstanceIds);
            FRandomStream SecondCampaignStream(91357);
            SecondDeck.Shuffle(SecondCampaignStream);

            TestTrue("Equal campaign seeds produce equal draw orders", FirstDeck.GetDrawPile() == SecondDeck.GetDrawPile());
        });

        It("draws an opening hand of seven", [this]()
        {
            FDADeckState Deck;
            for (int32 Index = 0; Index < 10; ++Index)
            {
                Deck.AddInstanceId(FGuid::NewGuid());
            }

            Deck.DrawOpeningHand();
            TestEqual("Opening hand size", Deck.GetHand().Num(), 7);
        });

        It("fills reserve at the hand cap and bottoms its oldest card when full", [this]()
        {
            FDADeckState Deck;
            TArray<FGuid> InstanceIds;
            for (int32 Index = 0; Index < 15; ++Index)
            {
                InstanceIds.Add(FGuid::NewGuid());
            }
            Deck.SetInstanceIds(InstanceIds);

            for (int32 Index = 0; Index < 10; ++Index)
            {
                Deck.DrawForCycle();
            }
            TestEqual("Hand remains capped", Deck.GetHand().Num(), 10);

            for (int32 Index = 0; Index < 3; ++Index)
            {
                Deck.DrawForCycle();
            }
            TestEqual("Reserve reaches cap", Deck.GetReserveQueue().Num(), 3);

            const FGuid OldestReserve = Deck.GetReserveQueue()[0];
            Deck.DrawForCycle();
            TestEqual("Reserve remains capped", Deck.GetReserveQueue().Num(), 3);
            TestFalse("Oldest reserve card was removed", Deck.GetReserveQueue().Contains(OldestReserve));
            TestEqual("Oldest reserve card moved to deck bottom", Deck.GetDrawPile()[0], OldestReserve);
        });

        It("moves placement and recovery atomically through the deployed zone", [this]()
        {
            FDADeckState Deck;
            TArray<FGuid> InstanceIds;
            for (int32 Index = 0; Index < 10; ++Index)
            {
                InstanceIds.Add(FGuid::NewGuid());
            }
            Deck.SetInstanceIds(InstanceIds);
            Deck.DrawOpeningHand();
            const FGuid Placed = Deck.GetHand()[0];
            FString Error;

            TestTrue("A hand member deploys exactly once",
                Deck.TryDeployFromHand(Placed, Error));
            TestFalse("A deployed member cannot deploy twice",
                Deck.TryDeployFromHand(Placed, Error));
            TestFalse("Deployment removes the card from hand",
                Deck.GetHand().Contains(Placed));
            TestTrue("Deployment persists in its explicit zone",
                Deck.GetDeployed().Contains(Placed));
            TestTrue("A later draw cannot draw the deployed member",
                Deck.DrawForCycle().IsSet()
                    && !Deck.GetHand().Contains(Placed)
                    && !Deck.GetDrawPile().Contains(Placed)
                    && !Deck.GetReserveQueue().Contains(Placed));

            TestTrue("Recovery removes the deployed membership atomically",
                Deck.TryRecoverDeployedInstance(Placed, Error));
            TestFalse("A recovered member is no longer deployed",
                Deck.GetDeployed().Contains(Placed));
            TestTrue("Recovery returns the member to one playable zone",
                Deck.GetHand().Contains(Placed));
            TestEqual("Every member remains in exactly one zone",
                Deck.GetDrawPile().Num() + Deck.GetHand().Num()
                    + Deck.GetReserveQueue().Num() + Deck.GetDeployed().Num(),
                InstanceIds.Num());
        });
    });
}
