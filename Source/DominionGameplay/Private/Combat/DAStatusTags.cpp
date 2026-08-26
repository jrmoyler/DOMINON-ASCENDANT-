#include "Combat/DAStatusTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Suppressed, "Status.Suppressed");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Shielded, "Status.Shielded");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Burning, "Status.Burning");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Disrupted, "Status.Disrupted");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Hacked, "Status.Hacked");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Regenerating, "Status.Regenerating");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Inspired, "Status.Inspired");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Routed, "Status.Routed");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Immobilized, "Status.Immobilized");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Marked, "Status.Marked");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Status_Downed, "Status.Downed");

const FGameplayTag& DAStatusTags::Suppressed() { return TAG_DA_Status_Suppressed; }
const FGameplayTag& DAStatusTags::Shielded() { return TAG_DA_Status_Shielded; }
const FGameplayTag& DAStatusTags::Burning() { return TAG_DA_Status_Burning; }
const FGameplayTag& DAStatusTags::Disrupted() { return TAG_DA_Status_Disrupted; }
const FGameplayTag& DAStatusTags::Hacked() { return TAG_DA_Status_Hacked; }
const FGameplayTag& DAStatusTags::Regenerating() { return TAG_DA_Status_Regenerating; }
const FGameplayTag& DAStatusTags::Inspired() { return TAG_DA_Status_Inspired; }
const FGameplayTag& DAStatusTags::Routed() { return TAG_DA_Status_Routed; }
const FGameplayTag& DAStatusTags::Immobilized() { return TAG_DA_Status_Immobilized; }
const FGameplayTag& DAStatusTags::Marked() { return TAG_DA_Status_Marked; }
const FGameplayTag& DAStatusTags::Downed() { return TAG_DA_Status_Downed; }
