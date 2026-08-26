#include "Adjacency/DAAdjacencySubsystem.h"
#include "Misc/AutomationTest.h"

BEGIN_DEFINE_SPEC(FDAAdjacencySpec, "Dominion.Simulation.Adjacency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAAdjacencySpec)

namespace
{
    FDAWorldAssetId MakeAssetId(const uint32 A, const uint32 B, const uint32 C, const uint32 D)
    {
        return FDAWorldAssetId(FGuid(A, B, C, D));
    }
}

void FDAAdjacencySpec::Define()
{
    It("invalidates the local spatial bucket and recalculates a research annex bonus next to a cognitive operations tower", [this]()
    {
        FDAAdjacencySubsystem Adjacency;
        const FDAWorldAssetId CognitiveTower = MakeAssetId(1, 0, 0, 0);
        const FDAWorldAssetId DistantAsset = MakeAssetId(2, 0, 0, 0);
        const FDAWorldAssetId ResearchAnnex = MakeAssetId(3, 0, 0, 0);

        Adjacency.AddRule(TEXT("synara.research_annex"), TEXT("synara.cognitive_operations_tower"), TEXT("Adjacency.CognitiveResearch"), 0.20f);
        Adjacency.RegisterAsset(CognitiveTower, TEXT("synara.cognitive_operations_tower"), FIntPoint(1, 1));
        Adjacency.RegisterAsset(DistantAsset, TEXT("synara.corner_exchange"), FIntPoint(24, 24));
        Adjacency.RebuildForAsset(DistantAsset);

        Adjacency.RegisterAsset(ResearchAnnex, TEXT("synara.research_annex"), FIntPoint(2, 1));

        TestEqual("Only the local bucket's registered assets were invalidated", Adjacency.GetLastInvalidatedAssetIds().Num(), 2);
        TestTrue("Tower is invalidated with the new neighbor", Adjacency.GetLastInvalidatedAssetIds().Contains(CognitiveTower));
        TestTrue("Research annex is invalidated", Adjacency.GetLastInvalidatedAssetIds().Contains(ResearchAnnex));
        TestFalse("Distant bucket is untouched", Adjacency.GetLastInvalidatedAssetIds().Contains(DistantAsset));

        const TArray<FDAAdjacencyModifier> Modifiers = Adjacency.RebuildForAsset(ResearchAnnex);
        TestEqual("Research annex receives one cached adjacency modifier", Modifiers.Num(), 1);
        if (Modifiers.Num() == 1)
        {
            TestTrue("Modifier identifies the cognitive research relationship", Modifiers[0].Name == TEXT("Adjacency.CognitiveResearch"));
            TestEqual("Modifier applies the authored twenty percent", Modifiers[0].Amount, 0.20f);
        }
        TestEqual("Distant asset cache remains empty", Adjacency.GetCachedModifiers(DistantAsset).Num(), 0);
    });
}
