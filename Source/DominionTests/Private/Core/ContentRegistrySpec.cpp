#include "Content/DACardDefinition.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDAContentRegistrySpec, "Dominion.Core.ContentRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAContentRegistrySpec)

void FDAContentRegistrySpec::Define()
{
    It("reports duplicate stable card definition IDs", [this]()
    {
        UDA_CardDefinition* First = NewObject<UDA_CardDefinition>();
        First->DefinitionId = FName("synara.adaptive_habitat");
        First->bPlaceable = false;

        UDA_CardDefinition* Second = NewObject<UDA_CardDefinition>();
        Second->DefinitionId = FName("synara.adaptive_habitat");
        Second->bPlaceable = false;

        UDAContentRegistrySubsystem* Registry = NewObject<UDAContentRegistrySubsystem>();
        TArray<FText> Errors;
        TestFalse("Registry rejects duplicate stable IDs", Registry->RebuildFromDefinitions({ First, Second }, Errors));
        TestTrue("Duplicate ID reported", Errors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("Duplicate card definition ID"));
        }));
        TestEqual("First definition remains addressable", Registry->GetCardDefinition(First->GetPrimaryAssetId()), First);
        TestEqual("Duplicate objects share stable primary identity", First->GetPrimaryAssetId(), Second->GetPrimaryAssetId());
    });
}
