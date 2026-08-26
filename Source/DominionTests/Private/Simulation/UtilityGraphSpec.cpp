#include "Networks/DAUtilityNetwork.h"
#include "Networks/DARoadGraph.h"
#include "Misc/AutomationTest.h"

BEGIN_DEFINE_SPEC(FDAUtilityGraphSpec, "Dominion.Simulation.UtilityGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAUtilityGraphSpec)

namespace
{
    FDAWorldAssetId MakeAssetId(const uint32 A, const uint32 B, const uint32 C, const uint32 D)
    {
        return FDAWorldAssetId(FGuid(A, B, C, D));
    }
}

void FDAUtilityGraphSpec::Define()
{
    It("fully supplies connected power demand until the microgrid capacity is exceeded", [this]()
    {
        FDAUtilityNetwork Network;
        const FDAWorldAssetId Microgrid = MakeAssetId(1, 0, 0, 0);
        const FDAWorldAssetId FirstFacility = MakeAssetId(2, 0, 0, 0);
        const FDAWorldAssetId SecondFacility = MakeAssetId(3, 0, 0, 0);
        const FDAWorldAssetId ThirdFacility = MakeAssetId(4, 0, 0, 0);

        Network.RegisterNode(EDAUtilityType::Power, Microgrid, 25.f, 0.f);
        Network.RegisterNode(EDAUtilityType::Power, FirstFacility, 0.f, 10.f);
        Network.RegisterNode(EDAUtilityType::Power, SecondFacility, 0.f, 12.f);
        Network.ConnectNodes(EDAUtilityType::Power, Microgrid, FirstFacility);
        Network.ConnectNodes(EDAUtilityType::Power, Microgrid, SecondFacility);

        TestTrue("First facility is fully supplied", Network.ResolveUtility(EDAUtilityType::Power, FirstFacility).State == EDAUtilityState::FullySupplied);
        TestTrue("Second facility is fully supplied", Network.ResolveUtility(EDAUtilityType::Power, SecondFacility).State == EDAUtilityState::FullySupplied);

        Network.RegisterNode(EDAUtilityType::Power, ThirdFacility, 0.f, 10.f);
        Network.ConnectNodes(EDAUtilityType::Power, Microgrid, ThirdFacility);

        const FDAUtilityResolution ThirdResolution = Network.ResolveUtility(EDAUtilityType::Power, ThirdFacility);
        TestTrue("Third facility reports a non-offline deficit", ThirdResolution.State != EDAUtilityState::FullySupplied && ThirdResolution.State != EDAUtilityState::Offline);
        TestEqual("Third facility receives the remaining connected capacity", ThirdResolution.SuppliedCapacity, 3.f);
    });

    It("removes data coverage only from the component disconnected by a disabled neural relay", [this]()
    {
        FDAUtilityNetwork Network;
        const FDAWorldAssetId DataSource = MakeAssetId(10, 0, 0, 0);
        const FDAWorldAssetId NeuralRelay = MakeAssetId(11, 0, 0, 0);
        const FDAWorldAssetId ConnectedSynaraAsset = MakeAssetId(12, 0, 0, 0);
        const FDAWorldAssetId OtherDistrictSource = MakeAssetId(13, 0, 0, 0);
        const FDAWorldAssetId OtherDistrictAsset = MakeAssetId(14, 0, 0, 0);

        Network.RegisterNode(EDAUtilityType::Data, DataSource, 10.f, 0.f);
        Network.RegisterNode(EDAUtilityType::Data, NeuralRelay, 0.f, 0.f);
        Network.RegisterNode(EDAUtilityType::Data, ConnectedSynaraAsset, 0.f, 4.f);
        Network.RegisterNode(EDAUtilityType::Data, OtherDistrictSource, 4.f, 0.f);
        Network.RegisterNode(EDAUtilityType::Data, OtherDistrictAsset, 0.f, 4.f);
        Network.ConnectNodes(EDAUtilityType::Data, DataSource, NeuralRelay);
        Network.ConnectNodes(EDAUtilityType::Data, NeuralRelay, ConnectedSynaraAsset);
        Network.ConnectNodes(EDAUtilityType::Data, OtherDistrictSource, OtherDistrictAsset);

        TestTrue("Synara asset begins with data", Network.ResolveUtility(EDAUtilityType::Data, ConnectedSynaraAsset).State == EDAUtilityState::FullySupplied);
        TestTrue("Other district begins with data", Network.ResolveUtility(EDAUtilityType::Data, OtherDistrictAsset).State == EDAUtilityState::FullySupplied);

        Network.SetNodeEnabled(EDAUtilityType::Data, NeuralRelay, false);

        TestTrue("Disconnected Synara asset is offline", Network.ResolveUtility(EDAUtilityType::Data, ConnectedSynaraAsset).State == EDAUtilityState::Offline);
        TestTrue("Unrelated district remains supplied", Network.ResolveUtility(EDAUtilityType::Data, OtherDistrictAsset).State == EDAUtilityState::FullySupplied);
    });

    It("reports road access from cached adjacency rather than a world scan", [this]()
    {
        FDARoadGraph Roads;
        const FDAWorldAssetId Depot = MakeAssetId(20, 0, 0, 0);
        const FDAWorldAssetId Junction = MakeAssetId(21, 0, 0, 0);

        Roads.AddNode(Depot);
        Roads.AddNode(Junction);
        Roads.ConnectNodes(Depot, Junction);

        TestTrue("Connected depot has road access", Roads.HasRoadAccess(Depot));
        Roads.SetNodeEnabled(Junction, false);
        TestFalse("Disabled road junction removes depot access", Roads.HasRoadAccess(Depot));
    });
}
