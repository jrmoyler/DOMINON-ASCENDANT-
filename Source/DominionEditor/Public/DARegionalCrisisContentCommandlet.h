#pragma once

#include "Commandlets/Commandlet.h"

#include "DARegionalCrisisContentCommandlet.generated.h"

/** Creates real event/quest DataAssets at the canonical manifest paths; no placeholder uassets are checked in. */
UCLASS()
class DOMINIONEDITOR_API UDARegionalCrisisContentCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UDARegionalCrisisContentCommandlet();
    virtual int32 Main(const FString& Params) override;
};
