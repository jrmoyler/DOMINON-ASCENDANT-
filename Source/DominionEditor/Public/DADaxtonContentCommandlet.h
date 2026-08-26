#pragma once

#include "Commandlets/Commandlet.h"

#include "DADaxtonContentCommandlet.generated.h"

/** Generates the Leader DataAsset and imports real Daxton character source packages. */
UCLASS()
class DOMINIONEDITOR_API UDADaxtonContentCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UDADaxtonContentCommandlet();
    virtual int32 Main(const FString& Params) override;
};
