#pragma once

#include "Commandlets/Commandlet.h"

#include "DAFirstAscensionContentCommandlet.generated.h"

/** Strict real-source generation and cook-visible coverage for Task 25 assets. */
UCLASS()
class DOMINIONEDITOR_API UDAFirstAscensionContentCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UDAFirstAscensionContentCommandlet();
    virtual int32 Main(const FString& Params) override;
};
