#include "Combat/DAAbilitySystemComponent.h"

#include "Combat/DACombatAttributeSet.h"
#include "GameFramework/Actor.h"

UDAAbilitySystemComponent::UDAAbilitySystemComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    SetIsReplicatedByDefault(true);
}

void UDAAbilitySystemComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* Owner = GetOwner();
    if (Owner == nullptr || !Owner->HasAuthority())
    {
        return;
    }

    if (UDACombatAttributeSet* Attributes = GetSet<UDACombatAttributeSet>())
    {
        Attributes->AdvanceDownedRecovery(DeltaTime);
    }
}
