#pragma once

#include "CoreMinimal.h"
#include "Save/DASaveSchema.h"

class DOMINIONCORE_API FDASaveMigration
{
public:
    // Migrates the JSON envelope one version at a time and preserves unrecognized fields.
    static FDASaveResult MigrateToCurrent(FString& SaveDocument);
};
