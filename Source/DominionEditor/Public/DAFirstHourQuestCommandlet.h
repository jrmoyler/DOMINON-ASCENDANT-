#pragma once

#include "Commandlets/Commandlet.h"

#include "DAFirstHourQuestCommandlet.generated.h"

/** Generates the nine quest Data Assets and Nia citizen asset from the canonical JSON authority. */
UCLASS()
class DOMINIONEDITOR_API UDAFirstHourQuestCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UDAFirstHourQuestCommandlet();
    virtual int32 Main(const FString& Params) override;
};
