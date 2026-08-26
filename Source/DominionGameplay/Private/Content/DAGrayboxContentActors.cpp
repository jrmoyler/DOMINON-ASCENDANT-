#include "Content/DAGrayboxContentActors.h"

#include "Components/SceneComponent.h"

ADAGrayboxBuildingActor::ADAGrayboxBuildingActor()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GrayboxRoot")));
}

ADAGrayboxUnitActor::ADAGrayboxUnitActor()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GrayboxRoot")));
}

ADAGrayboxLeaderActor::ADAGrayboxLeaderActor()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GrayboxRoot")));
}

ADAGrayboxWonderActor::ADAGrayboxWonderActor()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GrayboxRoot")));
}

ADAFounderHallGrayboxActor::ADAFounderHallGrayboxActor()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GrayboxRoot")));
}
