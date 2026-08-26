#pragma once

#include "Commandlets/Commandlet.h"

#include "DAUIAssetCommandlet.generated.h"

/** Generates Widget Blueprint and Enhanced Input caches from the canonical Task 21 manifest. */
UCLASS()
class DOMINIONEDITOR_API UDAUIAssetCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UDAUIAssetCommandlet();
    virtual int32 Main(const FString& Params) override;
};
