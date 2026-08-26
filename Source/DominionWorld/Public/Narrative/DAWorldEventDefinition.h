#pragma once

#include "CoreMinimal.h"
#include "Narrative/DANarrativeRecords.h"

#include "DAWorldEventDefinition.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAWorldEventDefinition
{
    GENERATED_BODY()

    const FDAWorldEventStageDefinition* FindStage(FName StageId) const;
    bool Validate(FString& OutError) const;
    FDAWorldEventDefinitionManifest BuildManifest() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName EventId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName SourceDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event", meta = (ClampMin = "1"))
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    EDAWorldEventScope Scope = EDAWorldEventScope::Local;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    FName InitialStageId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Event")
    TArray<FDAWorldEventStageDefinition> Stages;
};
