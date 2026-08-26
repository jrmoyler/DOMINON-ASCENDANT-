#pragma once

#include "CoreMinimal.h"
#include "Narrative/DANarrativeRecords.h"

#include "DAQuestDefinition.generated.h"

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAQuestDefinition
{
    GENERATED_BODY()

    const FDAQuestNodeDefinition* FindNode(FName NodeId) const;
    bool Validate(FString& OutError) const;
    FDAQuestDefinitionManifest BuildManifest() const;
    static const TArray<EDAQuestNodeType>& GetSupportedNodeTypes();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName QuestId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName SourceDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest", meta = (ClampMin = "1"))
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    FName StartNodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TArray<FName> RequiredWorldAssetBindingIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Quest")
    TArray<FDAQuestNodeDefinition> Nodes;
};
