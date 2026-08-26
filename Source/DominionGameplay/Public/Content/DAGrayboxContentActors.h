#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DAGrayboxContentActors.generated.h"

/** Source-resolvable stand-ins used until art-owned Blueprint prefabs exist. */
UCLASS(Blueprintable)
class DOMINIONGAMEPLAY_API ADAGrayboxBuildingActor : public AActor
{
    GENERATED_BODY()

public:
    ADAGrayboxBuildingActor();
};

UCLASS(Blueprintable)
class DOMINIONGAMEPLAY_API ADAGrayboxUnitActor : public AActor
{
    GENERATED_BODY()

public:
    ADAGrayboxUnitActor();
};

UCLASS(Blueprintable)
class DOMINIONGAMEPLAY_API ADAGrayboxLeaderActor : public AActor
{
    GENERATED_BODY()

public:
    ADAGrayboxLeaderActor();
};

UCLASS(Blueprintable)
class DOMINIONGAMEPLAY_API ADAGrayboxWonderActor : public AActor
{
    GENERATED_BODY()

public:
    ADAGrayboxWonderActor();
};

UCLASS(Blueprintable)
class DOMINIONGAMEPLAY_API ADAFounderHallGrayboxActor : public AActor
{
    GENERATED_BODY()

public:
    ADAFounderHallGrayboxActor();
};
