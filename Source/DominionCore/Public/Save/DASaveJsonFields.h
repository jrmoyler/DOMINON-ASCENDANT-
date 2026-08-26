#pragma once

#include "CoreMinimal.h"

// Envelope names are authored by the save service. Reflected FDACampaignSnapshot
// properties use FJsonObjectConverter's lower-camel field spelling.
namespace FDASaveJsonFields
{
    inline constexpr TCHAR SchemaVersion[] = TEXT("SchemaVersion");
    inline constexpr TCHAR ContentVersion[] = TEXT("contentVersion");
    inline constexpr TCHAR BuildVersion[] = TEXT("buildVersion");
    inline constexpr TCHAR Checksum[] = TEXT("Checksum");
    inline constexpr TCHAR Campaign[] = TEXT("Campaign");

    inline constexpr TCHAR HistoryTags[] = TEXT("historyTags");
    inline constexpr TCHAR OperationConflict[] = TEXT("operationConflict");
    inline constexpr TCHAR NarrativeState[] = TEXT("narrativeState");
    inline constexpr TCHAR ConquestState[] = TEXT("conquestState");
    inline constexpr TCHAR DaxtonState[] = TEXT("daxtonState");
    inline constexpr TCHAR AscensionState[] = TEXT("ascensionState");
    inline constexpr TCHAR CitySimulationState[] = TEXT("citySimulationState");
    inline constexpr TCHAR AppliedActionIds[] = TEXT("appliedActionIds");
    inline constexpr TCHAR ActionRecords[] = TEXT("actionRecords");
    inline constexpr TCHAR WorldState[] = TEXT("worldState");
    inline constexpr TCHAR Forgeweave[] = TEXT("forgeweave");
    inline constexpr TCHAR Initialized[] = TEXT("bInitialized");
    inline constexpr TCHAR CurrentWorldTick[] = TEXT("currentWorldTick");
    inline constexpr TCHAR Trade[] = TEXT("trade");
    inline constexpr TCHAR LastProcessedWorldTick[] = TEXT("lastProcessedWorldTick");
    inline constexpr TCHAR Capital[] = TEXT("capital");
    inline constexpr TCHAR WorldAssets[] = TEXT("worldAssets");
    inline constexpr TCHAR Resources[] = TEXT("resources");
    inline constexpr TCHAR Insight[] = TEXT("insight");
    inline constexpr TCHAR StructuralDamageRecords[] = TEXT("structuralDamageRecords");
    inline constexpr TCHAR CardDefinitionId[] = TEXT("cardDefinitionId");
}
