#include "Combat/DACoverSubsystem.h"
#include "Command/DACommandSubsystem.h"
#include "Units/DASquadEntity.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    struct FDACommandModeFixture
    {
        UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
        UDACommandSubsystem* Command = NewObject<UDACommandSubsystem>(GameInstance);
    };
}

BEGIN_DEFINE_SPEC(FDACommandModeSpec, "Dominion.Gameplay.CommandMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDACommandModeSpec)

void FDACommandModeSpec::Define()
{
    It("limits direct selection to three squads while the fourth remains under its doctrine", [this]()
    {
        FDACommandModeFixture Fixture;
        UDACommandSubsystem* Command = Fixture.Command;
        UDASquadEntity* First = NewObject<UDASquadEntity>();
        UDASquadEntity* Second = NewObject<UDASquadEntity>();
        UDASquadEntity* Third = NewObject<UDASquadEntity>();
        UDASquadEntity* Fourth = NewObject<UDASquadEntity>();
        Fourth->Initialize(false, EDASquadDoctrine::DefendCivilians);

        TestTrue("First squad registers", Command->RegisterSquad(First));
        TestTrue("Second squad registers", Command->RegisterSquad(Second));
        TestTrue("Third squad registers", Command->RegisterSquad(Third));
        TestTrue("Fourth squad registers", Command->RegisterSquad(Fourth));
        TestTrue("First squad enters direct selection", Command->TrySelectDirectly(First));
        TestTrue("Second squad enters direct selection", Command->TrySelectDirectly(Second));
        TestTrue("Third squad enters direct selection", Command->TrySelectDirectly(Third));
        TestFalse("A fourth squad cannot enter direct selection", Command->TrySelectDirectly(Fourth));
        TestEqual("Exactly three squads are directly selected", Command->GetDirectlySelectedSquadCount(), 3);
        TestFalse("The fourth squad is not directly controlled", Fourth->IsDirectlyControlled());
        TestEqual("The fourth squad retains its AI doctrine", static_cast<uint8>(Fourth->GetDoctrine()), static_cast<uint8>(EDASquadDoctrine::DefendCivilians));
    });

    It("allows one vehicle in addition to three directly selected squads", [this]()
    {
        FDACommandModeFixture Fixture;
        UDACommandSubsystem* Command = Fixture.Command;
        UDASquadEntity* Squads[] = {
            NewObject<UDASquadEntity>(), NewObject<UDASquadEntity>(), NewObject<UDASquadEntity>() };
        UDASquadEntity* Vehicle = NewObject<UDASquadEntity>();
        Vehicle->Initialize(true, EDASquadDoctrine::Hold);

        for (UDASquadEntity* Squad : Squads)
        {
            TestTrue("Squad registers", Command->RegisterSquad(Squad));
            TestTrue("Squad enters direct selection", Command->TrySelectDirectly(Squad));
        }
        TestTrue("Vehicle registers", Command->RegisterSquad(Vehicle));
        TestTrue("One vehicle may enter direct selection", Command->TrySelectDirectly(Vehicle));
        TestEqual("The squad cap remains three", Command->GetDirectlySelectedSquadCount(), 3);
        TestTrue("The vehicle selection slot is occupied", Command->HasDirectlySelectedVehicle());
    });

    It("maps the exact canonical morale thresholds", [this]()
    {
        UDASquadEntity* Squad = NewObject<UDASquadEntity>();

        Squad->SetMorale(100.f);
        TestEqual("100 morale is Inspired", static_cast<uint8>(Squad->GetMoraleState()), static_cast<uint8>(EDAMoraleState::Inspired));
        Squad->SetMorale(74.f);
        TestEqual("74 morale is Steady", static_cast<uint8>(Squad->GetMoraleState()), static_cast<uint8>(EDAMoraleState::Steady));
        Squad->SetMorale(49.f);
        TestEqual("49 morale is Shaken", static_cast<uint8>(Squad->GetMoraleState()), static_cast<uint8>(EDAMoraleState::Shaken));
        Squad->SetMorale(24.f);
        TestEqual("24 morale is Breaking", static_cast<uint8>(Squad->GetMoraleState()), static_cast<uint8>(EDAMoraleState::Breaking));
        Squad->SetMorale(0.f);
        TestEqual("0 morale is Rout", static_cast<uint8>(Squad->GetMoraleState()), static_cast<uint8>(EDAMoraleState::Rout));
    });

    It("owns tactical order state and rejects orders for doctrine-controlled squads", [this]()
    {
        FDACommandModeFixture Fixture;
        UDACommandSubsystem* Command = Fixture.Command;
        UDASquadEntity* Squad = NewObject<UDASquadEntity>();
        const FVector Destination(100.f, 200.f, 0.f);

        TestTrue("Squad registers", Command->RegisterSquad(Squad));
        TestFalse("A doctrine-controlled squad cannot receive a direct order", Command->IssueOrder(Squad, EDACommandOrder::Move, Destination));
        TestTrue("The squad enters direct selection", Command->TrySelectDirectly(Squad));
        TestTrue("A directly selected squad accepts a tactical order", Command->IssueOrder(Squad, EDACommandOrder::Move, Destination));
        TestEqual("The active order is retained by the squad tactical controller", static_cast<uint8>(Squad->GetActiveOrder()), static_cast<uint8>(EDACommandOrder::Move));
        TestTrue("The destination is retained by the squad tactical controller", Squad->GetDestination().Equals(Destination));
    });

    It("registers authored, ruin, and deployable cover deterministically and idempotently", [this]()
    {
        UWorld* CoverWorld = NewObject<UWorld>(GetTransientPackage());
        UDACoverSubsystem* Cover = NewObject<UDACoverSubsystem>(CoverWorld);
        const FVector AuthoredLocation(0.f, 0.f, 0.f);
        const FVector RuinLocation(100.f, 0.f, 0.f);
        const FVector DeployableLocation(200.f, 0.f, 0.f);

        TestTrue("An authored socket registers", Cover->RegisterAuthoredCoverSocket(TEXT("Barrier.A"), EDACoverType::Partial, AuthoredLocation));
        TestTrue("A selected ruin registers", Cover->RegisterRuinCover(TEXT("Ruin.A"), EDACoverType::Full, RuinLocation));
        TestTrue("A deployed barrier registers", Cover->RegisterDeployableCover(TEXT("Deployable.A"), EDACoverType::Hardened, DeployableLocation));
        TestTrue("Repeating the same authored registration is idempotent", Cover->RegisterAuthoredCoverSocket(TEXT("Barrier.A"), EDACoverType::Partial, AuthoredLocation));
        TestEqual("The registry contains one record per deterministic id", Cover->GetRegisteredCoverCount(), 3);
        TestFalse("An unnamed cover record is rejected", Cover->RegisterRuinCover(NAME_None, EDACoverType::Destructible, FVector::ZeroVector));
        TestFalse("A conflicting registration for an existing id is rejected", Cover->RegisterAuthoredCoverSocket(TEXT("Barrier.A"), EDACoverType::Destructible, AuthoredLocation));
        const FDACoverSocket* Authored = Cover->FindCoverSocket(TEXT("Barrier.A"));
        TestNotNull("The authored socket can be queried", Authored);
        if (Authored != nullptr)
        {
            TestEqual("The socket keeps its authored source", static_cast<uint8>(Authored->Source), static_cast<uint8>(EDACoverSource::Authored));
            TestEqual("The socket keeps its cover type", static_cast<uint8>(Authored->CoverType), static_cast<uint8>(EDACoverType::Partial));
        }
    });

    It("removes cover only from its registered source and permits deterministic re-registration", [this]()
    {
        UWorld* CoverWorld = NewObject<UWorld>(GetTransientPackage());
        UDACoverSubsystem* Cover = NewObject<UDACoverSubsystem>(CoverWorld);
        const FVector Location(300.f, 0.f, 0.f);

        TestTrue("An authored socket registers", Cover->RegisterAuthoredCoverSocket(TEXT("Barrier.Remove"), EDACoverType::Destructible, Location));
        TestFalse("A ruin removal cannot remove an authored socket", Cover->UnregisterRuinCover(TEXT("Barrier.Remove")));
        TestTrue("The matching source removes its socket", Cover->UnregisterAuthoredCoverSocket(TEXT("Barrier.Remove")));
        TestEqual("Exact-once removal clears the record", Cover->GetRegisteredCoverCount(), 0);
        TestFalse("A removed socket cannot be removed twice", Cover->UnregisterAuthoredCoverSocket(TEXT("Barrier.Remove")));
        TestTrue("The stable id may be re-registered after removal", Cover->RegisterAuthoredCoverSocket(TEXT("Barrier.Remove"), EDACoverType::Destructible, Location));
        TestEqual("Re-registration creates exactly one record", Cover->GetRegisteredCoverCount(), 1);
    });

    It("keeps classification and ownership immutable while registered and blocks cross-subsystem cap bypass", [this]()
    {
        FDACommandModeFixture FirstFixture;
        FDACommandModeFixture SecondFixture;
        UDASquadEntity* Squad = NewObject<UDASquadEntity>();

        Squad->Initialize(false, EDASquadDoctrine::Hold);
        TestTrue("The first subsystem registers the squad", FirstFixture.Command->RegisterSquad(Squad));
        TestFalse("Reclassification fails after registration", Squad->Initialize(true, EDASquadDoctrine::AggressiveAssault));
        TestFalse("Registered squad remains a squad", Squad->IsVehicle());
        TestEqual("Registered squad retains its doctrine", static_cast<uint8>(Squad->GetDoctrine()), static_cast<uint8>(EDASquadDoctrine::Hold));
        TestFalse("A second subsystem cannot claim the squad", SecondFixture.Command->RegisterSquad(Squad));
        TestTrue("The owner can select the squad", FirstFixture.Command->TrySelectDirectly(Squad));
        TestFalse("A second subsystem cannot select the foreign squad", SecondFixture.Command->TrySelectDirectly(Squad));
        TestEqual("A foreign selection cannot consume or bypass the second cap", SecondFixture.Command->GetDirectlySelectedSquadCount(), 0);

        TestTrue("Unregister succeeds for the owning subsystem", FirstFixture.Command->UnregisterSquad(Squad));
        TestFalse("Unregister clears direct control", Squad->IsDirectlyControlled());
        TestEqual("Unregister removes the squad selection slot", FirstFixture.Command->GetDirectlySelectedSquadCount(), 0);
        TestTrue("Unregister permits explicit reclassification", Squad->Initialize(true, EDASquadDoctrine::AggressiveAssault));
        TestTrue("A second subsystem can register the released entity", SecondFixture.Command->RegisterSquad(Squad));
        TestTrue("The reclassified vehicle fills the second subsystem vehicle slot", SecondFixture.Command->TrySelectDirectly(Squad));
        TestTrue("The second owner can unregister the selected vehicle", SecondFixture.Command->UnregisterSquad(Squad));
        TestFalse("Unregister clears the vehicle selection slot", SecondFixture.Command->HasDirectlySelectedVehicle());
        TestFalse("Unregister clears vehicle direct control", Squad->IsDirectlyControlled());
    });

    It("returns released and unregistered squads to doctrine fallback without changing morale or supply", [this]()
    {
        FDACommandModeFixture Fixture;
        UDASquadEntity* ReleasedSquad = NewObject<UDASquadEntity>();
        UDASquadEntity* UnregisteredSquad = NewObject<UDASquadEntity>();
        AActor* ReleasedTarget = NewObject<AActor>();
        AActor* UnregisteredTarget = NewObject<AActor>();
        const FVector ReleasedDestination(400.f, 100.f, 0.f);
        const FVector UnregisteredDestination(500.f, 100.f, 0.f);

        ReleasedSquad->SetMorale(49.f);
        ReleasedSquad->SetSupply(37.f);
        ReleasedSquad->SetTarget(ReleasedTarget);
        TestTrue("Released squad registers", Fixture.Command->RegisterSquad(ReleasedSquad));
        TestTrue("Released squad enters direct selection", Fixture.Command->TrySelectDirectly(ReleasedSquad));
        TestTrue("Released squad receives an attack order", Fixture.Command->IssueOrder(ReleasedSquad, EDACommandOrder::Attack, ReleasedDestination));
        TestTrue("Release succeeds", Fixture.Command->ReleaseDirectSelection(ReleasedSquad));
        TestFalse("Release clears direct control", ReleasedSquad->IsDirectlyControlled());
        TestEqual("Release restores the doctrine fallback order", static_cast<uint8>(ReleasedSquad->GetActiveOrder()), static_cast<uint8>(EDACommandOrder::Hold));
        TestTrue("Release clears the direct destination", ReleasedSquad->GetDestination().IsZero());
        TestNull("Release clears the direct target", ReleasedSquad->GetTarget());
        TestEqual("Release preserves morale", ReleasedSquad->GetMorale(), 49.f);
        TestEqual("Release preserves supply", ReleasedSquad->GetSupply(), 37.f);

        UnregisteredSquad->SetMorale(24.f);
        UnregisteredSquad->SetSupply(63.f);
        UnregisteredSquad->SetTarget(UnregisteredTarget);
        TestTrue("Unregistered squad registers", Fixture.Command->RegisterSquad(UnregisteredSquad));
        TestTrue("Unregistered squad enters direct selection", Fixture.Command->TrySelectDirectly(UnregisteredSquad));
        TestTrue("Unregistered squad receives a move order", Fixture.Command->IssueOrder(UnregisteredSquad, EDACommandOrder::Move, UnregisteredDestination));
        TestTrue("Unregister succeeds", Fixture.Command->UnregisterSquad(UnregisteredSquad));
        TestFalse("Unregister clears direct control", UnregisteredSquad->IsDirectlyControlled());
        TestEqual("Unregister restores the doctrine fallback order", static_cast<uint8>(UnregisteredSquad->GetActiveOrder()), static_cast<uint8>(EDACommandOrder::Hold));
        TestTrue("Unregister clears the direct destination", UnregisteredSquad->GetDestination().IsZero());
        TestNull("Unregister clears the direct target", UnregisteredSquad->GetTarget());
        TestEqual("Unregister preserves morale", UnregisteredSquad->GetMorale(), 24.f);
        TestEqual("Unregister preserves supply", UnregisteredSquad->GetSupply(), 63.f);
    });

    It("caps control-zone CP at six and makes invalid spending atomic", [this]()
    {
        FDACommandModeFixture Fixture;
        UDACommandSubsystem* Command = Fixture.Command;

        TestEqual("Baseline CP maximum is six", Command->GetMaximumCommandPoints(), 6);
        TestEqual("Command begins with no temporary CP", Command->GetCommandPoints(), 0);
        TestFalse("A control zone cannot award zero CP", Command->AwardControlZoneCommandPoints(0));
        TestFalse("A control zone cannot award negative CP", Command->AwardControlZoneCommandPoints(-1));
        TestTrue("A control zone awards CP", Command->AwardControlZoneCommandPoints(4));
        TestEqual("Awarded CP is tracked", Command->GetCommandPoints(), 4);
        TestTrue("Further control-zone rewards are accepted", Command->AwardControlZoneCommandPoints(10));
        TestEqual("Control-zone rewards clamp to the maximum", Command->GetCommandPoints(), 6);
        TestFalse("Zero-cost spending is invalid", Command->TrySpendCommandPoints(0));
        TestEqual("Invalid zero-cost spending changes nothing", Command->GetCommandPoints(), 6);
        TestFalse("Negative-cost spending is invalid", Command->TrySpendCommandPoints(-1));
        TestEqual("Invalid negative-cost spending changes nothing", Command->GetCommandPoints(), 6);
        TestFalse("Overspending is rejected", Command->TrySpendCommandPoints(7));
        TestEqual("Rejected overspending changes nothing", Command->GetCommandPoints(), 6);
        TestTrue("A valid tactical spend succeeds", Command->TrySpendCommandPoints(2));
        TestEqual("A valid tactical spend is deducted exactly once", Command->GetCommandPoints(), 4);
    });
}
