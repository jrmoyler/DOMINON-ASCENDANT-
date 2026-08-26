#pragma once

#include "NativeGameplayTags.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Suppressed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Shielded);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Burning);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Disrupted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Hacked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Regenerating);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Inspired);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Routed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Immobilized);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Marked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Status_Downed);

namespace DAStatusTags
{
    DOMINIONGAMEPLAY_API const FGameplayTag& Suppressed();
    DOMINIONGAMEPLAY_API const FGameplayTag& Shielded();
    DOMINIONGAMEPLAY_API const FGameplayTag& Burning();
    DOMINIONGAMEPLAY_API const FGameplayTag& Disrupted();
    DOMINIONGAMEPLAY_API const FGameplayTag& Hacked();
    DOMINIONGAMEPLAY_API const FGameplayTag& Regenerating();
    DOMINIONGAMEPLAY_API const FGameplayTag& Inspired();
    DOMINIONGAMEPLAY_API const FGameplayTag& Routed();
    DOMINIONGAMEPLAY_API const FGameplayTag& Immobilized();
    DOMINIONGAMEPLAY_API const FGameplayTag& Marked();
    DOMINIONGAMEPLAY_API const FGameplayTag& Downed();
}
