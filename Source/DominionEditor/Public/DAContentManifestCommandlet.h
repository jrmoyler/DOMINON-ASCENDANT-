#pragma once

#include "Commandlets/Commandlet.h"

#include "DAContentManifestCommandlet.generated.h"

/**
 * Creates the editor .uasset cache from the canonical JSON source.
 * Usage: UnrealEditor-Cmd DominionAscendant.uproject -run=DAContentManifest
 */
UCLASS()
class DOMINIONEDITOR_API UDAContentManifestCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UDAContentManifestCommandlet();
    virtual int32 Main(const FString& Params) override;
};
