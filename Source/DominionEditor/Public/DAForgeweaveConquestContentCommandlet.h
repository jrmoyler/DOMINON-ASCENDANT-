#pragma once

#include "Commandlets/Commandlet.h"

#include "DAForgeweaveConquestContentCommandlet.generated.h"

/** Generates real quest DataAssets from the frozen source manifest; never fabricates binary packages. */
UCLASS()
class DOMINIONEDITOR_API UDAForgeweaveConquestContentCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UDAForgeweaveConquestContentCommandlet();
    virtual int32 Main(const FString& Params) override;
};
