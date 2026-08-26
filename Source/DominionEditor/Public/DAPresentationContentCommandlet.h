#pragma once

#include "Commandlets/Commandlet.h"

#include "DAPresentationContentCommandlet.generated.h"

/** Imports/generates real presentation artifacts plus fingerprinted runtime definition packages. */
UCLASS()
class DOMINIONEDITOR_API UDAPresentationContentCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UDAPresentationContentCommandlet();
    virtual int32 Main(const FString& Params) override;
};
