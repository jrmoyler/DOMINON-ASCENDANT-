#pragma once

#include "CoreMinimal.h"

class FDADeckState;
class UDAContentRegistrySubsystem;

class DOMINIONCORE_API FDADeckRules
{
public:
    static bool Validate(
        const FDADeckState& Deck,
        const UDAContentRegistrySubsystem& ContentRegistry,
        TArray<FText>& Errors);

private:
    static int32 GetCopyLimit(const class UDA_CardDefinition& Definition);
};
